/* usb_config.h
 *
 * Copyright 2018 Paul Qureshi
 *
 * USB stack configuration
 */

#ifndef USB_CONFIG_H_
#define USB_CONFIG_H_

#include <util/delay.h>

/****************************************************************************************
* Hardware configuration
*/
// enable the internal 3.3V regulator
#define USB_USE_REGULATOR
// auto tune internal HF oscillator from USB Start Of Frame (SOF)
#define USB_OSC_USE_SOF

/****************************************************************************************
* USB configuration
*/

// USB vendor and product IDs, version number
#define USB_VID				0x9999
#define USB_PID				0x0200

#define USB_VERSION_MAJOR	1
#define USB_VERSION_MINOR	0


// Maximum power draw in milliamps
#define USB_POWER_MA		50

// USB strings
#define USB_STRING_MANUFACTURER		"Prototype"
#define USB_STRING_PRODUCT			"volume control"


// Generate a USB serial number from the MCU's unique identifiers. Can be
// disabled to save flash memory.
#define	USB_SERIAL_NUMBER


/****************************************************************************************
* Use Microsoft WCID descriptors
*/
//#define	USB_WCID
//#define USB_WCID_EXTENDED

#define WCID_REQUEST_ID			0x22
#define WCID_REQUEST_ID_STR		u"\x22"


/****************************************************************************************
* DFU (Device Firmware Update) run-time interface
*/
//#define USB_DFU_RUNTIME

extern void	CCPWrite(volatile uint8_t *address, uint8_t value);
static inline void dfu_cb_enter_dfu_mode(void)
{
	*(uint32_t *)(INTERNAL_SRAM_START) = 0x4c4f4144;	// "LOAD"
	_delay_ms(100);	// give USB time to send response
	RSTCTRL.SWRR = RSTCTRL_SWRST_bm;
}


/****************************************************************************************
* Enable HID, otherwise vendor specific bulk endpoints
*/
#define USB_HID
#define USB_HID_REPORT_SIZE		3
#define USB_HID_POLL_RATE_MS	0x01		// HID polling rate in milliseconds


// HID report descriptor
#if defined(USB_HID) && defined(HID_DECLARE_REPORT_DESCRIPTOR)
const __flash uint8_t hid_report_descriptor[] = {
	0x05, 0x01,		// usage page (generic desktop Choose the usage page "keyboard" is on
	0x09, 0x06,		// usage (keyboard) Device is a keyboard
	0xA1, 0x01,		// collection (application) This collection comprises all the data words
	0x05, 0x07,		//		usage page (key codes) Choose the key code usage page

	0x19, 0xE0,		// 		usage minimum (224) Choose key codes 224 to 231 which are modifier keys
	0x29, 0xE7,		// 		usage maximum (231) (left and right alt, shift, ctrl and win)
	0x15, 0x00,		// 		logical minimum (0) Each of these eight key codes will report ranging in
	0x25, 0x01,		// 		logical maximum (1) value from zero to one
	0x75, 0x01,		// 		report size (1) Assign each of these keys a 1-bit report
	0x95, 0x08,		// 		report count (8) Report eight times
	0x81, 0x02,		// 		input (data, variable, absolute) The defined byte above is an IN transaction

	0x95, 0x01,		// 		report count (1)
	0x75, 0x08,		// 		report size (8) Report eight bits one time
	0x81, 0x01,		// 		input (constant) Input the byte just described as a constant
	0x95, 0x05,		// 		report count (5)
	0x75, 0x01,		// 		report size (1) Report five bits one time
	0x05, 0x08,		// 		usage page (page# for LEDs) Choose LED usage page
	0x19, 0x01,		// 		usage minimum (1)
	0x29, 0x05,		// 		usage maximum (5) Define five LEDs
	0x91, 0x02,		// 		output (data, variable, absolute) The defined bits above are an OUT transaction

	0x95, 0x01,		// 		report count (1)
	0x75, 0x03,		// 		report size (3)
	0x91, 0x01,		// 		output (constant) Three bit padding for the OUT transaction

	0x95, 0x01,		// 		report count (1)
	0x75, 0x08,		// 		report size (8) Report six bytes
	0x15, 0x00,		// 		logical minimum (0)
	0x25, 0x65,		// 		logical maximum (101) The byte values can range from 0 to 101
	0x05, 0x07,		// 		usage page (key codes) Change usage page to key codes
	0x19, 0x00,		// 		usage minimum (0)
	0x29, 0x65,		// 		usage maximum (101) Select key code range of 0 to 101
	0x81, 0x00,		// 		input (data, array) Input the above six bytes
	0xC0			//	end collection End application collection
};
_Static_assert(sizeof(hid_report_descriptor) <= USB_EP0_BUFFER_SIZE, "HID descriptor exceeds EP0 buffer size");
_Static_assert(USB_HID_REPORT_SIZE <= USB_EP0_BUFFER_SIZE, "HID report exceeds EP0 buffer size");
#endif	// defined(USB_HID) && defined(HID_DECLARE_REPORT_DESCRIPTOR)


// GET_REPORT handlers. *report is USB_MAX_PACKET_SIZE.
// Return number of bytes in report, or -1 if not supported
#include <hid.h>
static inline int16_t hid_cb_get_report_input(uint8_t *report, uint8_t report_id)
{
	memcpy(report, hid_report, sizeof(hid_report));
	return sizeof(hid_report);
}

static inline int16_t hid_cb_get_report_output(uint8_t *report, uint8_t report_id)
{
	memcpy(report, hid_report, sizeof(hid_report));
	return sizeof(hid_report);
}

static inline int16_t hid_cb_get_report_feature(uint8_t *report, uint8_t report_id)
{
	return -1;
}

// SET_REPORT handlers
// Return true if report OK
static inline bool hid_cb_set_report_input(uint8_t *report, uint16_t report_length, uint8_t report_id)
{
	memcpy(out_hid_report, report, sizeof(hid_report));
	return sizeof(hid_report);
}

static inline bool hid_cb_set_report_output(uint8_t *report, uint16_t report_length, uint8_t report_id)
{
	memcpy(out_hid_report, report, sizeof(hid_report));
	return sizeof(hid_report);
}

static inline bool hid_cb_set_report_feature(uint8_t *report, uint16_t report_length, uint8_t report_id)
{
	return false;
}


#endif /* USB_CONFIG_H_ */
