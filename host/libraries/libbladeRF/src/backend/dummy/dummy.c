/*
 * Dummy backend to allow libbladeRF to build when no other backends are
 * enabled. This is intended for development purposes only, and should
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

#include "backend/backend.h"

#include "board/board.h"

static bool dummy_matches(bladerf_backend backend)
{
    return false;
}

/* We never "find" dummy devices */
static int dummy_probe(backend_probe_target probe_target,
                       struct bladerf_devinfo_list *info_list)
{
    return 0;
}

static int dummy_get_vid_pid(struct bladerf *dev, uint16_t *vid, uint16_t *pid)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int dummy_open(struct bladerf *dev, struct bladerf_devinfo *info)
{
    return BLADERF_ERR_NODEV;
}

static int dummy_set_fpga_protocol(struct bladerf *dev, backend_fpga_protocol fpga_protocol)
{
    return 0;
}

static void dummy_close(struct bladerf *dev)
{
    /* Nothing to do */
}

static int dummy_is_fw_ready(struct bladerf *dev)
{
    return 0;
}

static int dummy_get_flash_id(struct bladerf *dev, uint8_t *mid, uint8_t *did)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int dummy_load_fpga(struct bladerf *dev, const uint8_t *image, size_t image_size)
{
    return 0;
}

static int dummy_is_fpga_configured(struct bladerf *dev)
{
    return 0;
}

static int dummy_get_fw_version(struct bladerf *dev,
                                struct bladerf_version *version)
{
    return 0;
}

static int dummy_get_fpga_version(struct bladerf *dev,
                                  struct bladerf_version *version)
{
    return 0;
}

static int dummy_erase_flash_blocks(struct bladerf *dev,
                                    uint32_t eb, uint16_t count)
{
    return 0;
}

static int dummy_read_flash_pages(struct bladerf *dev, uint8_t *buf,
                                  uint32_t page, uint32_t count)
{
    return 0;
}

static int dummy_write_flash_pages(struct bladerf *dev, const uint8_t *buf,
                                   uint32_t page, uint32_t count)
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

static int dummy_get_cal(struct bladerf *dev, char *cal)
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

static int dummy_expansion_gpio_write(struct bladerf *dev,
                                      uint32_t mask, uint32_t val)
{
    return 0;
}

static int dummy_expansion_gpio_read(struct bladerf *dev, uint32_t *val)
{
    return 0;
}

static int dummy_expansion_gpio_dir_write(struct bladerf *dev,
                                          uint32_t mask, uint32_t val)
{
    return 0;
}

static int dummy_expansion_gpio_dir_read(struct bladerf *dev, uint32_t *val)
{
    return 0;
}

static int dummy_set_iq_gain_correction(struct bladerf *dev,
                                        bladerf_channel ch,
                                        int16_t value)
{
    return 0;
}

static int dummy_set_iq_phase_correction(struct bladerf *dev,
                                         bladerf_channel ch,
                                         int16_t value)
{
    return 0;
}

static int dummy_get_iq_gain_correction(struct bladerf *dev,
                                        bladerf_channel ch,
                                        int16_t *value)
{
    *value = 0;
    return 0;
}

static int dummy_get_iq_phase_correction(struct bladerf *dev,
                                         bladerf_channel ch,
                                         int16_t *value)
{
    *value = 0;
    return 0;
}

static int dummy_get_timestamp(struct bladerf *dev, bladerf_direction dir,
                               uint64_t *val)
{
    return 0;
}

static int dummy_si5338_read(struct bladerf *dev, uint8_t addr, uint8_t *data)
{
    return 0;
}

static int dummy_si5338_write(struct bladerf *dev, uint8_t addr, uint8_t data)
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

static int dummy_ina219_write(struct bladerf *dev, uint8_t cmd, uint16_t data)
{
    return 0;
}

static int dummy_ina219_read(struct bladerf *dev, uint8_t addr, uint16_t *data)
{
    return 0;
}

static int dummy_ad9361_spi_write(struct bladerf *dev, uint16_t cmd, uint64_t data)
{
    return 0;
}

static int dummy_ad9361_spi_read(struct bladerf *dev, uint16_t cmd, uint64_t *data)
{
    return 0;
}

static int dummy_adi_axi_write(struct bladerf *dev, uint32_t addr, uint32_t data)
{
    return 0;
}

static int dummy_adi_axi_read(struct bladerf *dev, uint32_t addr, uint32_t *data)
{
    return 0;
}

static int dummy_rffe_control_write(struct bladerf *dev, uint32_t value)
{
    return 0;
}

static int dummy_rffe_control_read(struct bladerf *dev, uint32_t *value)
{
    return 0;
}

static int dummy_rffe_fastlock_save(struct bladerf *dev, bool is_tx,
                                    uint8_t rffe_profile, uint16_t nios_profile)
{
    return 0;
}

static int dummy_ad56x1_vctcxo_trim_dac_write(struct bladerf *dev, uint16_t value)
{
    return 0;
}

static int dummy_ad56x1_vctcxo_trim_dac_read(struct bladerf *dev, uint16_t *value)
{
    return 0;
}

static int dummy_adf400x_write(struct bladerf *dev, uint8_t addr, uint32_t data)
{
    return 0;
}

static int dummy_adf400x_read(struct bladerf *dev, uint8_t addr, uint32_t *data)
{
    return 0;
}

static int dummy_vctcxo_dac_write(struct bladerf *dev, uint8_t addr, uint16_t value)
{
    return 0;
}

static int dummy_vctcxo_dac_read(struct bladerf *dev, uint8_t addr, uint16_t *value)
{
    return 0;
}

static int dummy_set_vctcxo_tamer_mode(struct bladerf *dev,
                                       bladerf_vctcxo_tamer_mode mode)
{
    return 0;
}

static int dummy_get_vctcxo_tamer_mode(struct bladerf *dev,
                                       bladerf_vctcxo_tamer_mode *mode)
{
    *mode = BLADERF_VCTCXO_TAMER_INVALID;
    return 0;
}

static int dummy_xb_spi(struct bladerf *dev, uint32_t value)
{
    return 0;
}

static int dummy_set_firmware_loopback(struct bladerf *dev, bool enable)
{
    return 0;
}

static int dummy_get_firmware_loopback(struct bladerf *dev, bool *is_enabled)
{
    *is_enabled = false;
    return 0;
}

static int dummy_enable_module(struct bladerf *dev, bladerf_direction dir,
                               bool enable)
{
    return 0;
}

static int dummy_init_stream(struct bladerf_stream *stream, size_t num_transfers)
{
    return 0;
}

static int dummy_stream(struct bladerf_stream *stream, bladerf_channel_layout layout)
{
    return 0;
}

static int dummy_submit_stream_buffer(struct bladerf_stream *stream,
                                      void *buffer,
                                      unsigned int timeout_ms,
                                      bool nonblock)
{
    return 0;
}

static void dummy_deinit_stream(struct bladerf_stream *stream)
{
    return;
}

static int dummy_retune(struct bladerf *dev, bladerf_channel ch,
                        uint64_t timestamp, uint16_t nint, uint32_t nfrac,
                        uint8_t  freqsel, uint8_t vcocap, bool low_band,
                        bool quick_tune)
{
    return 0;
}


static int dummy_load_fw_from_bootloader(bladerf_backend backend,
                                         uint8_t bus, uint8_t addr,
                                         struct fx3_firmware *fw)
{
    return 0;
}

static int dummy_read_fw_log(struct bladerf *dev, logger_entry *e)
{
    *e = LOG_EOF;
    return 0;
}

static int dummy_read_trigger(struct bladerf *dev, bladerf_channel ch,
                       bladerf_trigger_signal trigger, uint8_t * value)
{
    *value = 0;
    return 0;
}

static int dummy_write_trigger(struct bladerf *dev, bladerf_channel ch,
                        bladerf_trigger_signal trigger, uint8_t value)
{
    return 0;
}

const struct backend_fns backend_fns_dummy = {
    FIELD_INIT(.matches, dummy_matches),

    FIELD_INIT(.probe, dummy_probe),

    FIELD_INIT(.get_vid_pid, dummy_get_vid_pid),
    FIELD_INIT(.get_flash_id, dummy_get_flash_id),
    FIELD_INIT(.open, dummy_open),
    FIELD_INIT(.set_fpga_protocol, dummy_set_fpga_protocol),
    FIELD_INIT(.close, dummy_close),

    FIELD_INIT(.is_fw_ready, dummy_is_fw_ready),

    FIELD_INIT(.load_fpga, dummy_load_fpga),
    FIELD_INIT(.is_fpga_configured, dummy_is_fpga_configured),

    FIELD_INIT(.get_fw_version, dummy_get_fw_version),
    FIELD_INIT(.get_fpga_version, dummy_get_fpga_version),

    FIELD_INIT(.erase_flash_blocks, dummy_erase_flash_blocks),
    FIELD_INIT(.read_flash_pages, dummy_read_flash_pages),
    FIELD_INIT(.write_flash_pages, dummy_write_flash_pages),

    FIELD_INIT(.device_reset, dummy_device_reset),
    FIELD_INIT(.jump_to_bootloader, dummy_jump_to_bootloader),

    FIELD_INIT(.get_cal, dummy_get_cal),
    FIELD_INIT(.get_otp, dummy_get_otp),
    FIELD_INIT(.get_device_speed, dummy_get_device_speed),

    FIELD_INIT(.config_gpio_write, dummy_config_gpio_write),
    FIELD_INIT(.config_gpio_read, dummy_config_gpio_read),

    FIELD_INIT(.expansion_gpio_write, dummy_expansion_gpio_write),
    FIELD_INIT(.expansion_gpio_read, dummy_expansion_gpio_read),
    FIELD_INIT(.expansion_gpio_dir_write, dummy_expansion_gpio_dir_write),
    FIELD_INIT(.expansion_gpio_dir_read, dummy_expansion_gpio_dir_read),

    FIELD_INIT(.set_iq_gain_correction, dummy_set_iq_gain_correction),
    FIELD_INIT(.set_iq_phase_correction, dummy_set_iq_phase_correction),
    FIELD_INIT(.get_iq_gain_correction, dummy_get_iq_gain_correction),
    FIELD_INIT(.get_iq_phase_correction, dummy_get_iq_phase_correction),

    FIELD_INIT(.get_timestamp, dummy_get_timestamp),

    FIELD_INIT(.si5338_write, dummy_si5338_write),
    FIELD_INIT(.si5338_read, dummy_si5338_read),

    FIELD_INIT(.lms_write, dummy_lms_write),
    FIELD_INIT(.lms_read, dummy_lms_read),

    FIELD_INIT(.ina219_write, dummy_ina219_write),
    FIELD_INIT(.ina219_read, dummy_ina219_read),

    FIELD_INIT(.ad9361_spi_write, dummy_ad9361_spi_write),
    FIELD_INIT(.ad9361_spi_read, dummy_ad9361_spi_read),

    FIELD_INIT(.adi_axi_write, dummy_adi_axi_write),
    FIELD_INIT(.adi_axi_read, dummy_adi_axi_read),

    FIELD_INIT(.rffe_control_write, dummy_rffe_control_write),
    FIELD_INIT(.rffe_control_read, dummy_rffe_control_read),

    FIELD_INIT(.rffe_fastlock_save, dummy_rffe_fastlock_save),

    FIELD_INIT(.ad56x1_vctcxo_trim_dac_write, dummy_ad56x1_vctcxo_trim_dac_write),
    FIELD_INIT(.ad56x1_vctcxo_trim_dac_read, dummy_ad56x1_vctcxo_trim_dac_read),

    FIELD_INIT(.adf400x_write, dummy_adf400x_write),
    FIELD_INIT(.adf400x_read, dummy_adf400x_read),

    FIELD_INIT(.vctcxo_dac_write, dummy_vctcxo_dac_write),
    FIELD_INIT(.vctcxo_dac_read,  dummy_vctcxo_dac_read),

    FIELD_INIT(.set_vctcxo_tamer_mode, dummy_set_vctcxo_tamer_mode),
    FIELD_INIT(.get_vctcxo_tamer_mode, dummy_get_vctcxo_tamer_mode),

    FIELD_INIT(.xb_spi, dummy_xb_spi),

    FIELD_INIT(.set_firmware_loopback, dummy_set_firmware_loopback),
    FIELD_INIT(.get_firmware_loopback, dummy_get_firmware_loopback),

    FIELD_INIT(.enable_module, dummy_enable_module),

    FIELD_INIT(.init_stream, dummy_init_stream),
    FIELD_INIT(.stream, dummy_stream),
    FIELD_INIT(.submit_stream_buffer, dummy_submit_stream_buffer),
    FIELD_INIT(.deinit_stream, dummy_deinit_stream),

    FIELD_INIT(.retune, dummy_retune),

    FIELD_INIT(.load_fw_from_bootloader, dummy_load_fw_from_bootloader),

    FIELD_INIT(.read_fw_log, dummy_read_fw_log),

    FIELD_INIT(.read_trigger, dummy_read_trigger),
    FIELD_INIT(.write_trigger, dummy_write_trigger),

    FIELD_INIT(.name, "dummy"),
};
