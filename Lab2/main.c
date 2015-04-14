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
/*
 *  ======== main ========
 */
Void main()
{
		IntMasterDisable();
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
		prevy = FRAME_SIZE_Y/2 - (int)round((p_buffer[0] - ADC_OFFSET) * fScale);
		for(i = 1; i < FRAME_SIZE_X; i++) {
			y = FRAME_SIZE_Y/2 - (int)round((p_buffer[i] - ADC_OFFSET) * fScale);
			//			DrawPoint(i, y, 15);
			DrawLine(i-1, prevy, i, y, 15);
			prevy = y;
		}
		usprintf(pcStr, "CPU Load = %02d.%01d%%", cpu_load_int/10, cpu_load_int % 10); // convert time to string
		DrawString(0, 86, pcStr, 15, false); // draw string to frame buffer
		RIT128x96x4ImageDraw(g_pucFrame, 0, 0, FRAME_SIZE_X, FRAME_SIZE_Y);
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
			break;
		case 'D':
			break;
		case 'L':
			break;
		case 'R':
			break;
		case 'S':
			break;
		}
	}
}

void waveformTask(UArg arg0, UArg arg1) {
	while(1) {
		Semaphore_pend(, BIOS_WAIT_FOREVER);

		Semaphore_post(sem_display);
	}
}
