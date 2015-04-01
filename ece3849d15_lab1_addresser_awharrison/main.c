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

// #define BUTTON_CLOCK 200 // button scanning interrupt rate in Hz
#define ADC_BUFFER_SIZE 2048 // must be a power of 2
#define ADC_BUFFER_WRAP(i) ((i) & (ADC_BUFFER_SIZE - 1)) // index wrapping macro
#define SCREEN_SIZE	128

// Global Variables
unsigned long g_ulSystemClock; // system clock frequency in Hz
volatile int g_iADCBufferIndex = ADC_BUFFER_SIZE - 1; // latest sample index
volatile unsigned short g_pusADCBuffer[ADC_BUFFER_SIZE]; // circular buffer
volatile unsigned long g_ulADCErrors = 0; // number of missed ADC deadlines

// Function Prototypes
void TimerISR(void);
void ADC_ISR(void);

int main(void) {

	// Local Variables
	int trigger_val = 512; // 10-bit ADC
	int temp_index;
//	unsigned short temp_buffer[ADC_BUFFER_SIZE];
	int p_buffer[SCREEN_SIZE]; // number of pixles that can be on the screen at one time
	int i, j;
	char pcStr[200]; // string buffer
	// initialize the clock generator
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
	IntMasterEnable();

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

	while (true) {
		FillFrame(0);
		// half screen width behind most recent sample is sample 1984
		// 1/2 ADC units is 512
		temp_index = g_iADCBufferIndex;
		for(i = ADC_BUFFER_WRAP(temp_index - 64); i != (temp_index - 1024 - 64); i--) {
			if((g_pusADCBuffer[ADC_BUFFER_WRAP(i)] < trigger_val) && (g_pusADCBuffer[ADC_BUFFER_WRAP(i + 1)] > trigger_val)) {
				for(j = 0; j < SCREEN_SIZE; j++) {
					p_buffer[j] = g_pusADCBuffer[ADC_BUFFER_WRAP(i - 63  + j)];
				}
			}
		}
		for(i = 0; i < SCREEN_SIZE; i++) {
			DrawPoint(j, p_buffer[i]/96, 15);
		}
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


