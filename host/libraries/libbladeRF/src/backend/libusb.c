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
#include <libusb.h>

#include "bladerf_priv.h"
#include "async.h"
#include "backend/libusb.h"
#include "conversions.h"
#include "minmax.h"
#include "log.h"
#include "flash.h"

#define CTRL_TIMEOUT 1000
#define BULK_TIMEOUT 1000

#define EP_DIR_IN   LIBUSB_ENDPOINT_IN
#define EP_DIR_OUT  LIBUSB_ENDPOINT_OUT

#define EP_IN(x) (LIBUSB_ENDPOINT_IN | x)
#define EP_OUT(x) (x)

struct bladerf_lusb {
    libusb_device           *dev;
    libusb_device_handle    *handle;
    libusb_context          *context;
};

typedef enum {
    CORR_INVALID,
    CORR_FPGA,
    CORR_LMS
} corr_type;

const struct bladerf_fn bladerf_lusb_fn;

typedef enum {
    TRANSFER_UNINITIALIZED = 0,
    TRANSFER_AVAIL,
    TRANSFER_IN_FLIGHT,
    TRANSFER_CANCEL_PENDING
} transfer_status;

struct lusb_stream_data {
    size_t num_transfers;                   /**< Total number of allocated transfers */
    size_t num_avail_transfers;
    size_t i;                               /**< Index to next transfer */
    struct libusb_transfer **transfers;
    transfer_status *transfer_status;        /**< Status of each transfer */
};

static int error_libusb2bladerf(int error)
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

/* Quick wrapper for vendor commands that get/send a 32-bit integer value */
static int vendor_command_int(struct bladerf *dev,
                          uint8_t cmd, uint8_t ep_dir, int32_t *val)
{
    int32_t buf;
    int status;
    struct bladerf_lusb *lusb = dev->backend;

    if( ep_dir == EP_DIR_IN ) {
        *val = 0;
    } else {
        buf = *val;
    }
    status = libusb_control_transfer(
                lusb->handle,
                LIBUSB_RECIPIENT_INTERFACE | LIBUSB_REQUEST_TYPE_VENDOR | ep_dir,
                cmd,
                0,
                0,
                (unsigned char *)&buf,
                sizeof(buf),
                CTRL_TIMEOUT
             );

    if (status < 0) {
        log_debug( "status < 0: %s\n", libusb_error_name(status) );
        status = error_libusb2bladerf(status);
    } else if (status != sizeof(buf)) {
        log_debug( "status != sizeof(buf): %s\n", libusb_error_name(status) );
        status = BLADERF_ERR_IO;
    } else {
        if( ep_dir == EP_DIR_IN ) {
            *val = LE32_TO_HOST(buf);
        }
        status = 0;
    }

    return status;
}

/* Quick wrapper for vendor commands that get/send a 32-bit integer value + wValue */
static int vendor_command_int_value(struct bladerf *dev,
                          uint8_t cmd, uint16_t wValue, int32_t *val)
{
    int32_t buf;
    int status;
    struct bladerf_lusb *lusb = dev->backend;

    status = libusb_control_transfer(
                lusb->handle,
                LIBUSB_RECIPIENT_INTERFACE | LIBUSB_REQUEST_TYPE_VENDOR | EP_DIR_IN,
                cmd,
                wValue,
                0,
                (unsigned char *)&buf,
                sizeof(buf),
                CTRL_TIMEOUT
             );

    if (status < 0) {
        log_debug( "status < 0: %s\n", libusb_error_name(status) );
        status = error_libusb2bladerf(status);
    } else if (status != sizeof(buf)) {
        log_debug( "status != sizeof(buf): %s\n", libusb_error_name(status) );
        status = BLADERF_ERR_IO;
    } else {
        *val = LE32_TO_HOST(buf);
        status = 0;
    }

    return status;
}

/* Quick wrapper for vendor commands that get/send a 32-bit integer value + wIndex */
static int vendor_command_int_index(struct bladerf *dev,
                          uint8_t cmd, uint16_t wIndex, int32_t *val)
{
    int32_t buf;
    int status;
    struct bladerf_lusb *lusb = dev->backend;

    status = libusb_control_transfer(
                lusb->handle,
                LIBUSB_RECIPIENT_INTERFACE | LIBUSB_REQUEST_TYPE_VENDOR | EP_DIR_IN,
                cmd,
                0,
                wIndex,
                (unsigned char *)&buf,
                sizeof(buf),
                CTRL_TIMEOUT
             );

    if (status < 0) {
        log_debug( "status < 0: %s\n", libusb_error_name(status) );
        status = error_libusb2bladerf(status);
    } else if (status != sizeof(buf)) {
        log_debug( "status != sizeof(buf): %s\n", libusb_error_name(status) );
        status = BLADERF_ERR_IO;
    } else {
        *val = LE32_TO_HOST(buf);
        status = 0;
    }

    return status;
}

static int begin_fpga_programming(struct bladerf *dev)
{
    int result;
    int status = vendor_command_int(dev, BLADE_USB_CMD_BEGIN_PROG, EP_DIR_IN, &result);

    if (status < 0) {
        return status;
    } else {
        return 0;
    }
}

static int end_fpga_programming(struct bladerf *dev)
{
    int result;
    int status = vendor_command_int(dev, BLADE_USB_CMD_QUERY_FPGA_STATUS, EP_DIR_IN, &result);
    if (status < 0) {
        log_debug("Received response of (%d): %s\n",
                   status, libusb_error_name(status));
        return status;
    } else {
        return 0;
    }
}

static int lusb_is_fpga_configured(struct bladerf *dev)
{
    int result;
    int status = vendor_command_int(dev, BLADE_USB_CMD_QUERY_FPGA_STATUS, EP_DIR_IN, &result);

    if (status < 0) {
        return status;
    } else {
        return result;
    }
}

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

static int change_setting(struct bladerf *dev, uint8_t setting)
{
    int status;

    struct bladerf_lusb *lusb = dev->backend ;
    if (dev->legacy  & LEGACY_ALT_SETTING) {
        log_verbose("Legacy change to interface %d\n", setting);

        /*
         * Workaround: We're not seeing a transfer here in Windows, possibly because
         *             something in/under libusb_set_interface_alt_setting() assumes
         *             the interface is already set to the current value and
         *             suppresses this attempt. Hence, we force this transfer.
         */
#if WIN32
        status = libusb_control_transfer(lusb->handle,  LIBUSB_RECIPIENT_DEVICE | LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_ENDPOINT_OUT, 11, 0, setting, 0, 0, 0);
        if (status > 0) {
            status = 0;
        }
#else
        status = libusb_set_interface_alt_setting(lusb->handle, setting, 0);
#endif
        return status;
    } else {
        log_verbose( "Change to alternate interface %d\n", setting);
        return libusb_set_interface_alt_setting(lusb->handle, 0, setting);
    }
}

/* After performing a flash operation, switch back to either RF_LINK or the
 * FPGA loader.
 */
static int restore_post_flash_setting(struct bladerf *dev)
{
    int fpga_loaded = lusb_is_fpga_configured(dev);
    int status;

    if (fpga_loaded < 0) {
        log_debug("Not restoring alt interface setting - failed to check FPGA state\n");
        status = fpga_loaded;
    } else if (fpga_loaded) {
        status = change_setting(dev, USB_IF_RF_LINK);
    } else {
        /* Make sure we are using the configuration interface */
        if (dev->legacy & LEGACY_CONFIG_IF) {
            status = change_setting(dev, USB_IF_LEGACY_CONFIG);
        } else {
            status = change_setting(dev, USB_IF_CONFIG);
        }
    }

    return status;
}

int lusb_enable_module(struct bladerf *dev, bladerf_module m, bool enable) {
    int status;
    int32_t fx3_ret = -1;
    int ret_status = 0;
    uint16_t val;

    val = enable ? 1 : 0 ;

    if (dev->legacy) {
        int32_t val32 = val;
        status = vendor_command_int(dev, (m == BLADERF_MODULE_RX) ? BLADE_USB_CMD_RF_RX : BLADE_USB_CMD_RF_TX, EP_DIR_OUT, &val32);
        fx3_ret = 0;
    } else {
        status = vendor_command_int_value(
                dev, (m == BLADERF_MODULE_RX) ? BLADE_USB_CMD_RF_RX : BLADE_USB_CMD_RF_TX,
                val, &fx3_ret);
    }
    if(status) {
        log_warning("Could not enable RF %s (%d): %s\n",
                (m == BLADERF_MODULE_RX) ? "RX" : "TX", status, libusb_error_name(status) );
        ret_status = BLADERF_ERR_UNEXPECTED;
    } else if(fx3_ret) {
        log_warning("Error enabling RF %s (%d)\n",
                (m == BLADERF_MODULE_RX) ? "RX" : "TX", fx3_ret );
        ret_status = BLADERF_ERR_UNEXPECTED;
    }

    return ret_status;
}



/* FW < 1.5.0  does not have version strings */
static int lusb_fw_populate_version_legacy(struct bladerf *dev)
{
    int status;

    struct bladerf_fx3_version fw_ver;
    struct bladerf_lusb *lusb = dev->backend;

    status = libusb_control_transfer(
                lusb->handle,
                LIBUSB_RECIPIENT_INTERFACE |
                    LIBUSB_REQUEST_TYPE_VENDOR |
                    EP_DIR_IN,
                BLADE_USB_CMD_QUERY_VERSION,
                0,
                0,
                (unsigned char *)&fw_ver,
                sizeof(fw_ver),
                CTRL_TIMEOUT
             );

    if (status < 0) {
        log_debug("Legacy firmare version fetch failed with %d\n", status);
        status = BLADERF_ERR_IO;
    } else {
        dev->fw_version.major = LE16_TO_HOST(fw_ver.major);
        dev->fw_version.minor = LE16_TO_HOST(fw_ver.minor);
        dev->fw_version.patch = 0;
        snprintf((char*)dev->fw_version.describe, BLADERF_VERSION_STR_MAX,
                 "%d.%d.%d", dev->fw_version.major, dev->fw_version.minor,
                 dev->fw_version.patch);

        status = 0;
    }

    return status;
}

static int lusb_populate_fw_version(struct bladerf *dev)
{
    int status;
    struct bladerf_lusb *lusb = dev->backend;

    status = libusb_get_string_descriptor_ascii(lusb->handle,
                                                BLADE_USB_STR_INDEX_FW_VER,
                                                (unsigned char *)dev->fw_version.describe,
                                                BLADERF_VERSION_STR_MAX);

    /* If we ran into an issue, we're likely dealing with an older firmware.
     * Fall back to the legacy version*/
    if (status < 0) {
        log_debug("Failed to get version string descriptor (%d). "
                  "Using legacy fallback.\n", status);

        status = lusb_fw_populate_version_legacy(dev);
    } else {
        status = str2version(dev->fw_version.describe, &dev->fw_version);
        if (status != 0) {
            status = BLADERF_ERR_UNEXPECTED;
        }
    }
    return status;
}

/* Returns BLADERF_ERR_* on failure */
static int access_peripheral_v(struct bladerf_lusb *lusb, int per, int dir,
                                struct uart_cmd *cmd, int len)
{
    uint8_t buf[16] = { 0 };    /* Zeroing out to avoid some valgrind noise
                                 * on the reserved items that aren't currently
                                 * used (i.e., bytes 4-15 */

    int status, libusb_status, transferred;
    int i;

    /* Populate the buffer for transfer */
    buf[0] = UART_PKT_MAGIC;
    buf[1] = dir | per | len;
    for (i = 0; i < len ; i++) {
        buf[2 + i * 2] = cmd[i].addr;
        buf[3 + i * 2] = cmd[i].data;
    }

    log_verbose("Peripheral access: [ 0x%02x, 0x%02x, 0x%02x, 0x%02x ]\n",
                buf[0], buf[1], buf[2], buf[3]);

    /* Write down the command */
    libusb_status = libusb_bulk_transfer(lusb->handle, 0x02, buf, 16,
                                           &transferred,
                                           CTRL_TIMEOUT);

    if (libusb_status < 0) {
        log_error("Failed to access peripheral: %s\n",
                  libusb_error_name(libusb_status));
        return BLADERF_ERR_IO;
    }

    /* If it's a read, we'll want to read back the result */
    transferred = 0;
    libusb_status = status =  0;
    while (libusb_status == 0 && transferred != 16) {
        libusb_status = libusb_bulk_transfer(lusb->handle, 0x82, buf, 16,
                                             &transferred,
                                             CTRL_TIMEOUT);
    }

    if (libusb_status < 0) {
        return BLADERF_ERR_IO;
    }

    /* Save off the result if it was a read */
    if (dir == UART_PKT_MODE_DIR_READ) {
        for (i = 0; i < len; i++) {
            cmd[i].data = buf[3 + i * 2];
        }
    }

    return status;
}

/* Returns BLADERF_ERR_* on failure */
static int access_peripheral(struct bladerf_lusb *lusb, int per, int dir,
                                struct uart_cmd *cmd)
{
    return access_peripheral_v(lusb, per, dir, cmd, 1);
}

static int _read_bytes(struct bladerf *dev, int addr, int len, uint32_t *val) {
    int i = 0;
    int status = 0;
    struct uart_cmd cmd;
    struct bladerf_lusb *lusb = dev->backend;
    *val = 0;

    for (i = 0; i < 4; i++){
        cmd.addr = addr + i;
        cmd.data = 0xff;

        status = access_peripheral(
                                    lusb,
                                    UART_PKT_DEV_GPIO,
                                    UART_PKT_MODE_DIR_READ,
                                    &cmd
                                    );

        if (status < 0) {
            break;
        }

        *val |= (cmd.data << (i * 8));
    }

    if (status < 0){
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF,status);
    }

    return status;
}

static int _write_bytes(struct bladerf *dev, int addr, int len, uint32_t val) {
    int status;
    struct uart_cmd cmd;
    struct bladerf_lusb *lusb = dev->backend;

    int i;
    for (i = 0; i < len; i++) {
        cmd.addr = addr + i;
        cmd.data = (val >> (i * 8)) & 0xff ;
        status = access_peripheral(lusb, UART_PKT_DEV_GPIO,
                UART_PKT_MODE_DIR_WRITE, &cmd);

        if (status < 0) {
            bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
            return status;
        }
    }

    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
    }

    return status;
}

static int lusb_fpga_version_read(struct bladerf *dev, uint32_t *version) {
    return _read_bytes(dev, UART_PKT_DEV_FGPA_VERSION_ID, 4, version);
}


static int lusb_populate_fpga_version(struct bladerf *dev)
{
    int status;
    uint32_t version;

    status = lusb_fpga_version_read(dev,&version);
    if (status < 0) {
        log_debug( "Could not retrieve FPGA version\n" ) ;
        dev->fpga_version.major = 0;
        dev->fpga_version.minor = 0;
        dev->fpga_version.patch = 0;
    }
    else {
        log_debug( "Raw FPGA Version: 0x%8.8x\n", version ) ;
        dev->fpga_version.major = (version >>  0) & 0xff;
        dev->fpga_version.minor = (version >>  8) & 0xff;
        dev->fpga_version.patch = (version >> 16) & 0xffff;
    }
    snprintf((char*)dev->fpga_version.describe, BLADERF_VERSION_STR_MAX,
                 "%d.%d.%d", dev->fpga_version.major, dev->fpga_version.minor,
                 dev->fpga_version.patch);

    return 0;
}

#ifdef HAVE_LIBUSB_GET_VERSION
static void get_libusb_version(char *buf, size_t buf_len)
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

static int enable_rf(struct bladerf *dev) {
    int status;
    /*int32_t fx3_ret = -1;*/
    int ret_status = 0;
    /*uint16_t val;*/

    status = change_setting(dev, USB_IF_RF_LINK);
    if(status < 0) {
        status = error_libusb2bladerf(status);
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
        log_debug( "alt_setting issue: %s\n", libusb_error_name(status) );
        return status;
    }
    log_verbose( "Changed into RF link mode: %s\n", libusb_error_name(status) ) ;

    /* We can only read the FPGA version once we've switched over to the
     * RF_LINK mode */
    ret_status = lusb_populate_fpga_version(dev);
    return ret_status;
}

static int lusb_open(struct bladerf **device, struct bladerf_devinfo *info)
{
    int status, i, n, inf;
    int fx3_status;
    ssize_t count;
    struct bladerf *dev = NULL;
    struct bladerf_lusb *lusb = NULL;
    libusb_device **list;
    struct bladerf_devinfo thisinfo;

    libusb_context *context;

    *device = NULL;

    /* Initialize libusb for device tree walking */
    status = libusb_init(&context);
    if( status ) {
        log_error( "Could not initialize libusb: %s\n", libusb_error_name(status) );
        status = error_libusb2bladerf(status);
        goto lusb_open_done;
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

    count = libusb_get_device_list( context, &list );
    /* Iterate through all the USB devices */
    for( i = 0, n = 0; i < count; i++ ) {
        if( lusb_device_is_bladerf(list[i]) ) {
            log_verbose("Found a bladeRF (based upon VID/PID)\n");

            /* Open the USB device and get some information */
            status = lusb_get_devinfo( list[i], &thisinfo );
            if(status < 0) {
                log_debug("Could not open bladeRF device: %s\n",
                          libusb_error_name(status) );

                status = error_libusb2bladerf(status);
                goto lusb_open__err_context;
            }
            thisinfo.instance = n++;

            /* Check to see if this matches the info struct */
            if( bladerf_devinfo_matches( &thisinfo, info ) ) {

                /* Allocate backend structure and populate*/
                dev = (struct bladerf *)calloc(1, sizeof(struct bladerf));
                if (!dev) {
                    log_debug("Skipping instance %d due to failed allocation: "
                              "device handle.\n", thisinfo.instance);
                    continue;
                }

                dev->fpga_version.describe =
                    calloc(1, BLADERF_VERSION_STR_MAX + 1);

                if (!dev->fpga_version.describe) {
                    log_debug("Skipping instance %d due to failed allocation: "
                              "FPGA version string.\n", thisinfo.instance);
                    free(dev);
                    dev = NULL;
                    continue;
                }

                dev->fw_version.describe =
                    calloc(1, BLADERF_VERSION_STR_MAX + 1);

                if (!dev->fw_version.describe) {
                    log_debug("Skipping instance %d due to failed allocation\n",
                              thisinfo.instance);
                    free((void *)dev->fpga_version.describe);
                    free(dev);
                    dev = NULL;
                    continue;
                }

                lusb = (struct bladerf_lusb *)malloc(sizeof(struct bladerf_lusb));
                if (!lusb) {
                    log_debug("Skipping instance %d due to failed allocation\n",
                              thisinfo.instance);
                    free((void *)dev->fw_version.describe);
                    free((void *)dev->fpga_version.describe);
                    free(dev);
                    lusb = NULL;
                    dev = NULL;
                    continue;
                }

                /* Assign libusb function table, backend type and backend */
                dev->fn = &bladerf_lusb_fn;
                dev->backend = (void *)lusb;
                dev->legacy = 0;

                dev->transfer_timeout[BLADERF_MODULE_TX] = BULK_TIMEOUT;
                dev->transfer_timeout[BLADERF_MODULE_RX] = BULK_TIMEOUT;

                memcpy(&dev->ident, &thisinfo, sizeof(struct bladerf_devinfo));

                /* Populate the backend information */
                lusb->context = context;
                lusb->dev = list[i];
                lusb->handle = NULL;

                status = libusb_open(list[i], &lusb->handle);
                if(status < 0) {
                    log_debug( "Could not open bladeRF device: %s\n", libusb_error_name(status) );
                    status = error_libusb2bladerf(status);
                    goto lusb_open__err_device_list;
                }

                /* The FPGA version is populated when rf link is established
                 * (zeroize until then) */
                dev->fpga_version.major = 0;
                dev->fpga_version.minor = 0;
                dev->fpga_version.patch = 0;

                status = lusb_populate_fw_version(dev);
                if (status < 0) {
                    log_debug("Failed to populate FW version (instance %d): %s\n",
                              thisinfo.instance, bladerf_strerror(status));
                    goto lusb_open__err_device_list;
                }

                if (dev->fw_version.major < FW_LEGACY_ALT_SETTING_MAJOR ||
                        (dev->fw_version.major == FW_LEGACY_ALT_SETTING_MAJOR &&
                         dev->fw_version.minor < FW_LEGACY_ALT_SETTING_MINOR)) {
                    dev->legacy = LEGACY_ALT_SETTING;
                    log_verbose("Legacy alt setting detected.\n");
                }

                if (dev->fw_version.major < FW_LEGACY_CONFIG_IF_MAJOR ||
                        (dev->fw_version.major == FW_LEGACY_CONFIG_IF_MAJOR && dev->fw_version.minor < FW_LEGACY_CONFIG_IF_MINOR)) {
                    dev->legacy |= LEGACY_CONFIG_IF;
                    log_verbose("Legacy config i/f detected.\n");
                }

                /* Claim interfaces */
                for( inf = 0; inf < ((dev->legacy & LEGACY_ALT_SETTING) ? 3 : 1) ; inf++ ) {
                    status = libusb_claim_interface(lusb->handle, inf);
                    if(status < 0) {
                        log_debug( "Could not claim interface %i - %s\n", inf, libusb_error_name(status) );
                        status = error_libusb2bladerf(status);
                        goto lusb_open__err_device_list;
                    }
                }
                log_verbose( "Claimed all inferfaces successfully\n" );

                /* Just out of paranoia, put the device into a known state */
                status = change_setting(dev, USB_IF_NULL);
                if (status < 0) {
                    log_debug("Failed to switch to USB_IF_NULL\n");
                    goto lusb_open__err_device_list;
                }

                break;
            } else {
                log_verbose("Devinfo doesn't match - skipping"
                            "(instance=%d, serial=%d, bus/addr=%d\n",
                            bladerf_instance_matches(&thisinfo, info),
                            bladerf_serial_matches(&thisinfo, info),
                            bladerf_bus_addr_matches(&thisinfo, info));
            }
        }

        if( lusb_device_is_fx3(list[i]) ) {
            fx3_status = lusb_get_devinfo( list[i], &thisinfo );
            if( fx3_status ) {
                log_debug( "Could not open FX3 bootloader device: %s\n", libusb_error_name(fx3_status) );
                continue;
            }

            log_info("Found FX3 bootloader device on bus=%d addr=%d. "
                     "This may be a bladeRF.\n",
                     thisinfo.usb_bus, thisinfo.usb_addr);

            log_info("Use bladeRF-cli command \"recover %d %d "
                     "<FX3 firmware>\" to boot the bladeRF firmware.\n",
                     thisinfo.usb_bus, thisinfo.usb_addr);
        }
    }


    if (dev) {
        if (lusb_is_fpga_configured(dev)) {
            enable_rf(dev);
        }
    }


/* XXX I'd prefer if we made a call here to lusb_close(), but that would result
 *     in an attempt to release interfaces we haven't claimed... thoughts? */
lusb_open__err_device_list:
    libusb_free_device_list( list, 1 );
    if (status != 0 && lusb != NULL) {
        if (lusb->handle) {
            libusb_close(lusb->handle);
        }

        free(lusb);
        free(dev);
        lusb = NULL;
        dev = NULL;
    }

lusb_open__err_context:
    if( dev == NULL ) {
        libusb_exit(context);
    }

lusb_open_done:
    if (!status) {

        if (dev) {
            *device = dev;
        } else {
            log_debug("No devices available on the libusb backend.\n");
            status = BLADERF_ERR_NODEV;
        }
    }

    return status;
}

static int lusb_close(struct bladerf *dev)
{
    int status;
    int ret;
    int inf = 0;
    struct bladerf_lusb *lusb = dev->backend;

    /* It seems we need to switch back to our NULL interface before closing,
     * or else our device doesn't close upon exit in OSC and then fails to
     * re-open cleanly */
    ret = change_setting(dev, USB_IF_NULL);

    for( inf = 0; inf < ((dev->legacy & LEGACY_ALT_SETTING) ? 3 : 1) ; inf++ ) {
        status = libusb_release_interface(lusb->handle, inf);
        if (status < 0) {
            log_debug("error releasing interface %i\n", inf);

            if (ret == 0) {
                ret = error_libusb2bladerf(status);
            }
        }
    }

    libusb_close(lusb->handle);
    libusb_exit(lusb->context);
    free(dev->backend);
    free((void *)dev->fpga_version.describe);
    free((void *)dev->fw_version.describe);
    free(dev);

    return ret;
}

static int lusb_set_firmware_loopback(struct bladerf *dev, bool enable) {
    int result;
    int status = vendor_command_int_value(dev, BLADE_USB_CMD_SET_LOOPBACK, enable, &result);
    status |= change_setting(dev, USB_IF_NULL);
    status |= change_setting(dev, USB_IF_RF_LINK);

    if (status) {
        return status;
    }
    return 0;
}

static int lusb_load_fpga(struct bladerf *dev, uint8_t *image, size_t image_size)
{
    unsigned int wait_count;
    int status = 0;
    int transferred = 0;
    struct bladerf_lusb *lusb = dev->backend;

    /* Make sure we are using the configuration interface */
    if (dev->legacy & LEGACY_CONFIG_IF) {
        status = change_setting(dev, USB_IF_LEGACY_CONFIG);
    } else {
        status = change_setting(dev, USB_IF_CONFIG);
    }

    if(status < 0) {
        status = error_libusb2bladerf(status);
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
        log_debug( "alt_setting issue: %s\n", libusb_error_name(status) );
        return status;;
    }

    /* Begin programming */
    status = begin_fpga_programming(dev);
    if (status < 0) {
        status = error_libusb2bladerf(status);
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
        return status;
    }

    /* Send the file down */
    assert(image_size <= INT_MAX);
    status = libusb_bulk_transfer(lusb->handle, 0x2, image, (int)image_size,
                                  &transferred, 5 * CTRL_TIMEOUT);
    if (status < 0) {
        status = error_libusb2bladerf(status);
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
        return status;
    }

    /* End programming */
    status = end_fpga_programming(dev);
    if (status) {
        status = error_libusb2bladerf(status);
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
        return status;
    }

    /* Poll FPGA status to determine if programming was a success */
    wait_count = 10;
    status = 0;

    while (wait_count > 0 && status == 0) {
        status = lusb_is_fpga_configured(dev);
        if (status == 1) {
            break;
        }

        usleep(200000);
        wait_count--;
    }

    /* Failed to determine if FPGA is loaded */
    if (status < 0) {
        status = error_libusb2bladerf(status);
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
        return status;
    } else if (wait_count == 0 && !status) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, BLADERF_ERR_TIMEOUT);
        return BLADERF_ERR_TIMEOUT;
    }

    status = enable_rf(dev);

    return status;
}

static int erase_sector(struct bladerf *dev, uint16_t sector)
{
    struct bladerf_lusb *lusb = dev->backend;
    int status, erase_ret;

    status = libusb_control_transfer(
        lusb->handle,
        LIBUSB_RECIPIENT_INTERFACE | LIBUSB_REQUEST_TYPE_VENDOR
            | EP_DIR_IN,
        BLADE_USB_CMD_FLASH_ERASE,
        0,
        sector,
        (unsigned char *)&erase_ret,
        sizeof(erase_ret),
        CTRL_TIMEOUT
    );

    if (status != sizeof(erase_ret)
        || ((!(dev->legacy & LEGACY_CONFIG_IF)) &&  erase_ret != 0))
    {
        log_error("Failed to erase sector at 0x%02x\n",
                flash_from_sectors(sector));

        if (status < 0)
            log_debug("libusb status: %s\n", libusb_error_name(status));
        else if (erase_ret != 0)
            log_debug("Received erase failure status from FX3. error = %d\n",
                      erase_ret);
        else
            log_debug("Unexpected read size: %d\n", status);

        return BLADERF_ERR_IO;
    }

    log_debug("Erased sector 0x%04x.\n", flash_from_sectors(sector));
    return 0;
}

static int lusb_erase_flash(struct bladerf *dev, uint32_t addr, uint32_t len)
{
    int sector_addr, sector_len;
    int i;
    int status = 0;

    if(!flash_bounds_aligned(BLADERF_FLASH_ALIGNMENT_SECTOR, addr, len))
        return BLADERF_ERR_MISALIGNED;

    sector_addr = flash_to_sectors(addr);
    sector_len  = flash_to_sectors(len);

    status = change_setting(dev, USB_IF_SPI_FLASH);
    if (status) {
        log_debug("Failed to set interface: %s\n", libusb_error_name(status));
        return BLADERF_ERR_IO;
    }

    log_info("Erasing 0x%08x bytes starting at address 0x%08x.\n", len, addr);

    for (i=0; i < sector_len; i++) {
        status = erase_sector(dev, sector_addr + i);
        if(status)
            return status;
    }

    status = restore_post_flash_setting(dev);
    if (status != 0) {
        return status;
    } else {
        return len;
    }
}

static int read_buffer(struct bladerf *dev, uint8_t request,
                       uint8_t *buf, uint16_t len)
{
    struct bladerf_lusb *lusb = (struct bladerf_lusb *)dev->backend;
    int status, read_size;
    int buf_off;

    if (dev->usb_speed == BLADERF_DEVICE_SPEED_SUPER) {
        read_size = BLADERF_FLASH_PAGE_SIZE;
    } else if (dev->usb_speed == BLADERF_DEVICE_SPEED_HIGH) {
        read_size = 64;
    } else {
        log_debug("Encountered unknown USB speed in %s\n", __FUNCTION__);
        return BLADERF_ERR_UNEXPECTED;
    }

    /* only these two requests seem to use bytes in the control transfer
     * parameters instead of pages/sectors so watch out! */
    assert(request == BLADE_USB_CMD_READ_PAGE_BUFFER
           || request == BLADE_USB_CMD_READ_CAL_CACHE);

    assert(len % read_size == 0);

    for(buf_off = 0; buf_off < len; buf_off += read_size)
    {
        status = libusb_control_transfer(
                    lusb->handle,
                    LIBUSB_RECIPIENT_INTERFACE | LIBUSB_REQUEST_TYPE_VENDOR
                        | EP_DIR_IN,
                    request,
                    0,
                    buf_off, /* bytes */
                    &buf[buf_off],
                    read_size,
                    CTRL_TIMEOUT
        );

        if(status < 0) {
            log_debug("Failed to read page buffer at offset 0x%02x: %s\n",
                      buf_off, libusb_error_name(status));

            return error_libusb2bladerf(status);
        } else if(status != read_size) {
            log_debug("Got unexpected read size when writing page buffer at"
                      " offset 0x%02x: expected %d, got %d\n",
                      buf_off, read_size, status);

            return BLADERF_ERR_IO;
        }
    }

    return 0;
}

static int read_page_buffer(struct bladerf *dev, uint8_t *buf)
{
    return read_buffer(
        dev,
        BLADE_USB_CMD_READ_PAGE_BUFFER,
        buf,
        BLADERF_FLASH_PAGE_SIZE
    );
}

static int lusb_get_cal(struct bladerf *dev, char *cal) {
    return read_buffer(
        dev,
        BLADE_USB_CMD_READ_CAL_CACHE,
        (uint8_t*)cal,
        CAL_BUFFER_SIZE
    );
}

static int legacy_read_one_page(struct bladerf *dev,
                                uint16_t page,
                                uint8_t *buf)
{
    int status = 0;
    int read_size;
    struct bladerf_lusb *lusb = dev->backend;
    uint16_t buf_off;

    if (dev->usb_speed == BLADERF_DEVICE_SPEED_HIGH) {
        read_size = 64;
    } else if (dev->usb_speed == BLADERF_DEVICE_SPEED_SUPER) {
        read_size = BLADERF_FLASH_PAGE_SIZE;
    } else {
        log_debug("Encountered unknown USB speed in %s\n", __FUNCTION__);
        return BLADERF_ERR_UNEXPECTED;
    }

    for(buf_off=0; buf_off < BLADERF_FLASH_PAGE_SIZE; buf_off += read_size) {
        status = libusb_control_transfer(
            lusb->handle,
            LIBUSB_RECIPIENT_INTERFACE | LIBUSB_REQUEST_TYPE_VENDOR
                | EP_DIR_IN,
            BLADE_USB_CMD_FLASH_READ,
            0,
            page,
            &buf[buf_off],
            read_size,
            CTRL_TIMEOUT
        );

        if (status != read_size) {
            if (status < 0) {
                log_error("Failed to read back page at 0x%02x: %s\n",
                          flash_from_pages(page), libusb_error_name(status));
            } else {
                log_error("Unexpected read size: %d\n", status);
            }
            return BLADERF_ERR_IO;
        }
    }

    return 0;
}

/* Assumes the device is already configured for USB_IF_SPI_FLASH */
static int read_one_page(struct bladerf *dev, uint16_t page, uint8_t *buf)
{
    int32_t read_status = -1;
    int status;

    if (dev->legacy & LEGACY_CONFIG_IF)
        return legacy_read_one_page(dev, page, buf);

    status = vendor_command_int_index(
        dev,
        BLADE_USB_CMD_FLASH_READ,
        page,
        &read_status
    );

    if(status != LIBUSB_SUCCESS) {
        return status;
    }

    if(read_status != 0) {
        log_error("Failed to read page %d: %d\n", page, read_status);
        status = BLADERF_ERR_UNEXPECTED;
    }

    return read_page_buffer(dev, (uint8_t*)buf);
}

static int lusb_read_flash(struct bladerf *dev, uint32_t addr,
                           uint8_t *buf, uint32_t len)
{
    int status;
    int page_addr, page_len, i;
    unsigned int read;

    if(!flash_bounds_aligned(BLADERF_FLASH_ALIGNMENT_PAGE, addr, len))
        return BLADERF_ERR_MISALIGNED;

    page_addr = flash_to_pages(addr);
    page_len  = flash_to_pages(len);

    status = change_setting(dev, USB_IF_SPI_FLASH);
    if (status) {
        log_error("Failed to set interface: %s\n", libusb_error_name(status));
        return BLADERF_ERR_IO;
    }

    log_info("Reading 0x%08x bytes from address 0x%08x.\n", len, addr);

    read = 0;
    for (i=0; i < page_len; i++) {
        log_debug("Reading page 0x%04x.\n", flash_from_pages(i));
        status = read_one_page(dev, page_addr + i, buf + read);
        if(status)
            return status;

        read += BLADERF_FLASH_PAGE_SIZE;
    }

    status = restore_post_flash_setting(dev);
    if (status != 0) {
        return status;
    } else {
        return read;
    }
}

static int compare_page_buffers(uint8_t *page_buf, uint8_t *image_page)
{
    int i;
    for (i = 0; i < BLADERF_FLASH_PAGE_SIZE; i++) {
        if (page_buf[i] != image_page[i]) {
            return -i;
        }
    }

    return 0;
}

static int verify_one_page(struct bladerf *dev,
                           uint16_t page, uint8_t *image_buf)
{
    int status = 0;
    uint8_t page_buf[BLADERF_FLASH_PAGE_SIZE];
    unsigned int i;

    status = change_setting(dev, USB_IF_SPI_FLASH);
    if (status) {
        log_error("Failed to set interface: %s\n", libusb_error_name(status));
        return BLADERF_ERR_IO;
    }

    log_debug("Verifying page 0x%04x.\n", flash_from_pages(page));
    status = read_one_page(dev, page, page_buf);
    if(status)
        return status;

    status = compare_page_buffers(page_buf, image_buf);
    if(status < 0) {
        i = abs(status);
        log_error("bladeRF firmware verification failed at flash "
                  " address 0x%02x. Read 0x%02X, expected 0x%02X\n",
                  flash_from_pages(page) + i,
                  page_buf[i],
                  image_buf[i]
            );

        return BLADERF_ERR_IO;
    }

    return restore_post_flash_setting(dev);
}

static int verify_flash(struct bladerf *dev, uint32_t addr,
                        uint8_t *image, uint32_t len)
{
    int status = 0;
    int page_addr, page_len, i;
    uint8_t *image_buf;

    if(!flash_bounds_aligned(BLADERF_FLASH_ALIGNMENT_PAGE, addr, len))
        return BLADERF_ERR_MISALIGNED;

    page_addr = flash_to_pages(addr);
    page_len  = flash_to_pages(len);

    log_info("Verifying 0x%08x bytes at address 0x%08x\n", len, addr);

    for(i=0; i < page_len; i++) {
        image_buf = &image[flash_from_pages(i)];
        status = verify_one_page(dev, page_addr + i, image_buf);
        if(status < 0)
            break;
    }

    return status;
}

static int write_buffer(struct bladerf *dev,
                        uint8_t request,
                        uint16_t page,
                        uint8_t *buf)
{
    struct bladerf_lusb *lusb = dev->backend;
    int status;

    uint32_t buf_off;
    uint32_t write_size;

    if (dev->usb_speed == BLADERF_DEVICE_SPEED_SUPER) {
        write_size = BLADERF_FLASH_PAGE_SIZE;
    } else if (dev->usb_speed == BLADERF_DEVICE_SPEED_HIGH) {
        write_size = 64;
    } else {
        log_error("Encountered unknown USB speed in %s\n", __FUNCTION__);
        return BLADERF_ERR_UNEXPECTED;
    }

    for(buf_off = 0; buf_off < BLADERF_FLASH_PAGE_SIZE; buf_off += write_size)
    {
        status = libusb_control_transfer(
            lusb->handle,
            LIBUSB_RECIPIENT_INTERFACE | LIBUSB_REQUEST_TYPE_VENDOR
                | EP_DIR_OUT,
            request,
            0,
            dev->legacy & LEGACY_CONFIG_IF ? page : buf_off,
            &buf[buf_off],
            write_size,
            CTRL_TIMEOUT
        );

        if(status < 0) {
            log_error("Failed to write page buffer at offset 0x%02x"
                      " for page at 0x%02x: %s\n",
                      buf_off, flash_from_pages(page),
                      libusb_error_name(status));

            return error_libusb2bladerf(status);
        } else if((unsigned int)status != write_size) {
            log_error("Got unexpected write size when writing page buffer"
                      " at 0x%02x: expected %d, got %d\n",
                      flash_from_pages(page), write_size, status);

            return BLADERF_ERR_IO;
        }
    }

    return 0;
}

static int write_one_page(struct bladerf *dev, uint16_t page, uint8_t *buf)
{
    int status;
    int32_t write_status = -1;

    status = write_buffer(
        dev,
        dev->legacy & LEGACY_CONFIG_IF
            ? BLADE_USB_CMD_FLASH_WRITE : BLADE_USB_CMD_WRITE_PAGE_BUFFER,
        page,
        buf
    );
    if(status)
        return status;

    if(dev->legacy & LEGACY_CONFIG_IF)
        return 0;

    status = vendor_command_int_index(
        dev,
        BLADE_USB_CMD_FLASH_WRITE,
        page,
        &write_status
    );

    if(status != LIBUSB_SUCCESS) {
        return status;
    }

    if(write_status != 0) {
        log_error("Failed to write page at 0x%02x: %d\n",
                  flash_from_pages(page), write_status);

         return BLADERF_ERR_UNEXPECTED;
    }

#ifdef FLASH_VERIFY_PAGE_WRITES
    status = verify_one_page(dev, page, buf);
    if(status < 0)
        return status;
#endif

    return 0;
}

static int lusb_write_flash(struct bladerf *dev, uint32_t addr,
                        uint8_t *buf, uint32_t len)
{
    int status;
    int page_addr, page_len, written, i;

    if(!flash_bounds_aligned(BLADERF_FLASH_ALIGNMENT_PAGE, addr, len))
        return BLADERF_ERR_MISALIGNED;

    page_addr = flash_to_pages(addr);
    page_len  = flash_to_pages(len);

    status = change_setting(dev, USB_IF_SPI_FLASH);
    if (status) {
        log_error("Failed to set interface: %s\n", libusb_error_name(status));
        return BLADERF_ERR_IO;
    }

    log_info("Writing 0x%08x bytes to address 0x%08x.\n", len, addr);

    written = 0;
    for(i=0; i < page_len; i++) {
        log_debug("Writing page at 0x%04x.\n", flash_from_pages(i));
        status = write_one_page(dev, page_addr + i, buf + written);
        if(status)
            return status;

        written += BLADERF_FLASH_PAGE_SIZE;
    }

    status = restore_post_flash_setting(dev);
    if (status != 0) {
        return status;
    } else {
        return written;
    }
}

static int lusb_flash_firmware(struct bladerf *dev,
                               uint8_t *image, size_t image_size)
{
    int status;

    assert(image_size <= UINT32_MAX);

    status = lusb_erase_flash(dev, 0, (uint32_t)image_size);

    if (status >= 0) {
        status = lusb_write_flash(dev, 0, image, (uint32_t)image_size);
    }

    if (status >= 0) {
        status = verify_flash(dev, 0, image, (uint32_t)image_size);
    }

    return status;
}

static int lusb_device_reset(struct bladerf *dev)
{
    struct bladerf_lusb *lusb = dev->backend;
    int status;
    status = libusb_control_transfer(
                lusb->handle,
                LIBUSB_RECIPIENT_INTERFACE |
                    LIBUSB_REQUEST_TYPE_VENDOR |
                    EP_DIR_OUT,
                BLADE_USB_CMD_RESET,
                0,
                0,
                0,
                0,
                CTRL_TIMEOUT
             );

    if(status != LIBUSB_SUCCESS) {
        log_error("Error issuing reset: %s\n", libusb_error_name(status));
        return error_libusb2bladerf(status);
    } else {
        return status;
    }
}

static int lusb_jump_to_bootloader(struct bladerf *dev)
{
    struct bladerf_lusb *lusb = dev->backend;
    int status;
    status = libusb_control_transfer(
                lusb->handle,
                LIBUSB_RECIPIENT_INTERFACE |
                    LIBUSB_REQUEST_TYPE_VENDOR |
                    EP_DIR_OUT,
                BLADE_USB_CMD_JUMP_TO_BOOTLOADER,
                0,
                0,
                0,
                0,
                CTRL_TIMEOUT
             );

    if (status < 0) {
        log_error( "Error jumping to bootloader: %s\n", libusb_error_name(status) );
        status = error_libusb2bladerf(status);
    }

    return status;
}

static int lusb_get_otp(struct bladerf *dev, char *otp)
{
    int status;
    int otp_page = 0;
    int32_t read_status = -1;

    status = change_setting(dev, USB_IF_SPI_FLASH);
    if (status) {
        log_error("Failed to set interface: %s\n", libusb_error_name(status));
        return BLADERF_ERR_IO;
    }

    status = vendor_command_int_index(
            dev, BLADE_USB_CMD_READ_OTP,
            otp_page, &read_status);
    if(status != LIBUSB_SUCCESS) {
        return status;
    }

    if(read_status != 0) {
        log_error("Failed to read OTP page %d: %d\n",
                otp_page, read_status);
        return BLADERF_ERR_UNEXPECTED;
    }

    return read_page_buffer(dev, (uint8_t*)otp);
}

static int lusb_get_device_speed(struct bladerf *dev,
                                 bladerf_dev_speed *device_speed)
{
    int speed;
    int status = 0;
    struct bladerf_lusb *lusb = dev->backend;

    speed = libusb_get_device_speed(lusb->dev);
    if (speed == LIBUSB_SPEED_SUPER) {
        *device_speed = BLADERF_DEVICE_SPEED_SUPER;
    } else if (speed == LIBUSB_SPEED_HIGH) {
        *device_speed = BLADERF_DEVICE_SPEED_HIGH;
    } else {
        *device_speed = BLADERF_DEVICE_SPEED_UNKNOWN;

        if (speed == LIBUSB_SPEED_FULL) {
            log_error("Error: Full speed connection is not suppored.\n");
            status = BLADERF_ERR_UNSUPPORTED;
        } else if (speed == LIBUSB_SPEED_LOW) {
            log_error("Error: Low speed connection is not supported.\n");
            status = BLADERF_ERR_UNSUPPORTED;
        } else {
            log_error("Error: Unknown/unexpected device speed (%d)\n", speed);
            status = BLADERF_ERR_UNEXPECTED;
        }

    }

    return status;
}

static int lusb_config_gpio_write(struct bladerf *dev, uint32_t val)
{
    return _write_bytes(dev, 0, 4, val);
}

static int lusb_config_gpio_read(struct bladerf *dev, uint32_t *val)
{
    return _read_bytes(dev, 0, 4, val);
}

static int lusb_expansion_gpio_write(struct bladerf *dev, uint32_t val)
{
    return _write_bytes(dev, 40, 4, val);
}

static int lusb_expansion_gpio_read(struct bladerf *dev, uint32_t *val)
{
    return _read_bytes(dev, 40, 4, val);
}

static int lusb_expansion_gpio_dir_write(struct bladerf *dev, uint32_t val)
{
    return _write_bytes(dev, 44, 4, val);
}

static int lusb_expansion_gpio_dir_read(struct bladerf *dev, uint32_t *val)
{
    return _read_bytes(dev, 44, 4, val);
}

static int lusb_si5338_write(struct bladerf *dev, uint8_t addr, uint8_t data)
{
    int status;
    struct uart_cmd cmd;
    struct bladerf_lusb *lusb = dev->backend;

    cmd.addr = addr;
    cmd.data = data;

    log_verbose( "%s: 0x%2.2x 0x%2.2x\n", __FUNCTION__, addr, data );

    status = access_peripheral(lusb, UART_PKT_DEV_SI5338,
                               UART_PKT_MODE_DIR_WRITE, &cmd);

    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
    }

    return status;
}

static int lusb_si5338_read(struct bladerf *dev, uint8_t addr, uint8_t *data)
{
    int status = 0;
    struct uart_cmd cmd;
    struct bladerf_lusb *lusb = dev->backend;

    cmd.addr = addr;
    cmd.data = 0xff;

    status = access_peripheral(lusb, UART_PKT_DEV_SI5338,
                               UART_PKT_MODE_DIR_READ, &cmd);

    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
    } else {
        *data = cmd.data;
        log_verbose("%s: 0x%2.2x 0x%2.2x\n", __FUNCTION__, addr, *data);
    }


    return status;
}

static int lusb_lms_write(struct bladerf *dev, uint8_t addr, uint8_t data)
{
    int status;
    struct uart_cmd cmd;
    struct bladerf_lusb *lusb = dev->backend;

    cmd.addr = addr;
    cmd.data = data;

    log_verbose( "%s: 0x%2.2x 0x%2.2x\n", __FUNCTION__, addr, data );

    status = access_peripheral(lusb, UART_PKT_DEV_LMS,
                                UART_PKT_MODE_DIR_WRITE, &cmd);

    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
    }

    return status;
}

static int lusb_lms_read(struct bladerf *dev, uint8_t addr, uint8_t *data)
{
    int status;
    struct uart_cmd cmd;
    struct bladerf_lusb *lusb = dev->backend;

    cmd.addr = addr;
    cmd.data = 0xff;

    status = access_peripheral(lusb, UART_PKT_DEV_LMS,
                               UART_PKT_MODE_DIR_READ, &cmd);

    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
    } else {
        *data = cmd.data;
        log_verbose( "%s: 0x%2.2x 0x%2.2x\n", __FUNCTION__, addr, *data );
    }

    return status;
        }

static int lusb_dac_write(struct bladerf *dev, uint16_t value)
{
    int status;
    struct uart_cmd cmd;
    struct bladerf_lusb *lusb = dev->backend;

    cmd.addr = 34 ;
    cmd.data = value & 0xff ;
    status = access_peripheral(lusb, UART_PKT_DEV_GPIO,
                               UART_PKT_MODE_DIR_WRITE, &cmd);

    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
        return status;
    }

    cmd.addr = 35 ;
    cmd.data = (value>>8)&0xff ;
    status = access_peripheral(lusb, UART_PKT_DEV_GPIO,
                               UART_PKT_MODE_DIR_WRITE, &cmd);

    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
    }

    return status;
}

static int lusb_xb_spi(struct bladerf *dev, uint32_t value)
{
    int status;
    struct uart_cmd cmd;
    struct bladerf_lusb *lusb = dev->backend;

    int i;
    for (i = 0; i < 4; i++) {
        cmd.addr = 36 + i;
        cmd.data = (value >> (i * 8)) & 0xff ;
        status = access_peripheral(lusb, UART_PKT_DEV_GPIO,
                UART_PKT_MODE_DIR_WRITE, &cmd);

        if (status < 0) {
            bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
            return status;
        }
    }

    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
    }

    return status;
}


static const char * corr2str(bladerf_correction c)
{
    switch (c) {
        case BLADERF_CORR_LMS_DCOFF_I:
            return "BLADERF_CORR_LMS_DCOFF_I";
        case BLADERF_CORR_LMS_DCOFF_Q:
            return "BLADERF_CORR_LMS_DCOFF_Q";
        case BLADERF_CORR_FPGA_PHASE:
            return "BLADERF_CORR_FPGA_PHASE";
        case BLADERF_CORR_FPGA_GAIN:
            return "BLADERF_FPGA_GAIN";
        default:
            return "UNKNOWN";
    }
}

static int set_fpga_correction(struct bladerf *dev,
                               bladerf_correction corr,
                               uint8_t addr, int16_t value)
{
    int i = 0;
    int status = 0;
    struct uart_cmd cmd;
    struct bladerf_lusb *lusb = dev->backend;

    /* If this ia a gain correction add in the 1.0 value so 0 correction yields
     * an unscaled gain */
    if (corr == BLADERF_CORR_FPGA_GAIN) {
        value += (int16_t)4096;
    }

    for (i = 0; status == 0 && i < 2; i++) {
        cmd.addr = i + addr;

        cmd.data = (value>>(8*i))&0xff;
        status = access_peripheral(
                                    lusb,
                                    UART_PKT_DEV_GPIO,
                                    UART_PKT_MODE_DIR_WRITE,
                                    &cmd
                                  );

        if (status < 0) {
            break;
        }
    }

    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
    }
    return status;
}

static int set_lms_correction(struct bladerf *dev, bladerf_module module,
                              uint8_t addr, int16_t value)
{
    int status;
    uint8_t tmp;

    status = lusb_lms_read(dev, addr, &tmp);
    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
        return status;
    }

    /* Mask out any control bits in the RX DC correction area */
    if (module == BLADERF_MODULE_RX) {

        /* Bit 7 is unrelated to lms dc correction, save its state */
        tmp = tmp & (1 << 7);

        /* RX only has 6 bits of scale to work with, remove normalization */
        value >>= 5;

        if (value < 0) {
            value = (value <= -64) ? 0x3f :  (abs(value) & 0x3f);
            /*This register uses bit 6 to denote a negative gain */
            value |= (1 << 6);
        } else {
            value = (value >= 64) ? 0x3f : (value & 0x3f);
        }

        value |= tmp;
    } else {

        /* TX only has 7 bits of scale to work with, remove normalization */
        value >>= 4;

        /* LMS6002D 0x00 = -16, 0x80 = 0, 0xff = 15.9375 */
        if (value >= 0) {
            tmp = (value >= 128) ? 0x7f : (value & 0x7f);
            /* Assert bit 7 for positive numbers */
            value = (1 << 7) + tmp;
        } else {
            value = (value <= -128) ? 0x00 : (value & 0x7f);
        }
    }

    status = lusb_lms_write(dev, addr, (uint8_t)value);
    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
        return status;
    }

    return status;
}

static inline void get_correction_addr_type(bladerf_module module,
                                            bladerf_correction corr,
                                            uint8_t *addr, corr_type *type)
{
    switch (corr) {

        /* These items are controlled in the FPGA */

        case BLADERF_CORR_FPGA_PHASE:
            *type = CORR_FPGA;
            *addr = module == BLADERF_MODULE_TX ?
                        UART_PKT_DEV_TX_PHASE_ADDR : UART_PKT_DEV_RX_PHASE_ADDR;
            break;

        case BLADERF_CORR_FPGA_GAIN:
            *type = CORR_FPGA;
            *addr = module == BLADERF_MODULE_TX ?
                        UART_PKT_DEV_TX_GAIN_ADDR : UART_PKT_DEV_RX_GAIN_ADDR;
            break;

        /* These items are controlled by the LMS6002D */

        case BLADERF_CORR_LMS_DCOFF_I:
            *type = CORR_LMS;
            *addr = module == BLADERF_MODULE_TX ? 0x42 : 0x71;
            break;

        case BLADERF_CORR_LMS_DCOFF_Q:
            *type = CORR_LMS;
            *addr = module == BLADERF_MODULE_TX ? 0x43 : 0x72;
            break;

        default:
            *type = CORR_INVALID;
            *addr = 0xff;
    }
}

int lusb_set_correction(struct bladerf *dev, bladerf_module module,
                        bladerf_correction corr, int16_t value)
{
    int status;
    uint8_t addr;
    corr_type type;

    get_correction_addr_type(module, corr, &addr, &type);

    log_verbose("Setting %s = 0x%04x\n", corr2str(corr), value);

    switch (type) {
        case CORR_FPGA:
            status = set_fpga_correction(dev, corr, addr, value);
            break;

        case CORR_LMS:
            status = set_lms_correction(dev, module, addr, value);
            break;

        default:
            status = BLADERF_ERR_INVAL;
    }

    return status;
}

static int get_fpga_correction(struct bladerf *dev,
                               uint8_t addr, int16_t *value)
{
    int i = 0;
    int status = 0;
    struct uart_cmd cmd;
    struct bladerf_lusb *lusb = dev->backend;

    *value = 0;
    for (i = 0; status == 0 && i < 2; i++) {
        cmd.addr = i + addr;
        cmd.data = 0xff;
        status = access_peripheral(
                                    lusb,
                                    UART_PKT_DEV_GPIO,
                                    UART_PKT_MODE_DIR_READ,
                                    &cmd
                                  );

        *value |= (cmd.data << (i * 8));

        if (status < 0) {
            break;
        }
    }

    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
    }
    return status;
}

int lusb_get_timestamp(struct bladerf *dev, bladerf_module mod, uint64_t *value)
{
    int status = 0;
    struct uart_cmd cmds[4];
    unsigned char timestamp_bytes[8];
    int i;

    // offset 16 is the time tamer according to the Nios firmware
    cmds[0].addr = (mod == BLADERF_MODULE_RX ? 16 : 24);
    cmds[1].addr = (mod == BLADERF_MODULE_RX ? 17 : 25);
    cmds[2].addr = (mod == BLADERF_MODULE_RX ? 18 : 26);
    cmds[3].addr = (mod == BLADERF_MODULE_RX ? 19 : 27);
    cmds[0].data = cmds[1].data = cmds[2].data = cmds[3].data = 0xff;
    status = access_peripheral_v(
            dev->backend,
            UART_PKT_DEV_GPIO,
            UART_PKT_MODE_DIR_READ,
            cmds, 4
            );

    if (status)
        return status;

    for (i = 0; i < 4; i++)
        timestamp_bytes[i] = cmds[i].data;

    cmds[0].addr = (mod == BLADERF_MODULE_RX ? 20 : 28);
    cmds[1].addr = (mod == BLADERF_MODULE_RX ? 21 : 29);
    cmds[2].addr = (mod == BLADERF_MODULE_RX ? 22 : 30);
    cmds[3].addr = (mod == BLADERF_MODULE_RX ? 23 : 31);
    cmds[0].data = cmds[1].data = cmds[2].data = cmds[3].data = 0xff;
    status = access_peripheral_v(
            dev->backend,
            UART_PKT_DEV_GPIO,
            UART_PKT_MODE_DIR_READ,
            cmds, 4
            );

    if (status)
        return status;

    for (i = 0; i < 4; i++)
        timestamp_bytes[i + 4] = cmds[i].data;

    *value = 0;
    for (i = 7; i >= 0; i--) {
        *value |= ((uint64_t)timestamp_bytes[i]) << (i * 8);
    }
    return 0;
}

static int get_lms_correction(struct bladerf *dev,
                                bladerf_module module,
                                uint8_t addr, int16_t *value)
{
    uint8_t tmp;
    int status;

    status = lusb_lms_read(dev, addr, &tmp);
    if (status == 0) {
        /* Mask out any control bits in the RX DC correction area */
        if (module == BLADERF_MODULE_RX) {
            tmp = tmp & 0x7f;
            if (tmp & (1 << 6)) {
                *value = -(int16_t)(tmp & 0x3f);
            } else {
                *value = (int16_t)(tmp & 0x3f);
            }
            /* Renormalize to 2048 */
            *value <<= 5;
        } else {
            *value = (int16_t)tmp;
            /* Renormalize to 2048 */
            *value <<= 4;
        }
    } else {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
    }

    return status;
}

static int lusb_get_correction(struct bladerf *dev, bladerf_module module,
                               bladerf_correction corr, int16_t *value)
{
    int status;
    uint8_t addr;
    corr_type type;

    get_correction_addr_type(module, corr, &addr, &type);

    switch (type) {
        case CORR_LMS:
            status = get_lms_correction(dev, module, addr, value);
            break;

        case CORR_FPGA:
            status = get_fpga_correction(dev, addr, value);
            break;

        default:
            status = BLADERF_ERR_INVAL;
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

static void LIBUSB_CALL lusb_stream_cb(struct libusb_transfer *transfer);

/* Precondition: A transfer is available. */
static int submit_transfer(struct bladerf_stream *stream, void *buffer)
{
    int status;
    struct bladerf_lusb *lusb = stream->dev->backend;
    struct lusb_stream_data *stream_data = stream->backend_data;
    struct libusb_transfer *transfer;
    size_t bytes_per_buffer;
    const unsigned char ep =
        stream->module == BLADERF_MODULE_TX ? EP_OUT(0x01) : EP_IN(0x01);

    assert(stream_data->transfer_status[stream_data->i] == TRANSFER_AVAIL);
    transfer = stream_data->transfers[stream_data->i];

    switch (stream->format) {
        case BLADERF_FORMAT_SC16_Q11:
            bytes_per_buffer = c16_samples_to_bytes(stream->samples_per_buffer);
            break;

        default:
            assert(!"Unexpected format");
            return BLADERF_ERR_INVAL;
    }

    assert(bytes_per_buffer <= INT_MAX);
    libusb_fill_bulk_transfer(transfer,
                              lusb->handle,
                              ep,
                              buffer,
                              bytes_per_buffer,
                              lusb_stream_cb,
                              stream,
                              stream->dev->transfer_timeout[stream->module]);

    status = libusb_submit_transfer(transfer);

    if (status == 0) {
        stream_data->transfer_status[stream_data->i] = TRANSFER_IN_FLIGHT;
        stream_data->i = (stream_data->i + 1) % stream_data->num_transfers;
        assert(stream_data->num_avail_transfers != 0);
        stream_data->num_avail_transfers--;
    } else {
        log_error("Failed to submit transfer in %s: %s\n",
                  __FUNCTION__, libusb_error_name(status));
    }

    return error_libusb2bladerf(status);
}

static void LIBUSB_CALL lusb_stream_cb(struct libusb_transfer *transfer)
{
    struct bladerf_stream *stream  = transfer->user_data;
    void *next_buffer = NULL;
    struct bladerf_metadata metadata;
    struct lusb_stream_data *stream_data = stream->backend_data;
    size_t transfer_i;

    /* Currently unused - zero out for out own debugging sanity... */
    memset(&metadata, 0, sizeof(metadata));

    pthread_mutex_lock(&stream->lock);

    transfer_i = transfer_idx(stream_data, transfer);
    assert(stream_data->transfer_status[transfer_i] == TRANSFER_IN_FLIGHT ||
           stream_data->transfer_status[transfer_i] == TRANSFER_CANCEL_PENDING);

    if (transfer_i >= stream_data->num_transfers) {
        log_error("Unable to find transfer");
        stream->state = STREAM_SHUTTING_DOWN;
    } else {
        stream_data->transfer_status[transfer_i] = TRANSFER_AVAIL;
        stream_data->num_avail_transfers++;
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

            /* For the SC16Q11 format, we're 4 bytes per samples... */
            if ((transfer->actual_length & 3) != 0) {
                log_warning( "Fractional samples received - stream likely corrupt\n" );
            }
        }

       /* Call user callback requesting more data to transmit */
        next_buffer = stream->cb(
                        stream->dev,
                        stream,
                        &metadata,
                        transfer->buffer,
                        bytes_to_c16_samples(transfer->actual_length),
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
        if (stream_data->num_avail_transfers == stream_data->num_transfers) {
            stream->state = STREAM_DONE;
        } else {
            cancel_all_transfers(stream);
        }
    }

    pthread_mutex_unlock(&stream->lock);
}

static int lusb_stream_init(struct bladerf_stream *stream, size_t num_transfers)
{
    size_t i;
    struct lusb_stream_data *stream_data;
    int status = 0;

    /* Fill in backend stream information */
    stream_data = malloc(sizeof(struct lusb_stream_data));
    if (!stream_data) {
        return BLADERF_ERR_MEM;
    }

    /* Backend stream information */
    stream->backend_data = stream_data;

    stream_data->num_transfers = num_transfers;
    stream_data->num_avail_transfers = 0;
    stream_data->i = 0;

    stream_data->transfers = malloc(num_transfers * sizeof(struct libusb_transfer *));
    if (stream_data->transfers == NULL) {
        log_error("Failed to allocate libusb tranfers\n");
        status = BLADERF_ERR_MEM;
        goto error;
    }

    stream_data->transfer_status = calloc(num_transfers, sizeof(transfer_status));
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
                    stream_data->num_avail_transfers--;
                }
            }

            status = BLADERF_ERR_MEM;
            break;
        } else {
            stream_data->transfer_status[i] = TRANSFER_AVAIL;
            stream_data->num_avail_transfers++;
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

#ifndef LIBUSB_HANDLE_EVENTS_TIMEOUT_NSEC
#   define LIBUSB_HANDLE_EVENTS_TIMEOUT_NSEC    (15 * 1000)
#endif

static int lusb_stream(struct bladerf_stream *stream, bladerf_module module)
{
    size_t i;
    int status = 0;
    void *buffer;
    struct bladerf_metadata metadata;
    struct bladerf *dev = stream->dev;
    struct bladerf_lusb *lusb = dev->backend;
    struct lusb_stream_data *stream_data = stream->backend_data;
    struct timeval tv = { 0, LIBUSB_HANDLE_EVENTS_TIMEOUT_NSEC };

    /* Currently unused, so zero it out for a sanity check when debugging */
    memset(&metadata, 0, sizeof(metadata));

    pthread_mutex_lock(&stream->lock);

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
                if (stream_data->num_avail_transfers != stream_data->num_transfers) {
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
    pthread_mutex_unlock(&stream->lock);

    /* This loop is required so libusb can do callbacks and whatnot */
    while (stream->state != STREAM_DONE) {
        status = libusb_handle_events_timeout(lusb->context, &tv);

        if (status < 0 && status != LIBUSB_ERROR_INTERRUPTED) {
            log_warning("unexpected value from events processing: "
                        "%d: %s\n", status, libusb_error_name(status));
            status = error_libusb2bladerf(status);
        }
    }

    return status;
}

/* The top-level code will have aquired the stream->lock for us */
int lusb_submit_stream_buffer(struct bladerf_stream *stream,
                              void *buffer,
                              unsigned int timeout_ms)
{
    int status;
    struct lusb_stream_data *stream_data = stream->backend_data;
    struct timespec timeout_abs;

    if (buffer == BLADERF_STREAM_SHUTDOWN) {
        if (stream_data->num_avail_transfers == stream_data->num_transfers) {
            stream->state = STREAM_DONE;
        } else {
            stream->state = STREAM_SHUTTING_DOWN;
        }

        return 0;
    }


    status = populate_abs_timeout(&timeout_abs, timeout_ms);
    if (status != 0) {
        return BLADERF_ERR_UNEXPECTED;
    }

    while (stream_data->num_avail_transfers == 0 && status == 0) {
        status = pthread_cond_timedwait(&stream->can_submit_buffer,
                                        &stream->lock,
                                        &timeout_abs);
    }

    if (status == ETIMEDOUT) {
        return BLADERF_ERR_TIMEOUT;
    } else if (status != 0) {
        return BLADERF_ERR_UNEXPECTED;
    } else {
        return submit_transfer(stream, buffer);
    }
}


void lusb_deinit_stream(struct bladerf_stream *stream)
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
}

int lusb_probe(struct bladerf_devinfo_list *info_list)
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

const struct bladerf_fn bladerf_lusb_fn = {
    FIELD_INIT(.probe, lusb_probe),

    FIELD_INIT(.open, lusb_open),
    FIELD_INIT(.close, lusb_close),

    FIELD_INIT(.load_fpga, lusb_load_fpga),
    FIELD_INIT(.is_fpga_configured, lusb_is_fpga_configured),

    FIELD_INIT(.flash_firmware, lusb_flash_firmware),

    FIELD_INIT(.erase_flash, lusb_erase_flash),
    FIELD_INIT(.read_flash, lusb_read_flash),
    FIELD_INIT(.write_flash, lusb_write_flash),
    FIELD_INIT(.device_reset, lusb_device_reset),
    FIELD_INIT(.jump_to_bootloader, lusb_jump_to_bootloader),

    FIELD_INIT(.get_cal, lusb_get_cal),
    FIELD_INIT(.get_otp, lusb_get_otp),
    FIELD_INIT(.get_device_speed, lusb_get_device_speed),

    FIELD_INIT(.config_gpio_write, lusb_config_gpio_write),
    FIELD_INIT(.config_gpio_read, lusb_config_gpio_read),

    FIELD_INIT(.expansion_gpio_write, lusb_expansion_gpio_write),
    FIELD_INIT(.expansion_gpio_read, lusb_expansion_gpio_read),

    FIELD_INIT(.expansion_gpio_dir_write, lusb_expansion_gpio_dir_write),
    FIELD_INIT(.expansion_gpio_dir_read, lusb_expansion_gpio_dir_read),

    FIELD_INIT(.set_correction, lusb_set_correction),
    FIELD_INIT(.get_correction, lusb_get_correction),

    FIELD_INIT(.get_timestamp, lusb_get_timestamp),

    FIELD_INIT(.si5338_write, lusb_si5338_write),
    FIELD_INIT(.si5338_read, lusb_si5338_read),

    FIELD_INIT(.lms_write, lusb_lms_write),
    FIELD_INIT(.lms_read, lusb_lms_read),

    FIELD_INIT(.dac_write, lusb_dac_write),
    FIELD_INIT(.xb_spi, lusb_xb_spi),

    FIELD_INIT(.set_firmware_loopback, lusb_set_firmware_loopback),

    FIELD_INIT(.enable_module, lusb_enable_module),

    FIELD_INIT(.init_stream, lusb_stream_init),
    FIELD_INIT(.stream, lusb_stream),
    FIELD_INIT(.submit_stream_buffer, lusb_submit_stream_buffer),
    FIELD_INIT(.deinit_stream, lusb_deinit_stream),
};
