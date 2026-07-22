/*
 * USB Volume Control
 *
 * main.c
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "avr_du.h"
#include "hw.h"
#include "led.h"
#include "encoder.h"
#include "usb.h"
#include "hid.h"

extern void led_set_all(void);
extern uint8_t LED_vol[5];

void led_test(uint8_t reg0, uint8_t reg1)
{
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


int main(void)
{
	// prevent writing to memories
	NVMCTRL.CTRLB = NVMCTRL_APPDATAWP_bm | NVMCTRL_APPCODEWP_bm;
	NVMCTRL.CTRLC = NVMCTRL_BOOTROWWP_bm | NVMCTRL_UROWWP_bm;

	HW_init();
	LED_init();
/*
	for(;;)
	{
		uint8_t enc =	(ROTA_VPORT.IN & ROTA_PIN_bm ? (1<<0) : 0) |
						(ROTB_VPORT.IN & ROTB_PIN_bm ? (1<<1) : 0);
		LED_vol[0] = enc & 0b10 ? LED_RED_bm : LED_OFF_bm;
		LED_vol[1] = enc & 0b01 ? LED_RED_bm : LED_OFF_bm;
		led_set_all();
	}
*/
	ENC_init();
	usb_init();
	sei();
	//usb_detach();
	//_delay_ms(500);
	usb_attach();
	//hid_get_report();
	
	for(;;)
	{
		if (ENC_counter_AT != 0)
		{
			hid_report[0] = 1;
			cli();
			hid_report[1] = ENC_counter_AT;
			ENC_counter_AT = 0;
			sei();
			hid_send_report();
		}
	}
	
	int8_t pos = 3;
	LED_bar(pos, false, true);
	for(;;)
	{
		_delay_ms(100);
		if (ENC_counter_AT != 0)
		{
			cli();
			//int8_t counter = ENC_counter_AT;
			pos += ENC_counter_AT;
			ENC_counter_AT = 0;
			sei();
			
			//pos += counter;
			while (pos < 1)
				pos += 5;
			while (pos > 5)
				pos -= 5;
			LED_bar(pos, false, true);
		}
	}

	bool dot = false;
	for(;;)
	{
		for (uint8_t i = 0; i < 6; i++)
		{
			LED_bar(i, false, dot);
			_delay_ms(500);
		}
		for (uint8_t i = 0; i < 6; i++)
		{
			LED_bar(i, true, dot);
			_delay_ms(500);
		}
		dot = !dot;
	}

	for(;;)
	{
		for (uint8_t i = 0; i < 5; i++)
			LED_vol[i] = 1 << 0;
		led_set_all();
		_delay_ms(1000);
		for (uint8_t i = 0; i < 5; i++)
			LED_vol[i] = 1 << 1;
		led_set_all();
		_delay_ms(1000);
		for (uint8_t i = 0; i < 5; i++)
			LED_vol[i] = 0;
		led_set_all();
		//_delay_ms(1000);
	}
}

