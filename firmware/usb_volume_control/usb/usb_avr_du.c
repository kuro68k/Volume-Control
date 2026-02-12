/* usb_avr_du.c
 *
 * Copyright 2011-2014 Nonolith Labs
 * Copyright 2014 Technical Machine
 * Copyright 2017-2018 Paul Qureshi
 *
 * Low level USB driver for AVR DU series.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stddef.h>
#include "usb.h"
#include "usb_avr_du.h"
#include "usb_avr_du_internal.h"
#include "avr_du.h"


#define _USB_EP(epaddr) \
	USB_EP_pair_t* pair = &usb_du_endpoints[(epaddr & 0x3F)]; \
	USB_EP_t* e __attribute__ ((unused)) = &pair->ep[!!(epaddr&0x80)]; \


/**************************************************************************************************
* Initialize up USB after reset
*/
void usb_init()
{
#ifdef USB_USE_REGULATOR
	SYSCFG.VUSBCTRL = SYSCFG_USBVREG_bm;	// enable 3.3V regulator
#endif
#ifdef USB_OSC_USE_SOF
	CCPWrite(&CLKCTRL.OSCHFCTRLA, (CLKCTRL.OSCHFCTRLA & ~CLKCTRL_AUTOTUNE_gm) | CLKCTRL_AUTOTUNE_SOF_gc);
#endif

	USB0.INTCTRLA = USB_RESET_bm | USB_RESUME_bm | USB_SUSPEND_bm;
	USB0.INTCTRLB = USB_SETUP_bm | USB_TRNCOMPL_bm;

	usb_reset();
}

/**************************************************************************************************
* Reset USB stack after device or USB reset
*/
void usb_reset()
{
	USB0.EPPTR = (uint16_t)usb_du_endpoints;
	USB0.ADDR = 0;

	// endpoint 0 control IN/OUT
	usb_wait_until_rmw_done();
	USB0.STATUS[0].OUTCLR = ~USB_BUSNAK_bm;
	usb_wait_until_rmw_done();
	usb_du_endpoints[0].out.DATAPTR = (unsigned) ep0_buf_out;
	usb_du_endpoints[0].out.CTRL = USB_TYPE_CONTROL_gc |
								   USB_EP_size_to_gc(USB_EP0_MAX_PACKET_SIZE);
	usb_wait_until_rmw_done();
	USB0.STATUS[0].INCLR = ~USB_BUSNAK_bm;
	usb_wait_until_rmw_done();
	USB0.STATUS[0].INSET = USB_TOGGLE_bm;
	usb_wait_until_rmw_done();
	usb_du_endpoints[0].in.DATAPTR = (unsigned) ep0_buf_in;
	usb_du_endpoints[0].in.CTRL = USB_TYPE_CONTROL_gc | USB_MULTIPKT_bm | USB_AZLP_bm |
								  USB_EP_size_to_gc(USB_EP0_MAX_PACKET_SIZE);

#ifdef USB_HID
	usb_ep_enable(0x81, USB_TYPE_BULKINT_gc, 64, false);
#endif

	USB0.CTRLA = USB_ENABLE_bm | usb_num_endpoints;
}

/**************************************************************************************************
* Enable an endpoint.
*
* type				USB_EP_TYPE_*_gc
* buffer_size		maximum payload size for endpoint
* enable_interrupt	enable transaction complete interrupt
*/
inline void usb_ep_enable(uint8_t ep, uint8_t type, usb_size buffer_size, bool enable_interrupt)
{
	_USB_EP(ep);
	e->STATUS = USB_BUSNAK_bm | USB_TRNCOMPL_bm;
	e->CTRL = type | USB_EP_size_to_gc(buffer_size) | (enable_interrupt ? 0 : USB_TCDSBL_bm);
}

/**************************************************************************************************
* Disable an endpoint.
*/
inline void usb_ep_disable(uint8_t ep)
{
	_USB_EP(ep);
	e->CTRL = 0;
}

/**************************************************************************************************
* Reset endpoint, clearing all error flags and making ready for use.
*/
inline void usb_ep_reset(uint8_t ep)
{
	_USB_EP(ep);
	e->STATUS = USB_BUSNAK_bm | USB_TRNCOMPL_bm;
}

/**************************************************************************************************
* Start receiving data into buffer from host.
*/
inline void usb_ep_start_out(uint8_t ep, uint8_t* data, usb_size len)
{
	_USB_EP(ep);
	e->DATAPTR = (unsigned) data;
	usb_wait_until_rmw_done();
	USB0.STATUS[ep].OUTCLR = USB_BUSNAK_bm | USB_EPSETUP_bm;
}

/**************************************************************************************************
* Start sending data from buffer to host
*/
void usb_ep_start_in(uint8_t ep, const uint8_t* data, usb_size size)
{
	_USB_EP(ep);
	e->CTRL &= ~USB_DOSTALL_bm;
	e->DATAPTR = (uint16_t)data;
	e->CNT = size;
	e->MCNT = 0;	// for multi-packet
	usb_wait_until_rmw_done();
	USB0.STATUS[ep & 0x3F].INCLR = USB_BUSNAK_bm | USB_EPSETUP_bm;
}

/**************************************************************************************************
* Check if an endpoint is ready to start the next transaction
*/
inline bool usb_ep_is_ready(uint8_t ep)
{
	_USB_EP(ep);
	return (e->STATUS & USB_TRNCOMPL_bm);
}

/**************************************************************************************************
* Check if an unhandled transaction has completed on an endpoint
*/
//inline bool usb_ep_is_transaction_complete(uint8_t ep)
//{
//	_USB_EP(ep);
//	return e->STATUS & USB_TRNCOMPL_bm;
//}

/**************************************************************************************************
* Handle a completed transaction on an endpoint
*/
//void usb_ep_clear_transaction_complete(uint8_t ep)
//{
//	_USB_EP(ep);
//	LACR16(&(e->STATUS), USB_TRNCOMPL_bm | USB_BUSNAK_bm);
//}

/**************************************************************************************************
* Get the number of bytes available from a completed transaction on an OUT endpoint
*/
inline uint16_t usb_ep_get_out_transaction_length(uint8_t ep)
{
	_USB_EP(ep);
	return e->CNT;
}

/**************************************************************************************************
* Physically detach from USB bus
*/
void usb_detach(void) {
	USB0.CTRLB &= ~USB_ATTACH_bm;
}

/**************************************************************************************************
* Physically attach to USB bus
*/
void usb_attach(void) {
	USB0.CTRLB |= USB_ATTACH_bm;
}

/**************************************************************************************************
* Clear SETUP OUT stage on the default control pipe
*/
void usb_ep0_clear_out_setup(void) {
	usb_wait_until_rmw_done();
	USB0.STATUS[0].OUTCLR = USB_EPSETUP_bm | USB_BUSNAK_bm | USB_TRNCOMPL_bm | USB_UNFOVF_bm;
}

/**************************************************************************************************
* Enable the OUT stage on the default control pipe
*/
void usb_ep0_out(void) {
	usb_wait_until_rmw_done();
	USB0.STATUS[0].OUTCLR = USB_EPSETUP_bm | USB_BUSNAK_bm | USB_TRNCOMPL_bm | USB_UNFOVF_bm;
}

/**************************************************************************************************
* Enable the IN stage on the default control pipe
*/
void usb_ep0_in(uint8_t size) {
	//usb_wait_until_rmw_done();
	usb_ep_start_in(0x80, ep0_buf_in, size);
}

/**************************************************************************************************
* Stall the default control pipe
*/
void usb_ep0_stall_in(void) {
	_USB_EP(0x80);
	e->CTRL |= USB_DOSTALL_bm;
	usb_wait_until_rmw_done();
	USB0.STATUS[0].INCLR = USB_EPSETUP_bm;
}

/**************************************************************************************************
* Handle bus event interrupts
*/
ISR(USB0_BUSEVENT_vect)
{
	if (USB0.INTFLAGSA & (USB_STALLED_bm | USB_UNF_bm | USB_OVF_bm))	// CRC error, under/overflow
		USB0.INTFLAGSA = USB_STALLED_bm | USB_UNF_bm | USB_OVF_bm;

	// USB bus reset signal
	if (USB0.INTFLAGSA & USB_RESET_bm)
	{
		USB0.INTFLAGSA = USB_RESET_bm;
		usb_reset();
	}

	USB0.INTFLAGSA = USB_SUSPEND_bm | USB_RESUME_bm | USB_SOF_bm;
}

/**************************************************************************************************
* Handle transaction complete interrupts. Uncomment callbacks if required.
*/
ISR(USB0_TRNCOMPL_vect)
{
	USB0.FIFOWP = 0;	// clear TCIF
	USB0.INTFLAGSB = USB_SETUP_bm | USB_TRNCOMPL_bm;
//USART0.TXDATAL = '/';
	// EP0 (control) OUT/SETUP
	uint8_t status = usb_du_endpoints[0].out.STATUS;	// Read once to prevent race condition
	if (status & USB_EPSETUP_bm)
	{
		memcpy(&usb_setup, ep0_buf_out, sizeof(usb_setup));
		usb_wait_until_rmw_done();
		USB0.STATUS[0].OUTCLR = USB_TRNCOMPL_bm | USB_BUSNAK_bm | USB_EPSETUP_bm;
		if (((usb_setup.bmRequestType & 0x80) != 0) ||	// IN host requesting response
			(usb_setup.wLength == 0))					// OUT length 0 (status stage?)
			usb_handle_control_setup();
		// else deferred until data stage complete
	}
	//else if (status & USB_TRNCOMPL_bm)	// invalid?
	//{
	//	usb_handle_control_setup();
	//}

	// EP0 (control) IN
	if (usb_du_endpoints[0].in.STATUS & USB_TRNCOMPL_bm)
	{
		// SET_ADDRESS requests must only take effect after the response IN packet has
		// been sent.
		if ((usb_setup.bmRequestType & USB_REQTYPE_TYPE_MASK) == USB_REQTYPE_STANDARD)
		{
			if (usb_setup.bRequest == USB_REQ_SetAddress)
					USB0.ADDR = usb_setup.wValue & 0x7F;
		}
		//else
		//	usb_handle_control_in();
		usb_wait_until_rmw_done();
		USB0.STATUS[0].INCLR = USB_TRNCOMPL_bm;
	}

	// EP1 IN
	if (usb_du_endpoints[1].in.STATUS & USB_TRNCOMPL_bm)
	{
		//usb_wait_until_rmw_done();
		//USB0.STATUS[0].INCLR = USB_BUSNAK_bm;	// should be kept to discard further packets until next IN???
		// callback only???
	}

	// EP1 OUT
	if (usb_du_endpoints[1].out.STATUS & USB_TRNCOMPL_bm)
	{
		#ifdef USB_HID
			hid_get_report_received_callback();
		#endif
		// callback???
		usb_wait_until_rmw_done();
		USB0.STATUS[0].OUTCLR = USB_BUSNAK_bm;
	}

	// empty callback
	//usb_cb_completion();
}
