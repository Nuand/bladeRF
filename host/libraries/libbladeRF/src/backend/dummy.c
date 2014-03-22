/*
 * Dummy backend to allow libbladeRF to build when no other backends are
 * disabled. This is intended for development purposes only, and should
 * generally should not be enabled for libbladeRF releases.
 *
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
#include "bladerf_priv.h"

static int dummy_is_fpga_configured(struct bladerf *dev)
{
    return 0;
}

int dummy_enable_module(struct bladerf *dev, bladerf_module m, bool enable)
{
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

static int dummy_erase_flash(struct bladerf *dev, uint32_t addr, uint32_t len)
{
    return 0;

}

static int dummy_get_cal(struct bladerf *dev, char *cal) {
    return 0;
}

static int dummy_read_flash(struct bladerf *dev, uint32_t addr,
                           uint8_t *buf, uint32_t len)
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

int dummy_set_correction(struct bladerf *dev, bladerf_module module,
                          bladerf_correction corr, int16_t value)
{
    return 0;
}

int dummy_get_correction(struct bladerf *dev, bladerf_module module,
                          bladerf_correction corr, int16_t *value)
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

static int dummy_submit_stream_buffer(struct bladerf_stream *stream,
                                      void *buffer,
                                      unsigned int timeout_ms)
{
    return 0;
}

void dummy_deinit_stream(struct bladerf_stream *stream)
{
    return;
}

/* We never "find" dummy devices */
int dummy_probe(struct bladerf_devinfo_list *info_list)
{
    return 0;
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
    FIELD_INIT(.get_correction, dummy_get_correction),

    FIELD_INIT(.si5338_write, dummy_si5338_write),
    FIELD_INIT(.si5338_read, dummy_si5338_read),

    FIELD_INIT(.lms_write, dummy_lms_write),
    FIELD_INIT(.lms_read, dummy_lms_read),

    FIELD_INIT(.dac_write, dummy_dac_write),

    FIELD_INIT(.enable_module, dummy_enable_module),

    FIELD_INIT(.init_stream, dummy_stream_init),
    FIELD_INIT(.stream, dummy_stream),
    FIELD_INIT(.submit_stream_buffer, dummy_submit_stream_buffer),
    FIELD_INIT(.deinit_stream, dummy_deinit_stream),
};
