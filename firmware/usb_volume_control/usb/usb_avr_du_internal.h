/* avr_du_internal.h
 *
 * Copyright Atmel
 * Copyright 2011-2014 Nonolith Labs
 * Copyright 2018 Paul Qureshi
 *
 * AVR DU specific low level implementation
 */

#ifndef USB_AVR_DU_INTERNAL_H_
#define USB_AVR_DU_INTERNAL_H_

#include <avr/io.h>


#define USB_SERIAL_NUMBER_LENGTH_CHARS		(32)

// non-isochronous EPs are the same bits so can use this macro
#define USB_EP_size_to_gc(x)  ((x <= 8   )?USB_BUFSIZE_ISO_BUF8_gc:\
							   (x <= 16  )?USB_BUFSIZE_ISO_BUF16_gc:\
							   (x <= 32  )?USB_BUFSIZE_ISO_BUF32_gc:\
							   (x <= 64  )?USB_BUFSIZE_ISO_BUF64_gc:\
							   (x <= 128 )?USB_BUFSIZE_ISO_BUF128_gc:\
							   (x <= 256 )?USB_BUFSIZE_ISO_BUF256_gc:\
							   (x <= 512 )?USB_BUFSIZE_ISO_BUF512_gc:\
										   USB_BUFSIZE_ISO_BUF1023_gc)


static inline void usb_wait_until_rmw_done(void)
{
	while ((USB0.INTFLAGSB & USB_RMWBUSY_bm));
}


#endif	// USB_AVR_DU_INTERNAL_H_
