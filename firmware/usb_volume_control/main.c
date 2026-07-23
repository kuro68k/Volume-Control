/*
 * USB Volume Control
 *
 * main.c
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>
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
	ENC_init();
	usb_init();
	sei();
	usb_attach();
	hid_get_report();
	
	bool last_btn1 = false;
	bool last_btn2 = false;
	bool is_muted = false;
	for(;;)
	{
		bool btn1 = BTN1_PORT.IN & BTN1_PIN_bm;
		bool btn2 = BTN2_PORT.IN & BTN2_PIN_bm;
		if ((ENC_counter_AT != 0) || (btn1 != last_btn1) || (btn2 != last_btn2))
		{
			hid_report[0] = 1;
			cli();
			hid_report[1] = ENC_counter_AT;
			ENC_counter_AT = 0;
			sei();
			
			if ((btn1 != last_btn1) || is_muted)
				hid_report[1] = 0;	// no volume change when encoder button pressed
			
			hid_report[2] = btn1 ? 0 : 1 << 0;
			hid_report[2] |= btn2 ? 0 : 1 << 1;
			hid_send_report();
			last_btn1 = btn1;
			last_btn2 = btn2;
			
			if ((btn1 != last_btn1) || (btn2 != last_btn2))
				_delay_ms(50);		// debounce
		}
		
		if (out_hid_report_received_SIG)
		{
			LED_set(out_hid_report[1], out_hid_report[2], out_hid_report[3], out_hid_report[4], out_hid_report[5], out_hid_report[6]);
			is_muted = out_hid_report[7];
			hid_get_report();
		}
	}
}

