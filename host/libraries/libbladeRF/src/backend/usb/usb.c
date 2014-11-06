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

#include "rel_assert.h"
#include "bladerf_priv.h"
#include "backend/backend.h"
#include "backend/backend_config.h"
#include "backend/usb/usb.h"
#include "async.h"
#include "bladeRF.h"    /* Firmware interface */
#include "log.h"
#include "version_compat.h"

typedef enum {
    CORR_INVALID,
    CORR_FPGA,
    CORR_LMS
} corr_type;

static const struct usb_driver *usb_driver_list[] = BLADERF_USB_BACKEND_LIST;

struct backend_fns_usb;

static inline struct bladerf_usb *usb_backend(struct bladerf *dev, void **driver)
{
    struct bladerf_usb *ret = (struct bladerf_usb*)dev->backend;
    if (driver) {
        *driver = ret->driver;
    }
    return ret;
}


static int access_peripheral(struct bladerf *dev, uint8_t peripheral,
                             usb_direction dir, struct uart_cmd *cmd,
                             size_t len)
{
    void *driver;
    struct bladerf_usb *usb = usb_backend(dev, &driver);

    int status;
    size_t i;
    uint8_t buf[16] = { 0 };
    const uint8_t pkt_mode_dir = (dir == USB_DIR_HOST_TO_DEVICE) ?
                        UART_PKT_MODE_DIR_WRITE : UART_PKT_MODE_DIR_READ;

    assert(len <= ((sizeof(buf) - 2) / 2));

    /* Populate the buffer for transfer */
    buf[0] = UART_PKT_MAGIC;
    buf[1] = pkt_mode_dir | peripheral | (uint8_t)len;

    for (i = 0; i < len; i++) {
        buf[i * 2 + 2] = cmd[i].addr;
        buf[i * 2 + 3] = cmd[i].data;
    }

    /* Send the command */
    status = usb->fn->bulk_transfer(driver, PERIPHERAL_EP_OUT,
                                     buf, sizeof(buf),
                                     PERIPHERAL_TIMEOUT_MS);
    if (status != 0) {
        log_debug("Failed to write perperial access command: %s\n",
                  bladerf_strerror(status));
        return status;
    }

    /* Read back the ACK. The command data is only used for a read operation,
     * and is thrown away otherwise */
    status = usb->fn->bulk_transfer(driver, PERIPHERAL_EP_IN,
                                    buf, sizeof(buf),
                                    PERIPHERAL_TIMEOUT_MS);

    if (dir == UART_PKT_MODE_DIR_READ && status == 0) {
        for (i = 0; i < len; i++) {
            cmd[i].data = buf[i * 2 + 3];
        }
    }

    return status;
}

static inline int gpio_read(struct bladerf *dev, uint8_t addr, uint32_t *data)
{
    int status;
    size_t i;
    struct uart_cmd cmd;

    *data = 0;

    for (i = 0; i < sizeof(*data); i++) {
        assert((addr + i) <= UINT8_MAX);
        cmd.addr = (uint8_t)(addr + i);
        cmd.data = 0xff;

        status = access_peripheral(dev, UART_PKT_DEV_GPIO,
                                   USB_DIR_DEVICE_TO_HOST, &cmd, 1);

        if (status < 0) {
            return status;
        }

        *data |= (cmd.data << (i * 8));
    }

    return 0;
}

static inline int gpio_write(struct bladerf *dev, uint8_t addr, uint32_t data)
{
    int status;
    size_t i;
    struct uart_cmd cmd;

    for (i = 0; i < sizeof(data); i++) {
        assert((addr + i) <= UINT8_MAX);
        cmd.addr = (uint8_t)(addr + i);
        cmd.data = (data >> (i * 8)) & 0xff;

        status = access_peripheral(dev, UART_PKT_DEV_GPIO,
                                   USB_DIR_HOST_TO_DEVICE, &cmd, 1);

        if (status < 0) {
            return status;
        }
    }

    return 0;
}

static int load_fpga_version(struct bladerf *dev)
{
    int i, status;
    struct uart_cmd cmd;

    for (i = 0; i < 4; i++) {
        cmd.addr = UART_PKT_DEV_FGPA_VERSION_ID + i;
        cmd.data = 0xff;

        status = access_peripheral(dev, UART_PKT_DEV_GPIO,
                                   USB_DIR_DEVICE_TO_HOST, &cmd, 1);

        if (status != 0) {
            memset(&dev->fpga_version, 0, sizeof(dev->fpga_version));
            log_debug("Failed to read FPGA version[%d]: %s\n", i,
                        bladerf_strerror(status));
            return status;
        }

        switch (i) {
            case 0:
                dev->fpga_version.major = cmd.data;
                break;
            case 1:
                dev->fpga_version.minor = cmd.data;
                break;
            case 2:
                dev->fpga_version.patch = cmd.data;
                break;
            case 3:
                dev->fpga_version.patch |= (cmd.data << 8);
                break;
            default:
                assert(!"Bug");
        }
    }

    snprintf((char*)dev->fpga_version.describe, BLADERF_VERSION_STR_MAX,
             "%d.%d.%d", dev->fpga_version.major, dev->fpga_version.minor,
             dev->fpga_version.patch);

    return status;
}

/* Vendor command wrapper to gets a 32-bit integer and supplies a wIndex */
static inline int vendor_cmd_int_windex(struct bladerf *dev, uint8_t cmd,
                                        uint16_t windex, int32_t *val)
{
    void *driver;
    struct bladerf_usb *usb = usb_backend(dev, &driver);

    return usb->fn->control_transfer(driver,
                                      USB_TARGET_INTERFACE,
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
                                      USB_TARGET_INTERFACE,
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
                                      USB_TARGET_INTERFACE,
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

static inline int rflink_and_fpga_version_load(struct bladerf *dev)
{
    int status = change_setting(dev, USB_IF_RF_LINK);
    if (status == 0) {
        /* Read and store FPGA version info. This is only possible after
         * we've entered RF link mode */
        status = load_fpga_version(dev);
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

static int usb_probe(struct bladerf_devinfo_list *info_list)
{
    int status;
    size_t i;

    for (i = status = 0; i < ARRAY_SIZE(usb_driver_list); i++) {
        status = usb_driver_list[i]->fn->probe(info_list);
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

    dev->fn = &backend_fns_usb;
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

    /* Wait for SPI flash autoloading to complete, if needed */
    if (version_greater_or_equal(&dev->fw_version, 1, 8, 0)) {
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
        status = rflink_and_fpga_version_load(dev);
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

    return rflink_and_fpga_version_load(dev);
}

static inline int perform_erase(struct bladerf *dev, uint16_t block)
{
    int status, erase_ret;
    void *driver;
    struct bladerf_usb *usb = usb_backend(dev, &driver);

    status = usb->fn->control_transfer(driver,
                                        USB_TARGET_INTERFACE,
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
                                           USB_TARGET_INTERFACE,
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
                                            USB_TARGET_INTERFACE,
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

    return usb->fn->control_transfer(driver, USB_TARGET_INTERFACE,
                                      USB_REQUEST_VENDOR,
                                      USB_DIR_HOST_TO_DEVICE,
                                      BLADE_USB_CMD_RESET,
                                      0, 0, 0, 0, CTRL_TIMEOUT_MS);

}

static int usb_jump_to_bootloader(struct bladerf *dev)
{
    void *driver;
    struct bladerf_usb *usb = usb_backend(dev, &driver);

    return usb->fn->control_transfer(driver, USB_TARGET_INTERFACE,
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

static int usb_config_gpio_write(struct bladerf *dev, uint32_t val)
{
    return gpio_write(dev, 0, val);
}

static int usb_config_gpio_read(struct bladerf *dev, uint32_t *val)
{
    return gpio_read(dev, 0, val);
}

static int usb_expansion_gpio_write(struct bladerf *dev, uint32_t val)
{
    return gpio_write(dev, 40, val);
}

static int usb_expansion_gpio_read(struct bladerf *dev, uint32_t *val)
{
    return gpio_read(dev, 40, val);
}

static int usb_expansion_gpio_dir_write(struct bladerf *dev, uint32_t val)
{
    return gpio_write(dev, 44, val);
}

static int usb_expansion_gpio_dir_read(struct bladerf *dev, uint32_t *val)
{
    return gpio_read(dev, 44, val);
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

static int set_fpga_correction(struct bladerf *dev,
                               bladerf_correction corr,
                               uint8_t addr, int16_t value)
{
    int i;
    int status;
    struct uart_cmd cmd;

    /* If this is a gain correction add in the 1.0 value so 0 correction yields
     * an unscaled gain */
    if (corr == BLADERF_CORR_FPGA_GAIN) {
        value += (int16_t)4096;
    }

    for (i = status = 0; status == 0 && i < 2; i++) {
        cmd.addr = i + addr;

        cmd.data = (value >> (i * 8)) & 0xff;
        status = access_peripheral(dev, UART_PKT_DEV_GPIO,
                                   USB_DIR_HOST_TO_DEVICE, &cmd, 1);
    }

    return status;
}

static int usb_lms_write(struct bladerf *dev, uint8_t addr, uint8_t data)
{
    int status;
    struct uart_cmd cmd;

    cmd.addr = addr;
    cmd.data = data;

    log_verbose( "%s: 0x%2.2x 0x%2.2x\n", __FUNCTION__, addr, data );

    status = access_peripheral(dev, UART_PKT_DEV_LMS,
                               USB_DIR_HOST_TO_DEVICE, &cmd, 1);

    return status;
}

static int usb_lms_read(struct bladerf *dev, uint8_t addr, uint8_t *data)
{
    int status;
    struct uart_cmd cmd;

    cmd.addr = addr;
    cmd.data = 0xff;

    status = access_peripheral(dev, UART_PKT_DEV_LMS,
                               USB_DIR_DEVICE_TO_HOST, &cmd, 1);

    if (status == 0) {
        *data = cmd.data;
        log_verbose( "%s: 0x%2.2x 0x%2.2x\n", __FUNCTION__, addr, *data );
    }

    return status;
}

static int set_lms_correction(struct bladerf *dev, bladerf_module module,
                              uint8_t addr, int16_t value)
{
    int status;
    uint8_t tmp;

    status = usb_lms_read(dev, addr, &tmp);
    if (status != 0) {
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

    status = usb_lms_write(dev, addr, (uint8_t)value);
    if (status < 0) {
        return status;
    }

    return status;
}

static int usb_set_correction(struct bladerf *dev, bladerf_module module,
                              bladerf_correction corr, int16_t value)
{
    int status;
    uint8_t addr;
    corr_type type;

    get_correction_addr_type(module, corr, &addr, &type);

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

    return 0;
}

static int get_fpga_correction(struct bladerf *dev, bladerf_correction corr,
                               uint8_t addr, int16_t *value)
{
    int i;
    int status;
    struct uart_cmd cmd;

    *value = 0;
    for (i = status = 0; status == 0 && i < 2; i++) {
        cmd.addr = i + addr;
        cmd.data = 0xff;
        status = access_peripheral(dev, UART_PKT_DEV_GPIO,
                                   USB_DIR_DEVICE_TO_HOST, &cmd, 1);

        *value |= (cmd.data << (i * 8));
    }

    /* Gain corrections have an offset that needs to be accounted for */
    if (corr == BLADERF_CORR_FPGA_GAIN) {
        *value -= 4096;
    }

    return status;
}

static int get_lms_correction(struct bladerf *dev,
                                bladerf_module module,
                                uint8_t addr, int16_t *value)
{
    uint8_t tmp;
    int status;

    status = usb_lms_read(dev, addr, &tmp);
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
    }

    return status;
}

static int usb_get_correction(struct bladerf *dev, bladerf_module module,
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
            status = get_fpga_correction(dev, corr, addr, value);
            break;

        default:
            status = BLADERF_ERR_INVAL;
    }

    return status;
}

int usb_get_timestamp(struct bladerf *dev, bladerf_module mod, uint64_t *value)
{
    int status = 0;
    struct uart_cmd cmds[4];
    uint8_t timestamp_bytes[8];
    size_t i;

    /* Offset 16 is the time tamer according to the Nios firmware */
    cmds[0].addr = (mod == BLADERF_MODULE_RX ? 16 : 24);
    cmds[1].addr = (mod == BLADERF_MODULE_RX ? 17 : 25);
    cmds[2].addr = (mod == BLADERF_MODULE_RX ? 18 : 26);
    cmds[3].addr = (mod == BLADERF_MODULE_RX ? 19 : 27);
    cmds[0].data = cmds[1].data = cmds[2].data = cmds[3].data = 0xff;

    status = access_peripheral(dev, UART_PKT_DEV_GPIO, USB_DIR_DEVICE_TO_HOST,
                               cmds, ARRAY_SIZE(cmds));
    if (status != 0) {
        return status;
    } else {
        for (i = 0; i < 4; i++) {
            timestamp_bytes[i] = cmds[i].data;
        }
    }

    cmds[0].addr = (mod == BLADERF_MODULE_RX ? 20 : 28);
    cmds[1].addr = (mod == BLADERF_MODULE_RX ? 21 : 29);
    cmds[2].addr = (mod == BLADERF_MODULE_RX ? 22 : 30);
    cmds[3].addr = (mod == BLADERF_MODULE_RX ? 23 : 31);
    cmds[0].data = cmds[1].data = cmds[2].data = cmds[3].data = 0xff;

    status = access_peripheral(dev, UART_PKT_DEV_GPIO, USB_DIR_DEVICE_TO_HOST,
                               cmds, ARRAY_SIZE(cmds));

    if (status) {
        return status;
    } else {
        for (i = 0; i < 4; i++) {
            timestamp_bytes[i + 4] = cmds[i].data;
        }
    }

    memcpy(value, timestamp_bytes, sizeof(*value));
    *value = LE64_TO_HOST(*value);

    return 0;
}

static int usb_si5338_write(struct bladerf *dev, uint8_t addr, uint8_t data)
{
    struct uart_cmd cmd;

    cmd.addr = addr;
    cmd.data = data;

    log_verbose( "%s: 0x%2.2x 0x%2.2x\n", __FUNCTION__, addr, data );

    return access_peripheral(dev, UART_PKT_DEV_SI5338,
                             USB_DIR_HOST_TO_DEVICE, &cmd, 1);
}

static int usb_si5338_read(struct bladerf *dev, uint8_t addr, uint8_t *data)
{
    int status;
    struct uart_cmd cmd;

    cmd.addr = addr;
    cmd.data = 0xff;

    status = access_peripheral(dev, UART_PKT_DEV_SI5338,
                               USB_DIR_DEVICE_TO_HOST, &cmd, 1);

    if (status == 0) {
        *data = cmd.data;
        log_verbose("%s: 0x%2.2x 0x%2.2x\n", __FUNCTION__, addr, *data);
    }

    return status;
}


static int usb_dac_write(struct bladerf *dev, uint16_t value)
{
    int status;
    struct uart_cmd cmd;
    int base;

    /* FPGA v0.0.4 introduced a change to the location of the DAC registers */
    const bool legacy_location = version_less_than(&dev->fpga_version, 0, 0, 4);

    base = legacy_location ? 0 : 34;

    cmd.addr = base;
    cmd.data = value & 0xff;
    status = access_peripheral(dev, legacy_location ? UART_PKT_DEV_VCTCXO : UART_PKT_DEV_GPIO,
                               USB_DIR_HOST_TO_DEVICE, &cmd, 1);

    if (status < 0) {
        return status;
    }

    cmd.addr = base + 1;
    cmd.data = (value >> 8) & 0xff;
    status = access_peripheral(dev, legacy_location ? UART_PKT_DEV_VCTCXO : UART_PKT_DEV_GPIO,
                               USB_DIR_HOST_TO_DEVICE, &cmd, 1);

    return status;
}

static int usb_xb_spi(struct bladerf *dev, uint32_t value)
{
    return gpio_write(dev, 36, value);
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
                             unsigned int timeout_ms)
{
    void *driver;
    struct bladerf_usb *usb = usb_backend(stream->dev, &driver);
    return usb->fn->submit_stream_buffer(driver, stream, buffer, timeout_ms);
}

static void usb_deinit_stream(struct bladerf_stream *stream)
{
    void *driver;
    struct bladerf_usb *usb = usb_backend(stream->dev, &driver);
    usb->fn->deinit_stream(driver, stream);
}

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

    FIELD_INIT(.config_gpio_write, usb_config_gpio_write),
    FIELD_INIT(.config_gpio_read, usb_config_gpio_read),

    FIELD_INIT(.expansion_gpio_write, usb_expansion_gpio_write),
    FIELD_INIT(.expansion_gpio_read, usb_expansion_gpio_read),
    FIELD_INIT(.expansion_gpio_dir_write, usb_expansion_gpio_dir_write),
    FIELD_INIT(.expansion_gpio_dir_read, usb_expansion_gpio_dir_read),

    FIELD_INIT(.set_correction, usb_set_correction),
    FIELD_INIT(.get_correction, usb_get_correction),

    FIELD_INIT(.get_timestamp, usb_get_timestamp),

    FIELD_INIT(.si5338_write, usb_si5338_write),
    FIELD_INIT(.si5338_read, usb_si5338_read),

    FIELD_INIT(.lms_write, usb_lms_write),
    FIELD_INIT(.lms_read, usb_lms_read),

    FIELD_INIT(.dac_write, usb_dac_write),

    FIELD_INIT(.xb_spi, usb_xb_spi),

    FIELD_INIT(.set_firmware_loopback, usb_set_firmware_loopback),
    FIELD_INIT(.get_firmware_loopback, usb_get_firmware_loopback),

    FIELD_INIT(.enable_module, usb_enable_module),

    FIELD_INIT(.init_stream, usb_init_stream),
    FIELD_INIT(.stream, usb_stream),
    FIELD_INIT(.submit_stream_buffer, usb_submit_stream_buffer),
    FIELD_INIT(.deinit_stream, usb_deinit_stream)
};
