/***************************************************************************/
/*
 * Prototypes, structure definitions and macros.
 *
 * Copyright (c) 2000-2003 Johannes Erdfelt <johannes@erdfelt.com>
 *
 * This library is covered by the LGPL, read LICENSE for details.
 *
 * This file (and only this file) may alternatively be licensed under the
 * BSD license as well, read LICENSE for details.
 *
 */
/***************************************************************************/
/*
 * This file contains macros & structures derived from LIBUSB's usb.h.
 * Some of the structures have been modified to suit the needs of
 * Cameraderie and my own coding style.  To the extent permitted
 * by the LGPL, my changes and additions are licensed under the
 * Mozilla Public License v1.1
 * 
 * Copyright (c) 2006, 2007 Richard L Walsh <rich@e-vertise.com>
 *
 */
/***************************************************************************/

#ifndef _CAMLIBUSB_H_
#define _CAMLIBUSB_H_

#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/syslimits.h>

#pragma pack(1)

/***************************************************************************/

//  USB spec information

// Device and/or Interface Class codes
#define USB_CLASS_PER_INTERFACE     0       // for DeviceClass
#define USB_CLASS_AUDIO             1
#define USB_CLASS_COMM              2
#define USB_CLASS_HID               3
#define USB_CLASS_PHYSICAL          5
#define USB_CLASS_IMAGE             6
#define USB_CLASS_PRINTER           7
#define USB_CLASS_MASS_STORAGE      8
#define USB_CLASS_HUB               9
#define USB_CLASS_DATA              10
#define USB_CLASS_SMARTCARD         11
#define USB_CLASS_FIRM_UPD          12
#define USB_CLASS_SECURITY          13
#define USB_CLASS_DIAGNOSTIC        0xdc
#define USB_CLASS_WIRELESS          0xe0
#define USB_CLASS_APPL_SPEC         0xfe
#define USB_CLASS_VENDOR_SPEC       0xff

// Printer
#define USB_SUBCLASS_PRINTER        1
#define USB_PROTO_PRINTER_UNI       1
#define USB_PROTO_PRINTER_BI        2
#define USB_PROTO_PRINTER_1284      3

// Mass Storage
#define USB_SUBCLASS_RBC            1
#define USB_SUBCLASS_SFF8020I       2
#define USB_SUBCLASS_QIC157         3
#define USB_SUBCLASS_UFI            4
#define USB_SUBCLASS_SFF8070I       5
#define USB_SUBCLASS_SCSI           6
#define USB_PROTOCOL_MASS_CBI_I     0
#define USB_PROTOCOL_MASS_CBI       1
#define USB_PROTOCOL_MASS_BBB_OLD   2       // Not in the spec anymore
#define USB_PROTOCOL_MASS_BBB       80      // 'P' for the Iomega Zip drive

// Hub
#define USB_SUBCLASS_HUB            0
#define USB_PROTO_FSHUB             0
#define USB_PROTO_HSHUBSTT          0       // same as USB_PROTO_FSHUB
#define USB_PROTO_HSHUBMTT          1

/***************************************************************************/

//  Descriptor types
#define USB_DT_DEVICE               0x01
#define USB_DT_CONFIG               0x02
#define USB_DT_STRING               0x03
#define USB_DT_INTERFACE            0x04
#define USB_DT_ENDPOINT             0x05

#define USB_DT_HID                  0x21
#define USB_DT_REPORT               0x22
#define USB_DT_PHYSICAL             0x23
#define USB_DT_HUB                  0x29

//  Descriptor sizes per descriptor type
#define USB_DT_DEVICE_SIZE          18
#define USB_DT_CONFIG_SIZE          9
#define USB_DT_INTERFACE_SIZE       9
#define USB_DT_ENDPOINT_SIZE        7
#define USB_DT_ENDPOINT_AUDIO_SIZE  9       // Audio extension
#define USB_DT_HUB_NONVAR_SIZE      7

/***************************************************************************/

#define USB_ENDPOINT_TYPE_MASK          0x03    // in bmAttributes
#define USB_ENDPOINT_TYPE_CONTROL       0
#define USB_ENDPOINT_TYPE_ISOCHRONOUS   1
#define USB_ENDPOINT_TYPE_BULK          2
#define USB_ENDPOINT_TYPE_INTERRUPT     3

#define USB_ENDPOINT_ADDRESS_MASK   0x0f    // in bEndpointAddress
#define USB_ENDPOINT_DIR_MASK       0x80

#define USB_ENDPOINT_IN             0x80
#define USB_ENDPOINT_OUT            0x00

/***************************************************************************/

#define USB_MAXCONFIG               8
#define USB_MAXALTSETTING           128
#define USB_MAXINTERFACES           32
#define USB_MAXENDPOINTS            32

/***************************************************************************/

//  Standard requests

#define USB_REQ_GET_STATUS          0x00
#define USB_REQ_CLEAR_FEATURE       0x01
// 0x02 is reserved
#define USB_REQ_SET_FEATURE         0x03
// 0x04 is reserved
#define USB_REQ_SET_ADDRESS         0x05
#define USB_REQ_GET_DESCRIPTOR      0x06
#define USB_REQ_SET_DESCRIPTOR      0x07
#define USB_REQ_GET_CONFIGURATION   0x08
#define USB_REQ_SET_CONFIGURATION   0x09
#define USB_REQ_GET_INTERFACE       0x0A
#define USB_REQ_SET_INTERFACE       0x0B
#define USB_REQ_SYNCH_FRAME         0x0C

#define USB_TYPE_STANDARD           (0x00 << 5)
#define USB_TYPE_CLASS              (0x01 << 5)
#define USB_TYPE_VENDOR             (0x02 << 5)
#define USB_TYPE_RESERVED           (0x03 << 5)

#define USB_RECIP_DEVICE            0x00
#define USB_RECIP_INTERFACE         0x01
#define USB_RECIP_ENDPOINT          0x02
#define USB_RECIP_OTHER             0x03

/***************************************************************************/

//  All standard descriptors have these 2 fields in common

typedef struct _usb_descriptor_header {
    u_int8_t    bLength;
    u_int8_t    bDescriptorType;
} usb_descriptor_header;

/***************************************************************************/

typedef struct _usb_endpoint_descriptor {
    u_int8_t    bLength;
    u_int8_t    bDescriptorType;
    u_int8_t    bEndpointAddress;
    u_int8_t    bmAttributes;
    u_int16_t   wMaxPacketSize;
    u_int8_t    bInterval;
    u_int8_t    bRefresh;
    u_int8_t    bSynchAddress;
} usb_endpoint_descriptor;

/***************************************************************************/

typedef struct _usb_interface_descriptor {
    u_int8_t    bLength;
    u_int8_t    bDescriptorType;
    u_int8_t    bInterfaceNumber;
    u_int8_t    bAlternateSetting;
    u_int8_t    bNumEndpoints;
    u_int8_t    bInterfaceClass;
    u_int8_t    bInterfaceSubClass;
    u_int8_t    bInterfaceProtocol;
    u_int8_t    iInterface;
    usb_endpoint_descriptor *endpoint;
} usb_interface_descriptor;

/***************************************************************************/

typedef struct _usb_interface {
    usb_interface_descriptor *altsetting;
    int                      num_altsetting;
} usb_interface;

/***************************************************************************/

typedef struct _usb_config_descriptor {
    u_int8_t    bLength;
    u_int8_t    bDescriptorType;
    u_int16_t 	wTotalLength;
    u_int8_t  	bNumInterfaces;
    u_int8_t  	bConfigurationValue;
    u_int8_t    iConfiguration;
    u_int8_t    bmAttributes;
    u_int8_t    MaxPower;
    usb_interface *interface;
} usb_config_descriptor;

/***************************************************************************/

typedef struct _usb_device_descriptor {
    u_int8_t    bLength;
    u_int8_t    bDescriptorType;
    u_int16_t   bcdUSB;
    u_int8_t    bDeviceClass;
    u_int8_t    bDeviceSubClass;
    u_int8_t    bDeviceProtocol;
    u_int8_t    bMaxPacketSize0;
    u_int16_t   idVendor;
    u_int16_t   idProduct;
    u_int16_t   bcdDevice;
    u_int8_t    iManufacturer;
    u_int8_t    iProduct;
    u_int8_t    iSerialNumber;
    u_int8_t    bNumConfigurations;
} usb_device_descriptor;

/***************************************************************************/

typedef struct _usb_string_descriptor {
    u_int8_t    bLength;
    u_int8_t    bDescriptorType;
    u_int16_t   wData[1];
} usb_string_descriptor;

/***************************************************************************/

typedef struct _usb_hid_descriptor {
    u_int8_t    bLength;
    u_int8_t    bDescriptorType;
    u_int16_t   bcdHID;
    u_int8_t    bCountryCode;
    u_int8_t    bNumDescriptors;
//    u_int8_t  bReportDescriptorType;
//    u_int16_t wDescriptorLength;
} usb_hid_descriptor;

/***************************************************************************/

typedef struct _usb_ctrl_setup {
    u_int8_t    bRequestType;
    u_int8_t    bRequest;
    u_int16_t   wValue;
    u_int16_t   wIndex;
    u_int16_t   wLength;
} usb_ctrl_setup;

/***************************************************************************/

typedef struct _usb_device {
    struct _usb_device *    next;
    struct _usb_device *    prev;
    usb_device_descriptor   descriptor;
    usb_config_descriptor * config;
    u_int8_t                devnum;
} usb_device;

/***************************************************************************/

int         usb_find_devices( usb_device **devices);
void        usb_free_dev( usb_device *dev);

/***************************************************************************/

#pragma pack()

#endif // _CAMLIBUSB_H_

/***************************************************************************/

