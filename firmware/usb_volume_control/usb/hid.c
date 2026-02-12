/* hid.c
 *
 * Copyright 2018 Paul Qureshi
 *
 * Human Interface Device support
 */

#include <avr/io.h>
#include "usb.h"
#include "usb_config.h"

uint8_t hid_report[USB_HID_REPORT_SIZE] __attribute__((__aligned__(2)));
uint8_t out_hid_report[USB_HID_REPORT_SIZE] __attribute__((__aligned__(2)));
volatile bool out_hid_report_received_SIG = 0;

/* Send HID reports. Blocks until the endpoint is ready.
 */
void hid_send_report(void)
{
	while (!usb_ep_is_ready(0x81));
	usb_ep_start_in(0x81, hid_report, USB_HID_REPORT_SIZE);
}

/* Get HID reports.
 */
void hid_get_report(void)
{
	out_hid_report_received_SIG = 0;
	usb_ep_start_out(0x01, out_hid_report, USB_HID_REPORT_SIZE);
}
