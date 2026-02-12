/*
 * avr_du.h
 *
 */


#ifndef AVR_DU_H_
#define AVR_DU_H_


#define ARRAY_LENGTH(array) (sizeof((array))/sizeof((array)[0]))

#define NOP()	__asm__ __volatile__("nop")
#define	WDR()	__asm__ __volatile__("wdr")


extern void		CCPWrite(volatile uint8_t *address, uint8_t value);


#endif /* AVR_DU_H_ */

