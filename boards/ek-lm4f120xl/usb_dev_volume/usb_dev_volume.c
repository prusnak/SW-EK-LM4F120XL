//*****************************************************************************
//
// usb_dev_customhid.c - Main routines for the enumeration example.
//
// Copyright (c) 2009-2012 Texas Instruments Incorporated.  All rights reserved.
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
// This is a modification of part of revision 9453 of the EK-LM3S9D90 Firmware Package.
//Created by Jeff Lawrence and uses my usbhidcustom.c file

//
//*****************************************************************************
#include "inc/hw_ints.h"
#include "inc/hw_gpio.h"
#include "inc/hw_memmap.h"
#include "driverlib/fpu.h"
#include "inc/hw_types.h"
#include "driverlib/debug.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "usblib/usblib.h"
#include "usblib/usbhid.h"
#include "usblib/device/usbdevice.h"
#include "usblib/device/usbdhid.h"
#include "usblib/device/usbdhidcustom.h"
#include "usb_dev_volume_structs.h"


//*****************************************************************************
//
//! \addtogroup example_list
//! <h1>USB HID CustomHid Device (usb_dev_customhid)</h1>
//!
//! This example is a volume up and down HID device (right stellaris button increases volume, left decreases)
//! This utilizes usbhidcustom.c file which allows you to easily create custom devices (including combination HID devices)
//! Make sure usbhidcustom.c has this descriptor and header has CUSTOMHID_REPORT_SIZE=1
/*
	static const unsigned char g_pucCustomHidReportDescriptor[]=
	{

			UsagePage(USB_HID_CONSUMER_DEVICE),
			Usage(USB_HID_USAGE_CONSUMER_CONTROL),
			Collection(USB_HID_APPLICATION),

				LogicalMinimum(0),
				LogicalMaximum(1),
				Usage(USB_HID_VOLUME_UP),
				Usage(USB_HID_VOLUME_DOWN),
				ReportSize(1),
				ReportCount(2),
				Input(USB_HID_INPUT_DATA | USB_HID_INPUT_VARIABLE | USB_HID_INPUT_RELATIVE),

				ReportCount(6),
				Input(USB_HID_INPUT_CONSTANT | USB_HID_INPUT_ARRAY | USB_HID_INPUT_ABS),

			EndCollection,

	};

*/

//*****************************************************************************

//*****************************************************************************
//
// The system tick timer rate.
//
//*****************************************************************************
#define SYSTICKS_PER_SECOND     10 //Set lower so the volume change is more incremental

//*****************************************************************************
//
// Holds command bits used to signal the main loop to perform various tasks.
//
//*****************************************************************************
volatile unsigned long g_ulCommands;
#define TICK_EVENT              0

//*****************************************************************************
//
// A flag used to indicate whether or not we are currently connected to the USB
// host.
//
//*****************************************************************************
volatile tBoolean g_bConnected;

//*****************************************************************************
//
// Global system tick counter holds elapsed time since the application started
// expressed in 100ths of a second.
//
//*****************************************************************************
volatile unsigned long g_ulSysTickCount;

//*****************************************************************************
//
// The number of system ticks to wait for each USB packet to be sent before
// we assume the host has disconnected.  The value 50 equates to half a second.
//
//*****************************************************************************
#define MAX_SEND_DELAY          20



//*****************************************************************************
//
// This enumeration holds the various states that the customhid can be in during
// normal operation.
//
//*****************************************************************************
volatile enum
{
    //
    // Unconfigured.
    //
    CUSTOMHID_STATE_UNCONFIGURED,

    //
    // Nothing to send and not waiting on data.
    //
    CUSTOMHID_STATE_IDLE,

    //
    // Waiting on data to be sent out.
    //
    CUSTOMHID_STATE_SENDING
}
g_eCustomHidState = CUSTOMHID_STATE_UNCONFIGURED;

//*****************************************************************************
//
// The error routine that is called if the driver library encounters an error.
//
//*****************************************************************************
#ifdef DEBUG
void
__error__(char *pcFilename, unsigned long ulLine)
{
}
#endif

//*****************************************************************************
//
// This function handles notification messages from the customhid device driver.
//
//*****************************************************************************
unsigned long
CustomHidHandler(void *pvCBData, unsigned long ulEvent,
             unsigned long ulMsgData, void *pvMsgData)
{
    switch(ulEvent)
    {
        //
        // The USB host has connected to and configured the device.
        //
        case USB_EVENT_CONNECTED:
        {
            g_eCustomHidState = CUSTOMHID_STATE_IDLE;
            g_bConnected = true;

            break;
        }

        //
        // The USB host has disconnected from the device.
        //
        case USB_EVENT_DISCONNECTED:
        {
            g_bConnected = false;
            g_eCustomHidState = CUSTOMHID_STATE_UNCONFIGURED;

            break;
        }

        //
        // A report was sent to the host.  We are not free to send another.
        //
        case USB_EVENT_TX_COMPLETE:
        {
            g_eCustomHidState = CUSTOMHID_STATE_IDLE;
            break;
        }

    }

    return(0);
}

//***************************************************************************
//
// Wait for a period of time for the state to become idle.
//
// \param ulTimeoutTick is the number of system ticks to wait before
// declaring a timeout and returning \b false.
//
// This function polls the current keyboard state for ulTimeoutTicks system
// ticks waiting for it to become idle.  If the state becomes idle, the
// function returns true.  If it ulTimeoutTicks occur prior to the state
// becoming idle, false is returned to indicate a timeout.
//
// \return Returns \b true on success or \b false on timeout.
//
//***************************************************************************
tBoolean
WaitForSendIdle(unsigned long ulTimeoutTicks)
{
    unsigned long ulStart;
    unsigned long ulNow;
    unsigned long ulElapsed;

    ulStart = g_ulSysTickCount;
    ulElapsed = 0;

    while(ulElapsed < ulTimeoutTicks)
    {
        //
        // Is the customhid is idle, return immediately.
        //
        if(g_eCustomHidState == CUSTOMHID_STATE_IDLE)
        {
            return(true);
        }

        //
        // Determine how much time has elapsed since we started waiting.  This
        // should be safe across a wrap of g_ulSysTickCount.
        //
        ulNow = g_ulSysTickCount;
        ulElapsed = ((ulStart < ulNow) ? (ulNow - ulStart) :
                     (((unsigned long)0xFFFFFFFF - ulStart) + ulNow + 1));
    }

    //
    // If we get here, we timed out so return a bad return code to let the
    // caller know.
    //
    return(false);
}

//*****************************************************************************
//
// This function provides simulated movements of the .
//
//*****************************************************************************
void
CustomHidChangeHandler(void)
{
    unsigned long ulRetcode;
    unsigned long ulButton, ulButton2;


    //
    // Initialize report data
    //
    signed char VolumeReport[1]; //The array isn't needed but keeps consistent with reports with >1 byte
    VolumeReport[0]=0; //Don't normally send any data



    //
    //Read Left button and move mouse if pressed
    //
    ulButton = ROM_GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_4);
    if(ulButton == 0)
    {
    	VolumeReport[0]=2; //Volume Up (00000010)
    }

    //
    //Read Right button and sent ABCDEF if pressed
    //
    ulButton2 = ROM_GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_0);
    if(ulButton2 == 0)
    {
    	VolumeReport[0]=1; //Volume Down (00000001)
    }

    //
    // Send Device Report
    //
	g_eCustomHidState = CUSTOMHID_STATE_SENDING;
	ulRetcode = USBDHIDCustomHidStateChange((void *)&g_sCustomHidDevice, 0, VolumeReport); //0 is passed for reportID since this custom device doesn't have multiple reports

	// Did we schedule the report for transmission?
	//

	if(ulRetcode == CUSTOMHID_SUCCESS)
	{
		// Wait for the host to acknowledge the transmission if all went well.
		//
		if(!WaitForSendIdle(MAX_SEND_DELAY))
		{
			//
			// The transmission failed, so assume the host disconnected and go
			// back to waiting for a new connection.
			//
			g_bConnected = false;
		}
	}
}

//*****************************************************************************
//
// This is the interrupt handler for the SysTick interrupt.  It is called
// periodically and updates a global tick counter then sets a flag to tell the
// main loop to move the .
//
//*****************************************************************************
void
SysTickIntHandler(void)
{
	g_ulSysTickCount++;
    HWREGBITW(&g_ulCommands, TICK_EVENT) = 1;
}

//*****************************************************************************
//
// This is the main loop that runs the application.
//
//*****************************************************************************
int
main(void)
{

	ROM_FPULazyStackingEnable();
    // Set the clocking to run from the PLL at 50MHz.
    ROM_SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);

    // Configure USB pins
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    GPIOPinTypeUSBAnalog(GPIO_PORTD_BASE, GPIO_PIN_4 | GPIO_PIN_5);

    // Set the system tick to fire 100 times per second.
    ROM_SysTickEnable();
    ROM_SysTickIntEnable();
    ROM_SysTickPeriodSet(ROM_SysCtlClockGet() / SYSTICKS_PER_SECOND);

    // Initialize the on board buttons
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    // Right button is muxed so you need to unlock and configure
    HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY_DD;
    HWREG(GPIO_PORTF_BASE + GPIO_O_CR) |= 0x01;
    HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = 0;
    ROM_GPIOPinTypeGPIOInput(GPIO_PORTF_BASE, GPIO_PIN_4 |GPIO_PIN_0);
    ROM_GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_4 |GPIO_PIN_0, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);

    // Enable Output Status Light
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    ROM_GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3);
    // Set initial LED Status to RED to indicate not connected
     ROM_GPIOPinWrite(GPIO_PORTF_BASE,GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3, 2);

    // Set the USB stack mode to Device mode with VBUS monitoring.
    USBStackModeSet(0, USB_MODE_DEVICE, 0);

    // Pass the USB library our device information, initialize the USB
    // controller and connect the device to the bus.
    USBDHIDCustomHidInit(0, (tUSBDHIDCustomHidDevice *)&g_sCustomHidDevice);

    // Drop into the main loop.
    while(1)
    {
        // Wait for USB configuration to complete.
        while(!g_bConnected)
        {
        	   ROM_GPIOPinWrite(GPIO_PORTF_BASE,GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3, 2);
        }

        // Update the status to green when connected.
        ROM_GPIOPinWrite(GPIO_PORTF_BASE,GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3, 8);

        // Now keep processing the customhid as long as the host is connected.
        while(g_bConnected)
        {
            // If it is time to check the buttons and send a customhid report then do so.
            if(HWREGBITW(&g_ulCommands, TICK_EVENT) == 1)
            {
               HWREGBITW(&g_ulCommands, TICK_EVENT) = 0;
               CustomHidChangeHandler();
            }
        }
    }
}
