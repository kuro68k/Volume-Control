/*
 * led.c
 *
 */ 

#include <avr/io.h>
#include <stdint.h>
#include "hw.h"
#include "avr_du.h"
#include "led.h"

uint8_t LED_rgb = 0;	// xxxxxRGB
uint8_t LED_vol[6];		// xxxxxxRW

/******************************************************************************
* Set all LEDs
*/
void led_set_all(void)
{
	uint8_t reg0 = 0;	// RG B0 01 12
	uint8_t reg1 = 0;	// 23 34 45 5x
	
	reg0 = LED_rgb << 5;
	reg0 |= LED_vol[0] << 3;
	reg0 |= LED_vol[1] << 1;
	reg0 |= LED_vol[2] >> 1;
	
	reg1 |= LED_vol[2] << 7;
	reg1 |= LED_vol[3] << 5;
	reg1 |= LED_vol[4] << 3;
	reg1 |= LED_vol[5] << 1;
	
	SPI0.INTFLAGS = SPI_IF_bm;
	SPI0.DATA = reg1;
	while (!(SPI0.INTFLAGS & SPI_IF_bm))
		;
	SPI0.DATA = reg0;
	while (!(SPI0.INTFLAGS & SPI_IF_bm))
		;
	
	RCLK_PORT.OUTSET = RCLK_PIN_bm;
	NOP();
	RCLK_PORT.OUTCLR = RCLK_PIN_bm;
}

/******************************************************************************
* Set up hardware after reset.
*/
void LED_init(void)
{
	SPI0.CTRLB = SPI_SSD_bm | SPI_MODE_0_gc;
	SPI0.INTCTRL = 0;
	SPI0.CTRLA = SPI_MASTER_bm | SPI_CLK2X_bm | SPI_PRESC_DIV4_gc | SPI_ENABLE_bm;
	
	led_set_all();
}
