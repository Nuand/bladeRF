/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2013 Nuand LLC
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#ifndef BLADERF_GENUSB_TYPES_H_
#define BLADERF_GENUSB_TYPES_H_

#include "bladerf_priv.h"

typedef enum 
{
     TARGET_DEVICE, 
     TARGET_INTERFACE, 
     TARGET_ENDPOINT,
     TARGET_OTHER 
} GEN_USB_TRANSFER_TARGET_TYPE;

typedef enum 
{
    REQUEST_STANDARD, 
    REQUEST_CLASS, 
    REQUEST_VENDOR 
} GEN_USB_TRANSFER_REQUEST_TYPE;

typedef enum 
{
    DIRECTION_TO_DEVICE, 
    DIRECTION_FROM_DEVICE 
} GEN_USB_TRANSFER_DIRECTION;


/* USB Driver specific function table. These functions are required for each
   unique USB platform to operate the device. */
typedef struct {
    /* Backends probe for devices and append entries to this list using
     * bladerf_devinfo_list_append() */
    int (*probe)(struct bladerf_devinfo_list *info_list);
    int (*open)(struct bladerf **device, void **DriverData,struct bladerf_devinfo *info);
    int (*get_speed)(void *DriverData,bladerf_dev_speed *device_speed);
    int (*control_transfer)(void *DriverData,GEN_USB_TRANSFER_TARGET_TYPE Target, GEN_USB_TRANSFER_REQUEST_TYPE RequestType, GEN_USB_TRANSFER_DIRECTION Direction, uint8_t request, uint16_t wValue, uint16_t wIndex, void* Buffer, uint32_t BufferSize , uint32_t *BytesTransfered , uint32_t TimeOut );
    int (*change_setting)(void *DriverData, uint8_t setting);
    int (*bulk_transfer)(void *DriverData, uint8_t endpoint, void*Buffer, uint32_t BufferLength ,uint32_t *BytesTransfered, uint32_t TimeOut);
    int (*get_string_descriptor)(void *DriverData, uint8_t Index, void* Buffer, int BufferSize);
    int (*close)(void *DriverData);
    int (*stream_init)(void *DriverData,struct bladerf_stream *stream);
    int (*stream)(void *DriverData,struct bladerf_stream *stream, bladerf_module module);
    int (*deinit_stream)(void *DriverData,struct bladerf_stream *stream);
} generic_usb_fn ;

#endif
