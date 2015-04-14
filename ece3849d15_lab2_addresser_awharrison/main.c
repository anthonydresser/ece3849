/*
 *  ======== main.c ========
 */

#include <xdc/std.h>
#include <xdc/cfg/global.h>

#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>

#include <ti/sysbios/BIOS.h>

#include <ti/sysbios/knl/Task.h>

typedef char DataType;		// button/mailbox data type
/*
 *  ======== main ========
 */
Void main()
{ 
	DataType getButton;

	Mailbox_pend(mailbox0, &getButton, BIOS_WAITFOREVER);
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
	unsigned long presses = g_ulButtons;
	DataType button;
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








