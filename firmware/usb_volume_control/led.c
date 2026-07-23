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
uint8_t LED_vol[5];		// xxxxxxRW

/******************************************************************************
* Set all LEDs
*/
void led_set_all(void)
{
						// WR WR W
	uint8_t reg0 = 0;	// 21 10 0R GB
						//     R WR WR
	uint8_t reg1 = 0;	// xx x4 43 32
	
	reg0 = LED_rgb & 0b111;
	reg0 |= (LED_vol[0] & 0b11) << 3;
	reg0 |= (LED_vol[1] & 0b11) << 5;
	reg0 |= (LED_vol[2] & 0b11) << 7;
	reg0 = ~reg0;
	
	reg1 |= (LED_vol[2] & 0b11) >> 1;
	reg1 |= (LED_vol[3] & 0b11) << 1;
	reg1 |= (LED_vol[4] & 0b11) << 3;
	reg1 = ~reg1;
	
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
* Set individual LEDs
*/
void LED_set(uint8_t vol0, uint8_t vol1, uint8_t vol2, uint8_t vol3, uint8_t vol4, uint8_t rgb)
{
	LED_vol[0] = vol0;
	LED_vol[1] = vol1;
	LED_vol[2] = vol2;
	LED_vol[3] = vol3;
	LED_vol[4] = vol4;
	LED_rgb = rgb;
	led_set_all();
}


/******************************************************************************
* Set bar graph
*/
void LED_bar(uint8_t num_leds, bool red, bool dot)
{
	uint8_t colour = red ? LED_RED_bm : LED_WHITE_bm;
	for (uint8_t i = 0; i < 5; i++)
		LED_vol[i] = LED_OFF_bm;
	
	if (dot)
		LED_vol[num_leds - 1] = colour;
	else
	{
		for (uint8_t i = 0; i < num_leds; i++)
			LED_vol[i] = colour;
	}
	
	led_set_all();
}

/******************************************************************************
* Set up hardware after reset.
*/
void LED_init(void)
{
	PORTMUX.SPIROUTEA = PORTMUX_SPI0_ALT4_gc;
	SPI0.CTRLB = SPI_SSD_bm | SPI_MODE_0_gc;
	SPI0.INTCTRL = 0;
	SPI0.CTRLA = SPI_MASTER_bm | SPI_CLK2X_bm | SPI_PRESC_DIV4_gc | SPI_ENABLE_bm;
	
	led_set_all();
}
