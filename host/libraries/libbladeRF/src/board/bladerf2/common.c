/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2018 Nuand LLC
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

#include <string.h>

#include "board/board.h"
#include "capabilities.h"
#include "common.h"


/******************************************************************************/
/* Constants */
/******************************************************************************/

/* Board state to string map */
char const *bladerf2_state_to_string[] = {
    [STATE_UNINITIALIZED]   = "Uninitialized",
    [STATE_FIRMWARE_LOADED] = "Firmware Loaded",
    [STATE_FPGA_LOADED]     = "FPGA Loaded",
    [STATE_INITIALIZED]     = "Initialized",
};


/******************************************************************************/
/* Sample format control */
/******************************************************************************/

static inline int requires_timestamps(bladerf_format format, bool *required)
{
    switch (format) {
        case BLADERF_FORMAT_SC16_Q11_META:
            *required = true;
            break;

        case BLADERF_FORMAT_SC16_Q11:
            *required = false;
            break;

        default:
            return BLADERF_ERR_INVAL;
    }

    return 0;
}

int perform_format_config(struct bladerf *dev,
                          bladerf_direction dir,
                          bladerf_format format)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    struct bladerf2_board_data *board_data = dev->board_data;
    bool use_timestamps, other_using_timestamps;
    bladerf_channel other;
    uint32_t gpio_val;
    int status;

    status = requires_timestamps(format, &use_timestamps);
    if (status != 0) {
        log_debug("%s: Invalid format: %d\n", __FUNCTION__, format);
        return status;
    }

    switch (dir) {
        case BLADERF_RX:
            other = BLADERF_TX;
            break;

        case BLADERF_TX:
            other = BLADERF_RX;
            break;

        default:
            log_debug("Invalid direction: %d\n", dir);
            return BLADERF_ERR_INVAL;
    }

    status = requires_timestamps(board_data->module_format[other],
                                 &other_using_timestamps);
    if ((status == 0) && (other_using_timestamps != use_timestamps)) {
        log_debug("Format conflict detected: RX=%d, TX=%d\n");
        return BLADERF_ERR_INVAL;
    }

    CHECK_STATUS(dev->backend->config_gpio_read(dev, &gpio_val));

    if (use_timestamps) {
        gpio_val |= BLADERF_GPIO_TIMESTAMP;
    } else {
        gpio_val &= ~BLADERF_GPIO_TIMESTAMP;
    }

    CHECK_STATUS(dev->backend->config_gpio_write(dev, gpio_val));

    board_data->module_format[dir] = format;

    return 0;
}

int perform_format_deconfig(struct bladerf *dev, bladerf_direction dir)
{
    struct bladerf2_board_data *board_data = dev->board_data;

    switch (dir) {
        case BLADERF_RX:
        case BLADERF_TX:
            /* We'll reconfigure the HW when we call perform_format_config,
             * so we just need to update our stored information */
            board_data->module_format[dir] = -1;
            break;

        default:
            log_debug("%s: Invalid direction: %d\n", __FUNCTION__, dir);
            return BLADERF_ERR_INVAL;
    }

    return 0;
}


/******************************************************************************/
/* Size checks */
/******************************************************************************/

bool is_valid_fpga_size(struct bladerf *dev, bladerf_fpga_size fpga, size_t len)
{
    /* We do not build FPGAs with compression enabled. Therfore, they
     * will always have a fixed file size.
     */
    char const env_override[] = "BLADERF_SKIP_FPGA_SIZE_CHECK";

    bool valid;
    size_t expected;
    int status;

    status = dev->board->get_fpga_bytes(dev, &expected);
    if (status < 0) {
        return status;
    }

    /* Provide a means to override this check. This is intended to allow
     * folks who know what they're doing to work around this quickly without
     * needing to make a code change. (e.g., someone building a custom FPGA
     * image that enables compressoin) */
    if (getenv(env_override)) {
        log_info("Overriding FPGA size check per %s\n", env_override);
        valid = true;
    } else if (expected > 0) {
        valid = (len == expected);
    } else {
        log_debug("Unknown FPGA type (%d). Using relaxed size criteria.\n",
                  fpga);

        if (len < (1 * 1024 * 1024)) {
            valid = false;
        } else if (len >
                   (dev->flash_arch->tsize_bytes - BLADERF_FLASH_ADDR_FPGA)) {
            valid = false;
        } else {
            valid = true;
        }
    }

    if (!valid) {
        log_warning("Detected potentially incorrect FPGA file (length was %d, "
                    "expected %d).\n",
                    len, expected);

        log_debug("If you are certain this file is valid, you may define\n"
                  "BLADERF_SKIP_FPGA_SIZE_CHECK in your environment to skip "
                  "this check.\n\n");
    }

    return valid;
}

bool is_valid_fw_size(size_t len)
{
    /* Simple FW applications generally are significantly larger than this
     */
    if (len < (50 * 1024)) {
        return false;
    } else if (len > BLADERF_FLASH_BYTE_LEN_FIRMWARE) {
        return false;
    } else {
        return true;
    }
}

bladerf_tuning_mode default_tuning_mode(struct bladerf *dev)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    bladerf_tuning_mode mode;
    char const *env_var;
    extern struct controller_fns const rfic_fpga_control;

    mode = BLADERF_TUNING_MODE_HOST;

    /* Detect TX FPGA bug and report warning */
    if (BLADERF_TUNING_MODE_FPGA == mode && rfic_fpga_control.is_present(dev) &&
        version_fields_less_than(&board_data->fpga_version, 0, 10, 2)) {
        log_warning("FPGA v%u.%u.%u has errata related to FPGA-based tuning; "
                    "defaulting to host-based tuning. To use FPGA-based "
                    "tuning, update to FPGA v%u.%u.%u, or set the "
                    "BLADERF_DEFAULT_TUNING_MODE enviroment variable to "
                    "'fpga'.\n",
                    board_data->fpga_version.major,
                    board_data->fpga_version.minor,
                    board_data->fpga_version.patch, 0, 10, 2);
        mode = BLADERF_TUNING_MODE_HOST;
    }

    env_var = getenv("BLADERF_DEFAULT_TUNING_MODE");

    if (env_var != NULL) {
        if (!strcasecmp("host", env_var)) {
            mode = BLADERF_TUNING_MODE_HOST;
        } else if (!strcasecmp("fpga", env_var)) {
            /* Capability check */
            if (!have_cap(board_data->capabilities, BLADERF_CAP_FPGA_TUNING)) {
                log_error("The loaded FPGA version (%u.%u.%u) does not support "
                          "the tuning mode being used to override the default. "
                          "Ignoring BLADERF_DEFAULT_TUNING_MODE.\n",
                          board_data->fpga_version.major,
                          board_data->fpga_version.minor,
                          board_data->fpga_version.patch);
            } else {
                mode = BLADERF_TUNING_MODE_FPGA;
            }
        } else {
            log_debug("Invalid tuning mode override: %s\n", env_var);
        }
    }

    /* Check if the FPGA actually has RFIC control built in */
    if (BLADERF_TUNING_MODE_FPGA == mode &&
        !rfic_fpga_control.is_present(dev)) {
        log_debug("FPGA does not have RFIC tuning capabilities, "
                  "defaulting to host-based control.\n");
        mode = BLADERF_TUNING_MODE_HOST;
    }

    switch (mode) {
        case BLADERF_TUNING_MODE_HOST:
            log_debug("Default tuning mode: Host\n");
            break;
        case BLADERF_TUNING_MODE_FPGA:
            log_debug("Default tuning mode: FPGA\n");
            break;
        default:
            assert(!"Bug encountered.");
            mode = BLADERF_TUNING_MODE_HOST;
    }

    return mode;
}


/******************************************************************************/
/* Misc */
/******************************************************************************/

bool check_total_sample_rate(struct bladerf *dev)
{
    int status;
    uint32_t reg;
    size_t i;

    bladerf_sample_rate rate_accum = 0;
    size_t active_channels         = 0;

    const bladerf_sample_rate MAX_SAMPLE_THROUGHPUT = 80000000;

    /* Read RFFE control register */
    status = dev->backend->rffe_control_read(dev, &reg);
    if (status < 0) {
        return false;
    }

    /* Accumulate sample rates for all channels */
    if (_rffe_dir_enabled(reg, BLADERF_RX)) {
        bladerf_sample_rate rx_rate;

        status =
            dev->board->get_sample_rate(dev, BLADERF_CHANNEL_RX(0), &rx_rate);
        if (status < 0) {
            return false;
        }

        for (i = 0; i < dev->board->get_channel_count(dev, BLADERF_RX); ++i) {
            if (_rffe_ch_enabled(reg, BLADERF_CHANNEL_RX(i))) {
                rate_accum += rx_rate;
                ++active_channels;
            }
        }
    }

    if (_rffe_dir_enabled(reg, BLADERF_TX)) {
        bladerf_sample_rate tx_rate;

        status =
            dev->board->get_sample_rate(dev, BLADERF_CHANNEL_TX(0), &tx_rate);
        if (status < 0) {
            return false;
        }

        for (i = 0; i < dev->board->get_channel_count(dev, BLADERF_TX); ++i) {
            if (_rffe_ch_enabled(reg, BLADERF_CHANNEL_TX(i))) {
                rate_accum += tx_rate;
                ++active_channels;
            }
        }
    }

    log_verbose("%s: active_channels=%d, rate_accum=%d, maximum=%d\n",
                __FUNCTION__, active_channels, rate_accum,
                MAX_SAMPLE_THROUGHPUT);

    if (rate_accum > MAX_SAMPLE_THROUGHPUT) {
        log_warning("The total sample throughput for the %d active channel%s, "
                    "%g Msps, is greater than the recommended maximum sample "
                    "throughput, %g Msps. You may experience dropped samples "
                    "unless the sample rate is reduced, or some channels are "
                    "deactivated.\n",
                    active_channels, (active_channels == 1 ? "" : "s"),
                    rate_accum / 1e6, MAX_SAMPLE_THROUGHPUT / 1e6);
        return false;
    }

    return true;
}

bool does_rffe_dir_have_enabled_ch(uint32_t reg, bladerf_direction dir)
{
    switch (dir) {
        case BLADERF_RX:
            return _rffe_ch_enabled(reg, BLADERF_CHANNEL_RX(0)) ||
                   _rffe_ch_enabled(reg, BLADERF_CHANNEL_RX(1));
        case BLADERF_TX:
            return _rffe_ch_enabled(reg, BLADERF_CHANNEL_TX(0)) ||
                   _rffe_ch_enabled(reg, BLADERF_CHANNEL_TX(1));
    }

    return false;
}

int get_gain_offset(struct bladerf *dev, bladerf_channel ch, float *offset)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);
    NULL_CHECK(offset);

    struct bladerf_gain_range const *ranges = NULL;
    bladerf_frequency frequency             = 0;
    size_t i, ranges_len;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        ranges     = bladerf2_tx_gain_ranges;
        ranges_len = ARRAY_SIZE(bladerf2_tx_gain_ranges);
    } else {
        ranges     = bladerf2_rx_gain_ranges;
        ranges_len = ARRAY_SIZE(bladerf2_rx_gain_ranges);
    }

    CHECK_STATUS(dev->board->get_frequency(dev, ch, &frequency));

    for (i = 0; i < ranges_len; ++i) {
        struct bladerf_gain_range const *r = &(ranges[i]);
        struct bladerf_range const *rfreq  = &(r->frequency);

        // if the frequency range matches, and the range name is null,
        // then we found our match
        if (is_within_range(rfreq, frequency) && (NULL == r->name)) {
            *offset = r->offset;
            return 0;
        }
    }

    return BLADERF_ERR_INVAL;
}
