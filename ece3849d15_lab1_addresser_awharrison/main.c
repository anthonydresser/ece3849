/*
 * main.c
 */

#include "inc/hw_types.h"
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_sysctl.h"
#include "inc/lm3s8962.h"
#include "driverlib/sysctl.h"
#include "drivers/rit128x96x4.h"
#include "frame_graphics.h"
#include "utils/ustdlib.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"
#include "driverlib/gpio.h"
#include "buttons.h"
#include "driverlib/adc.h"
#include <math.h>

#define BUTTON_CLOCK 200 // button scanning interrupt rate in Hz
#define ADC_BUFFER_SIZE 2048 // must be a power of 2
#define ADC_BUFFER_WRAP(i) ((i) & (ADC_BUFFER_SIZE - 1)) // index wrapping macro
#define	ADC_BITS	10
#define PIXELS_PER_DIV	64
#define	VIN_RANGE	6
#define ADC_OFFSET	512
#define FIFO_SIZE 10		// FIFO capacity is 1 item fewer

// Global Variables
unsigned long g_ulSystemClock; // system clock frequency in Hz
volatile int g_iADCBufferIndex = ADC_BUFFER_SIZE - 1; // latest sample index
volatile unsigned short g_pusADCBuffer[ADC_BUFFER_SIZE]; // circular buffer
volatile unsigned long g_ulADCErrors = 0; // number of missed ADC deadlines
const char * const g_ppcVoltageScaleStr[] = {"100 mV", "200 mV", "500 mV", "1 V"};
const char * const g_ppcTimeScaleStr[] = {"24 us"};
volatile int state = 0;

typedef char DataType;		// FIFO data type
volatile DataType fifo[FIFO_SIZE];	// FIFO storage array
volatile int fifo_head = 0;	// index of the first item in the FIFO
volatile int fifo_tail = 0;	// index one step past the last item

// Function Prototypes
void TimerISR(void);
void ADC_ISR(void);
int fifo_put(DataType data);
int fifo_get(DataType *data);
unsigned long cpu_load_count(void);

// CPU load counters
unsigned long count_unloaded = 0;
unsigned long count_loaded = 0;
float cpu_load = 0.0;

int main(void) {

	// Local Variables
	int trigger_val = 512; // 10-bit ADC
	float fVoltsPerDiv[] = {1, 2, 5, 10};
	int temp_index;
	int y;
	int prevy;
	float fScale;
	char pcStr[50]; // string buffer
	int trigger_state = 0; //state for trigger, 0 = up trigger; 1 = down trigger
	int selection_state = 0; //state for selection, 0 = time scale, 1 = voltage scale, 2 = trigger state
	int voltage_state = 0; //state for voltage scale, 0 = 100mv, 1 = 200mv, 2 = 500mv, 3 = 1v
	int timing_state = 0; // 0 = 2us
	//	unsigned short temp_buffer[ADC_BUFFER_SIZE];
	int p_buffer[FRAME_SIZE_X]; // number of pixles that can be on the screen at one time
	int i, j;
	unsigned long ulDivider, ulPrescaler;
	DataType c;
	int cpu_load_int = 0;
	// initialize the clock generator
	IntMasterDisable();
	if (REVISION_IS_A2)
		SysCtlLDOSet(SYSCTL_LDO_2_75V);
	// sets the output of LDO to 2.75V

	SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN |
			SYSCTL_XTAL_8MHZ);
	// SYSCTL_SYSDIVE_4 divides the system clock source frequency by 4 to give = 50MHz
	// system oscillator is main
	// external frequency is PLL (phase-locked loop) using an 8MHz crystal

	g_ulSystemClock = SysCtlClockGet(); // gets processor clock rate
	// also the clock rate of the majority of peripheral modules

	RIT128x96x4Init(3500000); // initialize the OLED display
	// initializes SSI/SPI interface to OLED to 3.5MHz clock

	// initialization code
	// initialize a general purpose timer for periodic interrupts
	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0); // enables TIMER0
	TimerDisable(TIMER0_BASE, TIMER_BOTH);
	// TIMER0_BASE is the base address of the timer module
	// TIMER_BOTH disables TIMER_A and TIMER_B
	TimerConfigure(TIMER0_BASE, TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_PERIODIC);
	// configures timer
	// TIMER0_BASE is the base address of the timer module
	// TIMER_CFG_SPLIT_PAIR -> two half-width timers, must configure timers separately
	// TIMER_CFG_A_PERIODIC -> sets TIMER_A to a half-width timer

	// prescaler for a 16-bit timer
	ulPrescaler = (g_ulSystemClock / BUTTON_CLOCK - 1) >> 16;
	// system frequency div by 199 right shifted 16 bits

	// 16-bit divider (timer load value)
	ulDivider = g_ulSystemClock / (BUTTON_CLOCK * (ulPrescaler + 1)) - 1;
	TimerLoadSet(TIMER0_BASE, TIMER_A, ulDivider);
	// if the timer is running, then the ulDivider is immediately loaded into the timer
	TimerPrescaleSet(TIMER0_BASE, TIMER_A, ulPrescaler);
	// ulPrescaler must be between 0 and 255 for 16/32-bit timers
	TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
	// enables interrupt when TIMER A times out aka every 10ms
	TimerEnable(TIMER0_BASE, TIMER_A);

	IntPrioritySet(INT_TIMER0A, 32); // 0 = highest priority, 32 = next lower
	IntEnable(INT_TIMER0A);

	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER3);
	TimerDisable(TIMER3_BASE, TIMER_BOTH);
	TimerConfigure(TIMER3_BASE, TIMER_CFG_ONE_SHOT);
	TimerLoadSet(TIMER3_BASE, TIMER_A, (g_ulSystemClock/50) - 1); // 1 sec interval

	SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0); // enable the ADC
	SysCtlADCSpeedSet(SYSCTL_ADCSPEED_500KSPS); // specify 500ksps
	ADCSequenceDisable(ADC0_BASE, 0); // choose ADC sequence 0; disable before configuring
	ADCSequenceConfigure(ADC0_BASE, 0, ADC_TRIGGER_ALWAYS, 0); // specify the "Always" trigger
	ADCSequenceStepConfigure(ADC0_BASE, 0, 0, ADC_CTL_CH0 | ADC_CTL_IE | ADC_CTL_END); // in the 0th step, sample channel 0
	// enable interrupt, and make it the end of sequence
	ADCIntEnable(ADC0_BASE, 0); // enable ADC interrupt from sequence 0
	ADCSequenceEnable(ADC0_BASE, 0); // enable the sequence. it is now sampling
	IntPrioritySet(INT_ADC0, 0); // set ADC interrupt priority in the interrupt controller
	IntEnable(INT_ADC0); // enable ADC interrupt

	// configure GPIO used to read the state of the on-board push buttons
	// select button
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
	GPIOPinTypeGPIOInput(GPIO_PORTF_BASE, GPIO_PIN_1);
	GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_1, GPIO_STRENGTH_2MA,
			GPIO_PIN_TYPE_STD_WPU);
	// up button
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
	GPIOPinTypeGPIOInput(GPIO_PORTE_BASE, GPIO_PIN_0);
	GPIOPadConfigSet(GPIO_PORTE_BASE, GPIO_PIN_0, GPIO_STRENGTH_2MA,
			GPIO_PIN_TYPE_STD_WPU);
	//left button
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
	GPIOPinTypeGPIOInput(GPIO_PORTE_BASE, GPIO_PIN_2);
	GPIOPadConfigSet(GPIO_PORTE_BASE, GPIO_PIN_2, GPIO_STRENGTH_2MA,
			GPIO_PIN_TYPE_STD_WPU);
	//right button
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
	GPIOPinTypeGPIOInput(GPIO_PORTE_BASE, GPIO_PIN_3);
	GPIOPadConfigSet(GPIO_PORTE_BASE, GPIO_PIN_3, GPIO_STRENGTH_2MA,
			GPIO_PIN_TYPE_STD_WPU);
	//down button
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
	GPIOPinTypeGPIOInput(GPIO_PORTE_BASE, GPIO_PIN_1);
	GPIOPadConfigSet(GPIO_PORTE_BASE, GPIO_PIN_1, GPIO_STRENGTH_2MA,
			GPIO_PIN_TYPE_STD_WPU);

	fScale = (VIN_RANGE * PIXELS_PER_DIV)/((1 << ADC_BITS) * fVoltsPerDiv[0]);
	count_unloaded = cpu_load_count();
	IntMasterEnable();

	while (true) {
		count_loaded = cpu_load_count();
		cpu_load = 1.0 - (float)count_loaded/count_unloaded; // compute CPU load
		FillFrame(0);
		// Draw Grid
		for(i = 0; i <= 96; i += 12) {
			DrawLine(1, i, 128, i, 3);
		}
		for(i = 4; i <= 128; i += 12) {
			DrawLine(i, 1, i, 96, 3);
		}
		DrawLine(1, 48, 128, 48, 8);
		DrawLine(64, 1, 64, 96, 8);

		if(fifo_get(&c)) {
			switch(c) {
			case 'U':
				switch(selection_state){
				// case 0 = timing selection, case 1 = voltage selection, case 2 = trigger selection
				case 0:
					// timing state machine only has one state
					break;
				case 1:
					switch(voltage_state){
					// increments the voltage state 0 = 100mV/div, 1 = 200mV/div, 2 = 500mv/div, 3 = 1V/div
					case 0:
						voltage_state = 1;
						break;
					case 1:
						voltage_state = 2;
						break;
					case 2:
						voltage_state = 3;
						break;
					}
					break;
				case 2:
					// switches the trigger state
					trigger_state = !trigger_state;
					break;
				}
				break;
			case 'D':
				switch(selection_state){
				// decrements the voltage state
					case 0:
						break;
					case 1:
						switch(voltage_state){
						case 1:
							voltage_state = 0;
							break;
						case 2:
							voltage_state = 1;
							break;
						case 3:
							voltage_state = 2;
							break;
						}
						break;
					case 2:
						trigger_state = !trigger_state;
						break;
					}
				break;
			case 'L':
				// cycles through the selection states 2, 1, 0, 2, 1, 0
				switch(selection_state) {
				case 0:
					selection_state = 2;
					break;
				case 1:
					selection_state = 0;
					break;
				case 2:
					selection_state = 1;
					break;
				}
				break;
			case 'R':
				// cycles through the selection states 0, 1, 2, 0, 1, 2
				switch(selection_state) {
				case 0:
					selection_state = 1;
					break;
				case 1:
					selection_state = 2;
					break;
				case 2:
					selection_state = 0;
					break;
				}
				break;
			case 'S':
				// regardless of selection state, will change the trigger
				trigger_state = !trigger_state;
				break;
			}
		}
		fScale = (VIN_RANGE * PIXELS_PER_DIV)/((1 << ADC_BITS) * fVoltsPerDiv[voltage_state]); // fscale depends on the voltage state
		switch(selection_state) {
		case 0:
			// highlights the time division on the OLED screen
			DrawFilledRectangle(3, 0, 34, 8, 5);
			break;
		case 1:
			// highlights the voltage division on the OLED screen
			DrawFilledRectangle(47, 0, 90, 8, 5);
			break;
		case 2:
			// highlights the trigger state on the OLED screen
			DrawFilledRectangle(108, 0, 122, 14, 5);
			break;
		}
		if(!trigger_state) {
			// draws an upward trigger on the screen
			DrawLine(110, 12, 116, 12, 15);
			DrawLine(116, 12, 116, 2, 15);
			DrawLine(116, 2, 122, 2, 15);
			DrawLine(116, 4, 112, 8, 15);
			DrawLine(116, 4, 120, 8, 15);
		} else {
			// draws a downwards trigger on the screen
			DrawLine(110, 2, 116, 2, 15);
			DrawLine(116, 12, 116, 2, 15);
			DrawLine(116, 12, 122, 12, 15);
			DrawLine(116, 9, 112, 5, 15);
			DrawLine(116, 9, 120, 5, 15);
		}
		DrawString(50, 0, g_ppcVoltageScaleStr[voltage_state], 15, false);
		DrawString(5, 0, g_ppcTimeScaleStr[timing_state], 15, false);
		// half screen width behind most recent sample is sample 1984
		// 1/2 ADC units is 512
		temp_index = g_iADCBufferIndex;
		if(!trigger_state){
			for(i = ADC_BUFFER_WRAP(temp_index - FRAME_SIZE_X/2); i != (temp_index - 1024 - FRAME_SIZE_X/2); i--) {
				if((g_pusADCBuffer[ADC_BUFFER_WRAP(i)] < trigger_val) && (g_pusADCBuffer[ADC_BUFFER_WRAP(i + 1)] > trigger_val)) { // triggers on the rising edge
					for(j = 0; j < FRAME_SIZE_X; j++) {
						p_buffer[j] = g_pusADCBuffer[ADC_BUFFER_WRAP(i - 64  + j)];
					}
				}
			}
		} else {
			for(i = ADC_BUFFER_WRAP(temp_index - FRAME_SIZE_X/2); i != (temp_index - 1024 - FRAME_SIZE_X/2); i--) {
				if((g_pusADCBuffer[ADC_BUFFER_WRAP(i)] > trigger_val) && (g_pusADCBuffer[ADC_BUFFER_WRAP(i + 1)] < trigger_val)) { // triggers on the falling edge
					for(j = 0; j < FRAME_SIZE_X; j++) {
						p_buffer[j] = g_pusADCBuffer[ADC_BUFFER_WRAP(i - 64  + j)];
					}
				}
			}
		}
		prevy = FRAME_SIZE_Y/2 - (int)round((p_buffer[0] - ADC_OFFSET) * fScale);
		for(i = 1; i < FRAME_SIZE_X; i++) {
			y = FRAME_SIZE_Y/2 - (int)round((p_buffer[i] - ADC_OFFSET) * fScale);
			//			DrawPoint(i, y, 15);
			DrawLine(i-1, prevy, i, y, 15);
			prevy = y;
		}
		cpu_load_int = (int)(cpu_load*1000.0);
		usprintf(pcStr, "CPU Load = %02d.%01d%%", cpu_load_int/10, cpu_load_int % 10); // convert time to string
		DrawString(0, 86, pcStr, 15, false); // draw string to frame buffer
		RIT128x96x4ImageDraw(g_pucFrame, 0, 0, FRAME_SIZE_X, FRAME_SIZE_Y);
	}

	return 0;
}


void ADC_ISR(void) {
	ADC_ISC_R = ADC_ISC_IN0;// clear ADC sequence0 interrupt flag in the ADCISC register
	if (ADC0_OSTAT_R & ADC_OSTAT_OV0) { // check for ADC FIFO overflow
		g_ulADCErrors++; // count errors - step 1 of the signoff
		ADC0_OSTAT_R = ADC_OSTAT_OV0; // clear overflow condition
	}
	int buffer_index = ADC_BUFFER_WRAP(g_iADCBufferIndex + 1);
	g_pusADCBuffer[buffer_index] = ADC_SSFIFO0_R; // read sample from the ADC sequence0 FIFO
	g_iADCBufferIndex = buffer_index;
}

// put data into the FIFO, skip if full
// returns 1 on success, 0 if FIFO was full
int fifo_put(DataType data)
{
	int new_tail = fifo_tail + 1;
	if (new_tail >= FIFO_SIZE) new_tail = 0; // wrap around
	if (fifo_head != new_tail) {	// if the FIFO is not full
		fifo[fifo_tail] = data;		// store data into the FIFO
		fifo_tail = new_tail;		// advance FIFO tail index
		return 1;					// success
	}
	return 0;	// full
}

// get data from the FIFO
// returns 1 on success, 0 if FIFO was empty
int fifo_get(DataType *data)
{
	if (fifo_head != fifo_tail) {	// if the FIFO is not empty
		*data = fifo[fifo_head];	// read data from the FIFO
		if(fifo_head +1 >= FIFO_SIZE)
			fifo_head = 0;
		else
			fifo_head++;
		return 1;					// success
	}
	return 0;	// empty
}

void TimerISR(void) {
	unsigned long presses = g_ulButtons;
	TIMER0_ICR_R = TIMER_ICR_TATOCINT; // clear interrupt flag
	ButtonDebounce(
			//up button
			(~GPIO_PORTE_DATA_R & GPIO_PIN_0) +
			//left button
			(~GPIO_PORTE_DATA_R & GPIO_PIN_2) +
			//right button
			(~GPIO_PORTE_DATA_R & GPIO_PIN_3) +
			//down button
			(~GPIO_PORTE_DATA_R & GPIO_PIN_1) +
			//select button
			((~GPIO_PORTF_DATA_R & GPIO_PIN_1) << 3));
	presses = ~presses & g_ulButtons; // button press detector
	if (presses & 1) { // up button pressed
		fifo_put('U');
	}
	if (presses & 2) { // down button
		fifo_put('D');
	}
	if (presses & 4) { // left button
		fifo_put('L');
	}
	if (presses & 8) { // right button
		fifo_put('R');
	}
	if (presses & 16) { //select button
		fifo_put('S');
	}
}

unsigned long cpu_load_count(void)
{
	unsigned long i = 0;
	TimerIntClear(TIMER3_BASE, TIMER_TIMA_TIMEOUT);
	TimerEnable(TIMER3_BASE, TIMER_A); // start one-shot timer
	while (!(TimerIntStatus(TIMER3_BASE, 0) & TIMER_TIMA_TIMEOUT))
		i++;
	return i;
}
