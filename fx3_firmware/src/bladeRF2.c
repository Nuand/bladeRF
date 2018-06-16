/*
 * bladeRF 2.0 FX3 firmware (bladeRF1.c)
 *
 * Copyright (c) 2017 Nuand LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "bladeRF.h"
#include <cyu3error.h>

/* Nuand VID */
#define VID_LOW             (USB_NUAND_VENDOR_ID & 0xff)
#define VID_HIGH            (USB_NUAND_VENDOR_ID >> 8)

/* Nuand bladeRF PID */
#define BLADERF_PID_LOW     (USB_NUAND_BLADERF2_PRODUCT_ID & 0xff)
#define BLADERF_PID_HIGH    (USB_NUAND_BLADERF2_PRODUCT_ID >> 8)

/* Standard device descriptor for USB 3.0 */
const uint8_t CyFxUSB30DeviceDscr_bladeRF2[] __attribute__ ((aligned (32))) __attribute__ ((section(".usbdscr"))) =
{
    0x12,                                   /* Descriptor size */
    CY_U3P_USB_DEVICE_DESCR,                /* Device descriptor type */
    0x00,0x03,                              /* USB 3.0 */
    0x00,                                   /* Device class */
    0x00,                                   /* Device sub-class */
    0x00,                                   /* Device protocol */
    0x09,                                   /* Maxpacket size for EP0 : 2^9 */
    VID_LOW, VID_HIGH,                      /* Vendor ID */
    BLADERF_PID_LOW, BLADERF_PID_HIGH,      /* Product ID */
    0x00,0x00,                              /* Device release number */
    0x01,                                   /* Manufacture string index */
    0x02,                                   /* Product string index */
    0x03,                                   /* Serial number string index */
    0x01                                    /* Number of configurations */
};

/* Standard device descriptor for USB 2.0 */
const uint8_t CyFxUSB20DeviceDscr_bladeRF2[] __attribute__ ((aligned (32))) __attribute__ ((section(".usbdscr"))) =
{
    0x12,                                   /* Descriptor size */
    CY_U3P_USB_DEVICE_DESCR,                /* Device descriptor type */
    0x10,0x02,                              /* USB 2.10 */
    0x00,                                   /* Device class */
    0x00,                                   /* Device sub-class */
    0x00,                                   /* Device protocol */
    0x40,                                   /* Maxpacket size for EP0 : 64 bytes */
    VID_LOW, VID_HIGH,                      /* Vendor ID */
    BLADERF_PID_LOW, BLADERF_PID_HIGH,      /* Product ID */
    0x00,0x00,                              /* Device release number */
    0x01,                                   /* Manufacture string index */
    0x02,                                   /* Product string index */
    0x03,                                   /* Serial number string index */
    0x01                                    /* Number of configurations */
};

/* Standard product string descriptor */
const uint8_t CyFxUSBProductDscr_bladeRF2[] __attribute__ ((aligned (32))) __attribute__ ((section(".usbdscr"))) =
{
    0x18,                           /* Descriptor size */
    CY_U3P_USB_STRING_DESCR,        /* Device descriptor type */
    'b',0x00,
    'l',0x00,
    'a',0x00,
    'd',0x00,
    'e',0x00,
    'R',0x00,
    'F',0x00,
    ' ',0x00,
    '2',0x00,
    '.',0x00,
    '0',0x00
};

/* Place this buffer as the last buffer in the .usbdscr section so that no
 * other variable / code shares the same cache line. Do not add any other
 * variables / arrays in section. This will lead to variables sharing the
 * same cache line. */
const uint8_t CyFxUsbDscrAlignBuffer_bladeRF2[32] __attribute__ ((aligned (32))) __attribute__ ((section(".usbdscr"))) = {};

void NuandFpgaConfigSwFlipLut_bladeRF2(uint16_t flipLut[256])
{
    uint32_t i;

    for (i = 0; i < 256; i++) {
        flipLut[i] = i;
    }
}
