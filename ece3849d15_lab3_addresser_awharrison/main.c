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

unsigned volatile char g_periodInit = 1;
unsigned volatile long g_accumulatedPeriod = 0;
unsigned volatile int g_numPeriods = 0;
unsigned volatile long g_periodDiff = 0;
unsigned long g_SystemClock;
unsigned volatile long g_frequency = 0;

/*
 *  ======== main ========
 */
Void main() {
    g_SystemClock = SysCtlClockGet();
	IntMasterDisable();

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
	TimerControlEvent(TIMER0_BASE, TIMER_A, TIMER_EVENT_POS_EDGE);
	TimerLoadSet(TIMER0_BASE, TIMER_A, 0xffff);
	TimerIntEnable(TIMER0_BASE, TIMER_CAPA_EVENT);
	TimerEnable(TIMER0_BASE, TIMER_A);
    
    IntEnable(INT_TIMER0A);

    IntMasterEnable();

	BIOS_start();     /* enable interrupts and start SYS/BIOS */
}

void Timer0A_ISR() {
    static long previous  = 0;
    TIMER0_ICR_R = TIMER_ICR_CAECINT;
    
    if (g_periodInit) {
        g_periodInit = 0;
        previous = TIMER0_TAR_R;
    } else {
        long recent = TIMER0_TAR_R;
        g_periodDiff = previous - recent;
        g_accumulatedPeriod+= g_periodDiff;
        g_numPeriods++;
        previous = recent;
    }
}

void Timer1A_ISR() {
    TIMER1_ICR_R = TIMER_ICR_TATOCINT;
    
    float avgPeriod = (float)g_accumulatedPeriod/(float)g_numPeriods;
    
    g_frequency = ((1/avgPeriod) * g_SystemClock) * 1000;
    
    g_accumulatedPeriod = 0;
    g_numPeriods = 0;
}

