/*
 * Copyright (c) 2013 Nuand LLC
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
#ifndef _INCLUDED_BLADE_H_
#define _INCLUDED_BLADE_H_

#include "cyu3externcstart.h"
#include "cyu3types.h"
#include "cyu3usbconst.h"

/* 16/32 bit GPIF Configuration select */
/* Set BLADE_GPIF_16_32BIT_CONF_SELECT = 0 for 16 bit GPIF data bus.
 * Set BLADE_GPIF_16_32BIT_CONF_SELECT = 1 for 32 bit GPIF data bus.
 */
#define BLADE_GPIF_16_32BIT_CONF_SELECT (0)

#define BLADE_DMA_BUF_COUNT      (2)
#define BLADE_DMA_TX_SIZE        (0)
#define BLADE_DMA_RX_SIZE        (0)
#define BLADE_THREAD_STACK       (0x0400)
#define BLADE_THREAD_PRIORITY    (8)

#define CY_FX_EP_PRODUCER               0x01    /* EP 1 OUT */
#define BLADE_FPGA_EP_PRODUCER          0x02    /* EP 2 OUT */
#define CY_FX_EP_CONSUMER               0x81    /* EP 1 IN */

#define CY_FX_PRODUCER_USB_SOCKET    CY_U3P_UIB_SOCKET_PROD_1    /* USB Socket 1 is producer */
#define BLADE_FPGA_CONFIG_SOCKET     CY_U3P_UIB_SOCKET_PROD_2    /* USB Socket 2 is producer */
#define CY_FX_CONSUMER_USB_SOCKET    CY_U3P_UIB_SOCKET_CONS_1    /* USB Socket 1 is consumer */

/* Used with FX3 Silicon. */
#define CY_FX_PRODUCER_PPORT_SOCKET    CY_U3P_PIB_SOCKET_0    /* P-port Socket 0 is producer */
#define CY_FX_CONSUMER_PPORT_SOCKET    CY_U3P_PIB_SOCKET_3    /* P-port Socket 3 is consumer */

/* Extern definitions for the USB Descriptors */
extern const uint8_t CyFxUSB20DeviceDscr[];
extern const uint8_t CyFxUSB30DeviceDscr[];
extern const uint8_t CyFxUSBDeviceQualDscr[];
extern const uint8_t CyFxUSBFSConfigDscr[];
extern const uint8_t CyFxUSBHSConfigDscr[];
extern const uint8_t CyFxUSBBOSDscr[];
extern const uint8_t CyFxUSBSSConfigDscr[];
extern const uint8_t CyFxUSBStringLangIDDscr[];
extern const uint8_t CyFxUSBManufactureDscr[];
extern const uint8_t CyFxUSBProductDscr[];

#include "cyu3externcend.h"

#endif /* _INCLUDED_BLADE_H_ */

/*[]*/
