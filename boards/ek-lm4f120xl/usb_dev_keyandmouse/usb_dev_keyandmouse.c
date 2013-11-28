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
//Created by Jeff Lawrence based on the HID  example

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
#include "usb_dev_keyandmouse_structs.h"


//*****************************************************************************
//
//! \addtogroup example_list
//! <h1>USB HID CustomHid Device (usb_dev_customhid)</h1>
//!
//! This example application turns the evaluation board into a  custom mouse + keyboard device
//!  Pressing the Left Launchpad button will move the mouse, pressing the right will send abcdef to the active window
//!  This is a simple keyboard example so doesn't receive the keyboard led report or handle the scan codes
//   You must ensure that USBDHIDCustom.h has a matching report descriptor for this device and set CUSTOMHID_REPORT_SIZE= 9 in the header
//!
/*
static const unsigned char g_pucCustomHidReportDescriptor[]=
{
		 UsagePage(USB_HID_GENERIC_DESKTOP),
		    Usage(USB_HID_KEYBOARD),
		    Collection(USB_HID_APPLICATION),

		    	ReportID(1),
		        //
		        // Modifier keys.
		        // 8 - 1 bit values indicating the modifier keys (ctrl, shift...)
		        //
		        ReportSize(1),
		        ReportCount(8),
		        UsagePage(USB_HID_USAGE_KEYCODES),
		        UsageMinimum(224),
		        UsageMaximum(231),
		        LogicalMinimum(0),
		        LogicalMaximum(1),
		        Input(USB_HID_INPUT_DATA | USB_HID_INPUT_VARIABLE | USB_HID_INPUT_ABS),

		        //
		        // One byte of rsvd data required by HID spec.
		        //
		        ReportCount(1),
		        ReportSize(8),
		        Input(USB_HID_INPUT_CONSTANT),

		        //
		        // Keyboard LEDs.
		        // 5 - 1 bit values.
		        //
		        ReportCount(5),
		        ReportSize(1),
		        UsagePage(USB_HID_USAGE_LEDS),
		        UsageMinimum(1),
		        UsageMaximum(5),
		        Output(USB_HID_OUTPUT_DATA | USB_HID_OUTPUT_VARIABLE |
		               USB_HID_OUTPUT_ABS),
		        //
		        // 1 - 3 bit value to pad out to a full byte.
		        //
		        ReportCount(1),
		        ReportSize(3),
		        Output(USB_HID_OUTPUT_CONSTANT), //LED report padding

		        //
		        // The Key buffer.
		        // 6 - 8 bit values to store the current key state.
		        //
		        ReportCount(6),
		        ReportSize(8),
		        LogicalMinimum(0),
		        LogicalMaximum(101),
		        UsagePage(USB_HID_USAGE_KEYCODES),
		        UsageMinimum (0),
		        UsageMaximum (101),
		        Input(USB_HID_INPUT_DATA | USB_HID_INPUT_ARRAY),
		    EndCollection,

		UsagePage(USB_HID_GENERIC_DESKTOP),
			Usage(USB_HID_MOUSE),
			Collection(USB_HID_APPLICATION),
				Usage(USB_HID_POINTER),
				Collection(USB_HID_PHYSICAL),

					ReportID(2),
					//
					// The buttons.
					//
					UsagePage(USB_HID_BUTTONS),
					UsageMinimum(1),
					UsageMaximum(3),
					LogicalMinimum(0),
					LogicalMaximum(1),

					//
					// 3 - 1 bit values for the buttons.
					//
					ReportSize(1),
					ReportCount(3),
					Input(USB_HID_INPUT_DATA | USB_HID_INPUT_VARIABLE | USB_HID_INPUT_ABS),

					//
					// 1 - 5 bit unused constant value to fill the 8 bits.
					//
					ReportSize(5),
					ReportCount(1),
					Input(USB_HID_INPUT_CONSTANT | USB_HID_INPUT_ARRAY | USB_HID_INPUT_ABS),

					//
					// The X and Y axis.
					//
					UsagePage(USB_HID_GENERIC_DESKTOP),
					Usage(USB_HID_X),
					Usage(USB_HID_Y),
					LogicalMinimum(-127),
					LogicalMaximum(127),

					//
					// 2 - 8 bit Values for x and y.
					//
					ReportSize(8),
					ReportCount(2),
					Input(USB_HID_INPUT_DATA | USB_HID_INPUT_VARIABLE | USB_HID_INPUT_RELATIVE),


				EndCollection,
			EndCollection,

};
*/
//*****************************************************************************

//*****************************************************************************
//
// The system tick timer rate.
//
//*****************************************************************************
#define SYSTICKS_PER_SECOND     200

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
    // Initialize all keys to 0 - this un-presses a key if you press one
    //
    int KeyboardReportID=1;
    signed char KeyboardReport[8];
    KeyboardReport[0]=0; //modifier
    KeyboardReport[1]=0; //reserved always 0
    KeyboardReport[2]=0; //key1
    KeyboardReport[3]=0; //key2
    KeyboardReport[4]=0; //key3
    KeyboardReport[5]=0; //key4
    KeyboardReport[6]=0; //key5
    KeyboardReport[7]=0; //key6

    //
    // Initialize mouse values to 0, mouse movement is incremental so it won't reset the cursor
    //
    int MouseReportID=2;
    signed char MouseReport[3];
    MouseReport[0]=0; //Buttons
    MouseReport[1]=0; //X
    MouseReport[2]=0; //Y

    //
    //Read Left button and move mouse if pressed
    //
    ulButton = ROM_GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_4);
    if(ulButton == 0)
    {
    	MouseReport[0]=0; //Buttons
	MouseReport[1]=-15; //X increment
	MouseReport[2]=-5; //Y increment
    }

    //
    //Read Right button and sent ABCDEF if pressed
    //
    ulButton2 = ROM_GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_0);
    if(ulButton2 == 0)
    {
    	KeyboardReport[0]=0; //modifier (CTRL, ALT, SHIFT, etc)
    	KeyboardReport[1]=0; //Reserved, always equals 0
    	KeyboardReport[2]=HID_KEYB_USAGE_A; //key1 -these are HID scan codes (4=a)
    	KeyboardReport[3]=HID_KEYB_USAGE_B; //key2 - b
    	KeyboardReport[4]=HID_KEYB_USAGE_C; //key3 - c
    	KeyboardReport[5]=HID_KEYB_USAGE_D; //key4 - d
    	KeyboardReport[6]=HID_KEYB_USAGE_E; //key5 - e
    	KeyboardReport[7]=HID_KEYB_USAGE_F; //key6 - f
    }

    //
    // Send Keyboard Report
    //
	g_eCustomHidState = CUSTOMHID_STATE_SENDING;
	ulRetcode = USBDHIDCustomHidStateChange((void *)&g_sCustomHidDevice, KeyboardReportID, KeyboardReport);

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

    //
    // Send Mouse Report
    //
	g_eCustomHidState = CUSTOMHID_STATE_SENDING;
	ulRetcode = USBDHIDCustomHidStateChange((void *)&g_sCustomHidDevice, MouseReportID, MouseReport);

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
