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
#include <pthread.h>
#include <errno.h>
#include <libusb.h>
#include "bladeRF.h"    /* Firmware interface */

#include "backend/backend.h"
#include "backend/usb/usb.h"
#include "async.h"
#include "log.h"

#ifndef LIBUSB_HANDLE_EVENTS_TIMEOUT_NSEC
#   define LIBUSB_HANDLE_EVENTS_TIMEOUT_NSEC    (15 * 1000)
#endif

struct bladerf_lusb {
    libusb_device           *dev;
    libusb_device_handle    *handle;
    libusb_context          *context;
};

typedef enum {
    TRANSFER_UNINITIALIZED = 0,
    TRANSFER_AVAIL,
    TRANSFER_IN_FLIGHT,
    TRANSFER_CANCEL_PENDING
} transfer_status;

struct lusb_stream_data {
    size_t num_transfers;               /* Total # of allocated transfers */
    size_t num_avail;                   /* # of currently available transfers */
    size_t i;                           /* Index to next transfer */
    struct libusb_transfer **transfers; /* Array of transfer metadata */
    transfer_status *transfer_status;   /* Status of each transfer */

   /* Warn the first time we get a transfer callback out of order.
    * This shouldn't happen normaly, but we've seen it intermittently on
    * libusb 1.0.19 for Windows. Further investigation required...
    */
    bool out_of_order_event;
};

static inline struct bladerf_lusb * lusb_backend(struct bladerf *dev)
{
    struct bladerf_usb *usb;

    assert(dev && dev->backend);
    usb = (struct bladerf_usb *) dev->backend;

    assert(usb->driver);
    return (struct bladerf_lusb *) usb->driver;
}

/* Convert libusb error codes to libbladeRF error codes */
static int error_conv(int error)
{
    int ret;

    switch (error) {
        case 0:
            ret = 0;
            break;

        case LIBUSB_ERROR_IO:
            ret = BLADERF_ERR_IO;
            break;

        case LIBUSB_ERROR_INVALID_PARAM:
            ret = BLADERF_ERR_INVAL;
            break;

        case LIBUSB_ERROR_BUSY:
        case LIBUSB_ERROR_NO_DEVICE:
            ret = BLADERF_ERR_NODEV;
            break;

        case LIBUSB_ERROR_TIMEOUT:
            ret = BLADERF_ERR_TIMEOUT;
            break;

        case LIBUSB_ERROR_NO_MEM:
            ret = BLADERF_ERR_MEM;
            break;

        case LIBUSB_ERROR_NOT_SUPPORTED:
            ret = BLADERF_ERR_UNSUPPORTED;
            break;

        case LIBUSB_ERROR_OVERFLOW:
        case LIBUSB_ERROR_PIPE:
        case LIBUSB_ERROR_INTERRUPTED:
        case LIBUSB_ERROR_ACCESS:
        case LIBUSB_ERROR_NOT_FOUND:
        default:
            ret = BLADERF_ERR_UNEXPECTED;
    }

    return ret;
}

/* Returns libusb error codes */
static int get_devinfo(libusb_device *dev, struct bladerf_devinfo *info)
{
    int status = 0;
    libusb_device_handle *handle;
    struct libusb_device_descriptor desc;

    status = libusb_open(dev, &handle);
    if( status ) {
        log_debug("Couldn't populate devinfo - %s\n",
                  libusb_error_name(status));
    } else {
        /* Populate device info */
        info->backend = BLADERF_BACKEND_LIBUSB;
        info->usb_bus = libusb_get_bus_number(dev);
        info->usb_addr = libusb_get_device_address(dev);

        status = libusb_get_device_descriptor(dev, &desc);
        if (status != 0) {
            memset(info->serial, 0, BLADERF_SERIAL_LENGTH);
        } else {
            status = libusb_get_string_descriptor_ascii(
                                        handle, desc.iSerialNumber,
                                        (unsigned char *)&info->serial,
                                        BLADERF_SERIAL_LENGTH);

            /* Consider this to be non-fatal, otherwise firmware <= 1.1
             * wouldn't be able to get far enough to upgrade */
            if (status < 0) {
                log_debug("Failed to retrieve serial number\n");
                memset(info->serial, 0, BLADERF_SERIAL_LENGTH);
            } else {
                /* Adjust for > 0 return code */
                status = 0;
            }

        }

        libusb_close(handle);
    }

    return status;
}

static bool device_has_vid_pid(libusb_device *dev, uint16_t vid, uint16_t pid)
{
    int status;
    struct libusb_device_descriptor desc;
    bool match = false;

    status = libusb_get_device_descriptor(dev, &desc);
    if (status != 0) {
        log_debug("Couldn't get device descriptor: %s\n",
                  libusb_error_name(status));
    } else {
        match = (desc.idVendor == vid) && (desc.idProduct == pid);
    }

    return match;
}

static bool device_is_fx3_bootloader(libusb_device *dev)
{
    return device_has_vid_pid(dev, USB_CYPRESS_VENDOR_ID, USB_FX3_PRODUCT_ID) ||
           device_has_vid_pid(dev, USB_NUAND_VENDOR_ID,
                              USB_NUAND_BLADERF_BOOT_PRODUCT_ID);
}

static bool device_is_bladerf(libusb_device *dev)
{
    struct libusb_config_descriptor *cfg;
    int status;
    bool ret;

    if(!device_has_vid_pid(dev, USB_NUAND_VENDOR_ID,
                           USB_NUAND_BLADERF_PRODUCT_ID)) {

        return false;
    }

    status = libusb_get_config_descriptor(dev, 0, &cfg);
    if (status != 0) {
        log_debug("Failed to get configuration descriptor: %s\n",
                  libusb_error_name(status));
        return false;
    }

    /* As of firmware v0.4, we expect there to be 4 altsettings on the
     * first interface. */
    if (cfg->interface->num_altsetting != 4) {
        const uint8_t bus = libusb_get_bus_number(dev);
        const uint8_t addr = libusb_get_device_address(dev);

        log_warning("A bladeRF running incompatible firmware appears to be "
                    "present on bus=%u, addr=%u. If this is true, a firmware "
                    "update via the device's bootloader is required.\n\n",
                    bus, addr);

        ret = false;
    } else {
        ret = true;
    }

    libusb_free_config_descriptor(cfg);
    return ret;
}

static bool device_is_probe_target(backend_probe_target probe_target,
                                   libusb_device *dev)
{
    bool is_probe_target = false;

    switch (probe_target) {
        case BACKEND_PROBE_BLADERF:
            is_probe_target = device_is_bladerf(dev);
            if (is_probe_target) {
                log_verbose("Found a bladeRF\n");
            }
            break;

        case BACKEND_PROBE_FX3_BOOTLOADER:
            is_probe_target = device_is_fx3_bootloader(dev);
            if (is_probe_target) {
                log_verbose("Found an FX3 bootloader.\n");
            }
            break;

        default:
            assert(!"Invalid probe target");
    }

    return is_probe_target;
}

static int lusb_probe(backend_probe_target probe_target,
                      struct bladerf_devinfo_list *info_list)
{
    int status, i, n;
    ssize_t count;
    libusb_device **list;
    struct bladerf_devinfo info;

    libusb_context *context;

    /* Initialize libusb for device tree walking */
    status = libusb_init(&context);
    if (status) {
        log_error("Could not initialize libusb: %s\n",
                  libusb_error_name(status));
        goto lusb_probe_done;
    }

    count = libusb_get_device_list(context, &list);
    /* Iterate through all the USB devices */
    for (i = 0, n = 0; i < count && status == 0; i++) {
        if (device_is_probe_target(probe_target, list[i])) {

            /* Open the USB device and get some information */
            status = get_devinfo(list[i], &info);
            if (status) {
                /* We may not be able to open the device if another
                 * driver (e.g., CyUSB3) is associated with it. Therefore,
                 * just log to the debug level and carry on. */
                status = 0;
                log_debug("Could not open device: %s\n",
                          libusb_error_name(status) );
            } else {
                info.instance = n++;
                status = bladerf_devinfo_list_add(info_list, &info);
                if( status ) {
                    log_error("Could not add device to list: %s\n",
                              bladerf_strerror(status) );
                } else {
                    log_verbose("Added instance %d to device list\n",
                                info.instance);
                }
            }
        }
    }

    libusb_free_device_list(list, 1);
    libusb_exit(context);

lusb_probe_done:
    return status;
}

#ifdef HAVE_LIBUSB_GET_VERSION
static inline void get_libusb_version(char *buf, size_t buf_len)
{
    const struct libusb_version *version;
    version = libusb_get_version();

    snprintf(buf, buf_len, "%d.%d.%d.%d%s", version->major, version->minor,
             version->micro, version->nano, version->rc);
}
#else
static void get_libusb_version(char *buf, size_t buf_len)
{
    snprintf(buf, buf_len, "<= 1.0.9");
}
#endif


static int open_device(const struct bladerf_devinfo *info,
                       libusb_context *context,
                       libusb_device *libusb_dev_in,
                       struct bladerf_lusb **dev_out)
{
    int status;
    struct bladerf_lusb *dev;

    *dev_out = NULL;

    dev = (struct bladerf_lusb *) calloc(1, sizeof(dev[0]));
    if (dev == NULL) {
        log_debug("Failed allocate handle for instance %d.\n",
                  info->instance);

        /* Report "no device" so we could try again with
         * another matching device */
        return BLADERF_ERR_NODEV;
    }

    dev->context = context;
    dev->dev = libusb_dev_in;

    status = libusb_open(libusb_dev_in, &dev->handle);
    if (status < 0) {
        log_debug("Failed to open device instance %d: %s\n",
                  info->instance, libusb_error_name(status));

        status = error_conv(status);
        goto error;
    }

    status = libusb_claim_interface(dev->handle, 0);
    if (status < 0) {
        log_debug("Failed to claim interface 0 for instance %d: %s\n",
                  info->instance, libusb_error_name(status));

        status = error_conv(status);
        goto error;
    }

error:
    if (status != 0) {
        if (dev->handle != NULL) {
            libusb_close(dev->handle);
        }

        free(dev);
    } else {
        *dev_out = dev;
    }

    return status;
}

static int find_and_open_device(libusb_context *context,
                                const struct bladerf_devinfo *info_in,
                                struct bladerf_lusb **dev_out,
                                struct bladerf_devinfo *info_out)
{
    int status = BLADERF_ERR_NODEV;
    int i, n;
    ssize_t count;
    struct libusb_device **list;
    struct bladerf_devinfo curr_info;

    *dev_out = NULL;

    count = libusb_get_device_list(context, &list);
    if (count < 0) {
        if (count < INT_MIN) {
            /* Ensure we don't have a situation where we accidentally return 0
             * due to a narrowing coversion */
            return BLADERF_ERR_UNEXPECTED;
        } else {
            return error_conv((int) count);
        }
    }

    for (i = 0, n = 0; (i < count) && (*dev_out == NULL); i++) {
        if (device_is_bladerf(list[i])) {
            log_verbose("Found a bladeRF (idx=%d)\n", i);

            /* Open the USB device and get some information */
            status = get_devinfo(list[i], &curr_info);
            if (status < 0) {
                log_debug("Could not open bladeRF device: %s\n",
                        libusb_error_name(status) );
                status = BLADERF_ERR_NODEV;
                continue; /* Continue trying the next devices */
            } else {
                curr_info.instance = n++;
            }

            /* Check to see if this matches the info struct */
            if (bladerf_devinfo_matches(&curr_info, info_in)) {
                status = open_device(&curr_info, context, list[i], dev_out);
                if (status < 0) {
                    status = BLADERF_ERR_NODEV;
                    continue; /* Continue trying the next matching device */
                } else {
                    memcpy(info_out, &curr_info, sizeof(info_out[0]));
                }
            } else {
                status = BLADERF_ERR_NODEV;

                log_verbose("Devinfo doesn't match - skipping"
                        "(instance=%d, serial=%d, bus/addr=%d\n",
                        bladerf_instance_matches(&curr_info, info_in),
                        bladerf_serial_matches(&curr_info, info_in),
                        bladerf_bus_addr_matches(&curr_info, info_in));
            }
        }
    }

    if (status == 0) {
        /* Returning 0 indicates this function is providing a device */
        assert(*dev_out != NULL);
    }

    libusb_free_device_list(list, 1);
    return status;
}

#if ENABLE_USB_DEV_RESET_ON_OPEN
static int reset_and_reopen(libusb_context *context,
                            struct bladerf_lusb **dev,
                            struct bladerf_devinfo *info)
{
    int status;

    status = libusb_reset_device((*dev)->handle);
    if (status == 0) {
        log_verbose("USB port reset succeeded for bladeRF %s\n", info->serial);
        return 0;
    } else if (status == LIBUSB_ERROR_NO_DEVICE) {
        struct bladerf_devinfo new_info;

        /* The reset has caused the device to drop out and re-enumerate.
         *
         * We'll find it again via the info we gathered about it via its
         * serial number, which is now stored in the devinfo
         */
        log_verbose("Re-scan required after port reset for bladeRF %s\n",
                    info->serial);


        libusb_release_interface((*dev)->handle, 0);
        libusb_close((*dev)->handle);
        *dev = NULL;

        memcpy(&new_info, info, sizeof(new_info));
        new_info.usb_bus  = DEVINFO_BUS_ANY;
        new_info.usb_addr = DEVINFO_ADDR_ANY;

        status = find_and_open_device(context, &new_info, dev, info);

    } else {
        status = BLADERF_ERR_IO;
        log_verbose("Port reset failed for bladerf %s: %s\n",
                    info->serial, libusb_error_name(status));
    }

    return status;
}
#endif


static int lusb_open(void **driver,
                     struct bladerf_devinfo *info_in,
                     struct bladerf_devinfo *info_out)
{
    int status;
    struct bladerf_lusb *lusb = NULL;
    libusb_context *context;

    /* Initialize libusb for device tree walking */
    status = libusb_init(&context);
    if (status) {
        log_error("Could not initialize libusb: %s\n",
                  libusb_error_name(status));
        return error_conv(status);
    }

    /* We can only print this out when log output is enabled, or else we'll
     * get snagged by -Werror=unused-but-set-variable */
#   ifdef LOGGING_ENABLED
    {
        char buf[64];
        get_libusb_version(buf, sizeof(buf));
        log_verbose("Using libusb version: %s\n", buf);
    }
#   endif

    status = find_and_open_device(context, info_in, &lusb, info_out);
    if (status != 0) {
        libusb_exit(context);

        if (status == BLADERF_ERR_NODEV) {
            log_debug("No devices available on the libusb backend.\n");
        } else {
            log_debug("Failed to open bladeRF on libusb backend: %s\n",
                    bladerf_strerror(status));
        }
    } else {
        assert(lusb != NULL);

        /* Cosmin and Marian from Null Team (null.ro) and YateBTS(.com) found
         * that it is possible to recover from "Issue #95: Not enough bandwidth
         * for altsetting" by performing a USB port reset prior to actually
         * trying to use the device.
         */
#       if ENABLE_USB_DEV_RESET_ON_OPEN
        if (bladerf_usb_reset_device_on_open) {
            status = reset_and_reopen(context, &lusb, info_out);
        }
#       endif

        if (status == 0) {
            *driver = (void *) lusb;
        }
    }

    return status;
}

static int lusb_change_setting(void *driver, uint8_t setting)
{
    struct bladerf_lusb *lusb = (struct bladerf_lusb *) driver;

    int status = libusb_set_interface_alt_setting(lusb->handle, 0, setting);

    return error_conv(status);
}


static void lusb_close(void *driver)
{
    int status;
    struct bladerf_lusb *lusb = (struct bladerf_lusb *) driver;

    status = libusb_release_interface(lusb->handle, 0);
    if (status < 0) {
        log_error("Failed to release interface: %s\n",
                  libusb_error_name(status));
    }

    libusb_close(lusb->handle);
    libusb_exit(lusb->context);
    free(lusb);
}

static void lusb_close_bootloader(void *driver)
{
    int status;
    struct bladerf_lusb *lusb = (struct bladerf_lusb *) driver;

    if (lusb != NULL) {
        if (lusb->handle != NULL) {
            status = libusb_release_interface(lusb->handle, 0);
            if (status < 0) {
                log_debug("Failed to release interface: %s\n",
                           libusb_error_name(status));
            }

            libusb_close(lusb->handle);
        }

        if (lusb->context != NULL) {
            libusb_exit(lusb->context);
        }

        free(lusb);
    }
}

static inline bool bus_matches(uint8_t bus, struct libusb_device *d)
{
    return (bus == DEVINFO_BUS_ANY) ||
           (bus == libusb_get_bus_number(d));
}

static inline bool addr_matches(uint8_t addr, struct libusb_device *d)
{
    return (addr == DEVINFO_BUS_ANY) ||
           (addr == libusb_get_device_address(d));
}

static int lusb_open_bootloader(void **driver, uint8_t bus, uint8_t addr)
{
    int status;
    struct libusb_device **dev_list = NULL;
    ssize_t dev_list_size, i;
    struct bladerf_lusb *lusb;

    *driver = NULL;

    lusb = calloc(1, sizeof(lusb[0]));
    if (lusb == NULL) {
        return BLADERF_ERR_MEM;
    }

    status = libusb_init(&lusb->context);
    if (status != 0) {
        log_debug("Failed to initialize libusb context: %s\n",
                  libusb_error_name(status));
        goto error;
    }

    dev_list_size = libusb_get_device_list(lusb->context, &dev_list);
    if (dev_list_size < 0) {
        log_debug("Failed to get device list: %s\n", libusb_error_name(status));
        status = (int) dev_list_size;
        goto error;
    }

    for (i = 0; i < dev_list_size; i++) {
        if (device_is_fx3_bootloader(dev_list[i]) &&
            bus_matches(bus, dev_list[i]) &&
            addr_matches(addr, dev_list[i])) {


            status = libusb_open(dev_list[i], &lusb->handle);
            if (status != 0) {
                log_debug("Failed to open device: %s\n",
                          libusb_error_name(status));
                goto error;
            } else {
                status = libusb_claim_interface(lusb->handle, 0);
                if (status < 0) {
                    log_debug("Failed to claim interface: %s\n",
                              libusb_error_name(status));

                    goto error;
                } else {
                    log_verbose("Opened bootloader at %u:%u\n",
                                libusb_get_bus_number(dev_list[i]),
                                libusb_get_device_address(dev_list[i]));
                    *driver = lusb;
                }
                break;
            }
        }
    }

error:
    if (dev_list != NULL) {
        libusb_free_device_list(dev_list, 1);
    }

    if (status != 0) {
        status = error_conv(status);
        lusb_close_bootloader(lusb);
    } else if (*driver == NULL) {
        status = BLADERF_ERR_NODEV;
        lusb_close_bootloader(lusb);
    }

    return status;
}

static int lusb_get_speed(void *driver,
                          bladerf_dev_speed *device_speed)
{
    int speed;
    int status = 0;
    struct bladerf_lusb *lusb = (struct bladerf_lusb *) driver;

    speed = libusb_get_device_speed(lusb->dev);
    if (speed == LIBUSB_SPEED_SUPER) {
        *device_speed = BLADERF_DEVICE_SPEED_SUPER;
    } else if (speed == LIBUSB_SPEED_HIGH) {
        *device_speed = BLADERF_DEVICE_SPEED_HIGH;
    } else {
        *device_speed = BLADERF_DEVICE_SPEED_UNKNOWN;

        if (speed == LIBUSB_SPEED_FULL) {
            log_debug("Full speed connection is not suppored.\n");
            status = BLADERF_ERR_UNSUPPORTED;
        } else if (speed == LIBUSB_SPEED_LOW) {
            log_debug("Low speed connection is not supported.\n");
            status = BLADERF_ERR_UNSUPPORTED;
        } else {
            log_debug("Unknown/unexpected device speed (%d)\n", speed);
            status = BLADERF_ERR_UNEXPECTED;
        }
    }

    return status;
}

static inline uint8_t bm_request_type(usb_target target_type,
                                      usb_request req_type,
                                      usb_direction direction)
{
    uint8_t ret = 0;

    switch (target_type) {
        case USB_TARGET_DEVICE:
            ret |= LIBUSB_RECIPIENT_DEVICE;
            break;

        case USB_TARGET_INTERFACE:
            ret |= LIBUSB_RECIPIENT_INTERFACE;
            break;

        case USB_TARGET_ENDPOINT:
            ret |= LIBUSB_RECIPIENT_ENDPOINT;
            break;

        default:
            ret |= LIBUSB_RECIPIENT_OTHER;

    }

    switch (req_type) {
        case USB_REQUEST_STANDARD:
            ret |= LIBUSB_REQUEST_TYPE_STANDARD;
            break;

        case USB_REQUEST_CLASS:
            ret |= LIBUSB_REQUEST_TYPE_CLASS;
            break;

        case USB_REQUEST_VENDOR:
            ret |= LIBUSB_REQUEST_TYPE_VENDOR;
            break;
    }

    switch (direction) {
        case USB_DIR_HOST_TO_DEVICE:
            ret |= LIBUSB_ENDPOINT_OUT;
            break;

        case USB_DIR_DEVICE_TO_HOST:
            ret |= LIBUSB_ENDPOINT_IN;
            break;
    }

    return ret;
}

static int lusb_control_transfer(void *driver,
                                 usb_target target_type, usb_request req_type,
                                 usb_direction dir, uint8_t request,
                                 uint16_t wvalue, uint16_t windex,
                                 void *buffer, uint32_t buffer_len,
                                 uint32_t timeout_ms)
{

    int status;
    struct bladerf_lusb *lusb = (struct bladerf_lusb *) driver;
    const uint8_t bm_req_type = bm_request_type(target_type, req_type, dir);

    status = libusb_control_transfer(lusb->handle,
                                     bm_req_type, request,
                                     wvalue, windex,
                                     buffer, buffer_len,
                                     timeout_ms);

    if (status >= 0 && (uint32_t)status == buffer_len) {
        status = 0;
    } else {
        log_debug("%s failed: status = %d\n", __FUNCTION__, status);
    }

    return error_conv(status);
}

static int lusb_bulk_transfer(void *driver, uint8_t endpoint, void *buffer,
                              uint32_t buffer_len, uint32_t timeout_ms)
{
    int status;
    int n_transferred;
    struct bladerf_lusb *lusb = (struct bladerf_lusb *) driver;

    status = libusb_bulk_transfer(lusb->handle, endpoint, buffer, buffer_len,
                                  &n_transferred, timeout_ms);

    status = error_conv(status);
    if (status == 0 && ((uint32_t)n_transferred != buffer_len)) {
        log_debug("Short bulk transfer: requeted=%u, transferred=%u\n",
                  buffer_len, n_transferred);
        status = BLADERF_ERR_IO;
    }

    return status;
}

static int lusb_get_string_descriptor(void *driver, uint8_t index,
                                      void *buffer, uint32_t buffer_len)
{
    int status;
    struct bladerf_lusb *lusb = (struct bladerf_lusb *) driver;

    status = libusb_get_string_descriptor_ascii(lusb->handle, index,
                                                (unsigned char*)buffer,
                                                buffer_len);

    if (status > 0 && (uint32_t)status < buffer_len)  {
        status = 0;
    } else {
        status = BLADERF_ERR_UNEXPECTED;
    }

    return status;
}

/* At the risk of being a little inefficient, just keep attempting to cancel
 * everything. If a transfer's no longer active, we'll just get a NOT_FOUND
 * error -- no big deal.  Just accepting that alleviates the need to track
 * the status of each transfer...
 */
static inline void cancel_all_transfers(struct bladerf_stream *stream)
{
    size_t i;
    int status;
    struct lusb_stream_data *stream_data = stream->backend_data;

    for (i = 0; i < stream_data->num_transfers; i++) {
        if (stream_data->transfer_status[i] == TRANSFER_IN_FLIGHT) {

            status = libusb_cancel_transfer(stream_data->transfers[i]);
            if (status < 0 && status != LIBUSB_ERROR_NOT_FOUND) {
                log_error("Error canceling transfer (%d): %s\r\n",
                        status, libusb_error_name(status));
            } else {
                stream_data->transfer_status[i] = TRANSFER_CANCEL_PENDING;
            }
        }
    }
}

static inline size_t transfer_idx(struct lusb_stream_data *stream_data,
                                  struct libusb_transfer *transfer)
{
    size_t i;
    for (i = 0; i < stream_data->num_transfers; i++) {
        if (stream_data->transfers[i] == transfer) {
            return i;
        }
    }

    return UINT_MAX;
}

static int submit_transfer(struct bladerf_stream *stream, void *buffer);

static void LIBUSB_CALL lusb_stream_cb(struct libusb_transfer *transfer)
{
    struct bladerf_stream *stream = transfer->user_data;
    void *next_buffer = NULL;
    struct bladerf_metadata metadata;
    struct lusb_stream_data *stream_data = stream->backend_data;
    size_t transfer_i;

    /* Currently unused - zero out for out own debugging sanity... */
    memset(&metadata, 0, sizeof(metadata));

    MUTEX_LOCK(&stream->lock);

    transfer_i = transfer_idx(stream_data, transfer);
    assert(stream_data->transfer_status[transfer_i] == TRANSFER_IN_FLIGHT ||
           stream_data->transfer_status[transfer_i] == TRANSFER_CANCEL_PENDING);

    if (transfer_i >= stream_data->num_transfers) {
        log_error("Unable to find transfer");
        stream->state = STREAM_SHUTTING_DOWN;
    } else {
        stream_data->transfer_status[transfer_i] = TRANSFER_AVAIL;
        stream_data->num_avail++;
        pthread_cond_signal(&stream->can_submit_buffer);
    }

    /* Check to see if the transfer has been cancelled or errored */
    if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {

        /* Errored out for some reason .. */
        stream->state = STREAM_SHUTTING_DOWN;

        switch(transfer->status) {
            case LIBUSB_TRANSFER_CANCELLED:
                /* We expect this case when we begin tearing down the stream */
                break;

            case LIBUSB_TRANSFER_STALL:
                log_error("Hit stall for buffer %p\n", transfer->buffer);
                stream->error_code = BLADERF_ERR_IO;
                break;

            case LIBUSB_TRANSFER_ERROR:
                log_error("Got transfer error for buffer %p\n",
                          transfer->buffer);
                stream->error_code = BLADERF_ERR_IO;
                break;

            case LIBUSB_TRANSFER_OVERFLOW :
                log_error("Got transfer over for buffer %p, "
                            "transfer \"actual_length\" = %d\n",
                            transfer->buffer, transfer->actual_length);
                stream->error_code = BLADERF_ERR_IO;
                break;

            case LIBUSB_TRANSFER_TIMED_OUT:
                stream->error_code = BLADERF_ERR_TIMEOUT;
                break;

            case LIBUSB_TRANSFER_NO_DEVICE:
                stream->error_code = BLADERF_ERR_NODEV;
                break;

            default:
                log_error( "Unexpected transfer status: %d\n", transfer->status );
                break;
        }

    }

    if (stream->state == STREAM_RUNNING) {

        /* Sanity check for debugging purposes */
        if (transfer->length != transfer->actual_length) {
            log_warning( "Received short transfer\n" );
        }

       /* Call user callback requesting more data to transmit */
        next_buffer = stream->cb(
                        stream->dev,
                        stream,
                        &metadata,
                        transfer->buffer,
                        bytes_to_sc16q11(transfer->actual_length),
                        stream->user_data);

        if (next_buffer == BLADERF_STREAM_SHUTDOWN) {
            stream->state = STREAM_SHUTTING_DOWN;
        } else if (next_buffer != BLADERF_STREAM_NO_DATA) {
            int status = submit_transfer(stream, next_buffer);
            if (status != 0) {
                /* If this fails, we probably have a serious problem...so just
                 * shut it down. */
                stream->state = STREAM_SHUTTING_DOWN;
            }
        }
    }


    /* Check to see if all the transfers have been cancelled,
     * and if so, clean up the stream */
    if (stream->state == STREAM_SHUTTING_DOWN) {

        /* We know we're done when all of our transfers have returned to their
         * "available" states */
        if (stream_data->num_avail == stream_data->num_transfers) {
            stream->state = STREAM_DONE;
        } else {
            cancel_all_transfers(stream);
        }
    }

    MUTEX_UNLOCK(&stream->lock);
}

static inline struct libusb_transfer *
get_next_available_transfer(struct lusb_stream_data *stream_data)
{
    unsigned int n;
    size_t i = stream_data->i;

    for (n = 0; n < stream_data->num_transfers; n++) {
        if (stream_data->transfer_status[i] == TRANSFER_AVAIL) {

            if (stream_data->i != i &&
                stream_data->out_of_order_event == false) {

                log_warning("Transfer callback occurred out of order. "
                            "(Warning only this time.)\r\n");
                stream_data->out_of_order_event = true;
            }

            stream_data->i = i;
            return stream_data->transfers[i];
        }

        i = (i + 1) % stream_data->num_transfers;
    }

    return NULL;
}

/* Precondition: A transfer is available. */
static int submit_transfer(struct bladerf_stream *stream, void *buffer)
{
    int status;
    struct bladerf_lusb *lusb = lusb_backend(stream->dev);
    struct lusb_stream_data *stream_data = stream->backend_data;
    struct libusb_transfer *transfer;
    const size_t bytes_per_buffer = async_stream_buf_bytes(stream);
    size_t prev_idx;
    const unsigned char ep =
        stream->module == BLADERF_MODULE_TX ? SAMPLE_EP_OUT : SAMPLE_EP_IN;

    transfer = get_next_available_transfer(stream_data);
    assert(transfer != NULL);

    assert(bytes_per_buffer <= INT_MAX);
    libusb_fill_bulk_transfer(transfer,
                              lusb->handle,
                              ep,
                              buffer,
                              (int)bytes_per_buffer,
                              lusb_stream_cb,
                              stream,
                              stream->dev->transfer_timeout[stream->module]);

    prev_idx = stream_data->i;
    stream_data->transfer_status[stream_data->i] = TRANSFER_IN_FLIGHT;
    stream_data->i = (stream_data->i + 1) % stream_data->num_transfers;
    assert(stream_data->num_avail != 0);
    stream_data->num_avail--;

    /* FIXME We have an inherent issue here with lock ordering between
     *       stream->lock and libusb's underlying event lock, so we
     *       have to drop the stream->lock as a workaround.
     *
     *       This implies that a callback can potentially execute,
     *       including a callback for this transfer. Therefore, the transfer
     *       has been setup and its metadata logged.
     *
     *       Ultimately, we need to review our async scheme and associated
     *       lock schemes.
     */
    MUTEX_UNLOCK(&stream->lock);
    status = libusb_submit_transfer(transfer);
    MUTEX_LOCK(&stream->lock);

    if (status != 0) {
        log_error("Failed to submit transfer in %s: %s\n",
                  __FUNCTION__, libusb_error_name(status));

        /* We need to undo the metadata we updated prior to dropping
         * the lock and attempting to submit the transfer */
        assert(stream_data->transfer_status[prev_idx] == TRANSFER_IN_FLIGHT);
        stream_data->transfer_status[prev_idx] = TRANSFER_AVAIL;
        stream_data->num_avail++;
        if (stream_data->i == 0) {
            stream_data->i = stream_data->num_transfers - 1;
        } else {
            stream_data->i--;
        }
    }

    return error_conv(status);
}


static int lusb_init_stream(void *driver, struct bladerf_stream *stream,
                            size_t num_transfers)
{
    int status = 0;
    size_t i;
    struct lusb_stream_data *stream_data;

    /* Fill in backend stream information */
    stream_data = malloc(sizeof(struct lusb_stream_data));
    if (!stream_data) {
        return BLADERF_ERR_MEM;
    }

    /* Backend stream information */
    stream->backend_data = stream_data;
    stream_data->transfers = NULL;
    stream_data->transfer_status = NULL;
    stream_data->num_transfers = num_transfers;
    stream_data->num_avail = 0;
    stream_data->i = 0;
    stream_data->out_of_order_event = false;

    stream_data->transfers =
        malloc(num_transfers * sizeof(struct libusb_transfer *));

    if (stream_data->transfers == NULL) {
        log_error("Failed to allocate libusb tranfers\n");
        status = BLADERF_ERR_MEM;
        goto error;
    }

    stream_data->transfer_status =
        calloc(num_transfers, sizeof(transfer_status));

    if (stream_data->transfer_status == NULL) {
        log_error("Failed to allocated libusb transfer status array\n");
        status = BLADERF_ERR_MEM;
        goto error;
    }

    /* Create the libusb transfers */
    for (i = 0; i < stream_data->num_transfers; i++) {
        stream_data->transfers[i] = libusb_alloc_transfer(0);

        /* Upon error, start tearing down anything we've started allocating
         * and report that the stream is in a bad state */
        if (stream_data->transfers[i] == NULL) {

            /* Note: <= 0 not appropriate as we're dealing
             *       with an unsigned index */
            while (i > 0) {
                if (--i) {
                    libusb_free_transfer(stream_data->transfers[i]);
                    stream_data->transfers[i] = NULL;
                    stream_data->transfer_status[i] = TRANSFER_UNINITIALIZED;
                    stream_data->num_avail--;
                }
            }

            status = BLADERF_ERR_MEM;
            break;
        } else {
            stream_data->transfer_status[i] = TRANSFER_AVAIL;
            stream_data->num_avail++;
        }
    }

error:
    if (status != 0) {
        free(stream_data->transfer_status);
        free(stream_data->transfers);
        free(stream_data);
        stream->backend_data = NULL;
    }

    return status;
}

static int lusb_stream(void *driver, struct bladerf_stream *stream,
                       bladerf_module module)
{
    size_t i;
    int status = 0;
    void *buffer;
    struct bladerf_metadata metadata;
    struct bladerf *dev = stream->dev;
    struct bladerf_lusb *lusb = (struct bladerf_lusb *) driver;
    struct lusb_stream_data *stream_data = stream->backend_data;
    struct timeval tv = { 0, LIBUSB_HANDLE_EVENTS_TIMEOUT_NSEC };

    /* Currently unused, so zero it out for a sanity check when debugging */
    memset(&metadata, 0, sizeof(metadata));

    MUTEX_LOCK(&stream->lock);

    /* Set up initial set of buffers */
    for (i = 0; i < stream_data->num_transfers; i++) {
        if (module == BLADERF_MODULE_TX) {
            buffer = stream->cb(dev,
                                stream,
                                &metadata,
                                NULL,
                                stream->samples_per_buffer,
                                stream->user_data);

            if (buffer == BLADERF_STREAM_SHUTDOWN) {
                /* If we have transfers in flight and the user prematurely
                 * cancels the stream, we'll start shutting down */
                if (stream_data->num_avail != stream_data->num_transfers) {
                    stream->state = STREAM_SHUTTING_DOWN;
                } else {
                    /* No transfers have been shipped out yet so we can
                     * simply enter our "done" state */
                    stream->state = STREAM_DONE;
                }

                /* In either of the above we don't want to attempt to
                 * get any more buffers from the user */
                break;
            }
        } else {
            buffer = stream->buffers[i];
        }

        if (buffer != BLADERF_STREAM_NO_DATA) {
            status = submit_transfer(stream, buffer);

            /* If we failed to submit any transfers, cancel everything in
             * flight.  We'll leave the stream in the running state so we can
             * have libusb fire off callbacks with the cancelled status*/
            if (status < 0) {
                stream->error_code = status;
                cancel_all_transfers(stream);
            }
        }
    }
    MUTEX_UNLOCK(&stream->lock);

    /* This loop is required so libusb can do callbacks and whatnot */
    while (stream->state != STREAM_DONE) {
        status = libusb_handle_events_timeout(lusb->context, &tv);

        if (status < 0 && status != LIBUSB_ERROR_INTERRUPTED) {
            log_warning("unexpected value from events processing: "
                        "%d: %s\n", status, libusb_error_name(status));
            status = error_conv(status);
        }
    }

    return status;
}
/* The top-level code will have aquired the stream->lock for us */
int lusb_submit_stream_buffer(void *driver, struct bladerf_stream *stream,
                              void *buffer, unsigned int timeout_ms)
{
    int status = 0;
    struct lusb_stream_data *stream_data = stream->backend_data;
    struct timespec timeout_abs;

    if (buffer == BLADERF_STREAM_SHUTDOWN) {
        if (stream_data->num_avail == stream_data->num_transfers) {
            stream->state = STREAM_DONE;
        } else {
            stream->state = STREAM_SHUTTING_DOWN;
        }

        return 0;
    }

    if (timeout_ms != 0) {
        status = populate_abs_timeout(&timeout_abs, timeout_ms);
        if (status != 0) {
            return BLADERF_ERR_UNEXPECTED;
        }

        while (stream_data->num_avail == 0 && status == 0) {
            status = pthread_cond_timedwait(&stream->can_submit_buffer,
                    &stream->lock,
                    &timeout_abs);
        }
    } else {
        while (stream_data->num_avail == 0 && status == 0) {
            status = pthread_cond_wait(&stream->can_submit_buffer,
                                       &stream->lock);
        }
    }

    if (status == ETIMEDOUT) {
        log_debug("%s: Timed out waiting for a transfer to become availble.\n",
                  __FUNCTION__);
        return BLADERF_ERR_TIMEOUT;
    } else if (status != 0) {
        return BLADERF_ERR_UNEXPECTED;
    } else {
        return submit_transfer(stream, buffer);
    }
}

static int lusb_deinit_stream(void *driver, struct bladerf_stream *stream)
{
    size_t i;
    struct lusb_stream_data *stream_data = stream->backend_data;

    for (i = 0; i < stream_data->num_transfers; i++) {
        libusb_free_transfer(stream_data->transfers[i]);
        stream_data->transfers[i] = NULL;
        stream_data->transfer_status[i] = TRANSFER_UNINITIALIZED;
    }

    free(stream_data->transfers);
    free(stream_data->transfer_status);
    free(stream->backend_data);

    stream->backend_data = NULL;
    return 0;
}

static const struct usb_fns libusb_fns = {
    FIELD_INIT(.probe, lusb_probe),
    FIELD_INIT(.open, lusb_open),
    FIELD_INIT(.close, lusb_close),
    FIELD_INIT(.get_speed, lusb_get_speed),
    FIELD_INIT(.change_setting, lusb_change_setting),
    FIELD_INIT(.control_transfer, lusb_control_transfer),
    FIELD_INIT(.bulk_transfer, lusb_bulk_transfer),
    FIELD_INIT(.get_string_descriptor, lusb_get_string_descriptor),
    FIELD_INIT(.init_stream, lusb_init_stream),
    FIELD_INIT(.stream, lusb_stream),
    FIELD_INIT(.submit_stream_buffer, lusb_submit_stream_buffer),
    FIELD_INIT(.deinit_stream, lusb_deinit_stream),
    FIELD_INIT(.open_bootloader, lusb_open_bootloader),
    FIELD_INIT(.close_bootloader, lusb_close_bootloader),
};

const struct usb_driver usb_driver_libusb = {
    FIELD_INIT(.id, BLADERF_BACKEND_LIBUSB),
    FIELD_INIT(.fn, &libusb_fns)
};
