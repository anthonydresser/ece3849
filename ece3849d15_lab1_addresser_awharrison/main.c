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

// Global Variables
unsigned long g_ulSystemClock; // system clock frequency in Hz
volatile unsigned long g_ulTime = 0; // time in hundredths of a second
volatile int g_iADCBufferIndex = ADC_BUFFER_SIZE - 1; // latest sample index
volatile unsigned short g_pusADCBuffer[ADC_BUFFER_SIZE]; // circular buffer
volatile unsigned long g_ulADCErrors = 0; // number of missed ADC deadlines

// Function Prototypes
void TimerISR(void);
void ADC_ISR(void);

int main(void) {

	// Local Variables

	char pcStr[50]; // string buffer
	unsigned long ulTime; // local copy of g_ulTime
	unsigned int minutes;
	unsigned int seconds;
	unsigned int fractions;
	unsigned long ulDivider, ulPrescaler;
	int i; // counter for the analog clock face
	//	float dx, dy, sini, cosi;
	int x[60] = {64, 68, 72, 76, 79, 83, 86, 89, 92, 95, 97, 99, 100,
			101, 102, 102, 102, 101, 100, 99, 97, 95, 92, 89, 86, 83,
			79, 76, 72, 68, 64, 60, 56, 52, 49, 45, 42, 39, 36, 33,
			31, 29, 28, 27, 26, 26, 26, 27, 28, 29, 31, 33, 36, 39,
			42, 45, 49, 52, 56, 60
	};
	int y[60] = {14, 14, 15, 16, 17, 19, 21, 24, 27, 30, 33, 37, 40, 44, 48,
			52, 56, 60, 64, 67, 71, 74, 77, 80, 83, 85, 87, 88, 89, 90,
			90, 90, 89, 88, 87, 85, 83, 80, 77, 74, 71, 67, 64, 60, 56, 52,
			48, 44, 40, 37, 33, 30, 27, 24, 21, 19, 17, 16, 15, 14
	};

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

	IntPrioritySet(INT_TIMER0A, 0); // 0 = highest priority, 32 = next lower
	IntEnable(INT_TIMER0A);
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

	SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0); // enable the ADC
	SysCtlADCSpeedSet(SYSCTL_ADCSPEED_500KSPS); // specify 500ksps
	ADCSequenceDisable(ADC0_BASE, 0); // choose ADC sequence 0; disable before configuring
	ADCSequenceConfigure(ADC0_BASE, 0, ADC_TRIGGER_ALWAYS, 0); // specify the "Always" trigger
	ADCSequenceStepConfigure(ADC0_BASE, 0, 0, ADC_CTL_CH0 | ADC_CTL_IE | ADC_CTL_END); // in the 0th step, sample channel 0
	// enable interrupt, and make it the end of sequence
	ADCIntEnable(ADC0_BASE, 0); // enable ADC interrupt from sequence 0
	ADCSequenceEnable(ADC0_BASE, 0); // enable the sequence. it is now sampling
	IntPrioritySet(INT_ADC0, 32); // set ADC interrupt priority in the interrupt controller
	IntEnable(INT_ADC0); // enable ADC interrupt

	while (true) {
		FillFrame(0); // clear frame buffer
		ulTime = g_ulTime; // read volatile global only once

		fractions = ulTime % 100;
		seconds = (ulTime/100) % 60;
		minutes = ulTime/6000;

		usprintf(pcStr, "Time = %02d:%02d:%02d", minutes, seconds, fractions); // convert time to string
		DrawString(0, 0, pcStr, 15, false); // draw string to frame buffer
		DrawCircle(64, 52, 40, 2);
		//		for(i = 0; i < 360; i += 6) {
		//			sini = sin(i);
		//			cosi = cos(i);
		//			dx = 38.0*sini;
		//			dy = 38.0*cosi;
		//			x = 64 + dx;
		//			y = 52 + dy;
		//			DrawPoint(x, y, 8);
		//		}
		// draw the points on the circle
		for(i = 0; i <= 60; i++) {
			if((i % 5) == 0)
				DrawPoint(x[i], y[i], 15);
			else
				DrawPoint(x[i], y[i], 6);
		}
		// draw hands
		DrawLine(64, 52, x[seconds], y[seconds], 6);
		DrawLine(64, 52, x[minutes], y[minutes], 10);
		// copy frame to the OLED screen
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

void TimerISR(void) {
	static int tic = false;
	static int running = true;
	unsigned long presses = g_ulButtons;
	TIMER0_ICR_R = TIMER_ICR_TATOCINT; // clear interrupt flag
	ButtonDebounce(
			//select button
			((~GPIO_PORTF_DATA_R & GPIO_PIN_1) >> 1) +
			//up button
			(~GPIO_PORTE_DATA_R & GPIO_PIN_0) +
			//left button
			((~GPIO_PORTE_DATA_R & GPIO_PIN_2) >> 2) +
			//right button
			((~GPIO_PORTE_DATA_R & GPIO_PIN_3) >> 3) +
			//down button // not shifted because it will be the reset button
			((~GPIO_PORTE_DATA_R & GPIO_PIN_1)));
	presses = ~presses & g_ulButtons; // button press detector
	if (presses & 1) { // "select" button pressed
		running = !running;
	}
	if (presses & 2) {
		running = !running;
		g_ulTime = 0;
	}
	if (running) {
		if (tic) {
			g_ulTime++; // increment time every other ISR call
			tic = false;
		}
		else
			tic = true;
		//		if (g_ulTime == 360000)
		//			g_ulTime = 0;
	}
}

