/*
 *  ======== main.c ========
 */

#include <xdc/std.h>
#include <xdc/cfg/global.h>

#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>

#include <ti/sysbios/BIOS.h>

#include <ti/sysbios/knl/Task.h>

Event_Handle buttonEvent;
Mailbox_Handle buttonMailbox;

typedef char button;		// button/mailbox data type

/*
 *  ======== main ========
 */
Void main()
{ 
	Mailbox_Params mboxParams;
	Error_Block eb;

	Error_init(&eb);
	buttonEvent = Event_create(NULL, &eb);
	if(buttonEvent == NULL) {
		System_abort("Event create failed");
	}

	Mailbox_Params_init(&mboxParams);
	mboxParams.notEmptyEvent = buttonEvent;
	mboxParams.notEmptyEventId = Event_Id_00;
	mbox = Mailbox_create(sizeof(button), 10, &mboxParams, &eb);
	if(mbox == NULL){
		system_abort("Mailbox create failed")
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
