/*
 *  ======== main.c ========
 */

#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>

#include "network.h"
#include "lm3s2110.h"
#include "inc/hw_types.h"
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_sysctl.h"
#include "driverlib/sysctl.h"
#include "utils/ustdlib.h"
#include "driverlib/interrupt.h"
#include "driverlib/gpio.h"
#include "driverlib/comp.h"


/*
 *  ======== main ========
 */
Void main() {

	SysCtlPeripheralEnable(SYSCTL_PERIPH_COMP0);
	ComparatorConfigure(COMP_BASE, 0, COMP_TRIG_NONE|COMP_INT_HIGH|COMP_ASRCP_REF);
	ComparatorRefSet(COMP_BASE, COMP_REF_1_5125V);

	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
	GPIODirModeSet(GPIO_PORTB_BASE, GPIO_PIN_4, GPIO_DIR_MODE_HW); // C0- input
	GPIOPadConfigSet(GPIO_PORTB_BASE, GPIO_PIN_4, GPIO_STRENGTH_2MA,
	 GPIO_PIN_TYPE_ANALOG);

	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
	GPIODirModeSet(GPIO_PORTD_BASE, GPIO_PIN_7, GPIO_DIR_MODE_HW); // C0o output
	GPIOPadConfigSet(GPIO_PORTD_BASE, GPIO_PIN_7, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);

	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
	GPIODirModeSet(GPIO_PORTB_BASE, GPIO_PIN_0, GPIO_DIR_MODE_HW); // CCP0 input
	GPIOPadConfigSet(GPIO_PORTB_BASE, GPIO_PIN_0, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);
	TimerDisable(TIMER0_BASE, TIMER_BOTH);
	TimerConfigure(TIMER0_BASE, TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_CAP_TIME);
	TimerControlEvent(TIMER0_BASE, TIMER_A, ?);
	TimerLoadSet(TIMER0_BASE, TIMER_A, ?);
	TimerIntEnable(TIMER0_BASE, ?);
	TimerEnable(TIMER0_BASE, TIMER_A);

	BIOS_start();     /* enable interrupts and start SYS/BIOS */
}

