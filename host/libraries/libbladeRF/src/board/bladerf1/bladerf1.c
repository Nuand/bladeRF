/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2017 Nuand LLC
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
#include <errno.h>
#include <limits.h>

#include <libbladeRF.h>

#include "log.h"
#include "conversions.h"
#include "bladeRF.h"

#include "board/board.h"

#include "compatibility.h"
#include "capabilities.h"
#include "calibration.h"
#include "flash.h"

#include "driver/smb_clock.h"
#include "driver/si5338.h"
#include "driver/dac161s055.h"
#include "driver/spi_flash.h"
#include "driver/fpga_trigger.h"
#include "lms.h"
#include "nios_pkt_retune.h"
#include "band_select.h"

#include "backend/usb/usb.h"
#include "backend/backend_config.h"

#include "expansion/xb100.h"
#include "expansion/xb200.h"
#include "expansion/xb300.h"

#include "streaming/async.h"
#include "streaming/sync.h"

#include "devinfo.h"
#include "helpers/version.h"
#include "helpers/file.h"
#include "version.h"

/******************************************************************************
 *                          bladeRF1 board state                              *
 ******************************************************************************/

/* 1 TX, 1 RX */
#define NUM_MODULES 2

struct bladerf1_board_data {
    /* Board state */
    enum {
        STATE_UNINITIALIZED,
        STATE_FIRMWARE_LOADED,
        STATE_FPGA_LOADED,
        STATE_INITIALIZED,
    } state;

    /* Bitmask of capabilities determined by version numbers */
    uint64_t capabilities;

    /* Format currently being used with a module, or -1 if module is not used */
    bladerf_format module_format[NUM_MODULES];

    /* Which mode of operation we use for tuning */
    bladerf_tuning_mode tuning_mode;

    /* Calibration data */
    struct calibrations {
        struct dc_cal_tbl *dc_rx;
        struct dc_cal_tbl *dc_tx;
    } cal;
    uint16_t dac_trim;

    /* Board properties */
    bladerf_fpga_size fpga_size;
    /* Data message size */
    size_t msg_size;

    /* Version information */
    struct bladerf_version fpga_version;
    struct bladerf_version fw_version;
    char fpga_version_str[BLADERF_VERSION_STR_MAX+1];
    char fw_version_str[BLADERF_VERSION_STR_MAX+1];

    /* Synchronous interface handles */
    struct bladerf_sync sync[NUM_MODULES];
};

#define _CHECK_BOARD_STATE(_state, _locked) \
    do { \
        struct bladerf1_board_data *board_data = dev->board_data; \
        if (board_data->state < _state) { \
            log_error("Board state insufficient for operation " \
                      "(current \"%s\", requires \"%s\").\n", \
                      bladerf1_state_to_string[board_data->state], \
                      bladerf1_state_to_string[_state]); \
            if (_locked) { \
                MUTEX_UNLOCK(&dev->lock); \
            } \
            return BLADERF_ERR_NOT_INIT; \
        } \
    } while(0)

#define CHECK_BOARD_STATE(_state)           _CHECK_BOARD_STATE(_state, false)
#define CHECK_BOARD_STATE_LOCKED(_state)    _CHECK_BOARD_STATE(_state, true)

#define __round_int(x) (x >= 0 ? (int)(x + 0.5) : (int)(x - 0.5))
#define __round_int64(x) (x >= 0 ? (int64_t)(x + 0.5) : (int64_t)(x - 0.5))

#define __scale(r, v) ((float)(v) / (r)->scale)
#define __scale_int(r, v) (__round_int(__scale(r, v)))
#define __scale_int64(r, v) (__round_int64(__scale(r, v)))

#define __unscale(r, v) ((float)(v) * (r)->scale)
#define __unscale_int(r, v) (__round_int(__unscale(r, v)))
#define __unscale_int64(r, v) (__round_int64(__unscale(r, v)))

/******************************************************************************/
/* Constants */
/******************************************************************************/

/* Board state to string map */

static const char *bladerf1_state_to_string[] = {
    [STATE_UNINITIALIZED]   = "Uninitialized",
    [STATE_FIRMWARE_LOADED] = "Firmware Loaded",
    [STATE_FPGA_LOADED]     = "FPGA Loaded",
    [STATE_INITIALIZED]     = "Initialized",
};

/* RX gain offset */
#define BLADERF1_RX_GAIN_OFFSET -6.0f

/* Overall RX gain range */
static const struct bladerf_range bladerf1_rx_gain_range = {
    FIELD_INIT(.min, __round_int64(BLADERF_RXVGA1_GAIN_MIN + BLADERF_RXVGA2_GAIN_MIN + BLADERF1_RX_GAIN_OFFSET)),
    FIELD_INIT(.max, __round_int64(BLADERF_LNA_GAIN_MAX_DB + BLADERF_RXVGA1_GAIN_MAX + BLADERF_RXVGA2_GAIN_MAX + BLADERF1_RX_GAIN_OFFSET)),
    FIELD_INIT(.step, 1),
    FIELD_INIT(.scale, 1),
};

/* TX gain offset: 60 dB system gain ~= 0 dBm output */
#define BLADERF1_TX_GAIN_OFFSET 52.0f

/* Overall TX gain range */
static const struct bladerf_range bladerf1_tx_gain_range = {
    FIELD_INIT(.min, __round_int64(BLADERF_TXVGA1_GAIN_MIN + BLADERF_TXVGA2_GAIN_MIN + BLADERF1_TX_GAIN_OFFSET)),
    FIELD_INIT(.max, __round_int64(BLADERF_TXVGA1_GAIN_MAX + BLADERF_TXVGA2_GAIN_MAX + BLADERF1_TX_GAIN_OFFSET)),
    FIELD_INIT(.step, 1),
    FIELD_INIT(.scale, 1),
};

/* RX gain modes */

static const struct bladerf_gain_modes bladerf1_rx_gain_modes[] = {
    {
        FIELD_INIT(.name, "automatic"),
        FIELD_INIT(.mode, BLADERF_GAIN_DEFAULT)
    },
    {
        FIELD_INIT(.name, "manual"),
        FIELD_INIT(.mode, BLADERF_GAIN_MGC)
    },
};

struct bladerf_gain_stage_info {
    const char *name;
    struct bladerf_range range;
};

/* RX gain stages */

static const struct bladerf_gain_stage_info bladerf1_rx_gain_stages[] = {
    {
        FIELD_INIT(.name, "lna"),
        FIELD_INIT(.range, {
            FIELD_INIT(.min, 0),
            FIELD_INIT(.max, BLADERF_LNA_GAIN_MAX_DB),
            FIELD_INIT(.step, 3),
            FIELD_INIT(.scale, 1),
        }),
    },
    {
        FIELD_INIT(.name, "rxvga1"),
        FIELD_INIT(.range, {
            FIELD_INIT(.min, BLADERF_RXVGA1_GAIN_MIN),
            FIELD_INIT(.max, BLADERF_RXVGA1_GAIN_MAX),
            FIELD_INIT(.step, 1),
            FIELD_INIT(.scale, 1),
        }),
    },
    {
        FIELD_INIT(.name, "rxvga2"),
        FIELD_INIT(.range, {
            FIELD_INIT(.min, BLADERF_RXVGA2_GAIN_MIN),
            FIELD_INIT(.max, BLADERF_RXVGA2_GAIN_MAX),
            FIELD_INIT(.step, 3),
            FIELD_INIT(.scale, 1),
        }),
    },
};

/* TX gain stages */

static const struct bladerf_gain_stage_info bladerf1_tx_gain_stages[] = {
    {
        FIELD_INIT(.name, "txvga1"),
        FIELD_INIT(.range, {
            FIELD_INIT(.min, BLADERF_TXVGA1_GAIN_MIN),
            FIELD_INIT(.max, BLADERF_TXVGA1_GAIN_MAX),
            FIELD_INIT(.step, 1),
            FIELD_INIT(.scale, 1),
        }),
    },
    {
        FIELD_INIT(.name, "txvga2"),
        FIELD_INIT(.range, {
            FIELD_INIT(.min, BLADERF_TXVGA2_GAIN_MIN),
            FIELD_INIT(.max, BLADERF_TXVGA2_GAIN_MAX),
            FIELD_INIT(.step, 1),
            FIELD_INIT(.scale, 1),
        }),
    },
};

/* Sample Rate Range */

static const struct bladerf_range bladerf1_sample_rate_range = {
    FIELD_INIT(.min, BLADERF_SAMPLERATE_MIN),
    FIELD_INIT(.max, BLADERF_SAMPLERATE_REC_MAX),
    FIELD_INIT(.step, 1),
    FIELD_INIT(.scale, 1),
};

/* Bandwidth Range */

static const struct bladerf_range bladerf1_bandwidth_range = {
    FIELD_INIT(.min, BLADERF_BANDWIDTH_MIN),
    FIELD_INIT(.max, BLADERF_BANDWIDTH_MAX),
    FIELD_INIT(.step, 1),
    FIELD_INIT(.scale, 1),
};

/* Frequency Range */

static const struct bladerf_range bladerf1_frequency_range = {
    FIELD_INIT(.min, BLADERF_FREQUENCY_MIN),
    FIELD_INIT(.max, BLADERF_FREQUENCY_MAX),
    FIELD_INIT(.step, 1),
    FIELD_INIT(.scale, 1),
};

static const struct bladerf_range bladerf1_xb200_frequency_range = {
    FIELD_INIT(.min, 0),
    FIELD_INIT(.max, BLADERF_FREQUENCY_MAX),
    FIELD_INIT(.step, 1),
    FIELD_INIT(.scale, 1),
};

/* Loopback modes */

static const struct bladerf_loopback_modes bladerf1_loopback_modes[] = {
    {
        FIELD_INIT(.name, "none"),
        FIELD_INIT(.mode, BLADERF_LB_NONE)
    },
    {
        FIELD_INIT(.name, "firmware"),
        FIELD_INIT(.mode, BLADERF_LB_FIRMWARE)
    },
    {
        FIELD_INIT(.name, "bb_txlpf_rxvga2"),
        FIELD_INIT(.mode, BLADERF_LB_BB_TXLPF_RXVGA2)
    },
    {
        FIELD_INIT(.name, "bb_txlpf_rxlpf"),
        FIELD_INIT(.mode, BLADERF_LB_BB_TXLPF_RXLPF)
    },
    {
        FIELD_INIT(.name, "bb_txvga1_rxvga2"),
        FIELD_INIT(.mode, BLADERF_LB_BB_TXVGA1_RXVGA2)
    },
    {
        FIELD_INIT(.name, "bb_txvga1_rxlpf"),
        FIELD_INIT(.mode, BLADERF_LB_BB_TXVGA1_RXLPF)
    },
    {
        FIELD_INIT(.name, "rf_lna1"),
        FIELD_INIT(.mode, BLADERF_LB_RF_LNA1)
    },
    {
        FIELD_INIT(.name, "rf_lna2"),
        FIELD_INIT(.mode, BLADERF_LB_RF_LNA2)
    },
    {
        FIELD_INIT(.name, "rf_lna3"),
        FIELD_INIT(.mode, BLADERF_LB_RF_LNA3)
    },
};

/* RF ports */

struct bladerf_lms_port_name_map {
    const char *name;
    union {
        lms_lna rx_lna;
        lms_pa tx_pa;
    };
};

static const struct bladerf_lms_port_name_map bladerf1_rx_port_map[] = {
    {
        FIELD_INIT(.name, "none"),
        FIELD_INIT(.rx_lna, LNA_NONE),
    },
    {
        FIELD_INIT(.name, "lna1"),
        FIELD_INIT(.rx_lna, LNA_1),
    },
    {
        FIELD_INIT(.name, "lna2"),
        FIELD_INIT(.rx_lna, LNA_2),
    },
    {
        FIELD_INIT(.name, "lna3"),
        FIELD_INIT(.rx_lna, LNA_3),
    },
};

static const struct bladerf_lms_port_name_map bladerf1_tx_port_map[] = {
    {
        FIELD_INIT(.name, "aux"),
        FIELD_INIT(.tx_pa, PA_AUX),
    },
    {
        FIELD_INIT(.name, "pa1"),
        FIELD_INIT(.tx_pa, PA_1),
    },
    {
        FIELD_INIT(.name, "pa2"),
        FIELD_INIT(.tx_pa, PA_2),
    },
    {
        FIELD_INIT(.name, "none"),
        FIELD_INIT(.tx_pa, PA_NONE),
    },
};

/******************************************************************************/
/* Low-level Initialization */
/******************************************************************************/

static bladerf_tuning_mode tuning_get_default_mode(struct bladerf *dev)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    bladerf_tuning_mode mode;
    const char *env_var;

    if (have_cap(board_data->capabilities, BLADERF_CAP_FPGA_TUNING)) {
        mode = BLADERF_TUNING_MODE_FPGA;
    } else {
        mode = BLADERF_TUNING_MODE_HOST;
    }

    env_var = getenv("BLADERF_DEFAULT_TUNING_MODE");

    if (env_var != NULL) {
        if (!strcasecmp("host", env_var)) {
            mode = BLADERF_TUNING_MODE_HOST;
        } else if (!strcasecmp("fpga", env_var)) {
            mode = BLADERF_TUNING_MODE_FPGA;

            /* Just a friendly reminder... */
            if (!have_cap(board_data->capabilities, BLADERF_CAP_FPGA_TUNING)) {
                log_warning("The loaded FPGA version (%u.%u.%u) does not "
                            "support the tuning mode being used to override "
                            "the default.\n",
                            board_data->fpga_version.major, board_data->fpga_version.minor,
                            board_data->fpga_version.patch);
            }
        } else {
            log_debug("Invalid tuning mode override: %s\n", env_var);
        }
    }

    switch (mode) {
        case BLADERF_TUNING_MODE_HOST:
            log_debug("Default tuning mode: host\n");
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

static int bladerf1_apply_lms_dc_cals(struct bladerf *dev)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    int status = 0;
    struct bladerf_lms_dc_cals cals;
    const bool have_rx = (board_data->cal.dc_rx != NULL);
    const bool have_tx = (board_data->cal.dc_tx != NULL);

    cals.lpf_tuning = -1;
    cals.tx_lpf_i   = -1;
    cals.tx_lpf_q   = -1;
    cals.rx_lpf_i   = -1;
    cals.rx_lpf_q   = -1;
    cals.dc_ref     = -1;
    cals.rxvga2a_i  = -1;
    cals.rxvga2a_q  = -1;
    cals.rxvga2b_i  = -1;
    cals.rxvga2b_q  = -1;

    if (have_rx) {
        const struct bladerf_lms_dc_cals *reg_vals =
            &board_data->cal.dc_rx->reg_vals;

        cals.lpf_tuning = reg_vals->lpf_tuning;
        cals.rx_lpf_i   = reg_vals->rx_lpf_i;
        cals.rx_lpf_q   = reg_vals->rx_lpf_q;
        cals.dc_ref     = reg_vals->dc_ref;
        cals.rxvga2a_i  = reg_vals->rxvga2a_i;
        cals.rxvga2a_q  = reg_vals->rxvga2a_q;
        cals.rxvga2b_i  = reg_vals->rxvga2b_i;
        cals.rxvga2b_q  = reg_vals->rxvga2b_q;

        log_verbose("Fetched register values from RX DC cal table.\n");
    }

    if (have_tx) {
        const struct bladerf_lms_dc_cals *reg_vals =
            &board_data->cal.dc_tx->reg_vals;

        cals.tx_lpf_i = reg_vals->tx_lpf_i;
        cals.tx_lpf_q = reg_vals->tx_lpf_q;

        if (have_rx) {
            if (cals.lpf_tuning != reg_vals->lpf_tuning) {
                log_warning("LPF tuning mismatch in tables. "
                            "RX=0x%04x, TX=0x%04x",
                            cals.lpf_tuning, reg_vals->lpf_tuning);
            }
        } else {
            /* Have TX cal but no RX cal -- use the RX values that came along
             * for the ride when the TX table was generated */
            cals.rx_lpf_i  = reg_vals->rx_lpf_i;
            cals.rx_lpf_q  = reg_vals->rx_lpf_q;
            cals.dc_ref    = reg_vals->dc_ref;
            cals.rxvga2a_i = reg_vals->rxvga2a_i;
            cals.rxvga2a_q = reg_vals->rxvga2a_q;
            cals.rxvga2b_i = reg_vals->rxvga2b_i;
            cals.rxvga2b_q = reg_vals->rxvga2b_q;
        }

        log_verbose("Fetched register values from TX DC cal table.\n");
    }

    /* No TX table was loaded, so load LMS TX register cals from the RX table,
     * if available */
    if (have_rx && !have_tx) {
        const struct bladerf_lms_dc_cals *reg_vals =
            &board_data->cal.dc_rx->reg_vals;

        cals.tx_lpf_i = reg_vals->tx_lpf_i;
        cals.tx_lpf_q = reg_vals->tx_lpf_q;
    }

    if (have_rx || have_tx) {
        status = lms_set_dc_cals(dev, &cals);

        /* Force a re-tune so that we can apply the appropriate I/Q DC offset
         * values from our calibration table */
        if (status == 0) {
            int rx_status = 0;
            int tx_status = 0;

            if (have_rx) {
                bladerf_frequency rx_f;
                rx_status = dev->board->get_frequency(
                    dev, BLADERF_CHANNEL_RX(0), &rx_f);
                if (rx_status == 0) {
                    rx_status = dev->board->set_frequency(
                        dev, BLADERF_CHANNEL_RX(0), rx_f);
                }
            }

            if (have_tx) {
                bladerf_frequency rx_f;
                rx_status = dev->board->get_frequency(
                    dev, BLADERF_CHANNEL_RX(0), &rx_f);
                if (rx_status == 0) {
                    rx_status = dev->board->set_frequency(
                        dev, BLADERF_CHANNEL_RX(0), rx_f);
                }
            }

            /* Report the first of any failures */
            status = (rx_status == 0) ? tx_status : rx_status;
            if (status != 0) {
                return status;
            }
        }
    }

    return 0;
}

/**
 * Initialize device registers - required after power-up, but safe
 * to call multiple times after power-up (e.g., multiple close and reopens)
 */
static int bladerf1_initialize(struct bladerf *dev)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    struct bladerf_version required_fw_version;
    struct bladerf_version required_fpga_version;
    int status;
    uint32_t val;

    /* Read FPGA version */
    status = dev->backend->get_fpga_version(dev, &board_data->fpga_version);
    if (status < 0) {
        log_debug("Failed to get FPGA version: %s\n",
                  bladerf_strerror(status));
        return status;
    }
    log_verbose("Read FPGA version: %s\n", board_data->fpga_version.describe);

    /* Determine FPGA capabilities */
    board_data->capabilities |= bladerf1_get_fpga_capabilities(&board_data->fpga_version);
    log_verbose("Capability mask after FPGA load: 0x%016"PRIx64"\n",
                 board_data->capabilities);

    if (getenv("BLADERF_FORCE_LEGACY_NIOS_PKT")) {
        board_data->capabilities &= ~BLADERF_CAP_PKT_HANDLER_FMT;
        log_verbose("Using legacy packet handler format due to env var\n");
    }

    /* If the FPGA version check fails, just warn, but don't error out.
     *
     * If an error code caused this function to bail out, it would prevent a
     * user from being able to unload and reflash a bitstream being
     * "autoloaded" from SPI flash. */
    status = version_check(&bladerf1_fw_compat_table, &bladerf1_fpga_compat_table,
                           &board_data->fw_version, &board_data->fpga_version,
                           &required_fw_version, &required_fpga_version);
    if (status < 0) {
#if LOGGING_ENABLED
        if (status == BLADERF_ERR_UPDATE_FPGA) {
            log_warning("FPGA v%u.%u.%u was detected. Firmware v%u.%u.%u "
                        "requires FPGA v%u.%u.%u or later. Please load a "
                        "different FPGA version before continuing.\n\n",
                        board_data->fpga_version.major,
                        board_data->fpga_version.minor,
                        board_data->fpga_version.patch,
                        board_data->fw_version.major,
                        board_data->fw_version.minor,
                        board_data->fw_version.patch,
                        required_fpga_version.major,
                        required_fpga_version.minor,
                        required_fpga_version.patch);
        } else if (status == BLADERF_ERR_UPDATE_FW) {
            log_warning("FPGA v%u.%u.%u was detected, which requires firmware "
                        "v%u.%u.%u or later. The device firmware is currently "
                        "v%u.%u.%u. Please upgrade the device firmware before "
                        "continuing.\n\n",
                        board_data->fpga_version.major,
                        board_data->fpga_version.minor,
                        board_data->fpga_version.patch,
                        required_fw_version.major,
                        required_fw_version.minor,
                        required_fw_version.patch,
                        board_data->fw_version.major,
                        board_data->fw_version.minor,
                        board_data->fw_version.patch);
        }
#endif
    }

    /* Detect AGC FPGA bug and report warning */
    if( have_cap(board_data->capabilities, BLADERF_CAP_AGC_DC_LUT) &&
        version_fields_less_than(&board_data->fpga_version, 0, 8, 0) ) {
        log_warning("AGC commands for FPGA v%u.%u.%u are incompatible with "
                    "this version of libbladeRF. Please update to FPGA "
                    "v%u.%u.%u or newer to use AGC.\n",
                    board_data->fpga_version.major,
                    board_data->fpga_version.minor,
                    board_data->fpga_version.patch,
                    0, 8, 0 );
    }

    /* Set FPGA packet protocol */
    if (have_cap(board_data->capabilities, BLADERF_CAP_PKT_HANDLER_FMT)) {
        status = dev->backend->set_fpga_protocol(dev, BACKEND_FPGA_PROTOCOL_NIOSII);
    } else {
        status = dev->backend->set_fpga_protocol(dev, BACKEND_FPGA_PROTOCOL_NIOSII_LEGACY);
    }
    if (status < 0) {
        log_error("Unable to set backend FPGA protocol: %d\n", status);
        return status;
    }

    /* Readback the GPIO values to see if they are default or already set */
    status = dev->backend->config_gpio_read(dev, &val);
    if (status != 0) {
        log_debug("Failed to read GPIO config %s\n", bladerf_strerror(status));
        return status;
    }

    if ((val & 0x7f) == 0) {
        log_verbose( "Default GPIO value found - initializing device\n" );

        /* Set the GPIO pins to enable the LMS and select the low band */
        status = dev->backend->config_gpio_write(dev, 0x57);
        if (status != 0) {
            return status;
        }

        /* Disable the front ends */
        status = lms_enable_rffe(dev, BLADERF_CHANNEL_TX(0), false);
        if (status != 0) {
            return status;
        }

        status = lms_enable_rffe(dev, BLADERF_CHANNEL_RX(0), false);
        if (status != 0) {
            return status;
        }

        /* Set the internal LMS register to enable RX and TX */
        status = LMS_WRITE(dev, 0x05, 0x3e);
        if (status != 0) {
            return status;
        }

        /* LMS FAQ: Improve TX spurious emission performance */
        status = LMS_WRITE(dev, 0x47, 0x40);
        if (status != 0) {
            return status;
        }

        /* LMS FAQ: Improve ADC performance */
        status = LMS_WRITE(dev, 0x59, 0x29);
        if (status != 0) {
            return status;
        }

        /* LMS FAQ: Common mode voltage for ADC */
        status = LMS_WRITE(dev, 0x64, 0x36);
        if (status != 0) {
            return status;
        }

        /* LMS FAQ: Higher LNA Gain */
        status = LMS_WRITE(dev, 0x79, 0x37);
        if (status != 0) {
            return status;
        }

        /* Power down DC calibration comparators until they are need, as they
         * have been shown to introduce undesirable artifacts into our signals.
         * (This is documented in the LMS6 FAQ). */

        status = lms_set(dev, 0x3f, 0x80);  /* TX LPF DC cal comparator */
        if (status != 0) {
            return status;
        }

        status = lms_set(dev, 0x5f, 0x80);  /* RX LPF DC cal comparator */
        if (status != 0) {
            return status;
        }

        status = lms_set(dev, 0x6e, 0xc0);  /* RXVGA2A/B DC cal comparators */
        if (status != 0) {
            return status;
        }

        /* Configure charge pump current offsets */
        status = lms_config_charge_pumps(dev, BLADERF_CHANNEL_TX(0));
        if (status != 0) {
            return status;
        }

        status = lms_config_charge_pumps(dev, BLADERF_CHANNEL_RX(0));
        if (status != 0) {
            return status;
        }

        /* Set a default samplerate */
        status = si5338_set_sample_rate(dev, BLADERF_CHANNEL_TX(0), 1000000, NULL);
        if (status != 0) {
            return status;
        }

        status = si5338_set_sample_rate(dev, BLADERF_CHANNEL_RX(0), 1000000, NULL);
        if (status != 0) {
            return status;
        }

        board_data->tuning_mode = tuning_get_default_mode(dev);

        status = dev->board->set_frequency(dev, BLADERF_CHANNEL_TX(0), 2447000000U);
        if (status != 0) {
            return status;
        }

        status = dev->board->set_frequency(dev, BLADERF_CHANNEL_RX(0), 2484000000U);
        if (status != 0) {
            return status;
        }

        /* Set the calibrated VCTCXO DAC value */
        status = dac161s055_write(dev, board_data->dac_trim);
        if (status != 0) {
            return status;
        }

        /* Set the default gain mode */
        status = bladerf_set_gain_mode(dev, BLADERF_CHANNEL_RX(0), BLADERF_GAIN_DEFAULT);
        if (status != 0 && status != BLADERF_ERR_UNSUPPORTED) {
            return status;
        }
    } else {
        board_data->tuning_mode = tuning_get_default_mode(dev);
    }

    /* Check if we have an expansion board attached */
    status = dev->board->expansion_get_attached(dev, &dev->xb);
    if (status != 0) {
        return status;
    }

    /* Update device state */
    board_data->state = STATE_INITIALIZED;

    /* Set up LMS DC offset register calibration and initial IQ settings,
     * if any tables have been loaded already.
     *
     * This is done every time the device is opened (with an FPGA loaded),
     * as the user may change/update DC calibration tables without reloading the
     * FPGA.
     */
    status = bladerf1_apply_lms_dc_cals(dev);
    if (status != 0) {
        return status;
    }

    return 0;
}

/******************************************************************************
 *                        Generic Board Functions                             *
 ******************************************************************************/

/******************************************************************************/
/* Matches */
/******************************************************************************/

static bool bladerf1_matches(struct bladerf *dev)
{
    uint16_t vid, pid;
    int status;

    status = dev->backend->get_vid_pid(dev, &vid, &pid);
    if (status < 0) {
        return false;
    }

    if (vid == USB_NUAND_VENDOR_ID && pid == USB_NUAND_BLADERF_PRODUCT_ID) {
        return true;
    } else if (vid == USB_NUAND_LEGACY_VENDOR_ID && pid == USB_NUAND_BLADERF_LEGACY_PRODUCT_ID) {
        return true;
    }

    return false;
}

/******************************************************************************/
/* Open/close */
/******************************************************************************/

static int bladerf1_open(struct bladerf *dev, struct bladerf_devinfo *devinfo)
{
    struct bladerf1_board_data *board_data;
    struct bladerf_version required_fw_version;
    bladerf_dev_speed usb_speed;
    char filename[FILENAME_MAX];
    char *full_path;
    int status;

    /* Allocate board data */
    board_data = calloc(1, sizeof(struct bladerf1_board_data));
    if (board_data == NULL) {
        return BLADERF_ERR_MEM;
    }
    dev->board_data = board_data;

    /* Allocate flash architecture */
    dev->flash_arch = calloc(1, sizeof(struct bladerf_flash_arch));
    if (dev->flash_arch == NULL) {
        return BLADERF_ERR_MEM;
    }

    /* Initialize board data */
    board_data->fpga_version.describe = board_data->fpga_version_str;
    board_data->fw_version.describe   = board_data->fw_version_str;

    board_data->module_format[BLADERF_RX] = -1;
    board_data->module_format[BLADERF_TX] = -1;

    dev->flash_arch->status          = STATE_UNINITIALIZED;
    dev->flash_arch->manufacturer_id = 0x0;
    dev->flash_arch->device_id       = 0x0;

    /* Read firmware version */
    status = dev->backend->get_fw_version(dev, &board_data->fw_version);
    if (status < 0) {
        log_debug("Failed to get FW version: %s\n", bladerf_strerror(status));
        return status;
    }
    log_verbose("Read Firmware version: %s\n", board_data->fw_version.describe);

    /* Update device state */
    board_data->state = STATE_FIRMWARE_LOADED;

    /* Determine firmware capabilities */
    board_data->capabilities |=
        bladerf1_get_fw_capabilities(&board_data->fw_version);
    log_verbose("Capability mask before FPGA load: 0x%016" PRIx64 "\n",
                board_data->capabilities);

    /* Wait until firmware is ready */
    if (have_cap(board_data->capabilities, BLADERF_CAP_QUERY_DEVICE_READY)) {
        const unsigned int max_retries = 30;
        unsigned int i;
        int ready;

        for (i = 0; i < max_retries; i++) {
            ready = dev->backend->is_fw_ready(dev);
            if (ready != 1) {
                if (i == 0) {
                    log_info("Waiting for device to become ready...\n");
                } else {
                    log_debug("Retry %02u/%02u.\n", i + 1, max_retries);
                }
                usleep(1000000);
            } else {
                break;
            }
        }

        if (i >= max_retries) {
            log_debug("Timed out while waiting for device.\n");
            return BLADERF_ERR_TIMEOUT;
        }
    } else {
        log_info(
            "FX3 FW v%u.%u.%u does not support the \"device ready\" query.\n"
            "\tEnsure flash-autoloading completes before opening a device.\n"
            "\tUpgrade the FX3 firmware to avoid this message in the future.\n"
            "\n",
            board_data->fw_version.major, board_data->fw_version.minor,
            board_data->fw_version.patch);
    }

    /* Determine data message size */
    status = dev->backend->get_device_speed(dev, &usb_speed);
    if (status < 0) {
        log_debug("Failed to get device speed: %s\n", bladerf_strerror(status));
        return status;
    }
    switch (usb_speed) {
        case BLADERF_DEVICE_SPEED_SUPER:
            board_data->msg_size = USB_MSG_SIZE_SS;
            break;

        case BLADERF_DEVICE_SPEED_HIGH:
            board_data->msg_size = USB_MSG_SIZE_HS;
            break;

        default:
            log_error("Unsupported device speed: %d\n", usb_speed);
            return BLADERF_ERR_UNEXPECTED;
    }

    /* Verify that we have a sufficent firmware version before continuing. */
    status = version_check_fw(&bladerf1_fw_compat_table,
                              &board_data->fw_version, &required_fw_version);
    if (status != 0) {
#ifdef LOGGING_ENABLED
        if (status == BLADERF_ERR_UPDATE_FW) {
            log_warning("Firmware v%u.%u.%u was detected. libbladeRF v%s "
                        "requires firmware v%u.%u.%u or later. An upgrade via "
                        "the bootloader is required.\n\n",
                        board_data->fw_version.major,
                        board_data->fw_version.minor,
                        board_data->fw_version.patch, LIBBLADERF_VERSION,
                        required_fw_version.major, required_fw_version.minor,
                        required_fw_version.patch);
        }
#endif
        return status;
    }

    /* Probe SPI flash architecture information */
    if (have_cap(board_data->capabilities, BLADERF_CAP_FW_FLASH_ID)) {
        status = spi_flash_read_flash_id(dev, &dev->flash_arch->manufacturer_id,
                                         &dev->flash_arch->device_id);
        if (status < 0) {
            log_error("Failed to probe SPI flash ID information.\n");
        }
    } else {
        log_debug("FX3 firmware v%u.%u.%u does not support SPI flash ID. A "
                  "firmware update is recommended in order to probe the SPI "
                  "flash ID information.\n",
                  board_data->fw_version.major, board_data->fw_version.minor,
                  board_data->fw_version.patch);
    }

    /* Decode SPI flash ID information to figure out its architecture.
     * We need to know a little about the flash architecture before we can
     * read anything from it, including FPGA size and other cal data.
     * If the firmware does not have the capability to get the flash ID,
     * sane defaults will be chosen.
     *
     * Not checking return code because it is irrelevant. */
    spi_flash_decode_flash_architecture(dev, &board_data->fpga_size);

    /* VCTCXO trim and FPGA size are non-fatal indicators that we've
     * trashed the calibration region of flash. If these were made fatal,
     * we wouldn't be able to open the device to restore them. */

    status = spi_flash_read_vctcxo_trim(dev, &board_data->dac_trim);
    if (status < 0) {
        log_warning("Failed to get VCTCXO trim value: %s\n",
                    bladerf_strerror(status));
        log_debug("Defaulting DAC trim to 0x8000.\n");
        board_data->dac_trim = 0x8000;
    }

    status = spi_flash_read_fpga_size(dev, &board_data->fpga_size);
    if (status < 0) {
        log_warning("Failed to get FPGA size %s\n", bladerf_strerror(status));
    }

    /* If the flash architecture could not be decoded earlier, try again now
     * that the FPGA size is known. */
    if (dev->flash_arch->status != STATUS_SUCCESS) {
        status =
            spi_flash_decode_flash_architecture(dev, &board_data->fpga_size);
        if (status < 0) {
            log_debug("Assumptions were made about the SPI flash architecture! "
                      "Flash commands may not function as expected.\n");
        }
    }

    /* Skip further work if BLADERF_FORCE_NO_FPGA_PRESENT is set */
    if (getenv("BLADERF_FORCE_NO_FPGA_PRESENT")) {
        log_debug("Skipping FPGA configuration and initialization - "
                  "BLADERF_FORCE_NO_FPGA_PRESENT is set.\n");
        return 0;
    }

    /* Check for possible mismatch between the USB device identification and
     * the board's own knowledge. We need this to be a non-fatal condition,
     * so that the problem can be fixed easily. */
    if (board_data->fpga_size == BLADERF_FPGA_A4 ||
        board_data->fpga_size == BLADERF_FPGA_A9) {
        uint16_t vid, pid;

        log_critical("Device type mismatch! FPGA size %d is a bladeRF2 "
                     "characteristic, but the USB PID indicates bladeRF1. "
                     "Initialization cannot continue.\n",
                     board_data->fpga_size);
        log_info("You must download firmware v2.2.0 or later from "
                 "https://www.nuand.com/fx3/ and flash it (bladeRF-cli -f "
                 "/path/to/bladeRF_fw.img) before using this device.\n");

        status = dev->backend->get_vid_pid(dev, &vid, &pid);
        if (status < 0) {
            log_error("%s: get_vid_pid returned status %s\n", __FUNCTION__,
                      bladerf_strerror(status));
        }

        log_debug("vid_pid=%04x:%04x fpga_size=%d fw_version=%u.%u.%u\n", vid,
                  pid, board_data->fpga_size, board_data->fw_version.major,
                  board_data->fw_version.minor, board_data->fw_version.patch);

        log_warning("Skipping further initialization...\n");
        return 0;
    }

    /* This will be set in initialize() after we can determine which
     * methods the FPGA supports (based upon version number). */
    board_data->tuning_mode = BLADERF_TUNING_MODE_INVALID;

    /* Load any available calibration tables so that the LMS DC register
     * configurations may be loaded in initialize() */
    snprintf(filename, sizeof(filename), "%s_dc_rx.tbl", dev->ident.serial);
    full_path = file_find(filename);
    if (full_path != NULL) {
        log_debug("Loading RX calibration image %s\n", full_path);
        dc_cal_tbl_image_load(dev, &board_data->cal.dc_rx, full_path);
    }
    free(full_path);
    full_path = NULL;

    snprintf(filename, sizeof(filename), "%s_dc_tx.tbl", dev->ident.serial);
    full_path = file_find(filename);
    if (full_path != NULL) {
        log_debug("Loading TX calibration image %s\n", full_path);
        dc_cal_tbl_image_load(dev, &board_data->cal.dc_tx, full_path);
    }
    free(full_path);
    full_path = NULL;

    status = dev->backend->is_fpga_configured(dev);
    if (status < 0) {
        return status;
    } else if (status == 1) {
        board_data->state = STATE_FPGA_LOADED;
    } else if (status != 1 && board_data->fpga_size == BLADERF_FPGA_UNKNOWN) {
        log_warning("Unknown FPGA size. Skipping FPGA configuration...\n");
        log_warning("Skipping further initialization...\n");
        return 0;
    } else if (status != 1) {
        /* Try searching for an FPGA in the config search path */
        if (board_data->fpga_size == BLADERF_FPGA_40KLE) {
            full_path = file_find("hostedx40.rbf");
        } else if (board_data->fpga_size == BLADERF_FPGA_115KLE) {
            full_path = file_find("hostedx115.rbf");
        } else {
            log_error("Invalid FPGA size %d.\n", board_data->fpga_size);
            return BLADERF_ERR_UNEXPECTED;
        }

        if (full_path != NULL) {
            uint8_t *buf;
            size_t buf_size;

            log_debug("Loading FPGA from: %s\n", full_path);

            status = file_read_buffer(full_path, &buf, &buf_size);

            free(full_path);
            full_path = NULL;

            if (status != 0) {
                return status;
            }

            status = dev->backend->load_fpga(dev, buf, buf_size);
            if (status != 0) {
                log_warning("Failure loading FPGA: %s\n",
                            bladerf_strerror(status));
                return status;
            }

            board_data->state = STATE_FPGA_LOADED;
        } else {
            log_warning("FPGA bitstream file not found.\n");
            log_warning("Skipping further initialization...\n");
            return 0;
        }
    }

    /* Initialize the device before we try to interact with it.  In the case
     * of an autoloaded FPGA, we need to ensure the clocks are all running
     * before we can try to cancel any scheduled retunes or else the NIOS
     * hangs. */
    status = bladerf1_initialize(dev);
    if (status != 0) {
        return status;
    }

    if (have_cap(board_data->capabilities, BLADERF_CAP_SCHEDULED_RETUNE)) {
        /* Cancel any pending re-tunes that may have been left over as the
         * result of a user application crashing or forgetting to call
         * bladerf_close() */
        status =
            dev->board->cancel_scheduled_retunes(dev, BLADERF_CHANNEL_RX(0));
        if (status != 0) {
            log_warning("Failed to cancel any pending RX retunes: %s\n",
                        bladerf_strerror(status));
            return status;
        }

        status =
            dev->board->cancel_scheduled_retunes(dev, BLADERF_CHANNEL_TX(0));
        if (status != 0) {
            log_warning("Failed to cancel any pending TX retunes: %s\n",
                        bladerf_strerror(status));
            return status;
        }
    }

    return 0;
}

static void bladerf1_close(struct bladerf *dev)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    struct bladerf_flash_arch  *flash_arch = dev->flash_arch;
    int status;

    if (board_data) {
        sync_deinit(&board_data->sync[BLADERF_CHANNEL_RX(0)]);
        sync_deinit(&board_data->sync[BLADERF_CHANNEL_TX(0)]);

        status = dev->backend->is_fpga_configured(dev);
        if (status == 1 && have_cap(board_data->capabilities, BLADERF_CAP_SCHEDULED_RETUNE)) {
            /* We cancel schedule retunes here to avoid the device retuning
             * underneath the user, should they open it again in the future.
             *
             * This is intended to help developers avoid a situation during
             * debugging where they schedule "far" into the future, but then
             * hit a case where their program abort or exit early. If we didn't
             * cancel these scheduled retunes, they could potentially be left
             * wondering why the device is starting up or "unexpectedly"
             * switching to a different frequency later.
             */
            dev->board->cancel_scheduled_retunes(dev, BLADERF_CHANNEL_RX(0));
            dev->board->cancel_scheduled_retunes(dev, BLADERF_CHANNEL_TX(0));
        }

        /* Detach expansion board */
        switch (dev->xb) {
            case BLADERF_XB_100:
                xb100_detach(dev);
                break;
            case BLADERF_XB_200:
                xb200_detach(dev);
                break;
            case BLADERF_XB_300:
                xb300_detach(dev);
                break;
            default:
                break;
        }

        dc_cal_tbl_free(&board_data->cal.dc_rx);
        dc_cal_tbl_free(&board_data->cal.dc_tx);

        free(board_data);
        board_data = NULL;
    }

    if( flash_arch != NULL ) {
        free(flash_arch);
        flash_arch = NULL;
    }
}

/******************************************************************************/
/* Properties */
/******************************************************************************/

static bladerf_dev_speed bladerf1_device_speed(struct bladerf *dev)
{
    int status;
    bladerf_dev_speed usb_speed;

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    status = dev->backend->get_device_speed(dev, &usb_speed);
    if (status < 0) {
        return BLADERF_DEVICE_SPEED_UNKNOWN;
    }

    return usb_speed;
}

static int bladerf1_get_serial(struct bladerf *dev, char *serial)
{
    strcpy(serial, dev->ident.serial);

    return 0;
}

static int bladerf1_get_fpga_size(struct bladerf *dev, bladerf_fpga_size *size)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    *size = board_data->fpga_size;

    return 0;
}

static int bladerf1_get_fpga_bytes(struct bladerf *dev, size_t *size)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    switch (board_data->fpga_size) {
        case BLADERF_FPGA_40KLE:
            *size = 1191788;
            break;

        case BLADERF_FPGA_115KLE:
            *size = 3571462;
            break;

        default:
            log_debug("%s: unknown fpga_size: %x\n", board_data->fpga_size);
            return BLADERF_ERR_INVAL;
    }

    return 0;
}

static int bladerf1_get_flash_size(struct bladerf *dev, uint32_t *size, bool *is_guess)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    *size     = dev->flash_arch->tsize_bytes;
    *is_guess = (dev->flash_arch->status != STATUS_SUCCESS);

    return 0;
}

static int bladerf1_is_fpga_configured(struct bladerf *dev)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    return dev->backend->is_fpga_configured(dev);
}

static int bladerf1_get_fpga_source(struct bladerf *dev,
                                    bladerf_fpga_source *source)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    struct bladerf1_board_data *board_data = dev->board_data;

    if (!have_cap(board_data->capabilities, BLADERF_CAP_FW_FPGA_SOURCE)) {
        log_debug("%s: not supported by firmware\n", __FUNCTION__);
        *source = BLADERF_FPGA_SOURCE_UNKNOWN;
        return BLADERF_ERR_UNSUPPORTED;
    }

    *source = dev->backend->get_fpga_source(dev);

    return 0;
}

static uint64_t bladerf1_get_capabilities(struct bladerf *dev)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    return board_data->capabilities;
}

static size_t bladerf1_get_channel_count(struct bladerf *dev,
                                         bladerf_direction dir)
{
    return 1;
}

/******************************************************************************/
/* Versions */
/******************************************************************************/

static int bladerf1_get_fpga_version(struct bladerf *dev, struct bladerf_version *version)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    memcpy(version, &board_data->fpga_version, sizeof(*version));

    return 0;
}

static int bladerf1_get_fw_version(struct bladerf *dev, struct bladerf_version *version)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    memcpy(version, &board_data->fw_version, sizeof(*version));

    return 0;
}

/******************************************************************************/
/* Enable/disable */
/******************************************************************************/

static int perform_format_deconfig(struct bladerf *dev, bladerf_direction dir);

static int bladerf1_enable_module(struct bladerf *dev,
                                  bladerf_channel ch,
                                  bool enable)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (ch != BLADERF_CHANNEL_RX(0) && ch != BLADERF_CHANNEL_TX(0)) {
        return BLADERF_ERR_INVAL;
    }

    log_debug("Enable channel: %s - %s\n",
              BLADERF_CHANNEL_IS_TX(ch) ? "TX" : "RX",
              enable ? "True" : "False");

    if (enable == false) {
        sync_deinit(&board_data->sync[ch]);
        perform_format_deconfig(
            dev, BLADERF_CHANNEL_IS_TX(ch) ? BLADERF_TX : BLADERF_RX);
    }

    lms_enable_rffe(dev, ch, enable);
    status = dev->backend->enable_module(
        dev, BLADERF_CHANNEL_IS_TX(ch) ? BLADERF_TX : BLADERF_RX, enable);

    return status;
}

/******************************************************************************/
/* Gain */
/******************************************************************************/

/**
 * @brief      applies overall_gain to stage_gain, within the range max
 *
 * "Moves" gain from overall_gain to stage_gain, ensuring that overall_gain
 * doesn't go negative and stage_gain doesn't exceed range->max.
 *
 * @param[in]  range         The range for stage_gain
 * @param      stage_gain    The stage gain
 * @param      overall_gain  The overall gain
 */
static void _apportion_gain(struct bladerf_range const *range,
                            int *stage_gain,
                            int *overall_gain)
{
    int headroom  = __unscale_int(range, range->max) - *stage_gain;
    int allotment = (headroom >= *overall_gain) ? *overall_gain : headroom;

    /* Enforce step size */
    while (0 != (allotment % range->step)) {
        --allotment;
    }

    *stage_gain += allotment;
    *overall_gain -= allotment;
}

static bladerf_lna_gain _convert_gain_to_lna_gain(int gain)
{
    if (gain >= BLADERF_LNA_GAIN_MAX_DB) {
        return BLADERF_LNA_GAIN_MAX;
    }

    if (gain >= BLADERF_LNA_GAIN_MID_DB) {
        return BLADERF_LNA_GAIN_MID;
    }

    return BLADERF_LNA_GAIN_BYPASS;
}

static int _convert_lna_gain_to_gain(bladerf_lna_gain lnagain)
{
    switch (lnagain) {
        case BLADERF_LNA_GAIN_MAX:
            return BLADERF_LNA_GAIN_MAX_DB;
        case BLADERF_LNA_GAIN_MID:
            return BLADERF_LNA_GAIN_MID_DB;
        case BLADERF_LNA_GAIN_BYPASS:
            return 0;
        default:
            return -1;
    }
}

static int bladerf1_get_gain_stage_range(struct bladerf *dev,
                                         bladerf_channel ch,
                                         char const *stage,
                                         struct bladerf_range const **range)
{
    struct bladerf_gain_stage_info const *stage_infos;
    unsigned int stage_infos_len;
    size_t i;

    if (NULL == stage) {
        log_error("%s: stage is null\n", __FUNCTION__);
        return BLADERF_ERR_INVAL;
    }

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        stage_infos     = bladerf1_tx_gain_stages;
        stage_infos_len = ARRAY_SIZE(bladerf1_tx_gain_stages);
    } else {
        stage_infos     = bladerf1_rx_gain_stages;
        stage_infos_len = ARRAY_SIZE(bladerf1_rx_gain_stages);
    }

    for (i = 0; i < stage_infos_len; i++) {
        if (strcmp(stage_infos[i].name, stage) == 0) {
            if (NULL != range) {
                *range = &(stage_infos[i].range);
            }
            return 0;
        }
    }

    return BLADERF_ERR_INVAL;
}

static int set_rx_gain(struct bladerf *dev, int gain)
{
    struct bladerf_range const *lna_range    = NULL;
    struct bladerf_range const *rxvga1_range = NULL;
    struct bladerf_range const *rxvga2_range = NULL;
    bladerf_channel const ch                 = BLADERF_CHANNEL_RX(0);
    int orig_gain                            = gain;
    int lna, rxvga1, rxvga2;
    int status;

    // get our gain stage ranges!
    status = bladerf1_get_gain_stage_range(dev, ch, "lna", &lna_range);
    if (status < 0) {
        return status;
    }

    status = bladerf1_get_gain_stage_range(dev, ch, "rxvga1", &rxvga1_range);
    if (status < 0) {
        return status;
    }

    status = bladerf1_get_gain_stage_range(dev, ch, "rxvga2", &rxvga2_range);
    if (status < 0) {
        return status;
    }

    lna    = __unscale_int(lna_range, lna_range->min);
    rxvga1 = __unscale_int(rxvga1_range, rxvga1_range->min);
    rxvga2 = __unscale_int(rxvga2_range, rxvga2_range->min);

    // offset gain so that we can use it as a counter when apportioning gain
    gain -= __round_int((BLADERF1_RX_GAIN_OFFSET +
                         __unscale_int(lna_range, lna_range->min) +
                         __unscale_int(rxvga1_range, rxvga1_range->min) +
                         __unscale_int(rxvga2_range, rxvga2_range->min)));

    // apportion some gain to RXLNA (but only half of it for now)
    _apportion_gain(lna_range, &lna, &gain);
    if (lna > BLADERF_LNA_GAIN_MID_DB) {
        gain += (lna - BLADERF_LNA_GAIN_MID_DB);
        lna -= (lna - BLADERF_LNA_GAIN_MID_DB);
    }

    // apportion gain to RXVGA1
    _apportion_gain(rxvga1_range, &rxvga1, &gain);

    // apportion more gain to RXLNA
    _apportion_gain(lna_range, &lna, &gain);

    // apportion gain to RXVGA2
    _apportion_gain(rxvga2_range, &rxvga2, &gain);

    // if we still have remaining gain, it's because rxvga2 has a step size of
    // 3 dB. Steal a few dB from rxvga1...
    if (gain > 0 && rxvga1 >= __unscale_int(rxvga1_range, rxvga1_range->max)) {
        rxvga1 -= __unscale_int(rxvga2_range, rxvga2_range->step);
        gain += __unscale_int(rxvga2_range, rxvga2_range->step);

        _apportion_gain(rxvga2_range, &rxvga2, &gain);
        _apportion_gain(rxvga1_range, &rxvga1, &gain);
    }

    // verification
    if (gain != 0) {
        log_warning("%s: unable to achieve requested gain %d (missed by %d)\n",
                    __FUNCTION__, orig_gain, gain);
        log_debug("%s: gain=%d -> rxvga1=%d lna=%d rxvga2=%d remainder=%d\n",
                  __FUNCTION__, orig_gain, rxvga1, lna, rxvga2, gain);
    }

    // that should do it.  actually apply the changes:
    status = lms_lna_set_gain(dev, _convert_gain_to_lna_gain(lna));
    if (status < 0) {
        return status;
    }

    status = lms_rxvga1_set_gain(dev, __scale_int(rxvga1_range, rxvga1));
    if (status < 0) {
        return status;
    }

    status = lms_rxvga2_set_gain(dev, __scale_int(rxvga2_range, rxvga2));
    if (status < 0) {
        return status;
    }

    return 0;
}

static int get_rx_gain(struct bladerf *dev, int *gain)
{
    int status;
    bladerf_lna_gain lnagain;
    int lnagain_db;
    int rxvga1;
    int rxvga2;

    status = lms_lna_get_gain(dev, &lnagain);
    if (status < 0) {
        return status;
    }

    status = lms_rxvga1_get_gain(dev, &rxvga1);
    if (status < 0) {
        return status;
    }

    status = lms_rxvga2_get_gain(dev, &rxvga2);
    if (status < 0) {
        return status;
    }

    switch (lnagain) {
        case BLADERF_LNA_GAIN_BYPASS:
            lnagain_db = 0;
            break;

        case BLADERF_LNA_GAIN_MID:
            lnagain_db = BLADERF_LNA_GAIN_MID_DB;
            break;

        case BLADERF_LNA_GAIN_MAX:
            lnagain_db = BLADERF_LNA_GAIN_MAX_DB;
            break;

        default:
            return BLADERF_ERR_UNEXPECTED;
    }

    *gain = __round_int(lnagain_db + rxvga1 + rxvga2 + BLADERF1_RX_GAIN_OFFSET);

    return 0;
}

static int set_tx_gain(struct bladerf *dev, int gain)
{
    struct bladerf_range const *txvga1_range = NULL;
    struct bladerf_range const *txvga2_range = NULL;
    bladerf_channel const ch                 = BLADERF_CHANNEL_TX(0);
    int orig_gain                            = gain;
    int txvga1, txvga2;
    int status;

    // get our gain stage ranges!
    status = bladerf1_get_gain_stage_range(dev, ch, "txvga1", &txvga1_range);
    if (status < 0) {
        return status;
    }

    status = bladerf1_get_gain_stage_range(dev, ch, "txvga2", &txvga2_range);
    if (status < 0) {
        return status;
    }

    txvga1 = __unscale_int(txvga1_range, txvga1_range->min);
    txvga2 = __unscale_int(txvga2_range, txvga2_range->min);

    // offset gain so that we can use it as a counter when apportioning gain
    gain -= __round_int((BLADERF1_TX_GAIN_OFFSET +
                         __unscale_int(txvga1_range, txvga1_range->min) +
                         __unscale_int(txvga2_range, txvga2_range->min)));

    // apportion gain to TXVGA2
    _apportion_gain(txvga2_range, &txvga2, &gain);

    // apportion gain to TXVGA1
    _apportion_gain(txvga1_range, &txvga1, &gain);

    // verification
    if (gain != 0) {
        log_warning("%s: unable to achieve requested gain %d (missed by %d)\n",
                    __FUNCTION__, orig_gain, gain);
        log_debug("%s: gain=%d -> txvga2=%d txvga1=%d remainder=%d\n",
                  __FUNCTION__, orig_gain, txvga2, txvga1, gain);
    }

    status = lms_txvga1_set_gain(dev, txvga1);
    if (status < 0) {
        return status;
    }

    status = lms_txvga2_set_gain(dev, txvga2);
    if (status < 0) {
        return status;
    }

    return 0;
}

static int get_tx_gain(struct bladerf *dev, int *gain)
{
    int status;
    int txvga1;
    int txvga2;

    status = lms_txvga1_get_gain(dev, &txvga1);
    if (status < 0) {
        return status;
    }

    status = lms_txvga2_get_gain(dev, &txvga2);
    if (status < 0) {
        return status;
    }

    *gain = __round_int(txvga1 + txvga2 + BLADERF1_TX_GAIN_OFFSET);

    return 0;
}

static int bladerf1_set_gain(struct bladerf *dev, bladerf_channel ch, int gain)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (BLADERF_CHANNEL_TX(0) == ch) {
        return set_tx_gain(dev, gain);
    } else if (BLADERF_CHANNEL_RX(0) == ch) {
        return set_rx_gain(dev, gain);
    }

    return BLADERF_ERR_INVAL;
}

static int bladerf1_set_gain_mode(struct bladerf *dev,
                                  bladerf_module mod,
                                  bladerf_gain_mode mode)
{
    int status;
    uint32_t config_gpio;
    struct bladerf1_board_data *board_data = dev->board_data;

    uint32_t const TABLE_VERSION = 2; /* Required RX DC cal table version */

    /* Strings used in AGC-unavailable warnings */
    char const *MGC_WARN = "Manual gain control will be used instead.";
    char const *FPGA_STR = "download and install FPGA v0.7.0 or newer from "
                           "https://nuand.com/fpga/";
    char const *DCCAL_STR = "see \"Generating a DC offset table\" at "
                            "https://github.com/Nuand/bladeRF/wiki/"
                            "DC-offset-and-IQ-Imbalance-Correction";

    if (mod != BLADERF_MODULE_RX) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    if ((status = dev->backend->config_gpio_read(dev, &config_gpio))) {
        return status;
    }

    if (mode == BLADERF_GAIN_AUTOMATIC || mode == BLADERF_GAIN_DEFAULT) {
        if (!have_cap(board_data->capabilities, BLADERF_CAP_AGC_DC_LUT)) {
            log_warning("AGC not supported by FPGA. %s\n", MGC_WARN);
            log_info("To enable AGC, %s, then %s\n", FPGA_STR, DCCAL_STR);
            log_debug("%s: expected FPGA >= v0.7.0, got v%u.%u.%u\n",
                      __FUNCTION__, board_data->fpga_version.major,
                      board_data->fpga_version.minor,
                      board_data->fpga_version.patch);
            return BLADERF_ERR_UNSUPPORTED;
        }

        if (!board_data->cal.dc_rx) {
            log_warning("RX DC calibration table not found. %s\n", MGC_WARN);
            log_info("To enable AGC, %s\n", DCCAL_STR);
            return BLADERF_ERR_UNSUPPORTED;
        }

        if (board_data->cal.dc_rx->version != TABLE_VERSION) {
            log_warning("RX DC calibration table is out-of-date. %s\n",
                        MGC_WARN);
            log_info("To enable AGC, %s\n", DCCAL_STR);
            log_debug("%s: expected version %u, got %u\n", __FUNCTION__,
                      TABLE_VERSION, board_data->cal.dc_rx->version);

            return BLADERF_ERR_UNSUPPORTED;
        }

        config_gpio |= BLADERF_GPIO_AGC_ENABLE;
    } else if (mode == BLADERF_GAIN_MANUAL || mode == BLADERF_GAIN_MGC) {
        config_gpio &= ~BLADERF_GPIO_AGC_ENABLE;
    }

    return dev->backend->config_gpio_write(dev, config_gpio);
}

static int bladerf1_get_gain_mode(struct bladerf *dev,
                                  bladerf_module mod,
                                  bladerf_gain_mode *mode)
{
    int status;
    uint32_t config_gpio;

    if ((status = dev->backend->config_gpio_read(dev, &config_gpio))) {
        return status;
    }

    if (config_gpio & BLADERF_GPIO_AGC_ENABLE) {
        *mode = BLADERF_GAIN_DEFAULT;
    } else {
        *mode = BLADERF_GAIN_MGC;
    }

    return status;
}

static int bladerf1_get_gain(struct bladerf *dev, bladerf_channel ch, int *gain)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (BLADERF_CHANNEL_TX(0) == ch) {
        return get_tx_gain(dev, gain);
    } else if (BLADERF_CHANNEL_RX(0) == ch) {
        return get_rx_gain(dev, gain);
    }

    return BLADERF_ERR_INVAL;
}

static int bladerf1_get_gain_modes(struct bladerf *dev,
                                   bladerf_channel ch,
                                   struct bladerf_gain_modes const **modes)
{
    struct bladerf_gain_modes const *mode_infos;
    unsigned int mode_infos_len;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        mode_infos     = NULL;
        mode_infos_len = 0;
    } else {
        mode_infos     = bladerf1_rx_gain_modes;
        mode_infos_len = ARRAY_SIZE(bladerf1_rx_gain_modes);
    }

    if (modes != NULL) {
        *modes = mode_infos;
    }

    return mode_infos_len;
}

static int bladerf1_get_gain_range(struct bladerf *dev,
                                   bladerf_channel ch,
                                   struct bladerf_range const **range)
{
    if (BLADERF_CHANNEL_IS_TX(ch)) {
        *range = &bladerf1_tx_gain_range;
    } else {
        *range = &bladerf1_rx_gain_range;
    }

    return 0;
}

static int bladerf1_set_gain_stage(struct bladerf *dev,
                                   bladerf_channel ch,
                                   char const *stage,
                                   int gain)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    /* TODO implement gain clamping */

    switch (ch) {
        case BLADERF_CHANNEL_TX(0):
            if (strcmp(stage, "txvga1") == 0) {
                return lms_txvga1_set_gain(dev, gain);
            } else if (strcmp(stage, "txvga2") == 0) {
                return lms_txvga2_set_gain(dev, gain);
            } else {
                log_warning("%s: gain stage '%s' invalid\n", __FUNCTION__,
                            stage);
                return 0;
            }

        case BLADERF_CHANNEL_RX(0):
            if (strcmp(stage, "rxvga1") == 0) {
                return lms_rxvga1_set_gain(dev, gain);
            } else if (strcmp(stage, "rxvga2") == 0) {
                return lms_rxvga2_set_gain(dev, gain);
            } else if (strcmp(stage, "lna") == 0) {
                return lms_lna_set_gain(dev, _convert_gain_to_lna_gain(gain));
            } else {
                log_warning("%s: gain stage '%s' invalid\n", __FUNCTION__,
                            stage);
                return 0;
            }

        default:
            log_error("%s: channel %d invalid\n", __FUNCTION__, ch);
            return BLADERF_ERR_INVAL;
    }
}

static int bladerf1_get_gain_stage(struct bladerf *dev,
                                   bladerf_channel ch,
                                   char const *stage,
                                   int *gain)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    switch (ch) {
        case BLADERF_CHANNEL_TX(0):
            if (strcmp(stage, "txvga1") == 0) {
                return lms_txvga1_get_gain(dev, gain);
            } else if (strcmp(stage, "txvga2") == 0) {
                return lms_txvga2_get_gain(dev, gain);
            } else {
                log_warning("%s: gain stage '%s' invalid\n", __FUNCTION__,
                            stage);
                return 0;
            }

        case BLADERF_CHANNEL_RX(0):
            if (strcmp(stage, "rxvga1") == 0) {
                return lms_rxvga1_get_gain(dev, gain);
            } else if (strcmp(stage, "rxvga2") == 0) {
                return lms_rxvga2_get_gain(dev, gain);
            } else if (strcmp(stage, "lna") == 0) {
                int status;
                bladerf_lna_gain lnagain;
                status = lms_lna_get_gain(dev, &lnagain);
                if (status == 0) {
                    *gain = _convert_lna_gain_to_gain(lnagain);
                }
                return status;
            } else {
                log_warning("%s: gain stage '%s' invalid\n", __FUNCTION__,
                            stage);
                return 0;
            }

        default:
            log_error("%s: channel %d invalid\n", __FUNCTION__, ch);
            return BLADERF_ERR_INVAL;
    }
}

static int bladerf1_get_gain_stages(struct bladerf *dev,
                                    bladerf_channel ch,
                                    char const **stages,
                                    size_t count)
{
    struct bladerf_gain_stage_info const *stage_infos;
    unsigned int stage_infos_len;
    unsigned int i;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        stage_infos     = bladerf1_tx_gain_stages;
        stage_infos_len = ARRAY_SIZE(bladerf1_tx_gain_stages);
    } else {
        stage_infos     = bladerf1_rx_gain_stages;
        stage_infos_len = ARRAY_SIZE(bladerf1_rx_gain_stages);
    }

    if (stages != NULL) {
        count = (stage_infos_len < count) ? stage_infos_len : count;

        for (i = 0; i < count; i++) {
            stages[i] = stage_infos[i].name;
        }
    }

    return stage_infos_len;
}

/******************************************************************************/
/* Sample Rate */
/******************************************************************************/

static int bladerf1_set_sample_rate(struct bladerf *dev, bladerf_channel ch, unsigned int rate, unsigned int *actual)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return si5338_set_sample_rate(dev, ch, rate, actual);
}

static int bladerf1_get_sample_rate(struct bladerf *dev, bladerf_channel ch, unsigned int *rate)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return si5338_get_sample_rate(dev, ch, rate);
}

static int bladerf1_get_sample_rate_range(struct bladerf *dev, bladerf_channel ch, const struct bladerf_range **range)
{
    *range = &bladerf1_sample_rate_range;
    return 0;
}

static int bladerf1_set_rational_sample_rate(struct bladerf *dev, bladerf_channel ch, struct bladerf_rational_rate *rate, struct bladerf_rational_rate *actual)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return si5338_set_rational_sample_rate(dev, ch, rate, actual);
}

static int bladerf1_get_rational_sample_rate(struct bladerf *dev, bladerf_channel ch, struct bladerf_rational_rate *rate)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return si5338_get_rational_sample_rate(dev, ch, rate);
}

/******************************************************************************/
/* Bandwidth */
/******************************************************************************/

static int bladerf1_set_bandwidth(struct bladerf *dev,
                                  bladerf_channel ch,
                                  bladerf_bandwidth bandwidth,
                                  bladerf_bandwidth *actual)
{
    int status;
    lms_bw bw;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (bandwidth < BLADERF_BANDWIDTH_MIN) {
        bandwidth = BLADERF_BANDWIDTH_MIN;
        log_info("Clamping bandwidth to %d Hz\n", bandwidth);
    } else if (bandwidth > BLADERF_BANDWIDTH_MAX) {
        bandwidth = BLADERF_BANDWIDTH_MAX;
        log_info("Clamping bandwidth to %d Hz\n", bandwidth);
    }

    bw = lms_uint2bw(bandwidth);

    status = lms_lpf_enable(dev, ch, true);
    if (status != 0) {
        return status;
    }

    status = lms_set_bandwidth(dev, ch, bw);
    if (actual != NULL) {
        if (status == 0) {
            *actual = lms_bw2uint(bw);
        } else {
            *actual = 0;
        }
    }

    return status;
}

static int bladerf1_get_bandwidth(struct bladerf *dev,
                                  bladerf_channel ch,
                                  bladerf_bandwidth *bandwidth)
{
    int status;
    lms_bw bw;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    status = lms_get_bandwidth(dev, ch, &bw);
    if (status == 0) {
        *bandwidth = lms_bw2uint(bw);
    } else {
        *bandwidth = 0;
    }

    return status;
}

static int bladerf1_get_bandwidth_range(struct bladerf *dev,
                                        bladerf_channel ch,
                                        const struct bladerf_range **range)
{
    *range = &bladerf1_bandwidth_range;
    return 0;
}

/******************************************************************************/
/* Frequency */
/******************************************************************************/

static int bladerf1_set_frequency(struct bladerf *dev,
                                  bladerf_channel ch,
                                  bladerf_frequency frequency)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    const bladerf_xb attached              = dev->xb;
    int status;
    int16_t dc_i, dc_q;
    struct dc_cal_entry entry;
    const struct dc_cal_tbl *dc_cal = (ch == BLADERF_CHANNEL_RX(0))
                                          ? board_data->cal.dc_rx
                                          : board_data->cal.dc_tx;

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    log_debug("Setting %s frequency to %" BLADERF_PRIuFREQ "\n",
              channel2str(ch), frequency);

    if (attached == BLADERF_XB_200) {
        if (frequency < BLADERF_FREQUENCY_MIN) {
            status = xb200_set_path(dev, ch, BLADERF_XB200_MIX);
            if (status) {
                return status;
            }

            status = xb200_auto_filter_selection(dev, ch, frequency);
            if (status) {
                return status;
            }

            frequency = 1248000000 - frequency;
        } else {
            status = xb200_set_path(dev, ch, BLADERF_XB200_BYPASS);
            if (status) {
                return status;
            }
        }
    }

    switch (board_data->tuning_mode) {
        case BLADERF_TUNING_MODE_HOST:
            status = lms_set_frequency(dev, ch, (uint32_t)frequency);
            if (status != 0) {
                return status;
            }

            status = band_select(dev, ch, frequency < BLADERF1_BAND_HIGH);
            break;

        case BLADERF_TUNING_MODE_FPGA: {
            status = dev->board->schedule_retune(dev, ch, BLADERF_RETUNE_NOW,
                                                 frequency, NULL);
            break;
        }

        default:
            log_debug("Invalid tuning mode: %d\n", board_data->tuning_mode);
            status = BLADERF_ERR_INVAL;
            break;
    }
    if (status != 0) {
        return status;
    }

    if (dc_cal != NULL) {
        dc_cal_tbl_entry(dc_cal, (uint32_t)frequency, &entry);

        dc_i = entry.dc_i;
        dc_q = entry.dc_q;

        status = lms_set_dc_offset_i(dev, ch, dc_i);
        if (status != 0) {
            return status;
        }

        status = lms_set_dc_offset_q(dev, ch, dc_q);
        if (status != 0) {
            return status;
        }

        if (ch == BLADERF_CHANNEL_RX(0) &&
            have_cap(board_data->capabilities, BLADERF_CAP_AGC_DC_LUT)) {
            status = dev->backend->set_agc_dc_correction(
                dev, entry.max_dc_q, entry.max_dc_i, entry.mid_dc_q,
                entry.mid_dc_i, entry.min_dc_q, entry.min_dc_i);
            if (status != 0) {
                return status;
            }

            log_verbose("Set AGC DC offset cal (I, Q) to: Max (%d, %d) "
                        " Mid (%d, %d) Min (%d, %d)\n",
                        entry.max_dc_q, entry.max_dc_i, entry.mid_dc_q,
                        entry.mid_dc_i, entry.min_dc_q, entry.min_dc_i);
        }

        log_verbose("Set %s DC offset cal (I, Q) to: (%d, %d)\n",
                    (ch == BLADERF_CHANNEL_RX(0)) ? "RX" : "TX", dc_i, dc_q);
    }

    return 0;
}

static int bladerf1_get_frequency(struct bladerf *dev,
                                  bladerf_channel ch,
                                  bladerf_frequency *frequency)
{
    bladerf_xb200_path path;
    struct lms_freq f;
    int status = 0;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    status = lms_get_frequency(dev, ch, &f);
    if (status != 0) {
        return status;
    }

    if (f.x == 0) {
        /* If we see this, it's most often an indication that communication
         * with the LMS6002D is not occuring correctly */
        *frequency = 0;
        status     = BLADERF_ERR_IO;
    } else {
        *frequency = lms_frequency_to_hz(&f);
    }
    if (status != 0) {
        return status;
    }

    if (dev->xb == BLADERF_XB_200) {
        status = xb200_get_path(dev, ch, &path);
        if (status != 0) {
            return status;
        }
        if (path == BLADERF_XB200_MIX) {
            *frequency = 1248000000 - *frequency;
        }
    }

    return 0;
}

static int bladerf1_get_frequency_range(struct bladerf *dev,
                                        bladerf_channel ch,
                                        const struct bladerf_range **range)
{
    if (dev->xb == BLADERF_XB_200) {
        *range = &bladerf1_xb200_frequency_range;
    } else {
        *range = &bladerf1_frequency_range;
    }

    return 0;
}

static int bladerf1_select_band(struct bladerf *dev,
                                bladerf_channel ch,
                                bladerf_frequency frequency)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return band_select(dev, ch, frequency < BLADERF1_BAND_HIGH);
}

/******************************************************************************/
/* RF ports */
/******************************************************************************/

static int bladerf1_set_rf_port(struct bladerf *dev,
                                bladerf_channel ch,
                                const char *port)
{
    const struct bladerf_lms_port_name_map *port_map;
    unsigned int port_map_len;
    int status;
    size_t i;

    lms_lna rx_lna = LNA_NONE;
    lms_pa tx_pa   = PA_NONE;
    bool ok        = false;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    /* TODO: lms_pa_enable is not currently implemented */
    if (BLADERF_CHANNEL_IS_TX(ch)) {
        log_debug("%s: not implemented for TX channels, silently ignoring\n",
                  __FUNCTION__);
        return 0;
    }

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        port_map     = bladerf1_tx_port_map;
        port_map_len = ARRAY_SIZE(bladerf1_tx_port_map);
    } else {
        port_map     = bladerf1_rx_port_map;
        port_map_len = ARRAY_SIZE(bladerf1_rx_port_map);
    }

    for (i = 0; i < port_map_len; i++) {
        if (strcmp(port_map[i].name, port) == 0) {
            if (BLADERF_CHANNEL_IS_TX(ch)) {
                tx_pa = port_map[i].tx_pa;
            } else {
                rx_lna = port_map[i].rx_lna;
            }
            ok = true;
            break;
        }
    }

    if (!ok) {
        log_error("port '%s' not valid for channel %s\n", port,
                  channel2str(ch));
        return BLADERF_ERR_INVAL;
    }

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        for (i = 0; i < port_map_len; i++) {
            bool enable = (port_map[i].tx_pa == tx_pa);
#if 0
            status = lms_pa_enable(dev, port_map[i].tx_pa, enable);
#else
            log_verbose("%s: would %s pa %d but this is not implemented\n",
                        __FUNCTION__, enable ? "enable" : "disable", tx_pa);
            status = 0;
#endif  // 0
            if (status < 0) {
                break;
            }
        }
    } else {
        status = lms_select_lna(dev, rx_lna);
    }

    return status;
}

static int bladerf1_get_rf_port(struct bladerf *dev,
                                bladerf_channel ch,
                                const char **port)
{
    const struct bladerf_lms_port_name_map *port_map;
    unsigned int port_map_len;
    int status;
    size_t i;

    lms_lna rx_lna = LNA_NONE;
    lms_pa tx_pa   = PA_NONE;
    bool ok        = false;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    /* TODO: pa getter not currently implemented */
    if (BLADERF_CHANNEL_IS_TX(ch)) {
        log_debug("%s: not implemented for TX channels\n", __FUNCTION__);
        if (port != NULL) {
            *port = "auto";
        }
        return 0;
    }

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        port_map     = bladerf1_tx_port_map;
        port_map_len = ARRAY_SIZE(bladerf1_tx_port_map);

#if 0
        status = lms_get_pa(dev, &tx_pa);
#else
        status = 0;
#endif  // 0
    } else {
        port_map     = bladerf1_rx_port_map;
        port_map_len = ARRAY_SIZE(bladerf1_rx_port_map);

        status = lms_get_lna(dev, &rx_lna);
    }

    if (status < 0) {
        return status;
    }

    if (port != NULL) {
        for (i = 0; i < port_map_len; i++) {
            if (BLADERF_CHANNEL_IS_TX(ch)) {
                if (tx_pa == port_map[i].tx_pa) {
                    *port = port_map[i].name;
                    ok    = true;
                    break;
                }
            } else {
                if (rx_lna == port_map[i].rx_lna) {
                    *port = port_map[i].name;
                    ok    = true;
                    break;
                }
            }
        }
    }

    if (!ok) {
        if (port != NULL) {
            *port = "unknown";
        }
        log_error("%s: unexpected port id %d\n", __FUNCTION__,
                  BLADERF_CHANNEL_IS_TX(ch) ? tx_pa : rx_lna);
        return BLADERF_ERR_UNEXPECTED;
    }

    return 0;
}

static int bladerf1_get_rf_ports(struct bladerf *dev,
                                 bladerf_channel ch,
                                 const char **ports,
                                 unsigned int count)
{
    const struct bladerf_lms_port_name_map *port_map;
    unsigned int port_map_len;
    size_t i;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        /* Return a null list instead of bladerf1_tx_port_map */
        port_map     = NULL;
        port_map_len = 0;
    } else {
        port_map     = bladerf1_rx_port_map;
        port_map_len = ARRAY_SIZE(bladerf1_rx_port_map);
    }

    if (ports != NULL) {
        count = (port_map_len < count) ? port_map_len : count;

        for (i = 0; i < count; i++) {
            ports[i] = port_map[i].name;
        }
    }

    return port_map_len;
}

/******************************************************************************/
/* Scheduled Tuning */
/******************************************************************************/

static int bladerf1_get_quick_tune(struct bladerf *dev,
                                   bladerf_channel ch,
                                   struct bladerf_quick_tune *quick_tune)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return lms_get_quick_tune(dev, ch, quick_tune);
}

static int bladerf1_schedule_retune(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bladerf_timestamp timestamp,
                                    bladerf_frequency frequency,
                                    struct bladerf_quick_tune *quick_tune)

{
    struct bladerf1_board_data *board_data = dev->board_data;
    int status;
    struct lms_freq f;

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    if (!have_cap(board_data->capabilities, BLADERF_CAP_SCHEDULED_RETUNE)) {
        log_debug("This FPGA version (%u.%u.%u) does not support "
                  "scheduled retunes.\n",
                  board_data->fpga_version.major,
                  board_data->fpga_version.minor,
                  board_data->fpga_version.patch);

        return BLADERF_ERR_UNSUPPORTED;
    }

    if (quick_tune == NULL) {
        if (dev->xb == BLADERF_XB_200) {
           log_error("Consider supplying the quick_tune parameter to"
                 " bladerf_schedule_retune() when the XB-200 is enabled.\n");
        }
        status = lms_calculate_tuning_params((uint32_t)frequency, &f);
        if (status != 0) {
            return status;
        }
    } else {
        f.freqsel       = quick_tune->freqsel;
        f.vcocap        = quick_tune->vcocap;
        f.nint          = quick_tune->nint;
        f.nfrac         = quick_tune->nfrac;
        f.flags         = quick_tune->flags;
        f.xb_gpio       = quick_tune->xb_gpio;
        f.x             = 0;
        f.vcocap_result = 0;
    }

    return dev->backend->retune(dev, ch, timestamp, f.nint, f.nfrac, f.freqsel,
                                f.vcocap,
                                (f.flags & LMS_FREQ_FLAGS_LOW_BAND) != 0,
                                f.xb_gpio,
                                (f.flags & LMS_FREQ_FLAGS_FORCE_VCOCAP) != 0);
}

static int bladerf1_cancel_scheduled_retunes(struct bladerf *dev,
                                             bladerf_channel ch)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    int status;

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    if (have_cap(board_data->capabilities, BLADERF_CAP_SCHEDULED_RETUNE)) {
        status = dev->backend->retune(dev, ch, NIOS_PKT_RETUNE_CLEAR_QUEUE, 0,
                                      0, 0, 0, false, 0, false);
    } else {
        log_debug("This FPGA version (%u.%u.%u) does not support "
                  "scheduled retunes.\n",
                  board_data->fpga_version.major,
                  board_data->fpga_version.minor,
                  board_data->fpga_version.patch);

        return BLADERF_ERR_UNSUPPORTED;
    }

    return status;
}

/******************************************************************************/
/* DC/Phase/Gain Correction */
/******************************************************************************/

static int bladerf1_get_correction(struct bladerf *dev, bladerf_channel ch,
                                   bladerf_correction corr, int16_t *value)
{
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    switch (corr) {
        case BLADERF_CORR_PHASE:
            status = dev->backend->get_iq_phase_correction(dev, ch, value);
            break;

        case BLADERF_CORR_GAIN:
            status = dev->backend->get_iq_gain_correction(dev, ch, value);

            /* Undo the gain control offset */
            if (status == 0) {
                *value -= 4096;
            }
            break;

        case BLADERF_CORR_DCOFF_I:
            status = lms_get_dc_offset_i(dev, ch, value);
            break;

        case BLADERF_CORR_DCOFF_Q:
            status = lms_get_dc_offset_q(dev, ch, value);
            break;

        default:
            status = BLADERF_ERR_INVAL;
            log_debug("Invalid correction type: %d\n", corr);
            break;
    }

    return status;
}

static int bladerf1_set_correction(struct bladerf *dev, bladerf_channel ch,
                                   bladerf_correction corr, int16_t value)
{
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    switch (corr) {
        case BLADERF_CORR_PHASE:
            status = dev->backend->set_iq_phase_correction(dev, ch, value);
            break;

        case BLADERF_CORR_GAIN:
            /* Gain correction requires than an offset be applied */
            value += (int16_t) 4096;
            status = dev->backend->set_iq_gain_correction(dev, ch, value);
            break;

        case BLADERF_CORR_DCOFF_I:
            status = lms_set_dc_offset_i(dev, ch, value);
            break;

        case BLADERF_CORR_DCOFF_Q:
            status = lms_set_dc_offset_q(dev, ch, value);
            break;

        default:
            status = BLADERF_ERR_INVAL;
            log_debug("Invalid correction type: %d\n", corr);
            break;
    }

    return status;
}

/******************************************************************************/
/* Trigger */
/******************************************************************************/

static int bladerf1_trigger_init(struct bladerf *dev, bladerf_channel ch, bladerf_trigger_signal signal, struct bladerf_trigger *trigger)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (!have_cap(board_data->capabilities, BLADERF_CAP_TRX_SYNC_TRIG)) {
        log_debug("FPGA v%s does not support synchronization triggers.\n",
                  board_data->fpga_version.describe);
        return BLADERF_ERR_UNSUPPORTED;
    }

    return fpga_trigger_init(dev, ch, signal, trigger);
}

static int bladerf1_trigger_arm(struct bladerf *dev, const struct bladerf_trigger *trigger, bool arm, uint64_t resv1, uint64_t resv2)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (!have_cap(board_data->capabilities, BLADERF_CAP_TRX_SYNC_TRIG)) {
        log_debug("FPGA v%s does not support synchronization triggers.\n",
                  board_data->fpga_version.describe);
        return BLADERF_ERR_UNSUPPORTED;
    }

    /* resv1 & resv2 unused - may be allocated for use as timestamp and
     * other flags in the future */

    return fpga_trigger_arm(dev, trigger, arm);
}

static int bladerf1_trigger_fire(struct bladerf *dev, const struct bladerf_trigger *trigger)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (!have_cap(board_data->capabilities, BLADERF_CAP_TRX_SYNC_TRIG)) {
        log_debug("FPGA v%s does not support synchronization triggers.\n",
                  board_data->fpga_version.describe);
        return BLADERF_ERR_UNSUPPORTED;
    }

    return fpga_trigger_fire(dev, trigger);
}

static int bladerf1_trigger_state(struct bladerf *dev, const struct bladerf_trigger *trigger, bool *is_armed, bool *has_fired, bool *fire_requested, uint64_t *resv1, uint64_t *resv2)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (!have_cap(board_data->capabilities, BLADERF_CAP_TRX_SYNC_TRIG)) {
        log_debug("FPGA v%s does not support synchronization triggers.\n",
                  board_data->fpga_version.describe);
        return BLADERF_ERR_UNSUPPORTED;
    }

    status = fpga_trigger_state(dev, trigger, is_armed, has_fired, fire_requested);

    /* Reserved for future metadata (e.g., trigger counts, timestamp) */
    if (resv1 != NULL) {
        *resv1 = 0;
    }

    if (resv2 != NULL) {
        *resv2 = 0;
    }

    return status;
}

/******************************************************************************/
/* Streaming */
/******************************************************************************/

static inline int requires_timestamps(bladerf_format format, bool *required)
{
    int status = 0;

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

    return status;
}

/**
 * Perform the neccessary device configuration for the specified format
 * (e.g., enabling/disabling timestamp support), first checking that the
 * requested format would not conflict with the other stream direction.
 *
 * @param           dev     Device handle
 * @param[in]       dir     Direction that is currently being configured
 * @param[in]       format  Format the channel is being configured for
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
static int perform_format_config(struct bladerf *dev, bladerf_direction dir,
                                 bladerf_format format)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    int status = 0;
    bool use_timestamps;
    bladerf_channel other;
    bool other_using_timestamps;
    uint32_t gpio_val;

    status = requires_timestamps(format, &use_timestamps);
    if (status != 0) {
        log_debug("%s: Invalid format: %d\n", __FUNCTION__, format);
        return status;
    }

    if (use_timestamps && !have_cap(board_data->capabilities, BLADERF_CAP_TIMESTAMPS)) {
        log_warning("Timestamp support requires FPGA v0.1.0 or later.\n");
        return BLADERF_ERR_UPDATE_FPGA;
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

    status = dev->backend->config_gpio_read(dev, &gpio_val);
    if (status != 0) {
        return status;
    }

    if (use_timestamps) {
        gpio_val |= (BLADERF_GPIO_TIMESTAMP | BLADERF_GPIO_TIMESTAMP_DIV2);
    } else {
        gpio_val &= ~(BLADERF_GPIO_TIMESTAMP | BLADERF_GPIO_TIMESTAMP_DIV2);
    }

    status = dev->backend->config_gpio_write(dev, gpio_val);
    if (status == 0) {
        board_data->module_format[dir] = format;
    }

    return status;
}

/**
 * Deconfigure and update any state pertaining what a format that a stream
 * direction is no longer using.
 *
 * @param       dev     Device handle
 * @param[in]   dir     Direction that is currently being deconfigured
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
static int perform_format_deconfig(struct bladerf *dev, bladerf_direction dir)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    switch (dir) {
        case BLADERF_RX:
        case BLADERF_TX:
            /* We'll reconfigure the HW when we call perform_format_config, so
             * we just need to update our stored information */
            board_data->module_format[dir] = -1;
            break;

        default:
            log_debug("%s: Invalid direction: %d\n", __FUNCTION__, dir);
            return BLADERF_ERR_INVAL;
    }

    return 0;
}

static int bladerf1_init_stream(struct bladerf_stream **stream, struct bladerf *dev, bladerf_stream_cb callback, void ***buffers, size_t num_buffers, bladerf_format format, size_t samples_per_buffer, size_t num_transfers, void *user_data)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return async_init_stream(stream, dev, callback, buffers, num_buffers,
                             format, samples_per_buffer, num_transfers, user_data);
}

static int bladerf1_stream(struct bladerf_stream *stream, bladerf_channel_layout layout)
{
    bladerf_direction dir = layout & BLADERF_DIRECTION_MASK;
    int stream_status, fmt_status;

    if (layout != BLADERF_RX_X1 && layout != BLADERF_TX_X1) {
        return -EINVAL;
    }

    fmt_status = perform_format_config(stream->dev, dir, stream->format);
    if (fmt_status != 0) {
        return fmt_status;
    }

    stream_status = async_run_stream(stream, layout);

    fmt_status = perform_format_deconfig(stream->dev, dir);
    if (fmt_status != 0) {
        return fmt_status;
    }

    return stream_status;
}

static int bladerf1_submit_stream_buffer(struct bladerf_stream *stream, void *buffer, unsigned int timeout_ms, bool nonblock)
{
    return async_submit_stream_buffer(stream, buffer, timeout_ms, nonblock);
}

static void bladerf1_deinit_stream(struct bladerf_stream *stream)
{
    async_deinit_stream(stream);
}

static int bladerf1_set_stream_timeout(struct bladerf *dev, bladerf_direction dir, unsigned int timeout)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    MUTEX_LOCK(&board_data->sync[dir].lock);
    board_data->sync[dir].stream_config.timeout_ms = timeout;
    MUTEX_UNLOCK(&board_data->sync[dir].lock);

    return 0;
}

static int bladerf1_get_stream_timeout(struct bladerf *dev, bladerf_direction dir, unsigned int *timeout)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    MUTEX_LOCK(&board_data->sync[dir].lock);
    *timeout = board_data->sync[dir].stream_config.timeout_ms;
    MUTEX_UNLOCK(&board_data->sync[dir].lock);

    return 0;
}

static int bladerf1_sync_config(struct bladerf *dev, bladerf_channel_layout layout, bladerf_format format, unsigned int num_buffers, unsigned int buffer_size, unsigned int num_transfers, unsigned int stream_timeout)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    bladerf_direction dir = layout & BLADERF_DIRECTION_MASK;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (layout != BLADERF_RX_X1 && layout != BLADERF_TX_X1) {
        return -EINVAL;
    }

    status = perform_format_config(dev, dir, format);
    if (status == 0) {
        status = sync_init(&board_data->sync[dir], dev, layout,
                           format, num_buffers, buffer_size,
                           board_data->msg_size, num_transfers,
                           stream_timeout);
        if (status != 0) {
            perform_format_deconfig(dev, dir);
        }
    }

    return status;
}

static int bladerf1_sync_tx(struct bladerf *dev,
                            void const *samples,
                            unsigned int num_samples,
                            struct bladerf_metadata *metadata,
                            unsigned int timeout_ms)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    int status;

    if (!board_data->sync[BLADERF_TX].initialized) {
        return BLADERF_ERR_INVAL;
    }

    status = sync_tx(&board_data->sync[BLADERF_TX], samples, num_samples,
                     metadata, timeout_ms);

    return status;
}

static int bladerf1_sync_rx(struct bladerf *dev,
                            void *samples,
                            unsigned int num_samples,
                            struct bladerf_metadata *metadata,
                            unsigned int timeout_ms)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    int status;

    if (!board_data->sync[BLADERF_RX].initialized) {
        return BLADERF_ERR_INVAL;
    }

    status = sync_rx(&board_data->sync[BLADERF_RX], samples, num_samples,
                     metadata, timeout_ms);

    return status;
}

static int bladerf1_get_timestamp(struct bladerf *dev,
                                  bladerf_direction dir,
                                  bladerf_timestamp *value)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return dev->backend->get_timestamp(dev, dir, value);
}

/******************************************************************************/
/* FPGA/Firmware Loading/Flashing */
/******************************************************************************/

static bool is_valid_fpga_size(struct bladerf *dev,
                               bladerf_fpga_size fpga,
                               size_t len)
{
    static const char env_override[] = "BLADERF_SKIP_FPGA_SIZE_CHECK";
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

static int bladerf1_load_fpga(struct bladerf *dev, const uint8_t *buf, size_t length)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    int status;

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    if (!is_valid_fpga_size(dev, board_data->fpga_size, length)) {
        return BLADERF_ERR_INVAL;
    }

    MUTEX_LOCK(&dev->lock);

    status = dev->backend->load_fpga(dev, buf, length);
    if (status != 0) {
        MUTEX_UNLOCK(&dev->lock);
        return status;
    }

    /* Update device state */
    board_data->state = STATE_FPGA_LOADED;

    MUTEX_UNLOCK(&dev->lock);

    status = bladerf1_initialize(dev);
    if (status != 0) {
        return status;
    }

    return 0;
}

static int bladerf1_flash_fpga(struct bladerf *dev, const uint8_t *buf, size_t length)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    if (!is_valid_fpga_size(dev, board_data->fpga_size, length)) {
        return BLADERF_ERR_INVAL;
    }

    return spi_flash_write_fpga_bitstream(dev, buf, length);
}

static int bladerf1_erase_stored_fpga(struct bladerf *dev)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    return spi_flash_erase_fpga(dev);
}

static bool is_valid_fw_size(size_t len)
{
    /* Simple FW applications generally are significantly larger than this */
    if (len < (50 * 1024)) {
        return false;
    } else if (len > BLADERF_FLASH_BYTE_LEN_FIRMWARE) {
        return false;
    } else {
        return true;
    }
}

static int bladerf1_flash_firmware(struct bladerf *dev, const uint8_t *buf, size_t length)
{
    const char env_override[] = "BLADERF_SKIP_FW_SIZE_CHECK";

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    /* Sanity check firmware length.
     *
     * TODO in the future, better sanity checks can be performed when
     *      using the bladerf image format currently used to backup/restore
     *      calibration data
     */
    if (!getenv(env_override) && !is_valid_fw_size(length)) {
        log_info("Detected potentially invalid firmware file.\n");
        log_info("Define BLADERF_SKIP_FW_SIZE_CHECK in your evironment "
                "to skip this check.\n");
        return BLADERF_ERR_INVAL;
    }

    return spi_flash_write_fx3_fw(dev, buf, length);
}

static int bladerf1_device_reset(struct bladerf *dev)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    return dev->backend->device_reset(dev);
}

/******************************************************************************/
/* Tuning mode */
/******************************************************************************/

static int bladerf1_set_tuning_mode(struct bladerf *dev, bladerf_tuning_mode mode)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (mode == BLADERF_TUNING_MODE_FPGA &&
            !have_cap(board_data->capabilities, BLADERF_CAP_FPGA_TUNING)) {
        log_debug("The loaded FPGA version (%u.%u.%u) does not support the "
                  "provided tuning mode (%d)\n",
                  board_data->fpga_version.major, board_data->fpga_version.minor,
                  board_data->fpga_version.patch, mode);
        return BLADERF_ERR_UNSUPPORTED;
    }

    switch (mode) {
        case BLADERF_TUNING_MODE_HOST:
            log_debug("Tuning mode: host\n");
            break;
        case BLADERF_TUNING_MODE_FPGA:
            log_debug("Tuning mode: FPGA\n");
            break;
        default:
            assert(!"Invalid tuning mode.");
            return BLADERF_ERR_INVAL;
    }

    board_data->tuning_mode = mode;

    return 0;
}

static int bladerf1_get_tuning_mode(struct bladerf *dev, bladerf_tuning_mode *mode)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    *mode = board_data->tuning_mode;

    return 0;
}

/******************************************************************************/
/* Loopback */
/******************************************************************************/

static int bladerf1_get_loopback_modes(
    struct bladerf *dev, struct bladerf_loopback_modes const **modes)
{
    if (modes != NULL) {
        *modes = bladerf1_loopback_modes;
    }

    return ARRAY_SIZE(bladerf1_loopback_modes);
}

static int bladerf1_set_loopback(struct bladerf *dev, bladerf_loopback l)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (l == BLADERF_LB_FIRMWARE) {
        /* Firmware loopback was fully implemented in FW v1.7.1
         * (1.7.0 could enable it, but 1.7.1 also allowed readback,
         * so we'll enforce 1.7.1 here. */
        if (!have_cap(board_data->capabilities, BLADERF_CAP_FW_LOOPBACK)) {
            log_warning("Firmware v1.7.1 or later is required "
                        "to use firmware loopback.\n\n");
            status = BLADERF_ERR_UPDATE_FW;
            return status;
        } else {
            /* Samples won't reach the LMS when the device is in firmware
             * loopback mode. By placing the LMS into a loopback mode, we ensure
             * that the PAs will be disabled, and remain enabled across
             * frequency changes.
             */
            status = lms_set_loopback_mode(dev, BLADERF_LB_RF_LNA3);
            if (status != 0) {
                return status;
            }

            status = dev->backend->set_firmware_loopback(dev, true);
        }
    } else {
        /* If applicable, ensure FW loopback is disabled */
        if (have_cap(board_data->capabilities, BLADERF_CAP_FW_LOOPBACK)) {
            bool fw_lb_enabled = false;

            /* Query first, as the implementation of setting the mode
             * may interrupt running streams. The API don't guarantee that
             * switching loopback modes on the fly to work, but we can at least
             * try to avoid unnecessarily interrupting things...*/
            status = dev->backend->get_firmware_loopback(dev, &fw_lb_enabled);
            if (status != 0) {
                return status;
            }

            if (fw_lb_enabled) {
                status = dev->backend->set_firmware_loopback(dev, false);
                if (status != 0) {
                    return status;
                }
            }
        }

        status = lms_set_loopback_mode(dev, l);
    }

    return status;
}

static int bladerf1_get_loopback(struct bladerf *dev, bladerf_loopback *l)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    *l = BLADERF_LB_NONE;

    if (have_cap(board_data->capabilities, BLADERF_CAP_FW_LOOPBACK)) {
        bool fw_lb_enabled;
        status = dev->backend->get_firmware_loopback(dev, &fw_lb_enabled);
        if (status == 0 && fw_lb_enabled) {
            *l = BLADERF_LB_FIRMWARE;
        }
    }

    if (*l == BLADERF_LB_NONE) {
        status = lms_get_loopback_mode(dev, l);
    }

    return status;
}

/******************************************************************************/
/* Sample RX FPGA Mux */
/******************************************************************************/

static int bladerf1_set_rx_mux(struct bladerf *dev, bladerf_rx_mux mode)
{
    uint32_t rx_mux_val;
    uint32_t config_gpio;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    /* Validate desired mux mode */
    switch (mode) {
        case BLADERF_RX_MUX_BASEBAND:
        case BLADERF_RX_MUX_12BIT_COUNTER:
        case BLADERF_RX_MUX_32BIT_COUNTER:
        case BLADERF_RX_MUX_DIGITAL_LOOPBACK:
            rx_mux_val = ((uint32_t) mode) << BLADERF_GPIO_RX_MUX_SHIFT;
            break;

        default:
            log_debug("Invalid RX mux mode setting passed to %s(): %d\n",
                      mode, __FUNCTION__);
            return BLADERF_ERR_INVAL;
    }

    status = dev->backend->config_gpio_read(dev, &config_gpio);
    if (status != 0) {
        return status;
    }

    /* Clear out and assign the associated RX mux bits */
    config_gpio &= ~BLADERF_GPIO_RX_MUX_MASK;
    config_gpio |= rx_mux_val;

    return dev->backend->config_gpio_write(dev, config_gpio);
}

static int bladerf1_get_rx_mux(struct bladerf *dev, bladerf_rx_mux *mode)
{
    bladerf_rx_mux val;
    uint32_t config_gpio;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    status = dev->backend->config_gpio_read(dev, &config_gpio);
    if (status != 0) {
        return status;
    }

    /* Extract RX mux bits */
    config_gpio &= BLADERF_GPIO_RX_MUX_MASK;
    config_gpio >>= BLADERF_GPIO_RX_MUX_SHIFT;
    val = (bladerf_rx_mux) (config_gpio);

    /* Enure it's a valid/supported value */
    switch (val) {
        case BLADERF_RX_MUX_BASEBAND:
        case BLADERF_RX_MUX_12BIT_COUNTER:
        case BLADERF_RX_MUX_32BIT_COUNTER:
        case BLADERF_RX_MUX_DIGITAL_LOOPBACK:
            *mode = val;
            break;

        default:
            *mode = BLADERF_RX_MUX_INVALID;
            status = BLADERF_ERR_UNEXPECTED;
            log_debug("Invalid rx mux mode %d read from config gpio\n", val);
    }

    return status;
}

/******************************************************************************/
/* Low-level VCTCXO Tamer Mode */
/******************************************************************************/

static int bladerf1_set_vctcxo_tamer_mode(struct bladerf *dev,
                                          bladerf_vctcxo_tamer_mode mode)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (!have_cap(board_data->capabilities, BLADERF_CAP_VCTCXO_TAMING_MODE)) {
        log_debug("FPGA %s does not support VCTCXO taming via an input source\n",
                  board_data->fpga_version.describe);
        return BLADERF_ERR_UNSUPPORTED;
    }

    return dev->backend->set_vctcxo_tamer_mode(dev, mode);
}

static int bladerf1_get_vctcxo_tamer_mode(struct bladerf *dev,
                                          bladerf_vctcxo_tamer_mode *mode)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (!have_cap(board_data->capabilities, BLADERF_CAP_VCTCXO_TAMING_MODE)) {
        log_debug("FPGA %s does not support VCTCXO taming via an input source\n",
                  board_data->fpga_version.describe);
        return BLADERF_ERR_UNSUPPORTED;
    }

    return dev->backend->get_vctcxo_tamer_mode(dev, mode);
}

/******************************************************************************/
/* Low-level VCTCXO Trim DAC access */
/******************************************************************************/

static int bladerf1_get_vctcxo_trim(struct bladerf *dev, uint16_t *trim)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    *trim = board_data->dac_trim;

    return 0;
}

static int bladerf1_trim_dac_read(struct bladerf *dev, uint16_t *trim)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    if (!have_cap(board_data->capabilities, BLADERF_CAP_VCTCXO_TRIMDAC_READ)) {
        log_debug("FPGA %s does not support VCTCXO trimdac readback.\n",
                  board_data->fpga_version.describe);
        return BLADERF_ERR_UNSUPPORTED;
    }

    return dac161s055_read(dev, trim);
}

static int bladerf1_trim_dac_write(struct bladerf *dev, uint16_t trim)
{
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    return dac161s055_write(dev, trim);
}

/******************************************************************************/
/* Low-level Trigger control access */
/******************************************************************************/

static int bladerf1_read_trigger(struct bladerf *dev, bladerf_channel ch,
                                 bladerf_trigger_signal trigger, uint8_t *val)
{
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    return fpga_trigger_read(dev, ch, trigger, val);
}

static int bladerf1_write_trigger(struct bladerf *dev, bladerf_channel ch,
                                  bladerf_trigger_signal trigger, uint8_t val)
{
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    return fpga_trigger_write(dev, ch, trigger, val);
}

/******************************************************************************/
/* Low-level Configuration GPIO access */
/******************************************************************************/

static int bladerf1_config_gpio_read(struct bladerf *dev, uint32_t *val)
{
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    return dev->backend->config_gpio_read(dev, val);
}

static int bladerf1_config_gpio_write(struct bladerf *dev, uint32_t val)
{
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    return dev->backend->config_gpio_write(dev, val);
}

/******************************************************************************/
/* Low-level SPI Flash access */
/******************************************************************************/

static int bladerf1_erase_flash(struct bladerf *dev, uint32_t erase_block, uint32_t count)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    return spi_flash_erase(dev, erase_block, count);
}

static int bladerf1_read_flash(struct bladerf *dev, uint8_t *buf,
                               uint32_t page, uint32_t count)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    return spi_flash_read(dev, buf, page, count);
}

static int bladerf1_write_flash(struct bladerf *dev, const uint8_t *buf,
                                uint32_t page, uint32_t count)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    return spi_flash_write(dev, buf, page, count);
}

/******************************************************************************/
/* Expansion support */
/******************************************************************************/

static int bladerf1_expansion_attach(struct bladerf *dev, bladerf_xb xb)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    bladerf_xb attached;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    status = dev->board->expansion_get_attached(dev, &attached);
    if (status != 0) {
        return status;
    }

    if (xb != attached && attached != BLADERF_XB_NONE) {
        log_debug("%s: Switching XB types is not supported.\n", __FUNCTION__);
        return BLADERF_ERR_UNSUPPORTED;
    }

    if (xb == BLADERF_XB_100) {
        if (!have_cap(board_data->capabilities, BLADERF_CAP_MASKED_XBIO_WRITE)) {
            log_debug("%s: XB100 support requires FPGA v0.4.1 or later.\n",
                      __FUNCTION__);
            return BLADERF_ERR_UNSUPPORTED;
        }

        log_verbose("Attaching XB100\n");
        status = xb100_attach(dev);
        if (status != 0) {
            return status;
        }

        log_verbose("Enabling XB100\n");
        status = xb100_enable(dev, true);
        if (status != 0) {
            return status;
        }

        log_verbose("Initializing XB100\n");
        status = xb100_init(dev);
        if (status != 0) {
            return status;
        }
    } else if (xb == BLADERF_XB_200) {
        if (!have_cap(board_data->capabilities, BLADERF_CAP_XB200)) {
            log_debug("%s: XB200 support requires FPGA v0.0.5 or later\n",
                      __FUNCTION__);
            return BLADERF_ERR_UPDATE_FPGA;
        }

        log_verbose("Attaching XB200\n");
        status = xb200_attach(dev);
        if (status != 0) {
            return status;
        }

        log_verbose("Enabling XB200\n");
        status = xb200_enable(dev, true);
        if (status != 0) {
            return status;
        }

        log_verbose("Initializing XB200\n");
        status = xb200_init(dev);
        if (status != 0) {
            return status;
        }
    } else if (xb == BLADERF_XB_300) {
        log_verbose("Attaching XB300\n");
        status = xb300_attach(dev);
        if (status != 0) {
            return status;
        }

        log_verbose("Enabling XB300\n");
        status = xb300_enable(dev, true);
        if (status != 0) {
            return status;
        }

        log_verbose("Initializing XB300\n");
        status = xb300_init(dev);
        if (status != 0) {
            return status;
        }
    } else if (xb == BLADERF_XB_NONE) {
        log_debug("%s: Disabling an attached XB is not supported.\n",
                  __FUNCTION__);
        return BLADERF_ERR_UNSUPPORTED;
    } else {
        log_debug("%s: Unknown xb type: %d\n", __FUNCTION__, xb);
        return BLADERF_ERR_INVAL;
    }

    /* Cache what we have attached in our device handle to alleviate the
     * need to go read the device state */
    dev->xb = xb;

    return 0;
}

static int bladerf1_expansion_get_attached(struct bladerf *dev, bladerf_xb *xb)
{
    int status;

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    switch (dev->xb) {
        case BLADERF_XB_NONE:
        case BLADERF_XB_100:
        case BLADERF_XB_200:
        case BLADERF_XB_300:
            *xb = dev->xb;
            status = 0;
            break;
        default:
            log_debug("Device handle contains invalid XB id: %d\n", dev->xb);
            status = BLADERF_ERR_UNEXPECTED;
            break;
    }

    return status;
}

/******************************************************************************/
/* Board binding */
/******************************************************************************/

const struct board_fns bladerf1_board_fns = {
    FIELD_INIT(.matches, bladerf1_matches),
    FIELD_INIT(.open, bladerf1_open),
    FIELD_INIT(.close, bladerf1_close),
    FIELD_INIT(.device_speed, bladerf1_device_speed),
    FIELD_INIT(.get_serial, bladerf1_get_serial),
    FIELD_INIT(.get_fpga_size, bladerf1_get_fpga_size),
    FIELD_INIT(.get_fpga_bytes, bladerf1_get_fpga_bytes),
    FIELD_INIT(.get_flash_size, bladerf1_get_flash_size),
    FIELD_INIT(.is_fpga_configured, bladerf1_is_fpga_configured),
    FIELD_INIT(.get_fpga_source, bladerf1_get_fpga_source),
    FIELD_INIT(.get_capabilities, bladerf1_get_capabilities),
    FIELD_INIT(.get_channel_count, bladerf1_get_channel_count),
    FIELD_INIT(.get_fpga_version, bladerf1_get_fpga_version),
    FIELD_INIT(.get_fw_version, bladerf1_get_fw_version),
    FIELD_INIT(.set_gain, bladerf1_set_gain),
    FIELD_INIT(.get_gain, bladerf1_get_gain),
    FIELD_INIT(.set_gain_mode, bladerf1_set_gain_mode),
    FIELD_INIT(.get_gain_mode, bladerf1_get_gain_mode),
    FIELD_INIT(.get_gain_modes, bladerf1_get_gain_modes),
    FIELD_INIT(.get_gain_range, bladerf1_get_gain_range),
    FIELD_INIT(.set_gain_stage, bladerf1_set_gain_stage),
    FIELD_INIT(.get_gain_stage, bladerf1_get_gain_stage),
    FIELD_INIT(.get_gain_stage_range, bladerf1_get_gain_stage_range),
    FIELD_INIT(.get_gain_stages, bladerf1_get_gain_stages),
    FIELD_INIT(.set_sample_rate, bladerf1_set_sample_rate),
    FIELD_INIT(.set_rational_sample_rate, bladerf1_set_rational_sample_rate),
    FIELD_INIT(.get_sample_rate, bladerf1_get_sample_rate),
    FIELD_INIT(.get_sample_rate_range, bladerf1_get_sample_rate_range),
    FIELD_INIT(.get_rational_sample_rate, bladerf1_get_rational_sample_rate),
    FIELD_INIT(.set_bandwidth, bladerf1_set_bandwidth),
    FIELD_INIT(.get_bandwidth, bladerf1_get_bandwidth),
    FIELD_INIT(.get_bandwidth_range, bladerf1_get_bandwidth_range),
    FIELD_INIT(.get_frequency, bladerf1_get_frequency),
    FIELD_INIT(.set_frequency, bladerf1_set_frequency),
    FIELD_INIT(.get_frequency_range, bladerf1_get_frequency_range),
    FIELD_INIT(.select_band, bladerf1_select_band),
    FIELD_INIT(.set_rf_port, bladerf1_set_rf_port),
    FIELD_INIT(.get_rf_port, bladerf1_get_rf_port),
    FIELD_INIT(.get_rf_ports, bladerf1_get_rf_ports),
    FIELD_INIT(.get_quick_tune, bladerf1_get_quick_tune),
    FIELD_INIT(.schedule_retune, bladerf1_schedule_retune),
    FIELD_INIT(.cancel_scheduled_retunes, bladerf1_cancel_scheduled_retunes),
    FIELD_INIT(.get_correction, bladerf1_get_correction),
    FIELD_INIT(.set_correction, bladerf1_set_correction),
    FIELD_INIT(.trigger_init, bladerf1_trigger_init),
    FIELD_INIT(.trigger_arm, bladerf1_trigger_arm),
    FIELD_INIT(.trigger_fire, bladerf1_trigger_fire),
    FIELD_INIT(.trigger_state, bladerf1_trigger_state),
    FIELD_INIT(.enable_module, bladerf1_enable_module),
    FIELD_INIT(.init_stream, bladerf1_init_stream),
    FIELD_INIT(.stream, bladerf1_stream),
    FIELD_INIT(.submit_stream_buffer, bladerf1_submit_stream_buffer),
    FIELD_INIT(.deinit_stream, bladerf1_deinit_stream),
    FIELD_INIT(.set_stream_timeout, bladerf1_set_stream_timeout),
    FIELD_INIT(.get_stream_timeout, bladerf1_get_stream_timeout),
    FIELD_INIT(.sync_config, bladerf1_sync_config),
    FIELD_INIT(.sync_tx, bladerf1_sync_tx),
    FIELD_INIT(.sync_rx, bladerf1_sync_rx),
    FIELD_INIT(.get_timestamp, bladerf1_get_timestamp),
    FIELD_INIT(.load_fpga, bladerf1_load_fpga),
    FIELD_INIT(.flash_fpga, bladerf1_flash_fpga),
    FIELD_INIT(.erase_stored_fpga, bladerf1_erase_stored_fpga),
    FIELD_INIT(.flash_firmware, bladerf1_flash_firmware),
    FIELD_INIT(.device_reset, bladerf1_device_reset),
    FIELD_INIT(.set_tuning_mode, bladerf1_set_tuning_mode),
    FIELD_INIT(.get_tuning_mode, bladerf1_get_tuning_mode),
    FIELD_INIT(.get_loopback_modes, bladerf1_get_loopback_modes),
    FIELD_INIT(.set_loopback, bladerf1_set_loopback),
    FIELD_INIT(.get_loopback, bladerf1_get_loopback),
    FIELD_INIT(.get_rx_mux, bladerf1_get_rx_mux),
    FIELD_INIT(.set_rx_mux, bladerf1_set_rx_mux),
    FIELD_INIT(.set_vctcxo_tamer_mode, bladerf1_set_vctcxo_tamer_mode),
    FIELD_INIT(.get_vctcxo_tamer_mode, bladerf1_get_vctcxo_tamer_mode),
    FIELD_INIT(.get_vctcxo_trim, bladerf1_get_vctcxo_trim),
    FIELD_INIT(.trim_dac_read, bladerf1_trim_dac_read),
    FIELD_INIT(.trim_dac_write, bladerf1_trim_dac_write),
    FIELD_INIT(.read_trigger, bladerf1_read_trigger),
    FIELD_INIT(.write_trigger, bladerf1_write_trigger),
    FIELD_INIT(.config_gpio_read, bladerf1_config_gpio_read),
    FIELD_INIT(.config_gpio_write, bladerf1_config_gpio_write),
    FIELD_INIT(.erase_flash, bladerf1_erase_flash),
    FIELD_INIT(.read_flash, bladerf1_read_flash),
    FIELD_INIT(.write_flash, bladerf1_write_flash),
    FIELD_INIT(.expansion_attach, bladerf1_expansion_attach),
    FIELD_INIT(.expansion_get_attached, bladerf1_expansion_get_attached),
    FIELD_INIT(.name, "bladerf1"),
};

/******************************************************************************
 ******************************************************************************
 *                         bladeRF1-specific Functions                        *
 ******************************************************************************
 ******************************************************************************/

/******************************************************************************/
/* TX Gain */
/******************************************************************************/

int bladerf_set_txvga2(struct bladerf *dev, int gain)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_INITIALIZED);

    status = lms_txvga2_set_gain(dev, gain);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_get_txvga2(struct bladerf *dev, int *gain)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_INITIALIZED);

    status = lms_txvga2_get_gain(dev, gain);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_set_txvga1(struct bladerf *dev, int gain)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_INITIALIZED);

    status = lms_txvga1_set_gain(dev, gain);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_get_txvga1(struct bladerf *dev, int *gain)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_INITIALIZED);

    status = lms_txvga1_get_gain(dev, gain);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

/******************************************************************************/
/* RX Gain */
/******************************************************************************/

int bladerf_set_lna_gain(struct bladerf *dev, bladerf_lna_gain gain)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_INITIALIZED);

    status = lms_lna_set_gain(dev, gain);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_get_lna_gain(struct bladerf *dev, bladerf_lna_gain *gain)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_INITIALIZED);

    status = lms_lna_get_gain(dev, gain);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_set_rxvga1(struct bladerf *dev, int gain)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_INITIALIZED);

    status = lms_rxvga1_set_gain(dev, gain);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_get_rxvga1(struct bladerf *dev, int *gain)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_INITIALIZED);

    status = lms_rxvga1_get_gain(dev, gain);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_set_rxvga2(struct bladerf *dev, int gain)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_INITIALIZED);

    status = lms_rxvga2_set_gain(dev, gain);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_get_rxvga2(struct bladerf *dev, int *gain)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_INITIALIZED);

    status = lms_rxvga2_get_gain(dev, gain);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

/******************************************************************************/
/* LPF Bypass */
/******************************************************************************/

int bladerf_set_lpf_mode(struct bladerf *dev, bladerf_channel ch,
                         bladerf_lpf_mode mode)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_INITIALIZED);

    status = lms_lpf_set_mode(dev, ch, mode);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_get_lpf_mode(struct bladerf *dev, bladerf_channel ch,
                         bladerf_lpf_mode *mode)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_INITIALIZED);

    status = lms_lpf_get_mode(dev, ch, mode);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

/******************************************************************************/
/* Sample Internal/Direct */
/******************************************************************************/

int bladerf_get_sampling(struct bladerf *dev, bladerf_sampling *sampling)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_INITIALIZED);

    status = lms_get_sampling(dev, sampling);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_set_sampling(struct bladerf *dev, bladerf_sampling sampling)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_INITIALIZED);

    status = lms_select_sampling(dev, sampling);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

/******************************************************************************/
/* SMB Clock Configuration */
/******************************************************************************/

int bladerf_get_smb_mode(struct bladerf *dev, bladerf_smb_mode *mode)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_INITIALIZED);

    status = smb_clock_get_mode(dev, mode);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_set_smb_mode(struct bladerf *dev, bladerf_smb_mode mode)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_INITIALIZED);

    status = smb_clock_set_mode(dev, mode);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_get_smb_frequency(struct bladerf *dev, unsigned int *rate)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_INITIALIZED);

    status = si5338_get_smb_freq(dev, rate);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_set_smb_frequency(struct bladerf *dev, uint32_t rate, uint32_t *actual)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_INITIALIZED);

    status = si5338_set_smb_freq(dev, rate, actual);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_get_rational_smb_frequency(struct bladerf *dev, struct bladerf_rational_rate *rate)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_INITIALIZED);

    status = si5338_get_rational_smb_freq(dev, rate);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_set_rational_smb_frequency(struct bladerf *dev, struct bladerf_rational_rate *rate, struct bladerf_rational_rate *actual)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_INITIALIZED);

    status = si5338_set_rational_smb_freq(dev, rate, actual);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

/******************************************************************************/
/* DC Calibration */
/******************************************************************************/

int bladerf_calibrate_dc(struct bladerf *dev, bladerf_cal_module module)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_INITIALIZED);

    status = lms_calibrate_dc(dev, module);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

/******************************************************************************/
/* Low-level Si5338 access */
/******************************************************************************/

int bladerf_si5338_read(struct bladerf *dev, uint8_t address, uint8_t *val)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_FPGA_LOADED);

    status = dev->backend->si5338_read(dev,address,val);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_si5338_write(struct bladerf *dev, uint8_t address, uint8_t val)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_FPGA_LOADED);

    status = dev->backend->si5338_write(dev,address,val);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

/******************************************************************************/
/* Low-level LMS access */
/******************************************************************************/

int bladerf_lms_read(struct bladerf *dev, uint8_t address, uint8_t *val)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_FPGA_LOADED);

    status = dev->backend->lms_read(dev,address,val);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_lms_write(struct bladerf *dev, uint8_t address, uint8_t val)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_FPGA_LOADED);

    status = dev->backend->lms_write(dev,address,val);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_lms_set_dc_cals(struct bladerf *dev,
                            const struct bladerf_lms_dc_cals *dc_cals)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_INITIALIZED);

    status = lms_set_dc_cals(dev, dc_cals);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_lms_get_dc_cals(struct bladerf *dev,
                            struct bladerf_lms_dc_cals *dc_cals)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_INITIALIZED);

    status = lms_get_dc_cals(dev, dc_cals);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

/******************************************************************************/
/* Low-level XB SPI access */
/******************************************************************************/

int bladerf_xb_spi_write(struct bladerf *dev, uint32_t val)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_FPGA_LOADED);

    status = dev->backend->xb_spi(dev, val);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}
