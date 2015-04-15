/*
 *  ======== main.c ========
 */

#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>

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

typedef char DataType;		// button/mailbox data type

#define ADC_BUFFER_SIZE 2048 // must be a power of 2
#define ADC_BUFFER_WRAP(i) ((i) & (ADC_BUFFER_SIZE - 1)) // index wrapping macro
#define	ADC_BITS	10
#define PIXELS_PER_DIV	64
#define	VIN_RANGE	6
#define ADC_OFFSET	512

// Global Variables
unsigned long g_ulSystemClock; // system clock frequency in Hz
volatile int g_iADCBufferIndex = ADC_BUFFER_SIZE - 1; // latest sample index
volatile unsigned short g_pusADCBuffer[ADC_BUFFER_SIZE]; // circular buffer
volatile unsigned long g_ulADCErrors = 0; // number of missed ADC deadlines
const char * const g_ppcVoltageScaleStr[] = {"100 mV", "200 mV", "500 mV", "1 V"};
const char * const g_ppcTimeScaleStr[] = {"24 us"};
volatile int state = 0;
int cpu_load_int = 0;
int p_buffer[FRAME_SIZE_X]; // number of pixles that can be on the screen at one time

// CPU load counters
unsigned long count_unloaded = 0;
unsigned long count_loaded = 0;
float cpu_load = 0.0;

//// probably will be moved to tasks
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

// function prototypes
unsigned long cpu_load_count(void);
/*
 *  ======== main ========
 */
Void main()
{
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

	SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0); // enable the ADC
	SysCtlADCSpeedSet(SYSCTL_ADCSPEED_500KSPS); // specify 500ksps
	ADCSequenceDisable(ADC0_BASE, 0); // choose ADC sequence 0; disable before configuring
	ADCSequenceConfigure(ADC0_BASE, 0, ADC_TRIGGER_ALWAYS, 0); // specify the "Always" trigger
	ADCSequenceStepConfigure(ADC0_BASE, 0, 0, ADC_CTL_CH0 | ADC_CTL_IE | ADC_CTL_END); // in the 0th step, sample channel 0
	// enable interrupt, and make it the end of sequence
	ADCIntEnable(ADC0_BASE, 0); // enable ADC interrupt from sequence 0
	ADCSequenceEnable(ADC0_BASE, 0); // enable the sequence. it is now sampling
	// IntPrioritySet(INT_ADC0, 0); // set ADC interrupt priority in the interrupt controller
	// IntEnable(INT_ADC0); // enable ADC interrupt

	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER3);
		TimerDisable(TIMER3_BASE, TIMER_BOTH);
		TimerConfigure(TIMER3_BASE, TIMER_CFG_ONE_SHOT);
		TimerLoadSet(TIMER3_BASE, TIMER_A, (g_ulSystemClock/50) - 1); // 1 sec interval

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

	BIOS_start();     /* enable interrupts and start SYS/BIOS */
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

void TimerISR(void) {
	Semaphore_post(sem_button);
}

void buttonTask(UArg arg0, UArg arg1) {
	unsigned long presses = g_ulButtons;
	DataType button;
	while(1) {
		Semaphore_pend(sem_button, BIOS_WAIT_FOREVER);
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
			button = 'U';
		}
		if (presses & 2) { // down button
			button = 'D';
		}
		if (presses & 4) { // left button
			button = 'L';
		}
		if (presses & 8) { // right button
			button = 'R';
		}
		if (presses & 16) { //select button
			button = 'S';
		}
		Mailbox_post(mailbox0, &button, BIOS_NO_WAIT);
	}
}

void displayTask(UArg arg0, UArg arg1) {
	while(1) {
		Semaphore_pend(sem_display, BIOS_WAIT_FOREVER);
		Semaphore_pend(sem_buf, BIOS_WAIT_FOREVER);
		prevy = FRAME_SIZE_Y/2 - (int)round((p_buffer[0] - ADC_OFFSET) * fScale);
		for(i = 1; i < FRAME_SIZE_X; i++) {
			y = FRAME_SIZE_Y/2 - (int)round((p_buffer[i] - ADC_OFFSET) * fScale);
			//			DrawPoint(i, y, 15);
			DrawLine(i-1, prevy, i, y, 15);
			prevy = y;
		}
		Semaphore_post(sem_buf);
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
		usprintf(pcStr, "CPU Load = %02d.%01d%%", cpu_load_int/10, cpu_load_int % 10); // convert time to string
		DrawString(0, 86, pcStr, 15, false); // draw string to frame buffer
		RIT128x96x4ImageDraw(g_pucFrame, 0, 0, FRAME_SIZE_X, FRAME_SIZE_Y);
		Semaphore_post(sem_waveform);
	}
}

// pends on the button mailbox
// when it receives a command, it modifies the oscilloscope settings and
// the signal display
void inputTask(UArg arg0, UArg arg1) {
	DataType getButton;
	while(1) {
		Mailbox_pend(mailbox0, &getButton, BIOS_WAIT_FOREVER);
		switch(getButton) {
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
	Semaphore_post(sem_display);
}

void waveformTask(UArg arg0, UArg arg1) {
	int temp_index;
	int temp_trigger_state;
	while(1) {
		Semaphore_pend(sem_waveform, BIOS_WAIT_FOREVER);
		temp_index = g_iADCBufferIndex;
		temp_trigger_state = trigger_state;
		Semaphore_pend(sem_buf, BIOS_WAIT_FOREVER);
		if(!temp_trigger_state){
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
		Semaphore_post(sem_buf);
		Semaphore_post(sem_display);
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
