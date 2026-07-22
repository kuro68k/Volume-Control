/*
 * led.h
 *
 */ 


#ifndef LED_H_
#define LED_H_

#include <stdbool.h>


#define RGB_R_bm		(1<<2)
#define RGB_G_bm		(1<<1)
#define RGB_B_bm		(1<<0)

#define LED_OFF_bm		(0b00)
#define LED_WHITE_bm	(0b10)
#define LED_RED_bm		(0b01)
#define LED_BOTH_bm		(0b11)

extern void LED_init(void);
extern void LED_bar(uint8_t num_leds, bool red, bool dot);

#endif /* LED_H_ */