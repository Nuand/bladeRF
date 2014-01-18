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
#include "backend/dummy.h"
#include "conversions.h"
#include "minmax.h"
#include "log.h"
#include "flash.h"

struct bladerf_dummy {
    void           *dev;
};

const struct bladerf_fn bladerf_dummy_fn;

struct dummy_stream_data {
    int  completed;
};

/* Quick wrapper for vendor commands that get/send a 32-bit integer value */
static int vendor_command_int(struct bladerf *dev,
                          uint8_t cmd, uint8_t ep_dir, int32_t *val)
{
    return 0;
}

/* Quick wrapper for vendor commands that get/send a 32-bit integer value + wValue */
static int vendor_command_int_value(struct bladerf *dev,
                          uint8_t cmd, uint16_t wValue, int32_t *val)
{
    return 0;

}

/* Quick wrapper for vendor commands that get/send a 32-bit integer value + wIndex */
static int vendor_command_int_index(struct bladerf *dev,
                          uint8_t cmd, uint16_t wIndex, int32_t *val)
{
    return 0;
}

static int begin_fpga_programming(struct bladerf *dev)
{
    return 0;
}

static int end_fpga_programming(struct bladerf *dev)
{
    return 0;
}

static int dummy_is_fpga_configured(struct bladerf *dev)
{
    return 0;
}


static int change_setting(struct bladerf *dev, uint8_t setting)
{
    return 0;
}

/* After performing a flash operation, switch back to either RF_LINK or the
 * FPGA loader.
 */
static int restore_post_flash_setting(struct bladerf *dev)
{
    return 0;
}

int dummy_enable_module(struct bladerf *dev, bladerf_module m, bool enable) 
{
    return 0;
}



/* FW < 1.5.0  does not have version strings */
static int dummy_fw_populate_version_legacy(struct bladerf *dev)
{
    return 0;
}

static int dummy_populate_fw_version(struct bladerf *dev)
{
    return 0;
}

/* Returns BLADERF_ERR_* on failure */
static int access_peripheral(struct bladerf_dummy *dummy, int per, int dir,
                                struct uart_cmd *cmd)
{
    return 0;
}

static int dummy_fpga_version_read(struct bladerf *dev, uint32_t *version) {
    return 0;
}


static int dummy_populate_fpga_version(struct bladerf *dev)
{
    return 0;
}

static int enable_rf(struct bladerf *dev) {
    return 0;
}

static int dummy_open(struct bladerf **device, struct bladerf_devinfo *info)
{
    return BLADERF_ERR_NODEV;
}

static int dummy_close(struct bladerf *dev)
{
    return 0;
}

static int dummy_load_fpga(struct bladerf *dev, uint8_t *image, size_t image_size)
{
    return 0;
}

static int erase_sector(struct bladerf *dev, uint16_t sector)
{
    return 0;
}

static int dummy_erase_flash(struct bladerf *dev, uint32_t addr, uint32_t len)
{
    return 0;

}

static int read_buffer(struct bladerf *dev, uint8_t request,
                       uint8_t *buf, uint16_t len)
{
    return 0;
}

static int read_page_buffer(struct bladerf *dev, uint8_t *buf)
{
    return 0;
}

static int dummy_get_cal(struct bladerf *dev, char *cal) {
    return 0;
}

static int legacy_read_one_page(struct bladerf *dev,
                                uint16_t page,
                                uint8_t *buf)
{
    return 0;
}

/* Assumes the device is already configured for USB_IF_SPI_FLASH */
static int read_one_page(struct bladerf *dev, uint16_t page, uint8_t *buf)
{
    return 0;
}

static int dummy_read_flash(struct bladerf *dev, uint32_t addr,
                           uint8_t *buf, uint32_t len)
{
    return 0;
}

static int compare_page_buffers(uint8_t *page_buf, uint8_t *image_page)
{
    return 0;
}

static int verify_one_page(struct bladerf *dev,
                           uint16_t page, uint8_t *image_buf)
{
    return 0;
}

static int verify_flash(struct bladerf *dev, uint32_t addr,
                        uint8_t *image, uint32_t len)
{
    return 0;
}

static int write_buffer(struct bladerf *dev,
                        uint8_t request,
                        uint16_t page,
                        uint8_t *buf)
{
    return 0;
}

static int write_one_page(struct bladerf *dev, uint16_t page, uint8_t *buf)
{
    return 0;
}

static int dummy_write_flash(struct bladerf *dev, uint32_t addr,
                        uint8_t *buf, uint32_t len)
{
    return 0;
}

static int dummy_flash_firmware(struct bladerf *dev,
                               uint8_t *image, size_t image_size)
{
    return 0;
}

static int dummy_device_reset(struct bladerf *dev)
{
    return 0;
}

static int dummy_jump_to_bootloader(struct bladerf *dev)
{
    return 0;
}

static int dummy_get_otp(struct bladerf *dev, char *otp)
{
    return 0;
}

static int dummy_get_device_speed(struct bladerf *dev,
                                 bladerf_dev_speed *device_speed)
{
    return 0;
}

static int dummy_config_gpio_write(struct bladerf *dev, uint32_t val)
{
    return 0;
}

static int dummy_config_gpio_read(struct bladerf *dev, uint32_t *val)
{
    return 0;
}

static int dummy_si5338_write(struct bladerf *dev, uint8_t addr, uint8_t data)
{
    return 0;
}

static int dummy_si5338_read(struct bladerf *dev, uint8_t addr, uint8_t *data)
{
    return 0;
}

static int dummy_lms_write(struct bladerf *dev, uint8_t addr, uint8_t data)
{
    return 0;
}

static int dummy_lms_read(struct bladerf *dev, uint8_t addr, uint8_t *data)
{
    return 0;
}

static int dummy_dac_write(struct bladerf *dev, uint16_t value)
{
    return 0;
}

static int set_fpga_correction(struct bladerf *dev, uint8_t addr, int16_t value)
{
    return 0;
}

static int dummy_set_correction(struct bladerf *dev, bladerf_correction_module module, int16_t value)
{
    return 0;
}


static int print_fpga_correction(struct bladerf *dev, uint8_t addr, int16_t *value)
{
    return 0;
}

static int dummy_print_correction(struct bladerf *dev, bladerf_correction_module module, int16_t *value)
{
    return 0;
}

static int dummy_tx(struct bladerf *dev, bladerf_format format, void *samples,
                   int n, struct bladerf_metadata *metadata)
{
    return 0;
}

static int dummy_rx(struct bladerf *dev, bladerf_format format, void *samples,
                   int n, struct bladerf_metadata *metadata)
{
    return 0;
}


static int dummy_stream_init(struct bladerf_stream *stream)
{
    return 0;
}

static int dummy_stream(struct bladerf_stream *stream, bladerf_module module)
{
    return 0;
}

void dummy_deinit_stream(struct bladerf_stream *stream)
{
    return;
}

void dummy_set_transfer_timeout(struct bladerf *dev, bladerf_module module, int timeout) {
}

int dummy_get_transfer_timeout(struct bladerf *dev, bladerf_module module) {
  return 0;
}

int dummy_probe(struct bladerf_devinfo_list *info_list)
{
  return 0;
}

static int dummy_get_stats(struct bladerf *dev, struct bladerf_stats *stats)
{
    /* TODO Implementation requires FPGA support */
    return BLADERF_ERR_UNSUPPORTED;
}

const struct bladerf_fn bladerf_dummy_fn = {
    FIELD_INIT(.probe, dummy_probe),

    FIELD_INIT(.open, dummy_open),
    FIELD_INIT(.close, dummy_close),

    FIELD_INIT(.load_fpga, dummy_load_fpga),
    FIELD_INIT(.is_fpga_configured, dummy_is_fpga_configured),

    FIELD_INIT(.flash_firmware, dummy_flash_firmware),

    FIELD_INIT(.erase_flash, dummy_erase_flash),
    FIELD_INIT(.read_flash, dummy_read_flash),
    FIELD_INIT(.write_flash, dummy_write_flash),
    FIELD_INIT(.device_reset, dummy_device_reset),
    FIELD_INIT(.jump_to_bootloader, dummy_jump_to_bootloader),

    FIELD_INIT(.get_cal, dummy_get_cal),
    FIELD_INIT(.get_otp, dummy_get_otp),
    FIELD_INIT(.get_device_speed, dummy_get_device_speed),

    FIELD_INIT(.config_gpio_write, dummy_config_gpio_write),
    FIELD_INIT(.config_gpio_read, dummy_config_gpio_read),

    FIELD_INIT(.set_correction, dummy_set_correction),
    FIELD_INIT(.print_correction, dummy_print_correction),

    FIELD_INIT(.si5338_write, dummy_si5338_write),
    FIELD_INIT(.si5338_read, dummy_si5338_read),

    FIELD_INIT(.lms_write, dummy_lms_write),
    FIELD_INIT(.lms_read, dummy_lms_read),

    FIELD_INIT(.dac_write, dummy_dac_write),

    FIELD_INIT(.enable_module, dummy_enable_module),
    FIELD_INIT(.rx, dummy_rx),
    FIELD_INIT(.tx, dummy_tx),

    FIELD_INIT(.init_stream, dummy_stream_init),
    FIELD_INIT(.stream, dummy_stream),
    FIELD_INIT(.deinit_stream, dummy_deinit_stream),

    FIELD_INIT(.set_transfer_timeout, dummy_set_transfer_timeout),
    FIELD_INIT(.get_transfer_timeout, dummy_get_transfer_timeout),

    FIELD_INIT(.stats, dummy_get_stats)
};
