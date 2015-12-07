/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014 Nuand LLC
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
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>

#include "usb.h"
#include "rel_assert.h"
#include "bladerf_priv.h"
#include "backend/backend.h"
#include "backend/backend_config.h"
#include "backend/usb/usb.h"
#include "async.h"
#include "bladeRF.h"    /* Firmware interface */
#include "log.h"
#include "capabilities.h"
#include "minmax.h"
#include "conversions.h"
#include "nios_pkt_formats.h"
#include "nios_legacy_access.h"
#include "nios_access.h"

#if ENABLE_USB_DEV_RESET_ON_OPEN
bool bladerf_usb_reset_device_on_open = true;
#endif

typedef enum {
    CORR_INVALID,
    CORR_FPGA,
    CORR_LMS
} corr_type;

static const struct usb_driver *usb_driver_list[] = BLADERF_USB_BACKEND_LIST;

/* FW declaration of fn table declared at the end of this file */
const struct backend_fns backend_fns_usb_legacy;

/* Vendor command wrapper to gets a 32-bit integer and supplies a wIndex */
static inline int vendor_cmd_int_windex(struct bladerf *dev, uint8_t cmd,
                                        uint16_t windex, int32_t *val)
{
    void *driver;
    struct bladerf_usb *usb = usb_backend(dev, &driver);

    return usb->fn->control_transfer(driver,
                                      USB_TARGET_DEVICE,
                                      USB_REQUEST_VENDOR,
                                      USB_DIR_DEVICE_TO_HOST,
                                      cmd, 0, windex,
                                      val, sizeof(uint32_t),
                                      CTRL_TIMEOUT_MS);
}

/* Vendor command wrapper to get a 32-bit integer and supplies wValue */
static inline int vendor_cmd_int_wvalue(struct bladerf *dev, uint8_t cmd,
                                        uint16_t wvalue, int32_t *val)
{
    void *driver;
    struct bladerf_usb *usb = usb_backend(dev, &driver);

    return usb->fn->control_transfer(driver,
                                      USB_TARGET_DEVICE,
                                      USB_REQUEST_VENDOR,
                                      USB_DIR_DEVICE_TO_HOST,
                                      cmd, wvalue, 0,
                                      val, sizeof(uint32_t),
                                      CTRL_TIMEOUT_MS);
}


/* Vendor command that gets/sets a 32-bit integer value */
static inline int vendor_cmd_int(struct bladerf *dev, uint8_t cmd,
                                 usb_direction dir, int32_t *val)
{
    void *driver;
    struct bladerf_usb *usb = usb_backend(dev, &driver);

    return usb->fn->control_transfer(driver,
                                      USB_TARGET_DEVICE,
                                      USB_REQUEST_VENDOR,
                                      dir, cmd, 0, 0,
                                      val, sizeof(int32_t),
                                      CTRL_TIMEOUT_MS);
}

static inline int change_setting(struct bladerf *dev, uint8_t setting)
{
    int status;
    void *driver;
    struct bladerf_usb *usb = usb_backend(dev, &driver);

    log_verbose("Changing to USB alt setting %u\n", setting);

    status = usb->fn->change_setting(driver, setting);
    if (status != 0) {
        log_debug("Failed to change setting: %s\n", bladerf_strerror(status));
    }

    return status;
}

static int usb_is_fpga_configured(struct bladerf *dev)
{
    int result = -1;
    int status;

    /* This environment variable provides a means to force libbladeRF to not
     * attempt to access the FPGA.
     *
     * This provides a workaround for the situation where a user did not remove
     * an FPGA in SPI flash prior to flashing new firmware and updating
     * libbladeRF. Specifically, this has proven to be a problem with pre-v0.0.1
     * FPGA images, as they do not provide version readback functionality.
     */
    if (getenv("BLADERF_FORCE_NO_FPGA_PRESENT")) {
        log_debug("Reporting no FPGA present - "
                  "BLADERF_FORCE_NO_FPGA_PRESENT is set.\n");
        return 0;
    }

    status = vendor_cmd_int(dev, BLADE_USB_CMD_QUERY_FPGA_STATUS,
                            USB_DIR_DEVICE_TO_HOST, &result);

    if (status < 0) {
        return status;
    } else if (result == 0 || result == 1) {
        return result;
    } else {
        log_debug("Unexpected result from FPGA status query: %d\n", result);
        return BLADERF_ERR_UNEXPECTED;
    }
}

/* Switch to RF link mode, fetch the FPGA version, and determine the
 * device's capabilities */
static int post_fpga_load_init(struct bladerf *dev)
{
    int status = change_setting(dev, USB_IF_RF_LINK);
    if (status == 0) {
        /* Read and store FPGA version info. This is only possible after
         * we've entered RF link mode.
         *
         * The legacy mode is used here since we can't yet determine if
         * the FPGA is capable of using the newer packet formats. */
        status = nios_legacy_get_fpga_version(dev, &dev->fpga_version);
        if (status != 0) {
            return status;
        } else {
            log_verbose("Read FPGA version: %s\n", dev->fpga_version.describe);
        }
    }

    /* Update device capabilities mask based upon FPGA version number */
    capabilities_init_post_fpga_load(dev);

    if (!getenv("BLADERF_FORCE_LEGACY_NIOS_PKT")) {
        /* Switch to our newer NIOS II request/response packet format
         * if the FPGA supports it */
        if (have_cap(dev, BLADERF_CAP_PKT_HANDLER_FMT)) {
            log_verbose("Using current packet handler formats\n");
            dev->fn = &backend_fns_usb;
        } else {
            log_verbose("Using legacy packet handler format\n");
        }
    } else {
        dev->capabilities &= ~(BLADERF_CAP_PKT_HANDLER_FMT);
        log_verbose("Using legacy packet handler format due to env var\n");
    }

    return status;
}

/* After performing a flash operation, switch back to either RF_LINK or the
 * FPGA loader.
 */
static int restore_post_flash_setting(struct bladerf *dev)
{
    int fpga_loaded = usb_is_fpga_configured(dev);
    int status;

    if (fpga_loaded < 0) {
        status = fpga_loaded;
        log_debug("Failed to determine if FPGA is loaded (%d)\n", fpga_loaded);
    } else if (fpga_loaded) {
        status = change_setting(dev, USB_IF_RF_LINK);
    } else {
        status = change_setting(dev, USB_IF_CONFIG);
    }

    if (status < 0) {
        log_debug("Failed to restore alt setting: %s\n",
                  bladerf_strerror(status));
    }
    return status;
}

static bool usb_matches(bladerf_backend backend)
{
    return backend == BLADERF_BACKEND_ANY ||
           backend == BLADERF_BACKEND_LINUX ||
           backend == BLADERF_BACKEND_LIBUSB ||
           backend == BLADERF_BACKEND_CYPRESS;
}

static int usb_probe(backend_probe_target probe_target,
                     struct bladerf_devinfo_list *info_list)
{
    int status;
    size_t i;

    for (i = status = 0; i < ARRAY_SIZE(usb_driver_list); i++) {
        status = usb_driver_list[i]->fn->probe(probe_target, info_list);
    }

    return status;
}

static void usb_close(struct bladerf *dev)
{
    int status;
    void *driver;
    struct bladerf_usb *usb = usb_backend(dev, &driver);

    if (usb != NULL) {
        /* It seems we need to switch back to our NULL interface before closing,
         * or else our device doesn't close upon exit in OSX and then fails to
         * re-open cleanly */
        status = usb->fn->change_setting(driver, USB_IF_NULL);
        if (status != 0) {
            log_error("Failed to switch to NULL interface: %s\n",
                    bladerf_strerror(status));
        }

        usb->fn->close(driver);
        free(usb);
        dev->backend = NULL;
    }
}

static inline int populate_fw_version(struct bladerf_usb *usb,
                                      struct bladerf_version *version)
{
    int status = usb->fn->get_string_descriptor(
                                    usb->driver,
                                    BLADE_USB_STR_INDEX_FW_VER,
                                    (unsigned char *)version->describe,
                                    BLADERF_VERSION_STR_MAX);

    if (status == 0) {
        status = str2version(version->describe, version);
    } else {
        log_warning("Failed to retrieve firmware version. This may be due "
                    "to an old firmware version that does not support "
                    "this request. A firmware update via the bootloader is "
                    "required.\n\n");
        status = BLADERF_ERR_UPDATE_FW;
    }
    return status;
}

static int usb_open(struct bladerf *dev, struct bladerf_devinfo *info)
{
    int status;
    size_t i;
    struct bladerf_usb *usb = (struct bladerf_usb*) malloc(sizeof(*usb));

    if (usb == NULL) {
        return BLADERF_ERR_MEM;
    }

    /* Default to legacy-mode access until we determine the FPGA is
     * capable of handling newer request formats */
    dev->fn = &backend_fns_usb_legacy;
    dev->backend = usb;

    status = BLADERF_ERR_NODEV;
    for (i = 0; i < ARRAY_SIZE(usb_driver_list) && status == BLADERF_ERR_NODEV; i++) {
        if (info->backend == BLADERF_BACKEND_ANY ||
            usb_driver_list[i]->id == info->backend) {

            usb->fn = usb_driver_list[i]->fn;
            status = usb->fn->open(&usb->driver, info, &dev->ident);
        }
    }

    if (status != 0) {
        free(usb);
        dev->backend = NULL;
        dev->fn = NULL;
        return status;
    }

    dev->transfer_timeout[BLADERF_MODULE_TX] = BULK_TIMEOUT_MS;
    dev->transfer_timeout[BLADERF_MODULE_RX] = BULK_TIMEOUT_MS;


    /* The FPGA version is populated when rf link is established
     * (zeroize until then) */
    dev->fpga_version.major = 0;
    dev->fpga_version.minor = 0;
    dev->fpga_version.patch = 0;

    status = populate_fw_version(usb, &dev->fw_version);
    if (status < 0) {
        log_debug("Failed to populate FW version: %s\n",
                  bladerf_strerror(status));
        return status;
    }

    /* Fetch initial device capabilities based upon firmware version */
    capabilities_init_pre_fpga_load(dev);

    /* Wait for SPI flash autoloading to complete, if needed */
    if (have_cap(dev, BLADERF_CAP_QUERY_DEVICE_READY)) {
        const unsigned int max_retries = 30;
        unsigned int i;
        int status;
        int32_t device_ready = 0;

        for (i = 0; (device_ready != 1) && i < max_retries; i++) {
            status = vendor_cmd_int(dev, BLADE_USB_CMD_QUERY_DEVICE_READY,
                                         USB_DIR_DEVICE_TO_HOST, &device_ready);

            if (status != 0 || (device_ready != 1)) {
                if (i == 0) {
                    log_info("Waiting for device to become ready...\n");
                } else {
                    log_debug("Retry %02u/%02u.\n", i + 1, max_retries);
                }

                usleep(1000000);
            }
        }

        if (i >= max_retries) {
            log_debug("Timed out while waiting for device.\n");
            return BLADERF_ERR_TIMEOUT;
        }
    } else {
        const unsigned int major = dev->fw_version.major;
        const unsigned int minor = dev->fw_version.minor;
        const unsigned int patch = dev->fw_version.patch;

        log_info("FX3 FW v%u.%u.%u does not support the \"device ready\" query.\n"
                 "\tEnsure flash-autoloading completes before opening a device.\n"
                 "\tUpgrade the FX3 firmware to avoid this message in the future.\n"
                 "\n", major, minor, patch);
    }

    /* Just out of paranoia, put the device into a known state */
    status = change_setting(dev, USB_IF_NULL);
    if (status < 0) {
        log_debug("Failed to switch to USB_IF_NULL\n");
        goto error;
    }

    status = usb_is_fpga_configured(dev);
    if (status > 0) {
        status = post_fpga_load_init(dev);
    }

error:
    if (status != 0) {
        usb_close(dev);
    }

    return status;
}

static int begin_fpga_programming(struct bladerf *dev)
{
    int result;
    int status = vendor_cmd_int(dev, BLADE_USB_CMD_BEGIN_PROG,
                                USB_DIR_DEVICE_TO_HOST, &result);

    if (status != 0) {
        return status;
    } else if (result != 0) {
        log_debug("Startg fpga programming, result = %d\n", result);
        return BLADERF_ERR_UNEXPECTED;
    } else {
        return 0;
    }
}

static int usb_load_fpga(struct bladerf *dev, uint8_t *image, size_t image_size)
{
    void *driver;
    struct bladerf_usb *usb = usb_backend(dev, &driver);

    unsigned int wait_count;
    const unsigned int timeout_ms = (2 * CTRL_TIMEOUT_MS);
    int status;

    /* Switch to the FPGA configuration interface */
    status = change_setting(dev, USB_IF_CONFIG);
    if(status < 0) {
        log_debug("Failed to switch to FPGA config setting: %s\n",
                  bladerf_strerror(status));
        return status;
    }

    /* Begin programming */
    status = begin_fpga_programming(dev);
    if (status < 0) {
        log_debug("Failed to initiate FPGA programming: %s\n",
                  bladerf_strerror(status));
        return status;
    }

    /* Send the file down */
    assert(image_size <= UINT32_MAX);
    status = usb->fn->bulk_transfer(driver, PERIPHERAL_EP_OUT, image,
                                    (uint32_t)image_size, timeout_ms);
    if (status < 0) {
        log_debug("Failed to write FPGA bitstream to FPGA: %s\n",
                  bladerf_strerror(status));
        return status;
    }

    /* Poll FPGA status to determine if programming was a success */
    wait_count = 10;
    status = 0;

    while (wait_count > 0 && status == 0) {
        status = usb_is_fpga_configured(dev);
        if (status == 1) {
            break;
        }

        usleep(200000);
        wait_count--;
    }

    /* Failed to determine if FPGA is loaded */
    if (status < 0) {
        log_debug("Failed to determine if FPGA is loaded: %s\n",
                  bladerf_strerror(status));
        return status;
    } else if (wait_count == 0 && status != 0) {
        log_debug("Timeout while waiting for FPGA configuration status\n");
        return BLADERF_ERR_TIMEOUT;
    }

    return post_fpga_load_init(dev);
}

static inline int perform_erase(struct bladerf *dev, uint16_t block)
{
    int status, erase_ret;
    void *driver;
    struct bladerf_usb *usb = usb_backend(dev, &driver);

    status = usb->fn->control_transfer(driver,
                                        USB_TARGET_DEVICE,
                                        USB_REQUEST_VENDOR,
                                        USB_DIR_DEVICE_TO_HOST,
                                        BLADE_USB_CMD_FLASH_ERASE,
                                        0, block,
                                        &erase_ret, sizeof(erase_ret),
                                        CTRL_TIMEOUT_MS);


    return status;
}

static int usb_erase_flash_blocks(struct bladerf *dev,
                                  uint32_t eb, uint16_t count)
{
    int status, restore_status;
    uint16_t i;

    status = change_setting(dev, USB_IF_SPI_FLASH);
    if (status != 0) {
        return status;
    }

    log_info("Erasing %u blocks starting at block %u\n", count, eb);

    for (i = 0; i < count; i++) {
        status = perform_erase(dev, eb + i);
        if (status == 0) {
            log_info("Erased block %u%c", eb + i, (i+1) == count ? '\n':'\r' );
        } else {
            log_debug("Failed to erase block %u: %s\n",
                    eb + i, bladerf_strerror(status));
            goto error;
        }
    }

    log_info("Done erasing %u blocks\n", count);

error:
    restore_status = restore_post_flash_setting(dev);
    return status != 0 ? status : restore_status;
}

static inline int read_page(struct bladerf *dev, uint8_t read_operation,
                            uint16_t page, uint8_t *buf)
{
    void *driver;
    struct bladerf_usb *usb = usb_backend(dev, &driver);
    int status;
    int32_t op_status;
    uint16_t read_size;
    uint16_t offset;
    uint8_t request;

    if (dev->usb_speed == BLADERF_DEVICE_SPEED_SUPER) {
        read_size = BLADERF_FLASH_PAGE_SIZE;
    } else if (dev->usb_speed == BLADERF_DEVICE_SPEED_HIGH) {
        read_size = 64;
    } else {
        log_debug("Encountered unknown USB speed in %s\n", __FUNCTION__);
        return BLADERF_ERR_UNEXPECTED;
    }

    if (read_operation == BLADE_USB_CMD_FLASH_READ ||
        read_operation == BLADE_USB_CMD_READ_OTP) {

        status = vendor_cmd_int_windex(dev, read_operation, page, &op_status);
        if (status != 0) {
            return status;
        } else if (op_status != 0) {
            log_error("Firmware page read (op=%d) failed at page %u: %d\n",
                    read_operation, page, op_status);
            return BLADERF_ERR_UNEXPECTED;
        }

        /* Both of these operations require a read from the FW's page buffer */
        request = BLADE_USB_CMD_READ_PAGE_BUFFER;

    } else if (read_operation == BLADE_USB_CMD_READ_CAL_CACHE) {
        request = read_operation;
    } else {
        assert(!"Bug - invalid read_operation value");
        return BLADERF_ERR_UNEXPECTED;
    }

    /* Retrieve data from the firmware page buffer */
    for (offset = 0; offset < BLADERF_FLASH_PAGE_SIZE; offset += read_size) {
        status = usb->fn->control_transfer(driver,
                                           USB_TARGET_DEVICE,
                                           USB_REQUEST_VENDOR,
                                           USB_DIR_DEVICE_TO_HOST,
                                           request,
                                           0,
                                           offset, /* in bytes */
                                           buf + offset,
                                           read_size,
                                           CTRL_TIMEOUT_MS);

        if(status < 0) {
            log_debug("Failed to read page buffer at offset 0x%02x: %s\n",
                      offset, bladerf_strerror(status));
            return status;
        }
    }

    return 0;
}

static int usb_read_flash_pages(struct bladerf *dev, uint8_t *buf,
                                uint32_t page_u32, uint32_t count_u32)
{
    int status;
    size_t n_read;
    uint16_t i;

    /* 16-bit control transfer fields are used for these.
     * The current bladeRF build only has a 4MiB flash, anyway. */
    const uint16_t page = (uint16_t) page_u32;
    const uint16_t count = (uint16_t) count_u32;

    assert(page == page_u32);
    assert(count == count_u32);

    status = change_setting(dev, USB_IF_SPI_FLASH);
    if (status != 0) {
        return status;
    }

    log_info("Reading %u pages starting at page %u\n", count, page);

    for (n_read = i = 0; i < count; i++) {
        log_info("Reading page %u%c", page + i, (i+1) == count ? '\n':'\r' );

        status = read_page(dev, BLADE_USB_CMD_FLASH_READ,
                           page + i, buf + n_read);
        if (status != 0) {
            goto error;
        }

        n_read += BLADERF_FLASH_PAGE_SIZE;
    }

    log_info("Done reading %u pages\n", count);

error:
    status = restore_post_flash_setting(dev);
    return status;
}

static int write_page(struct bladerf *dev, uint16_t page, const uint8_t *buf)
{
    int status;
    int32_t commit_status;
    uint16_t offset;
    uint16_t write_size;
    void *driver;
    struct bladerf_usb *usb = usb_backend(dev, &driver);

    if (dev->usb_speed == BLADERF_DEVICE_SPEED_SUPER) {
        write_size = BLADERF_FLASH_PAGE_SIZE;
    } else if (dev->usb_speed == BLADERF_DEVICE_SPEED_HIGH) {
        write_size = 64;
    } else {
        assert(!"BUG - unexpected device speed");
        return BLADERF_ERR_UNEXPECTED;
    }

    /* Write the data to the firmware's page buffer.
     * Casting away the buffer's const-ness here is gross, but this buffer
     * will not be written to on an out transfer. */
    for (offset = 0; offset < BLADERF_FLASH_PAGE_SIZE; offset += write_size) {
        status = usb->fn->control_transfer(driver,
                                            USB_TARGET_DEVICE,
                                            USB_REQUEST_VENDOR,
                                            USB_DIR_HOST_TO_DEVICE,
                                            BLADE_USB_CMD_WRITE_PAGE_BUFFER,
                                            0,
                                            offset,
                                            (uint8_t*)&buf[offset],
                                            write_size,
                                            CTRL_TIMEOUT_MS);

        if(status < 0) {
            log_error("Failed to write page buffer at offset 0x%02x "
                      "for page %u: %s\n",
                      offset, page, bladerf_strerror(status));
            return status;
        }
    }

    /* Commit the page to flash */
    status = vendor_cmd_int_windex(dev, BLADE_USB_CMD_FLASH_WRITE,
                                   page, &commit_status);

    if (status != 0) {
        log_error("Failed to commit page %u: %s\n", page,
                  bladerf_strerror(status));
        return status;

    } else if (commit_status != 0) {
        log_error("Failed to commit page %u, FW returned %d\n", page,
                  commit_status);

         return BLADERF_ERR_UNEXPECTED;
    }

    return 0;
}

static int usb_write_flash_pages(struct bladerf *dev, const uint8_t *buf,
                                 uint32_t page_u32, uint32_t count_u32)

{
    int status, restore_status;
    uint16_t i;
    size_t n_written;

    /* 16-bit control transfer fields are used for these.
     * The current bladeRF build only has a 4MiB flash, anyway. */
    const uint16_t page = (uint16_t) page_u32;
    const uint16_t count = (uint16_t) count_u32;

    assert(page == page_u32);
    assert(count == count_u32);

    status = change_setting(dev, USB_IF_SPI_FLASH);
    if (status != 0) {
        return status;
    }

    log_info("Writing %u pages starting at page %u\n", count, page);

    n_written = 0;
    for (i = 0; i < count; i++) {
        log_info("Writing page %u%c", page + i, (i+1) == count ? '\n':'\r');

        status = write_page(dev, page + i, buf + n_written);
        if (status) {
            goto error;
        }

        n_written += BLADERF_FLASH_PAGE_SIZE;
    }
    log_info("Done writing %u pages\n", count );

error:
    restore_status = restore_post_flash_setting(dev);
    if (status != 0) {
        return status;
    } else if (restore_status != 0) {
        return restore_status;
    } else {
        return 0;
    }
}

static int usb_device_reset(struct bladerf *dev)
{
    void *driver;
    struct bladerf_usb *usb = usb_backend(dev, &driver);

    return usb->fn->control_transfer(driver, USB_TARGET_DEVICE,
                                      USB_REQUEST_VENDOR,
                                      USB_DIR_HOST_TO_DEVICE,
                                      BLADE_USB_CMD_RESET,
                                      0, 0, 0, 0, CTRL_TIMEOUT_MS);

}

static int usb_jump_to_bootloader(struct bladerf *dev)
{
    void *driver;
    struct bladerf_usb *usb = usb_backend(dev, &driver);

    return usb->fn->control_transfer(driver, USB_TARGET_DEVICE,
                                      USB_REQUEST_VENDOR,
                                      USB_DIR_HOST_TO_DEVICE,
                                      BLADE_USB_CMD_JUMP_TO_BOOTLOADER,
                                      0, 0, 0, 0, CTRL_TIMEOUT_MS);
}

static int usb_get_cal(struct bladerf *dev, char *cal)
{
    const uint16_t dummy_page = 0;
    int status, restore_status;

    assert(CAL_BUFFER_SIZE == BLADERF_FLASH_PAGE_SIZE);

    status = change_setting(dev, USB_IF_SPI_FLASH);
    if (status) {
        return status;
    }

    status = read_page(dev, BLADE_USB_CMD_READ_CAL_CACHE,
                       dummy_page, (uint8_t*)cal);

    restore_status = restore_post_flash_setting(dev);
    return status == 0 ? restore_status : status;
}

static int usb_get_otp(struct bladerf *dev, char *otp)
{
    int status, restore_status;
    const uint16_t dummy_page = 0;

    status = change_setting(dev, USB_IF_SPI_FLASH);
    if (status) {
        return status;
    }

    status = read_page(dev, BLADE_USB_CMD_READ_OTP, dummy_page, (uint8_t*)otp);
    restore_status = restore_post_flash_setting(dev);
    return status == 0 ? restore_status : status;
}

static int usb_get_device_speed(struct bladerf *dev, bladerf_dev_speed *speed)
{
    void *driver;
    struct bladerf_usb *usb = usb_backend(dev, &driver);

    return usb->fn->get_speed(driver, speed);
}

static int usb_set_firmware_loopback(struct bladerf *dev, bool enable) {
    int result;
    int status;

    status = vendor_cmd_int_wvalue(dev, BLADE_USB_CMD_SET_LOOPBACK,
                                   enable, &result);
    if (status != 0) {
        return status;
    }


    status = change_setting(dev, USB_IF_NULL);
    if (status == 0) {
        status = change_setting(dev, USB_IF_RF_LINK);
    }

    return status;
}

static int usb_get_firmware_loopback(struct bladerf *dev, bool *is_enabled)
{
    int status, result;

    status = vendor_cmd_int(dev, BLADE_USB_CMD_GET_LOOPBACK,
                            USB_DIR_DEVICE_TO_HOST, &result);

    if (status == 0) {
        *is_enabled = (result != 0);
    }

    return status;
}

static int usb_enable_module(struct bladerf *dev, bladerf_module m, bool enable)
{
    int status;
    int32_t fx3_ret = -1;
    const uint16_t val = enable ? 1 : 0;
    const uint8_t cmd = (m == BLADERF_MODULE_RX) ?
                            BLADE_USB_CMD_RF_RX : BLADE_USB_CMD_RF_TX;

    status = vendor_cmd_int_wvalue(dev, cmd, val, &fx3_ret);
    if (status != 0) {
        log_debug("Could not enable RF %s (%d): %s\n",
                    (m == BLADERF_MODULE_RX) ? "RX" : "TX",
                    status, bladerf_strerror(status));

    } else if (fx3_ret != 0) {
        log_warning("FX3 reported error=0x%x when %s RF %s\n",
                    fx3_ret,
                    enable ? "enabling" : "disabling",
                    (m == BLADERF_MODULE_RX) ? "RX" : "TX");

        /* FIXME: Work around what seems to be a harmless failure.
         *        It appears that in firmware or in the lib, we may be
         *        attempting to disable an already disabled module, or
         *        enabling an already enabled module.
         *
         *        Further investigation required
         *
         *        0x44 corresponds to CY_U3P_ERROR_ALREADY_STARTED
         */
        if (fx3_ret != 0x44) {
            status = BLADERF_ERR_UNEXPECTED;
        }
    }

    return status;
}

static int usb_init_stream(struct bladerf_stream *stream, size_t num_transfers)
{
    void *driver;
    struct bladerf_usb *usb = usb_backend(stream->dev, &driver);
    return usb->fn->init_stream(driver, stream, num_transfers);
}

static int usb_stream(struct bladerf_stream *stream, bladerf_module module)
{
    void *driver;
    struct bladerf_usb *usb = usb_backend(stream->dev, &driver);
    return usb->fn->stream(driver, stream, module);
}

int usb_submit_stream_buffer(struct bladerf_stream *stream, void *buffer,
                             unsigned int timeout_ms, bool nonblock)
{
    void *driver;
    struct bladerf_usb *usb = usb_backend(stream->dev, &driver);
    return usb->fn->submit_stream_buffer(driver, stream, buffer,
                                         timeout_ms, nonblock);
}

static void usb_deinit_stream(struct bladerf_stream *stream)
{
    void *driver;
    struct bladerf_usb *usb = usb_backend(stream->dev, &driver);
    usb->fn->deinit_stream(driver, stream);
}

/*
 * Information about the boot image format and boot over USB caan be found in
 * Cypress AN76405: EZ-USB (R) FX3 (TM) Boot Options:
 *  http://www.cypress.com/?docID=49862
 *
 *  There's a request (bRequset = 0xc0) for the bootloader revision.
 *  However, there doesn't appear to be any documented reason to check this and
 *  behave differently depending upon the returned value.
 */

/* Command fields for FX3 firmware upload vendor requests */
#define FX3_BOOTLOADER_LOAD_BREQUEST     0xa0
#define FX3_BOOTLOADER_ADDR_WVALUE(addr) (HOST_TO_LE16(addr & 0xffff))
#define FX3_BOOTLOADER_ADDR_WINDEX(addr) (HOST_TO_LE16(((addr >> 16) & 0xffff)))
#define FX3_BOOTLOADER_MAX_LOAD_LEN      4096

static int write_and_verify_fw_chunk(struct bladerf_usb *usb, uint32_t addr,
                                     uint8_t *data, uint32_t len,
                                     uint8_t *readback_buf) {

    int status;
    log_verbose("Writing %u bytes to bootloader @ 0x%08x\n", len, addr);
    status = usb->fn->control_transfer(usb->driver,
                                       USB_TARGET_DEVICE,
                                       USB_REQUEST_VENDOR,
                                       USB_DIR_HOST_TO_DEVICE,
                                       FX3_BOOTLOADER_LOAD_BREQUEST,
                                       FX3_BOOTLOADER_ADDR_WVALUE(addr),
                                       FX3_BOOTLOADER_ADDR_WINDEX(addr),
                                       data,
                                       len,
                                       CTRL_TIMEOUT_MS);

    if (status != 0) {
        log_debug("Failed to write FW chunk (%d)\n", status);
        return status;
    }

    log_verbose("Reading back %u bytes from bootloader @ 0x%08x\n", len, addr);
    status = usb->fn->control_transfer(usb->driver,
                                       USB_TARGET_DEVICE,
                                       USB_REQUEST_VENDOR,
                                       USB_DIR_DEVICE_TO_HOST,
                                       FX3_BOOTLOADER_LOAD_BREQUEST,
                                       FX3_BOOTLOADER_ADDR_WVALUE(addr),
                                       FX3_BOOTLOADER_ADDR_WINDEX(addr),
                                       readback_buf,
                                       len,
                                       CTRL_TIMEOUT_MS);

    if (status != 0) {
        log_debug("Failed to read back FW chunk (%d)\n", status);
        return status;
    }

    if (memcmp(data, readback_buf, len) != 0) {
        log_debug("Readback did match written data.\n");
        status = BLADERF_ERR_UNEXPECTED;
    }

    return status;
}

static int execute_fw_from_bootloader(struct bladerf_usb *usb, uint32_t addr)
{
    int status;

    status = usb->fn->control_transfer(usb->driver,
                                       USB_TARGET_DEVICE,
                                       USB_REQUEST_VENDOR,
                                       USB_DIR_HOST_TO_DEVICE,
                                       FX3_BOOTLOADER_LOAD_BREQUEST,
                                       FX3_BOOTLOADER_ADDR_WVALUE(addr),
                                       FX3_BOOTLOADER_ADDR_WINDEX(addr),
                                       NULL,
                                       0,
                                       CTRL_TIMEOUT_MS);

    if (status != 0 && status != BLADERF_ERR_IO) {
        log_debug("Failed to exec firmware: %s\n:",
                  bladerf_strerror(status));

    } else if (status == BLADERF_ERR_IO) {
        /* The device might drop out from underneath us as it starts executing
         * the new firmware */
        log_verbose("Device returned IO error due to FW boot.\n");
        status = 0;
    } else {
        log_verbose("Booting new FW.\n");
    }

    return status;
}

static int write_fw_to_bootloader(void *driver, struct fx3_firmware *fw)
{
    int status = 0;
    uint32_t to_write;
    uint32_t data_len;
    uint32_t addr;
    uint8_t *data;
    bool got_section;

    uint8_t *readback = malloc(FX3_BOOTLOADER_MAX_LOAD_LEN);
    if (readback == NULL) {
        return BLADERF_ERR_MEM;
    }

    do {
        got_section = fx3_fw_next_section(fw, &addr, &data, &data_len);
        if (got_section) {
            /* data_len should never be zero, as fw->num_sections should NOT
             * include the terminating section in its count */
            assert(data_len != 0);

            do {
                to_write = u32_min(data_len, FX3_BOOTLOADER_MAX_LOAD_LEN);

                status = write_and_verify_fw_chunk(driver,
                                                   addr, data, to_write,
                                                   readback);

                data_len -= to_write;
                addr += to_write;
                data += to_write;
            } while (data_len != 0 && status == 0);
        }
    } while (got_section && status == 0);

    if (status == 0) {
        status = execute_fw_from_bootloader(driver, fx3_fw_entry_point(fw));
    }

    free(readback);
    return status;
}

static int usb_load_fw_from_bootloader(bladerf_backend backend,
                                       uint8_t bus, uint8_t addr,
                                       struct fx3_firmware *fw)
{
    int status = 0;
    size_t i;
    struct bladerf_usb usb;

    for (i = 0; i < ARRAY_SIZE(usb_driver_list); i++) {

        if ((backend == BLADERF_BACKEND_ANY) ||
            (usb_driver_list[i]->id == backend)) {

            usb.fn = usb_driver_list[i]->fn;
            status = usb.fn->open_bootloader(&usb.driver, bus, addr);
            if (status == 0) {
                status = write_fw_to_bootloader(&usb, fw);
                usb.fn->close_bootloader(usb.driver);
                break;
            }
        }
    }

    return status;
}

/* Default handlers for operations unsupported by the NIOS II legacy packet
 * format */
static int set_vctcxo_tamer_mode_unsupported(struct bladerf *dev,
                                             bladerf_vctcxo_tamer_mode mode)
{
    log_debug("Operation not supported with legacy NIOS packet format.\n");
    return BLADERF_ERR_UNSUPPORTED;
}

static int get_vctcxo_tamer_mode_unsupported(struct bladerf *dev,
                                             bladerf_vctcxo_tamer_mode *mode)
{
    *mode = BLADERF_VCTCXO_TAMER_INVALID;
    log_debug("Operation not supported with legacy NIOS packet format.\n");
    return BLADERF_ERR_UNSUPPORTED;
}

static int usb_read_fw_log(struct bladerf *dev, logger_entry *e)
{
    int status;
    *e = LOG_EOF;

    if (have_cap(dev, BLADERF_CAP_READ_FW_LOG_ENTRY)) {
        status = vendor_cmd_int(dev, BLADE_USB_CMD_READ_LOG_ENTRY,
                                USB_DIR_DEVICE_TO_HOST, (int32_t*) e);
    } else {
        log_debug("FX3 FW v%s does not support log retrieval.\n",
                  dev->fw_version.describe);
        status = BLADERF_ERR_UNSUPPORTED;
    }

    return status;
}


/* USB backend that used legacy format for communicating with NIOS II */
const struct backend_fns backend_fns_usb_legacy = {
    FIELD_INIT(.matches, usb_matches),

    FIELD_INIT(.probe, usb_probe),

    FIELD_INIT(.open, usb_open),
    FIELD_INIT(.close, usb_close),

    FIELD_INIT(.load_fpga, usb_load_fpga),
    FIELD_INIT(.is_fpga_configured, usb_is_fpga_configured),

    FIELD_INIT(.erase_flash_blocks, usb_erase_flash_blocks),
    FIELD_INIT(.read_flash_pages, usb_read_flash_pages),
    FIELD_INIT(.write_flash_pages, usb_write_flash_pages),

    FIELD_INIT(.device_reset, usb_device_reset),
    FIELD_INIT(.jump_to_bootloader, usb_jump_to_bootloader),

    FIELD_INIT(.get_cal, usb_get_cal),
    FIELD_INIT(.get_otp, usb_get_otp),
    FIELD_INIT(.get_device_speed, usb_get_device_speed),

    FIELD_INIT(.config_gpio_write, nios_legacy_config_write),
    FIELD_INIT(.config_gpio_read, nios_legacy_config_read),

    FIELD_INIT(.expansion_gpio_write, nios_legacy_expansion_gpio_write),
    FIELD_INIT(.expansion_gpio_read, nios_legacy_expansion_gpio_read),
    FIELD_INIT(.expansion_gpio_dir_write, nios_legacy_expansion_gpio_dir_write),
    FIELD_INIT(.expansion_gpio_dir_read, nios_legacy_expansion_gpio_dir_read),

    FIELD_INIT(.set_iq_gain_correction, nios_legacy_set_iq_gain_correction),
    FIELD_INIT(.set_iq_phase_correction, nios_legacy_set_iq_phase_correction),
    FIELD_INIT(.get_iq_gain_correction, nios_legacy_get_iq_gain_correction),
    FIELD_INIT(.get_iq_phase_correction, nios_legacy_get_iq_phase_correction),

    FIELD_INIT(.get_timestamp, nios_legacy_get_timestamp),

    FIELD_INIT(.si5338_write, nios_legacy_si5338_write),
    FIELD_INIT(.si5338_read, nios_legacy_si5338_read),

    FIELD_INIT(.lms_write, nios_legacy_lms6_write),
    FIELD_INIT(.lms_read, nios_legacy_lms6_read),

    FIELD_INIT(.vctcxo_dac_write, nios_legacy_vctcxo_trim_dac_write),
    FIELD_INIT(.vctcxo_dac_read, nios_vctcxo_trim_dac_read),

    FIELD_INIT(.set_vctcxo_tamer_mode, set_vctcxo_tamer_mode_unsupported),
    FIELD_INIT(.get_vctcxo_tamer_mode, get_vctcxo_tamer_mode_unsupported),

    FIELD_INIT(.xb_spi, nios_legacy_xb200_synth_write),

    FIELD_INIT(.set_firmware_loopback, usb_set_firmware_loopback),
    FIELD_INIT(.get_firmware_loopback, usb_get_firmware_loopback),

    FIELD_INIT(.enable_module, usb_enable_module),

    FIELD_INIT(.init_stream, usb_init_stream),
    FIELD_INIT(.stream, usb_stream),
    FIELD_INIT(.submit_stream_buffer, usb_submit_stream_buffer),
    FIELD_INIT(.deinit_stream, usb_deinit_stream),

    FIELD_INIT(.retune, nios_retune),

    FIELD_INIT(.load_fw_from_bootloader, usb_load_fw_from_bootloader),

    FIELD_INIT(.read_fw_log, usb_read_fw_log),
};

/* USB backend for use with FPGA supporting update NIOS II packet formats */
const struct backend_fns backend_fns_usb = {
    FIELD_INIT(.matches, usb_matches),

    FIELD_INIT(.probe, usb_probe),

    FIELD_INIT(.open, usb_open),
    FIELD_INIT(.close, usb_close),

    FIELD_INIT(.load_fpga, usb_load_fpga),
    FIELD_INIT(.is_fpga_configured, usb_is_fpga_configured),

    FIELD_INIT(.erase_flash_blocks, usb_erase_flash_blocks),
    FIELD_INIT(.read_flash_pages, usb_read_flash_pages),
    FIELD_INIT(.write_flash_pages, usb_write_flash_pages),

    FIELD_INIT(.device_reset, usb_device_reset),
    FIELD_INIT(.jump_to_bootloader, usb_jump_to_bootloader),

    FIELD_INIT(.get_cal, usb_get_cal),
    FIELD_INIT(.get_otp, usb_get_otp),
    FIELD_INIT(.get_device_speed, usb_get_device_speed),

    FIELD_INIT(.config_gpio_write, nios_config_write),
    FIELD_INIT(.config_gpio_read, nios_config_read),

    FIELD_INIT(.expansion_gpio_write, nios_expansion_gpio_write),
    FIELD_INIT(.expansion_gpio_read, nios_expansion_gpio_read),
    FIELD_INIT(.expansion_gpio_dir_write, nios_expansion_gpio_dir_write),
    FIELD_INIT(.expansion_gpio_dir_read, nios_expansion_gpio_dir_read),

    FIELD_INIT(.set_iq_gain_correction, nios_set_iq_gain_correction),
    FIELD_INIT(.set_iq_phase_correction, nios_set_iq_phase_correction),
    FIELD_INIT(.get_iq_gain_correction, nios_get_iq_gain_correction),
    FIELD_INIT(.get_iq_phase_correction, nios_get_iq_phase_correction),

    FIELD_INIT(.get_timestamp, nios_get_timestamp),

    FIELD_INIT(.si5338_write, nios_si5338_write),
    FIELD_INIT(.si5338_read, nios_si5338_read),

    FIELD_INIT(.lms_write, nios_lms6_write),
    FIELD_INIT(.lms_read, nios_lms6_read),

    FIELD_INIT(.vctcxo_dac_write, nios_vctcxo_trim_dac_write),
    FIELD_INIT(.vctcxo_dac_read, nios_vctcxo_trim_dac_read),

    FIELD_INIT(.set_vctcxo_tamer_mode, nios_set_vctcxo_tamer_mode),
    FIELD_INIT(.get_vctcxo_tamer_mode, nios_get_vctcxo_tamer_mode),

    FIELD_INIT(.xb_spi, nios_xb200_synth_write),

    FIELD_INIT(.set_firmware_loopback, usb_set_firmware_loopback),
    FIELD_INIT(.get_firmware_loopback, usb_get_firmware_loopback),

    FIELD_INIT(.enable_module, usb_enable_module),

    FIELD_INIT(.init_stream, usb_init_stream),
    FIELD_INIT(.stream, usb_stream),
    FIELD_INIT(.submit_stream_buffer, usb_submit_stream_buffer),
    FIELD_INIT(.deinit_stream, usb_deinit_stream),

    FIELD_INIT(.retune, nios_retune),

    FIELD_INIT(.load_fw_from_bootloader, usb_load_fw_from_bootloader),

    FIELD_INIT(.read_fw_log, usb_read_fw_log),
};
