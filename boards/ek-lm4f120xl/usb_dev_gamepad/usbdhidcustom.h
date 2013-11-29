//*****************************************************************************
//
// usbdhidmisc.h - Public header file for the USB HID misc device class
//                  driver
//
// Copyright (c) 2008-2012 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
// 
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
// 
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
// 
// This is part of revision 9453 of the Stellaris USB Library.
//
//*****************************************************************************

#ifndef __USBDHIDCUSTOMHID_H__
#define __USBDHIDCUSTOMHID_H__

//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
//! \addtogroup hid_misc_device_class_api
//! @{
//
//*****************************************************************************

//*****************************************************************************
//
// PRIVATE
//
// The first few sections of this header are private defines that are used by
// the USB HID misc code and are here only to help with the application
// allocating the correct amount of memory for the HID misc device code.
//
//*****************************************************************************

//*****************************************************************************
//
// PRIVATE
//
// The size (in bytes) of the misc input report sent to the host.
// If you have a single report with multiple ReportIDs - make this the size of the largest single report + 1 for the ID
// Example Keyboard = 1 for ID, 1 for Modifier, 1 for reserved, 6 for keypresses = 9
//*****************************************************************************
//#define CUSTOMHID_REPORT_SIZE       9 //Keyboard + Mouse + 2 ReportIDs
#define CUSTOMHID_REPORT_SIZE       6 //Gamepad w/ No reportID
//#define CUSTOMHID_REPORT_SIZE       1 //volume up and down HID device

//*****************************************************************************
//
// PRIVATE
//
// This enumeration holds the various states that the misc can be in during
// normal operation.
//
//*****************************************************************************
typedef enum
{
    //
    // Unconfigured.
    //
    HID_CUSTOMHID_STATE_UNCONFIGURED,

    //
    // No keys to send and not waiting on data.
    //
    HID_CUSTOMHID_STATE_IDLE,

    //
    // Waiting on report data from the host.
    //
    HID_CUSTOMHID_STATE_WAIT_DATA,

    //
    // Waiting on data to be sent out.
    //
    HID_CUSTOMHID_STATE_SEND
}
tCustomHidState;

//*****************************************************************************
//
// PRIVATE
//
// This structure provides the private instance data structure for the USB
// HID CustomHid device.  This structure forms the RAM workspace used by each
// instance of the misc.
//
//*****************************************************************************
typedef struct
{
    //
    // The USB configuration number set by the host or 0 of the device is
    // currently unconfigured.
    //
    unsigned char ucUSBConfigured;

    //
    // The protocol requested by the host, USB_HID_PROTOCOL_BOOT or
    // USB_HID_PROTOCOL_REPORT.
    //
    unsigned char ucProtocol;

    //
    // A buffer used to hold the last input report sent to the host.
    //
    unsigned char pucReport[CUSTOMHID_REPORT_SIZE];

    //
    // The current state of the misc interrupt IN endpoint.
    //
    volatile tCustomHidState eCustomHidState;

    //
    // The idle timeout control structure for our input report.  This is
    // required by the lower level HID driver.
    //
    tHIDReportIdle sReportIdle;

    //
    // The lower level HID driver's instance data.
    //
    tHIDInstance sHIDInstance;

    //
    // This is needed for the lower level HID driver.
    //
    tUSBDHIDDevice sHIDDevice;
}
tHIDCustomHidInstance;

#ifdef DEPRECATED
//*****************************************************************************
//
// The number of bytes of workspace required by the HID misc driver.
// The client must provide a block of RAM of at least this size in the
// tHIDCustomHidInstance field of the tUSBHIDCustomHidDevice structure passed on
// USBDHIDCustomHidInit().  The HID misc driver needs space for the generic HID
// interface + the CustomHid Report Buffer + HID misc interface.
//
// This value is deprecated and should not be used, any new code should just
// pass in a tHIDCustomHidInstance structure in the psPrivateHIDCustomHidData field.
//
//*****************************************************************************
#define USB_HID_CUSTOMHID_WORKSPACE_SIZE \
                                 (sizeof(tHIDCustomHidInstance))
#endif

//*****************************************************************************
//
//! This structure is used by the application to define operating parameters
//! for the HID misc device.
//
//*****************************************************************************
typedef struct
{
    //
    //! The vendor ID that this device is to present in the device descriptor.
    //
    unsigned short usVID;

    //
    //! The product ID that this device is to present in the device descriptor.
    //
    unsigned short usPID;

    //
    //! The maximum power consumption of the device, expressed in milliamps.
    //
    unsigned short usMaxPowermA;

    //
    //! Indicates whether the device is self- or bus-powered and whether or not
    //! it supports remote wakeup.  Valid values are USB_CONF_ATTR_SELF_PWR or
    //! USB_CONF_ATTR_BUS_PWR, optionally ORed with USB_CONF_ATTR_RWAKE.
    //
    unsigned char ucPwrAttributes;

    //
    //! A pointer to the callback function which will be called to notify
    //! the application of events relating to the operation of the misc.
    //
    tUSBCallback pfnCallback;

    //
    //! A client-supplied pointer which will be sent as the first
    //! parameter in all calls made to the misc callback, pfnCallback.
    //
    void *pvCBData;

    //
    //! A pointer to the string descriptor array for this device.  This array
    //! must contain the following string descriptor pointers in this order.
    //! Language descriptor, Manufacturer name string (language 1), Product
    //! name string (language 1), Serial number string (language 1),HID
    //! Interface description string (language 1), Configuration description
    //! string (language 1).
    //!
    //! If supporting more than 1 language, the descriptor block (except for
    //! string descriptor 0) must be repeated for each language defined in the
    //! language descriptor.
    //
    const unsigned char * const *ppStringDescriptors;

    //
    //! The number of descriptors provided in the ppStringDescriptors
    //! array.  This must be (1 + (5 * (num languages))).
    //
    unsigned long ulNumStringDescriptors;

    //
    //! A pointer to private instance data for this device.  This memory must
    //! remain accessible for as long as the misc device is in use and must
    //! not be modified by any code outside the HID misc driver.
    //
    tHIDCustomHidInstance *psPrivateHIDCustomHidData;
}
tUSBDHIDCustomHidDevice;

//*****************************************************************************
//
//! This return code from USBDHIDCustomHidStateChange indicates success.
//
//*****************************************************************************
#define CUSTOMHID_SUCCESS           0

//*****************************************************************************
//
//! This return code from USBDHIDCustomHidStateChange indicates that an error was
//! reported while attempting to send a report to the host.  A client should
//! assume that the host has disconnected if this return code is seen.
//
//*****************************************************************************
#define CUSTOMHID_ERR_TX_ERROR      2

//*****************************************************************************
//
//! USBDHIDCustomHidStateChange returns this value if it is called before the
//! USB host has connected and configured the device.  All misc state
//! information passed on the call will have been ignored.
//
//*****************************************************************************
#define CUSTOMHID_ERR_NOT_CONFIGURED \
                                4

//**
//*****************************************************************************
//
// API Function Prototypes
//
//*****************************************************************************
extern void *USBDHIDCustomHidInit(unsigned long ulIndex,
                              const tUSBDHIDCustomHidDevice *psDevice);
extern void *USBDHIDCustomHidCompositeInit(unsigned long ulIndex,
                                       const tUSBDHIDCustomHidDevice *psDevice);
extern void USBDHIDCustomHidTerm(void *pvInstance);
extern void *USBDHIDCustomHidSetCBData(void *pvInstance, void *pvCBData);
extern unsigned long USBDHIDCustomHidStateChange(void *pvInstance, char ReportID, signed char HIDData[]);
extern void USBDHIDCustomHidPowerStatusSet(void *pvInstance,
                                       unsigned char ucPower);
extern tBoolean USBDHIDCustomHidRemoteWakeupRequest(void *pvInstance);

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif // __USBDHIDCUSTOMHID_H__
