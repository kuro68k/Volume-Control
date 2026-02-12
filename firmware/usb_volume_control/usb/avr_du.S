/*
 * xmega.S
 *
 */

#include <avr/io.h>

CCP_IOREG_gc = (0xD8<<0)

.global CCPWrite



.section .ccpwrite,"ax",@progbits
CCPWrite:
	movw	Z, r24							; address
	ldi		r18, CCP_IOREG_gc
	cli
	out		CCP, r18
	st		Z, r22
	sei
//	out		SREG, r18
	ret
