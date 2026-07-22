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
#define USB_PID				0x0277

#define USB_VERSION_MAJOR	1
#define USB_VERSION_MINOR	0


// Maximum power draw in milliamps
#define USB_POWER_MA		100

// USB strings
#define USB_STRING_MANUFACTURER		"Keio"
#define USB_STRING_PRODUCT			"Volume Control"


// Generate a USB serial number from the MCU's unique identifiers. Can be
// disabled to save flash memory.
//#define	USB_SERIAL_NUMBER


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
#define USB_HID_REPORT_SIZE		2
#define USB_HID_POLL_RATE_MS	0x10		// HID polling rate in milliseconds


// HID report descriptor
#if defined(USB_HID) && defined(HID_DECLARE_REPORT_DESCRIPTOR)
const __flash uint8_t hid_report_descriptor[] = {
	// --------------------------------------------------
	// Report ID 1: Relative Volume Delta (Input)
	// --------------------------------------------------
	0x05, 0x0C,        // USAGE_PAGE (Consumer)
	0x09, 0x01,        // USAGE (Consumer Control)
	0xA1, 0x01,        // COLLECTION (Application)
	0x85, 0x01,        //   REPORT_ID (1)
	0x09, 0xE0,        //   USAGE (Volume) -> Linear Control
	0x15, 0x81,        //   LOGICAL_MINIMUM (-127)
	0x25, 0x7F,        //   LOGICAL_MAXIMUM (127)
	0x75, 0x08,        //   REPORT_SIZE (8 bits)
	0x95, 0x01,        //   REPORT_COUNT (1)
	0x81, 0x06,        //   INPUT (Data, Var, Rel) <--- Relative flag is key!
	0xC0,              // END_COLLECTION
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
