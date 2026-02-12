/*
 * hw.c
 *
 */ 

#include <avr/io.h>
#include "avr_du.h"
#include "hw.h"

/******************************************************************************
* Set up hardware after reset.
*/
void HW_init(void)
{
	PORTA.OUT = 0;
	PORTA.DIR = RCLK_PIN_bm;
	PORTA.PIN1CTRL = PORT_PULLUPEN_bm;			// BTN1
	
	PORTC.OUT = 0;
	PORTC.DIR = 0;
	PORTC.PIN3CTRL = PORT_PULLUPEN_bm;			// BTN2
	
	PORTD.OUT = 0;
	PORTD.DIR = SCK_PIN_bm | MOSI_PIN_bm;
	PORTD.PIN5CTRL = PORT_PULLUPEN_bm;			// ROTB
	PORTD.PIN7CTRL = PORT_PULLUPEN_bm;			// ROTA

	// set CPU clock
	CCPWrite(&CLKCTRL.OSCHFCTRLA, CLKCTRL_ALGSEL_BIN_gc | CLKCTRL_FRQSEL_24M_gc | CLKCTRL_AUTOTUNE_SOF_gc);
	while (!(CLKCTRL.MCLKSTATUS & CLKCTRL_OSCHFS_bm));
	CCPWrite(&CLKCTRL.MCLKCTRLA, CLKCTRL_CLKSEL_OSCHF_gc);
	CCPWrite(&CLKCTRL.MCLKCTRLB, 0);			// no division
	CLKCTRL.MCLKTIMEBASE = 24;
}
