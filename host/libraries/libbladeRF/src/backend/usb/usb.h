/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014-2015 Nuand LLC
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

#ifndef BACKEND_USB_H_
#define BACKEND_USB_H_

#include "bladerf_priv.h"
#include "fx3_fw.h"

#if ENABLE_USB_DEV_RESET_ON_OPEN
extern bool bladerf_usb_reset_device_on_open;
#endif

#ifndef SAMPLE_EP_IN
#   define SAMPLE_EP_IN 0x81
#endif

#ifndef SAMPLE_EP_OUT
#   define SAMPLE_EP_OUT 0x01
#endif

#ifndef PERIPHERAL_EP_IN
#   define PERIPHERAL_EP_IN 0x82
#endif

#ifndef PERIPHERAL_EP_OUT
#   define PERIPHERAL_EP_OUT 0x02
#endif

#ifndef PERIPHERAL_TIMEOUT_MS
#   define PERIPHERAL_TIMEOUT_MS 250
#endif

/* Be careful when lowering this value. The control request for flash erase
 * operations take some time */
#ifndef CTRL_TIMEOUT_MS
#   define CTRL_TIMEOUT_MS 1000
#endif

#ifndef BULK_TIMEOUT_MS
#   define BULK_TIMEOUT_MS  1000
#endif

/* Size of a host<->FPGA message in BYTES */
#define USB_MSG_SIZE_SS    2048
#define USB_MSG_SIZE_HS    1024

typedef enum {
    USB_TARGET_DEVICE,
    USB_TARGET_INTERFACE,
    USB_TARGET_ENDPOINT,
    USB_TARGET_OTHER
} usb_target;

typedef enum {
    USB_REQUEST_STANDARD,
    USB_REQUEST_CLASS,
    USB_REQUEST_VENDOR
} usb_request;

typedef enum {
    USB_DIR_HOST_TO_DEVICE = 0x00,
    USB_DIR_DEVICE_TO_HOST = 0x80
} usb_direction;


/**
 * USB backend driver function table
 *
 * All return values are expected to be 0 on success, or a BLADERF_ERR_*
 * value on failure
 */
struct usb_fns {
    int (*probe)(backend_probe_target probe_target,
                 struct bladerf_devinfo_list *info_list);

    /* Populates the `driver` pointer with a handle for the specific USB driver.
     * `info_in` describes the device to open, and may contain wildcards.
     * On success, the driver should fill in `info_out` with the complete
     * details of the device. */
    int (*open)(void **driver,
                struct bladerf_devinfo *info_in,
                struct bladerf_devinfo *info_out);

    void (*close)(void *driver);

    int (*get_speed)(void *driver, bladerf_dev_speed *speed);

    int (*change_setting)(void *driver, uint8_t setting);

    int (*control_transfer)(void *driver,
                            usb_target target_type,
                            usb_request req_type,
                            usb_direction direction,
                            uint8_t request, uint16_t wvalue, uint16_t windex,
                            void *buffer, uint32_t buffer_len,
                            uint32_t timeout_ms);

    int (*bulk_transfer)(void *driver, uint8_t endpoint,
                         void *buffer, uint32_t buffer_len,
                         uint32_t timeout_ms);

    int (*get_string_descriptor)(void *driver, uint8_t index, void *buffer,
                                 uint32_t buffer_len);

    int (*init_stream)(void *driver, struct bladerf_stream *stream,
                       size_t num_transfers);

    int (*stream)(void *driver, struct bladerf_stream *stream,
                  bladerf_module module);

    int (*submit_stream_buffer)(void *driver, struct bladerf_stream *stream,
                                void *buffer, unsigned int timeout_ms,
                                bool nonblock);

    int (*deinit_stream)(void *driver, struct bladerf_stream *stream);

    int (*open_bootloader)(void **driver, uint8_t bus, uint8_t addr);
    void (*close_bootloader)(void *driver);
};

struct usb_driver {
    const bladerf_backend id;
    const struct usb_fns *fn;
};

struct bladerf_usb {
    const struct usb_fns *fn;
    void *driver;
};

static inline struct bladerf_usb *usb_backend(struct bladerf *dev, void **driver)
{
    struct bladerf_usb *ret = (struct bladerf_usb*)dev->backend;
    if (driver) {
        *driver = ret->driver;
    }
    return ret;
}

#endif
