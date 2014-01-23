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
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include "rel_assert.h"
#include <stdio.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <inttypes.h>
#include <bladeRF.h>

#include "bladerf_priv.h"
#include "backend_config.h"
#include "backend/generic_usb.h"
#include "backend/generic_usb_types.h"
#include "conversions.h"
#include "minmax.h"
#include "log.h"
#include "flash.h"
#include <libusb.h>


static int lusb_device_is_bladerf(libusb_device *dev)
{
    int err;
    int rv = 0;
    struct libusb_device_descriptor desc;

    err = libusb_get_device_descriptor(dev, &desc);
    if( err ) {
        log_debug( "Couldn't open libusb device - %s\n", libusb_error_name(err) );
    } else {
        if( desc.idVendor == USB_NUAND_VENDOR_ID && desc.idProduct == USB_NUAND_BLADERF_PRODUCT_ID ) {
            rv = 1;
        } else {
            log_verbose("Non-bladeRF device found: VID %04x, PID %04x\n",
                        desc.idVendor, desc.idProduct);
        }
    }
    return rv;
}

static int lusb_device_is_fx3(libusb_device *dev)
{
    int err;
    int rv = 0;
    struct libusb_device_descriptor desc;

    err = libusb_get_device_descriptor(dev, &desc);
    if( err ) {
        log_debug( "Couldn't open libusb device - %s\n", libusb_error_name(err) );
    } else {
        if(
            (desc.idVendor == USB_CYPRESS_VENDOR_ID && desc.idProduct == USB_FX3_PRODUCT_ID) ||
            (desc.idVendor == USB_NUAND_VENDOR_ID && desc.idProduct == USB_NUAND_BLADERF_BOOT_PRODUCT_ID)
            ) {
            rv = 1;
        }
    }
    return rv;
}

static int lusb_get_devinfo(libusb_device *dev, struct bladerf_devinfo *info)
{
    int status = 0;
    libusb_device_handle *handle;
    struct libusb_device_descriptor desc;

    status = libusb_open( dev, &handle );
    if( status ) {
        log_debug( "Couldn't populate devinfo - %s\n", libusb_error_name(status) );
    } else {
        /* Populate device info */
        info->backend = BLADERF_BACKEND_LIBUSB;
        info->usb_bus = libusb_get_bus_number(dev);
        info->usb_addr = libusb_get_device_address(dev);

        status = libusb_get_device_descriptor(dev, &desc);
        if (status) {
            memset(info->serial, 0, BLADERF_SERIAL_LENGTH);
        } else {
            status = libusb_get_string_descriptor_ascii(handle,
                                                        desc.iSerialNumber,
                                                        (unsigned char *)&info->serial,
                                                        BLADERF_SERIAL_LENGTH);

            /* Consider this to be non-fatal, otherwise firmware <= 1.1
             * wouldn't be able to get far enough to upgrade */
            if (status < 0) {
                log_debug("Failed to retrieve serial number\n");
                memset(info->serial, 0, BLADERF_SERIAL_LENGTH);
            }

            /* Additinally, adjust for > 0 return code */
            status = 0;
        }

        libusb_close( handle );
    }


    return status;
}


static int genusb_libusb_probe(struct bladerf_devinfo_list *info_list)
{
    int status, i, n;
    ssize_t count;
    libusb_device **list;
    struct bladerf_devinfo info;

    libusb_context *context;

    /* Initialize libusb for device tree walking */
    status = libusb_init(&context);
    if( status ) {
        log_error( "Could not initialize libusb: %s\n", libusb_error_name(status) );
        goto lusb_probe_done;
    }

    count = libusb_get_device_list( context, &list );
    /* Iterate through all the USB devices */
    for( i = 0, n = 0; i < count && status == 0; i++ ) {
        if( lusb_device_is_bladerf(list[i]) ) {
            log_verbose("Found bladeRF (based upon VID/PID)\n");

            /* Open the USB device and get some information */
            status = lusb_get_devinfo( list[i], &info );
            if( status ) {
                log_error( "Could not open bladeRF device: %s\n", libusb_error_name(status) );
            } else {
                info.instance = n++;
                status = bladerf_devinfo_list_add(info_list, &info);
                if( status ) {
                    log_error( "Could not add device to list: %s\n", bladerf_strerror(status) );
                } else {
                    log_verbose("Added instance %d to device list\n",
                                info.instance);
                }
            }
        }

        if( lusb_device_is_fx3(list[i]) ) {
            status = lusb_get_devinfo( list[i], &info );
            if( status ) {
                log_error( "Could not open bladeRF device: %s\n", libusb_error_name(status) );
                continue;
            }

            log_info("Found FX3 bootloader device on bus=%d addr=%d. "
                     "This may be a bladeRF.\n",
                     info.usb_bus, info.usb_addr);

            log_info("Use bladeRF-cli command \"recover %d %d "
                     "<FX3 firmware>\" to boot the bladeRF firmware.\n",
                      info.usb_bus, info.usb_addr);
        }
    }
    libusb_free_device_list(list,1);
    libusb_exit(context);

lusb_probe_done:
    return status;

}

const generic_usb_fn bladerf_genusb_libusb_fn = {
    FIELD_INIT(.probe, genusb_libusb_probe),
};
