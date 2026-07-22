/*
 * encoder.c
 *
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include "hw.h"

static volatile uint8_t last_bits = 0;
volatile uint8_t ENC_up_count_AT = 0;
volatile uint8_t ENC_down_count_AT = 0;
volatile int8_t ENC_counter_AT = 0;

static volatile const uint8_t quad_map[4][2] =
							// BA
	{	{ 0b10, 0b01 },		// 00
		{ 0b00, 0b11 },		// 01
		{ 0b11, 0b00 },		// 10
		{ 0b01, 0b10 } };	// 11

//*****************************************************************************
// Read the current encoder bits
//
inline static uint8_t get_bits(void)
{
	return	(ROTA_VPORT.IN & ROTA_PIN_bm ? (1<<0) : 0) |
			(ROTB_VPORT.IN & ROTB_PIN_bm ? (1<<1) : 0);
}

//*****************************************************************************
// Init after reset
//
void ENC_init(void)
{
	last_bits = get_bits();
	/*
	TCB0.CTRLB = TCB_CNTMODE_INT_gc;
	TCB0.EVCTRL = 0;
	TCB0.INTFLAGS = TCB_OVF_bm;	// clear interrupt flag
	TCB0.INTCTRL = TCB_OVF_bm;
	TCB0.CNT = 0;
	TCB0.CCMP = 0x2EDF;			// 1ms
	TCB0.CTRLA = TCB_RUNSTDBY_bm | TCB_CLKSEL_DIV2_gc | TCB_ENABLE_bm;
	*/
	
	PORTD.PIN5CTRL = PORT_ISC_BOTHEDGES_gc;
	PORTD.PIN7CTRL = PORT_ISC_BOTHEDGES_gc;
}

//*****************************************************************************
// Check for movement of the encoder periodically
//
ISR(PORTD_PORT_vect)
{
	VPORTD.INTFLAGS = 0xFF;
	uint8_t bits = get_bits();
	if (quad_map[last_bits][0] == bits)
		ENC_counter_AT--;
	if (quad_map[last_bits][1] == bits)
		ENC_counter_AT++;
	last_bits = bits;
}

//*****************************************************************************
// Check for movement of the encoder periodically
//
ISR(TCB0_INT_vect)
{
	uint8_t bits = get_bits();
	if (bits != last_bits)
	{
		if (quad_map[last_bits][0] == bits)
			ENC_down_count_AT++;
		if (quad_map[last_bits][1] == bits)
			ENC_up_count_AT++;
		last_bits = bits;
	}
}