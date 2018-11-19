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

#include <errno.h>
#include <string.h>

#include <libbladeRF.h>

#include "conversions.h"
#include "log.h"
#define LOGGER_ID_STRING
#include "bladeRF.h"
#include "logger_entry.h"
#include "logger_id.h"
#include "rel_assert.h"

#include "../bladerf1/flash.h"
#include "board/board.h"
#include "capabilities.h"
#include "compatibility.h"

#include "ad936x.h"
#include "driver/fpga_trigger.h"
#include "driver/fx3_fw.h"
#include "driver/ina219.h"
#include "driver/spi_flash.h"
#include "nios_pkt_retune2.h"

#include "backend/backend_config.h"
#include "backend/usb/usb.h"

#include "streaming/async.h"
#include "streaming/sync.h"

#include "devinfo.h"
#include "helpers/file.h"
#include "helpers/version.h"
#include "helpers/wallclock.h"
#include "version.h"


/******************************************************************************
 *                          bladeRF2 board state                              *
 ******************************************************************************/

#define NUM_MODULES 2

enum bladerf2_vctcxo_trim_source {
    TRIM_SOURCE_NONE,
    TRIM_SOURCE_TRIM_DAC,
    TRIM_SOURCE_PLL,
    TRIM_SOURCE_AUXDAC
};

struct bladerf2_board_data {
    /* Board state */
    enum {
        STATE_UNINITIALIZED,
        STATE_FIRMWARE_LOADED,
        STATE_FPGA_LOADED,
        STATE_INITIALIZED,
    } state;

    /* AD9361 PHY Handle */
    struct ad9361_rf_phy *phy;

    /* Bitmask of capabilities determined by version numbers */
    uint64_t capabilities;

    /* Format currently being used with a module, or -1 if module is not used */
    bladerf_format module_format[NUM_MODULES];

    /* Which mode of operation we use for tuning */
    bladerf_tuning_mode tuning_mode;

    /* Board properties */
    bladerf_fpga_size fpga_size;
    /* Data message size */
    size_t msg_size;

    /* Version information */
    struct bladerf_version fpga_version;
    struct bladerf_version fw_version;
    char fpga_version_str[BLADERF_VERSION_STR_MAX + 1];
    char fw_version_str[BLADERF_VERSION_STR_MAX + 1];

    /* Synchronous interface handles */
    struct bladerf_sync sync[2];

    /* TX Mute Status */
    bool tx_mute[2];

    /* VCTCXO trim state */
    enum bladerf2_vctcxo_trim_source trim_source;
    uint16_t trimdac_last_value;   /**< saved running value */
    uint16_t trimdac_stored_value; /**< cached value read from SPI flash */

    /* RFIC FIR Filter status */
    bool low_samplerate_mode;
    bladerf_rfic_rxfir rxfir, rxfir_orig;
    bladerf_rfic_txfir txfir, txfir_orig;

    /* Quick Tune Profile Status */
    uint16_t quick_tune_tx_profile;
    uint16_t quick_tune_rx_profile;
};

/* Macro for logging and returning an error status. This should be used for
 * errors defined in the \ref RETCODES list. */
#define RETURN_ERROR_STATUS(_what, _status)                   \
    {                                                         \
        log_error("%s: %s failed: %s\n", __FUNCTION__, _what, \
                  bladerf_strerror(_status));                 \
        return _status;                                       \
    }

/* Macro for converting, logging, and returning libad9361 error codes. */
#define RETURN_ERROR_AD9361(_what, _status)                          \
    {                                                                \
        RETURN_ERROR_STATUS(_what, errno_ad9361_to_bladerf(_status)) \
    }

/* Macro for logging and returning ::BLADERF_ERR_INVAL */
#define RETURN_INVAL_ARG(_what, _arg, _why)                               \
    {                                                                     \
        log_error("%s: %s '%s' invalid: %s\n", __FUNCTION__, _what, _arg, \
                  _why);                                                  \
        return BLADERF_ERR_INVAL;                                         \
    }

#define RETURN_INVAL(_what, _why)                                     \
    {                                                                 \
        log_error("%s: %s invalid: %s\n", __FUNCTION__, _what, _why); \
        return BLADERF_ERR_INVAL;                                     \
    }

/* Responsible for checking for null pointers on dev and commonly-accessed
 * members of dev, as well as the board state. Also responsible for unlocking
 * if things go terribly wrong. */
#define _CHECK_BOARD_STATE(_state, _locked)                              \
    {                                                                    \
        struct bladerf2_board_data *board_data;                          \
                                                                         \
        if (NULL == dev) {                                               \
            RETURN_INVAL("dev", "not initialized");                      \
        }                                                                \
                                                                         \
        if (NULL == dev->board || NULL == dev->backend) {                \
            if (_locked) {                                               \
                MUTEX_UNLOCK(&dev->lock);                                \
            }                                                            \
            RETURN_INVAL("dev->board||dev->backend", "not initialized"); \
        }                                                                \
                                                                         \
        board_data = dev->board_data;                                    \
                                                                         \
        if (board_data->state < _state) {                                \
            log_error("%s: Board state insufficient for operation "      \
                      "(current \"%s\", requires \"%s\").\n",            \
                      __FUNCTION__,                                      \
                      bladerf2_state_to_string[board_data->state],       \
                      bladerf2_state_to_string[_state]);                 \
                                                                         \
            if (_locked) {                                               \
                MUTEX_UNLOCK(&dev->lock);                                \
            }                                                            \
                                                                         \
            return BLADERF_ERR_NOT_INIT;                                 \
        }                                                                \
    }

// clang-format off
#define CHECK_BOARD_STATE(_state)           _CHECK_BOARD_STATE(_state, false)
#define CHECK_BOARD_STATE_LOCKED(_state)    _CHECK_BOARD_STATE(_state, true)
// clang-format on

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

enum bladerf2_band { BAND_SHUTDOWN, BAND_LOW, BAND_HIGH };

struct bladerf_ad9361_gain_mode_map {
    bladerf_gain_mode brf_mode;
    enum rf_gain_ctrl_mode ad9361_mode;
};

struct bladerf_gain_range {
    char const *name;
    struct bladerf_range frequency;
    struct bladerf_range gain;
    float offset;
};

struct bladerf_ad9361_port_name_map {
    const char *name;
    uint32_t id;
};

struct range_band_map {
    enum bladerf2_band band;
    const struct bladerf_range range;
};

struct band_port_map {
    enum bladerf2_band band;
    uint32_t spdt;
    uint32_t ad9361_port;
};

static const bladerf_frequency BLADERF_VCTCXO_FREQUENCY = 38400000;
static const bladerf_frequency BLADERF_REFIN_DEFAULT    = 10000000;

// clang-format off

// Config GPIO
#define CFG_GPIO_POWERSOURCE        0
#define CFG_GPIO_PLL_EN             11
#define CFG_GPIO_CLOCK_OUTPUT       17
#define CFG_GPIO_CLOCK_SELECT       18

// RFFE control
#define RFFE_CONTROL_RESET_N        0
#define RFFE_CONTROL_ENABLE         1
#define RFFE_CONTROL_TXNRX          2
#define RFFE_CONTROL_EN_AGC         3
#define RFFE_CONTROL_SYNC_IN        4
#define RFFE_CONTROL_RX_BIAS_EN     5
#define RFFE_CONTROL_RX_SPDT_1      6   // 6 and 7
#define RFFE_CONTROL_RX_SPDT_2      8   // 8 and 9
#define RFFE_CONTROL_TX_BIAS_EN     10
#define RFFE_CONTROL_TX_SPDT_1      11  // 11 and 12
#define RFFE_CONTROL_TX_SPDT_2      13  // 13 and 14
#define RFFE_CONTROL_MIMO_RX_EN_0   15
#define RFFE_CONTROL_MIMO_TX_EN_0   16
#define RFFE_CONTROL_MIMO_RX_EN_1   17
#define RFFE_CONTROL_MIMO_TX_EN_1   18
#define RFFE_CONTROL_ADF_MUXOUT     19   // input only
#define RFFE_CONTROL_CTRL_OUT       24   // input only, 24 through 31
#define RFFE_CONTROL_SPDT_MASK      0x3
#define RFFE_CONTROL_SPDT_SHUTDOWN  0x0  // no connection
#define RFFE_CONTROL_SPDT_LOWBAND   0x2  // RF1 <-> RF3
#define RFFE_CONTROL_SPDT_HIGHBAND  0x1  // RF1 <-> RF2

// Trim DAC control
#define TRIMDAC_MASK                0x3FFC // 2 through 13
#define TRIMDAC_EN                  14     // 14 and 15
#define TRIMDAC_EN_MASK             0x3
#define TRIMDAC_EN_ACTIVE           0x0
#define TRIMDAC_EN_HIGHZ            0x3

/* Number of fast lock profiles that can be stored in the Nios
 * Make sure this number matches that of the Nios' devices.h */
#define NUM_BBP_FASTLOCK_PROFILES   256

/* Number of fast lock profiles that can be stored in the RFFE
 * Make sure this number matches that of the Nios' devices.h */
#define NUM_RFFE_FASTLOCK_PROFILES  8

/* Board state to string map */
static const char *bladerf2_state_to_string[] = {
    [STATE_UNINITIALIZED]   = "Uninitialized",
    [STATE_FIRMWARE_LOADED] = "Firmware Loaded",
    [STATE_FPGA_LOADED]     = "FPGA Loaded",
    [STATE_INITIALIZED]     = "Initialized",
};

/* Gain mode mappings */
static const struct bladerf_ad9361_gain_mode_map bladerf2_rx_gain_mode_map[] = {
    {   
        FIELD_INIT(.brf_mode, BLADERF_GAIN_MGC),
        FIELD_INIT(.ad9361_mode, RF_GAIN_MGC)
    },
    {   
        FIELD_INIT(.brf_mode, BLADERF_GAIN_FASTATTACK_AGC),
        FIELD_INIT(.ad9361_mode, RF_GAIN_FASTATTACK_AGC)
    },
    {   
        FIELD_INIT(.brf_mode, BLADERF_GAIN_SLOWATTACK_AGC),
        FIELD_INIT(.ad9361_mode, RF_GAIN_SLOWATTACK_AGC)
    },
    {   
        FIELD_INIT(.brf_mode, BLADERF_GAIN_HYBRID_AGC),
        FIELD_INIT(.ad9361_mode, RF_GAIN_HYBRID_AGC)
    },
};

/* RX gain ranges */
/* Reference: ad9361.c, ad9361_gt_tableindex and ad9361_init_gain_tables */
static const struct bladerf_gain_range bladerf2_rx_gain_ranges[] = {
    {
        FIELD_INIT(.name, NULL),
        FIELD_INIT(.frequency, {
            FIELD_INIT(.min,    0),
            FIELD_INIT(.max,    1300000000),
            FIELD_INIT(.step,   1),
            FIELD_INIT(.scale,  1),
        }),
        FIELD_INIT(.gain, {
            FIELD_INIT(.min,    1 - 17),
            FIELD_INIT(.max,    77 - 17),
            FIELD_INIT(.step,   1),
            FIELD_INIT(.scale,  1),
        }),
        FIELD_INIT(.offset, -17.0f),
    },
    {
        FIELD_INIT(.name, NULL),
        FIELD_INIT(.frequency, {
            FIELD_INIT(.min,    1300000000UL),
            FIELD_INIT(.max,    4000000000UL),
            FIELD_INIT(.step,   1),
            FIELD_INIT(.scale,  1),
        }),
        FIELD_INIT(.gain, {
            FIELD_INIT(.min,    -4 - 11),
            FIELD_INIT(.max,    71 - 11),
            FIELD_INIT(.step,   1),
            FIELD_INIT(.scale,  1),
        }),
        FIELD_INIT(.offset, -11.0f),
    },
    {
        FIELD_INIT(.name, NULL),
        FIELD_INIT(.frequency, {
            FIELD_INIT(.min,    4000000000UL),
            FIELD_INIT(.max,    6000000000UL),
            FIELD_INIT(.step,   1),
            FIELD_INIT(.scale,  1),
        }),
        FIELD_INIT(.gain, {
            FIELD_INIT(.min,    -10 - 2),
            FIELD_INIT(.max,    62 - 2),
            FIELD_INIT(.step,   1),
            FIELD_INIT(.scale,  1),
        }),
        FIELD_INIT(.offset, -2.0f),
    },
    {
        FIELD_INIT(.name, "full"),
        FIELD_INIT(.frequency, {
            FIELD_INIT(.min,    0),
            FIELD_INIT(.max,    1300000000),
            FIELD_INIT(.step,   1),
            FIELD_INIT(.scale,  1),
        }),
        FIELD_INIT(.gain, {
            FIELD_INIT(.min,    1),
            FIELD_INIT(.max,    77),
            FIELD_INIT(.step,   1),
            FIELD_INIT(.scale,  1),
        }),
        FIELD_INIT(.offset, 0),
    },
    {
        FIELD_INIT(.name, "full"),
        FIELD_INIT(.frequency, {
            FIELD_INIT(.min,    1300000000UL),
            FIELD_INIT(.max,    4000000000UL),
            FIELD_INIT(.step,   1),
            FIELD_INIT(.scale,  1),
        }),
        FIELD_INIT(.gain, {
            FIELD_INIT(.min,    -4),
            FIELD_INIT(.max,    71),
            FIELD_INIT(.step,   1),
            FIELD_INIT(.scale,  1),
        }),
        FIELD_INIT(.offset, 0),
    },
    {
        FIELD_INIT(.name, "full"),
        FIELD_INIT(.frequency, {
            FIELD_INIT(.min,    4000000000UL),
            FIELD_INIT(.max,    6000000000UL),
            FIELD_INIT(.step,   1),
            FIELD_INIT(.scale,  1),
        }),
        FIELD_INIT(.gain, {
            FIELD_INIT(.min,    -10),
            FIELD_INIT(.max,    62),
            FIELD_INIT(.step,   1),
            FIELD_INIT(.scale,  1),
        }),
        FIELD_INIT(.offset, 0),
    },
};

/* TX gain offset: 60 dB system gain ~= 0 dBm output */
#define BLADERF2_TX_GAIN_OFFSET 66.0f

/* Overall TX gain range */
static const struct bladerf_gain_range bladerf2_tx_gain_ranges[] = {
    {
        FIELD_INIT(.name, NULL),
        FIELD_INIT(.frequency, {
            FIELD_INIT(.min,    47000000UL),
            FIELD_INIT(.max,    6000000000UL),
            FIELD_INIT(.step,   1),
            FIELD_INIT(.scale,  1),
        }),
        FIELD_INIT(.gain, {
            FIELD_INIT(.min,    __round_int(1000*(-89.750 + 66.0))),
            FIELD_INIT(.max,    __round_int(1000*(0 + 66.0))),
            FIELD_INIT(.step,   250),
            FIELD_INIT(.scale,  0.001F),
        }),
        FIELD_INIT(.offset, 66.0f),
    },
    {
        FIELD_INIT(.name, "dsa"),
        FIELD_INIT(.frequency, {
            FIELD_INIT(.min,    47000000UL),
            FIELD_INIT(.max,    6000000000UL),
            FIELD_INIT(.step,   1),
            FIELD_INIT(.scale,  1),
        }),
        FIELD_INIT(.gain, {
            FIELD_INIT(.min,    -89750),
            FIELD_INIT(.max,    0),
            FIELD_INIT(.step,   250),
            FIELD_INIT(.scale,  0.001F),
        }),
        FIELD_INIT(.offset, 0),
    },
};

/* RX gain modes */
static const struct bladerf_gain_modes bladerf2_rx_gain_modes[] = {
    {
        FIELD_INIT(.name, "automatic"),
        FIELD_INIT(.mode, BLADERF_GAIN_DEFAULT)
    },
    {
        FIELD_INIT(.name, "manual"),
        FIELD_INIT(.mode, BLADERF_GAIN_MGC)
    },
    {
        FIELD_INIT(.name, "fast"),
        FIELD_INIT(.mode, BLADERF_GAIN_FASTATTACK_AGC)
    },
    {
        FIELD_INIT(.name, "slow"),
        FIELD_INIT(.mode, BLADERF_GAIN_SLOWATTACK_AGC)
    },
    {
        FIELD_INIT(.name, "hybrid"),
        FIELD_INIT(.mode, BLADERF_GAIN_HYBRID_AGC)
    }
};

/* Sample Rate Range */
static const struct bladerf_range bladerf2_sample_rate_range = {
    FIELD_INIT(.min,    520834),
    FIELD_INIT(.max,    61440000),
    FIELD_INIT(.step,   1),
    FIELD_INIT(.scale,  1),
};

/* Sample rates requiring a 4x interpolation/decimation */
static const struct bladerf_range bladerf2_sample_rate_range_4x = {
    FIELD_INIT(.min,    520834),
    FIELD_INIT(.max,    2083334),
    FIELD_INIT(.step,   1),
    FIELD_INIT(.scale,  1),
};

/* Bandwidth Range */
static const struct bladerf_range bladerf2_bandwidth_range = {
    FIELD_INIT(.min,    200000),
    FIELD_INIT(.max,    56000000),
    FIELD_INIT(.step,   1),
    FIELD_INIT(.scale,  1),
};

/* Frequency Ranges */
static const struct bladerf_range bladerf2_rx_frequency_range = {
    FIELD_INIT(.min,    70000000),
    FIELD_INIT(.max,    6000000000),
    FIELD_INIT(.step,   2),
    FIELD_INIT(.scale,  1),
};

static const struct bladerf_range bladerf2_tx_frequency_range = {
    FIELD_INIT(.min,    47000000),
    FIELD_INIT(.max,    6000000000),
    FIELD_INIT(.step,   2),
    FIELD_INIT(.scale,  1),
};

static const struct bladerf_range bladerf2_pll_refclk_range = {
    FIELD_INIT(.min, 5000000),
    FIELD_INIT(.max, 300000000),
    FIELD_INIT(.step, 1),
    FIELD_INIT(.scale, 1),
};

/* RF Ports */
static const struct bladerf_ad9361_port_name_map bladerf2_rx_port_map[] = {
    {   FIELD_INIT(.name, "A_BALANCED"),    FIELD_INIT(.id, AD936X_A_BALANCED), },
    {   FIELD_INIT(.name, "B_BALANCED"),    FIELD_INIT(.id, AD936X_B_BALANCED), },
    {   FIELD_INIT(.name, "C_BALANCED"),    FIELD_INIT(.id, AD936X_C_BALANCED), },
    {   FIELD_INIT(.name, "A_N"),           FIELD_INIT(.id, AD936X_A_N),        },
    {   FIELD_INIT(.name, "A_P"),           FIELD_INIT(.id, AD936X_A_P),        },
    {   FIELD_INIT(.name, "B_N"),           FIELD_INIT(.id, AD936X_B_N),        },
    {   FIELD_INIT(.name, "B_P"),           FIELD_INIT(.id, AD936X_B_P),        },
    {   FIELD_INIT(.name, "C_N"),           FIELD_INIT(.id, AD936X_C_N),        },
    {   FIELD_INIT(.name, "C_P"),           FIELD_INIT(.id, AD936X_C_P),        },
    {   FIELD_INIT(.name, "TX_MON1"),       FIELD_INIT(.id, AD936X_TX_MON1),    },
    {   FIELD_INIT(.name, "TX_MON2"),       FIELD_INIT(.id, AD936X_TX_MON2),    },
    {   FIELD_INIT(.name, "TX_MON1_2"),     FIELD_INIT(.id, AD936X_TX_MON1_2),  },
};

static const struct bladerf_ad9361_port_name_map bladerf2_tx_port_map[] = {
    {   FIELD_INIT(.name, "TXA"),           FIELD_INIT(.id, AD936X_TXA),        },
    {   FIELD_INIT(.name, "TXB"),           FIELD_INIT(.id, AD936X_TXB),        },
};

static const struct range_band_map bladerf2_rx_range_band_map[] = {
    {
        FIELD_INIT(.band, BAND_LOW),
        FIELD_INIT(.range, {
            FIELD_INIT(.min,    70000000UL),
            FIELD_INIT(.max,    3000000000UL),
            FIELD_INIT(.step,   2),
            FIELD_INIT(.scale,  1)
        }),
    },
    {
        FIELD_INIT(.band, BAND_HIGH),
        FIELD_INIT(.range, {
            FIELD_INIT(.min,    3000000000UL),
            FIELD_INIT(.max,    6000000000UL),
            FIELD_INIT(.step,   2),
            FIELD_INIT(.scale,  1)
        }),
    },
};

static const struct range_band_map bladerf2_tx_range_band_map[] = {
    {
        FIELD_INIT(.band, BAND_LOW),
        FIELD_INIT(.range, {
            FIELD_INIT(.min,    46875000UL),
            FIELD_INIT(.max,    3000000000UL),
            FIELD_INIT(.step,   2),
            FIELD_INIT(.scale,  1)
        }),
    },
    {
        FIELD_INIT(.band, BAND_HIGH),
        FIELD_INIT(.range, {
            FIELD_INIT(.min,    3000000000UL),
            FIELD_INIT(.max,    6000000000UL),
            FIELD_INIT(.step,   2),
            FIELD_INIT(.scale,  1)
        }),
    },
};

static const struct band_port_map bladerf2_rx_band_port_map[] = {
    {
        FIELD_INIT(.band,           BAND_SHUTDOWN),
        FIELD_INIT(.spdt,           RFFE_CONTROL_SPDT_SHUTDOWN),
        FIELD_INIT(.ad9361_port,    0),
    },
    {
        FIELD_INIT(.band,           BAND_LOW),
        FIELD_INIT(.spdt,           RFFE_CONTROL_SPDT_LOWBAND),
        FIELD_INIT(.ad9361_port,    AD936X_B_BALANCED),
    },
    {
        FIELD_INIT(.band,           BAND_HIGH),
        FIELD_INIT(.spdt,           RFFE_CONTROL_SPDT_HIGHBAND),
        FIELD_INIT(.ad9361_port,    AD936X_A_BALANCED),
    },
};

static const struct band_port_map bladerf2_tx_band_port_map[] = {
    {
        FIELD_INIT(.band,           BAND_SHUTDOWN),
        FIELD_INIT(.spdt,           RFFE_CONTROL_SPDT_SHUTDOWN),
        FIELD_INIT(.ad9361_port,    0),
    },
    {
        FIELD_INIT(.band,           BAND_LOW),
        FIELD_INIT(.spdt,           RFFE_CONTROL_SPDT_LOWBAND),
        FIELD_INIT(.ad9361_port,    AD936X_TXB),
    },
    {
        FIELD_INIT(.band,           BAND_HIGH),
        FIELD_INIT(.spdt,           RFFE_CONTROL_SPDT_HIGHBAND),
        FIELD_INIT(.ad9361_port,    AD936X_TXA),
    },
};

/* Loopback modes */

static const struct bladerf_loopback_modes bladerf2_loopback_modes[] = {
    {
        FIELD_INIT(.name, "none"),
        FIELD_INIT(.mode, BLADERF_LB_NONE)
    },
    {
        FIELD_INIT(.name, "firmware"),
        FIELD_INIT(.mode, BLADERF_LB_FIRMWARE)
    },
    {
        FIELD_INIT(.name, "rf_bist"),
        FIELD_INIT(.mode, BLADERF_LB_RFIC_BIST)
    },
};

// clang-format on

/******************************************************************************/
/* Forward declarations */
/******************************************************************************/

static int bladerf2_select_band(struct bladerf *dev,
                                bladerf_channel ch,
                                bladerf_frequency frequency);
static int bladerf2_get_frequency(struct bladerf *dev,
                                  bladerf_channel ch,
                                  bladerf_frequency *frequency);
static int bladerf2_read_flash_vctcxo_trim(struct bladerf *dev, uint16_t *trim);
static int bladerf2_get_sample_rate(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bladerf_sample_rate *rate);


/******************************************************************************/
/* Externs */
/******************************************************************************/

extern AD9361_InitParam bladerf2_rfic_init_params;
extern AD9361_RXFIRConfig bladerf2_rfic_rx_fir_config;
extern AD9361_TXFIRConfig bladerf2_rfic_tx_fir_config;
extern AD9361_RXFIRConfig bladerf2_rfic_rx_fir_config_dec2;
extern AD9361_TXFIRConfig bladerf2_rfic_tx_fir_config_int2;
extern AD9361_RXFIRConfig bladerf2_rfic_rx_fir_config_dec4;
extern AD9361_TXFIRConfig bladerf2_rfic_tx_fir_config_int4;
extern const float ina219_r_shunt;


/******************************************************************************/
/* Helpers */
/******************************************************************************/

static int errno_ad9361_to_bladerf(int err)
{
    switch (err) {
        case EIO:
            return BLADERF_ERR_IO;
        case EAGAIN:
            return BLADERF_ERR_WOULD_BLOCK;
        case ENOMEM:
            return BLADERF_ERR_MEM;
        case EFAULT:
            return BLADERF_ERR_UNEXPECTED;
        case ENODEV:
            return BLADERF_ERR_NODEV;
        case EINVAL:
            return BLADERF_ERR_INVAL;
        case ETIMEDOUT:
            return BLADERF_ERR_TIMEOUT;
    }

    return BLADERF_ERR_UNEXPECTED;
}

static bool _is_within_range(struct bladerf_range const *range, int64_t value)
{
    if (NULL == range) {
        log_error("%s: range is null\n", __FUNCTION__);
        return false;
    }

    return (__scale(range, value) >= range->min &&
            __scale(range, value) <= range->max);
}

static int64_t _clamp_to_range(struct bladerf_range const *range, int64_t value)
{
    if (NULL == range) {
        log_error("%s: range is null\n", __FUNCTION__);
        return value;
    }

    if (__scale(range, value) < range->min) {
        log_warning("Requested value %" PRIi64
                    " is below range [%g,%g], clamping to %" PRIi64 "\n",
                    value, __unscale(range, range->min),
                    __unscale(range, range->max),
                    __unscale_int64(range, range->min));
        value = __unscale_int64(range, range->min);
    }

    if (__scale(range, value) > range->max) {
        log_warning("Requested value %" PRIi64
                    " is above range [%g,%g], clamping to %" PRIi64 "\n",
                    value, __unscale(range, range->min),
                    __unscale(range, range->max),
                    __unscale_int64(range, range->max));
        value = __unscale_int64(range, range->max);
    }

    return value;
}

static enum bladerf2_band _get_band_by_freq(bladerf_channel ch,
                                            bladerf_frequency freq)
{
    const struct range_band_map *band_map;
    size_t band_map_len;
    int64_t freqi = (int64_t)freq;
    size_t i;

    /* Select RX vs TX */
    if (BLADERF_CHANNEL_IS_TX(ch)) {
        band_map     = bladerf2_tx_range_band_map;
        band_map_len = ARRAY_SIZE(bladerf2_tx_range_band_map);
    } else {
        band_map     = bladerf2_rx_range_band_map;
        band_map_len = ARRAY_SIZE(bladerf2_rx_range_band_map);
    }

    /* Determine the band for the given frequency */
    for (i = 0; i < band_map_len; ++i) {
        if (_is_within_range(&band_map[i].range, freqi)) {
            return band_map[i].band;
        }
    }

    /* Not a valid frequency */
    log_warning("Frequency %" BLADERF_PRIuFREQ " not found in band map\n",
                freq);
    return BAND_SHUTDOWN;
}

static const struct band_port_map *_get_band_port_map_by_freq(
    bladerf_channel ch, bool enabled, bladerf_frequency freq)
{
    enum bladerf2_band band;
    const struct band_port_map *port_map;
    size_t port_map_len;
    size_t i;

    /* Determine which band to use */
    band = enabled ? _get_band_by_freq(ch, freq) : BAND_SHUTDOWN;

    /* Select the band->port map for RX vs TX */
    if (BLADERF_CHANNEL_IS_TX(ch)) {
        port_map     = bladerf2_tx_band_port_map;
        port_map_len = ARRAY_SIZE(bladerf2_tx_band_port_map);
    } else {
        port_map     = bladerf2_rx_band_port_map;
        port_map_len = ARRAY_SIZE(bladerf2_rx_band_port_map);
    }

    if (NULL == port_map) {
        log_error("%s: port_map is null\n", __FUNCTION__);
        return NULL;
    }

    /* Search through the band->port map for the desired band */
    for (i = 0; i < port_map_len; i++) {
        if (port_map[i].band == band) {
            return &port_map[i];
        }
    }

    /* Wasn't found, return a null ptr */
    log_warning("Frequency %" BLADERF_PRIuFREQ " not found in port map\n",
                freq);
    return NULL;
}

static int _modify_spdt_bits_by_freq(uint32_t *reg,
                                     bladerf_channel ch,
                                     bool enabled,
                                     bladerf_frequency freq)
{
    const struct band_port_map *port_map;
    uint32_t shift;

    if (NULL == reg) {
        RETURN_INVAL("reg", "is null");
    }

    /* Look up the port configuration for this frequency */
    port_map = _get_band_port_map_by_freq(ch, enabled, freq);

    if (NULL == port_map) {
        RETURN_INVAL("_get_band_port_map_by_freq", "returned null");
    }

    /* Modify the reg bits accordingly */
    switch (ch) {
        case BLADERF_CHANNEL_RX(0):
            shift = RFFE_CONTROL_RX_SPDT_1;
            break;
        case BLADERF_CHANNEL_RX(1):
            shift = RFFE_CONTROL_RX_SPDT_2;
            break;
        case BLADERF_CHANNEL_TX(0):
            shift = RFFE_CONTROL_TX_SPDT_1;
            break;
        case BLADERF_CHANNEL_TX(1):
            shift = RFFE_CONTROL_TX_SPDT_2;
            break;
        default:
            RETURN_INVAL("ch", "not recognized");
    }

    *reg &= ~(RFFE_CONTROL_SPDT_MASK << shift);
    *reg |= port_map->spdt << shift;

    return 0;
}

static int _set_ad9361_port_by_freq(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bool enabled,
                                    bladerf_frequency freq)
{
    const struct band_port_map *port_map;
    struct bladerf2_board_data *board_data;
    struct ad9361_rf_phy *phy;
    int status;

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    board_data = dev->board_data;
    phy        = board_data->phy;

    /* Look up the port configuration for this frequency */
    port_map = _get_band_port_map_by_freq(ch, enabled, freq);

    if (NULL == port_map) {
        RETURN_INVAL("_get_band_port_map_by_freq", "returned null");
    }

    /* Set the AD9361 port accordingly */
    if (BLADERF_CHANNEL_IS_TX(ch)) {
        status = ad9361_set_tx_rf_port_output(phy, port_map->ad9361_port);
    } else {
        status = ad9361_set_rx_rf_port_input(phy, port_map->ad9361_port);
    }

    if (status < 0) {
        RETURN_ERROR_AD9361("setting rf port", status);
    }

    return 0;
}

static int _get_rffe_control_bit_for_dir(bladerf_direction dir)
{
    switch (dir) {
        case BLADERF_RX:
            return RFFE_CONTROL_ENABLE;
        case BLADERF_TX:
            return RFFE_CONTROL_TXNRX;
        default:
            return UINT32_MAX;
    }
}

static int _get_rffe_control_bit_for_ch(bladerf_channel ch)
{
    switch (ch) {
        case BLADERF_CHANNEL_RX(0):
            return RFFE_CONTROL_MIMO_RX_EN_0;
        case BLADERF_CHANNEL_RX(1):
            return RFFE_CONTROL_MIMO_RX_EN_1;
        case BLADERF_CHANNEL_TX(0):
            return RFFE_CONTROL_MIMO_TX_EN_0;
        case BLADERF_CHANNEL_TX(1):
            return RFFE_CONTROL_MIMO_TX_EN_1;
        default:
            return UINT32_MAX;
    }
}

static bool _is_rffe_ch_enabled(uint32_t reg, bladerf_channel ch)
{
    /* Given a register read from the RFFE, determine if ch is enabled */
    return (reg >> _get_rffe_control_bit_for_ch(ch)) & 0x1;
}

static bool _is_rffe_dir_enabled(uint32_t reg, bladerf_direction dir)
{
    /* Given a register read from the RFFE, determine if dir is enabled */
    return (reg >> _get_rffe_control_bit_for_dir(dir)) & 0x1;
}

static bool _does_rffe_dir_have_enabled_ch(uint32_t reg, bladerf_direction dir)
{
    switch (dir) {
        case BLADERF_RX:
            return _is_rffe_ch_enabled(reg, BLADERF_CHANNEL_RX(0)) ||
                   _is_rffe_ch_enabled(reg, BLADERF_CHANNEL_RX(1));
        case BLADERF_TX:
            return _is_rffe_ch_enabled(reg, BLADERF_CHANNEL_TX(0)) ||
                   _is_rffe_ch_enabled(reg, BLADERF_CHANNEL_TX(1));
    }

    return false;
}

static uint32_t _get_tx_gain_cache(struct bladerf *dev, bladerf_channel ch)
{
    struct bladerf2_board_data *board_data;
    struct ad9361_rf_phy *phy;

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    board_data = dev->board_data;
    phy        = board_data->phy;

    switch (ch) {
        case BLADERF_CHANNEL_TX(0):
            return phy->tx1_atten_cached;
        case BLADERF_CHANNEL_TX(1):
            return phy->tx2_atten_cached;
        default:
            RETURN_INVAL("ch", "is not a valid TX channel");
    }
}

static int _set_tx_gain_cache(struct bladerf *dev,
                              bladerf_channel ch,
                              uint32_t atten_mdb)
{
    struct bladerf2_board_data *board_data;
    struct ad9361_rf_phy *phy;

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    board_data = dev->board_data;
    phy        = board_data->phy;

    switch (ch) {
        case BLADERF_CHANNEL_TX(0):
            phy->tx1_atten_cached = atten_mdb;
            return 0;
        case BLADERF_CHANNEL_TX(1):
            phy->tx2_atten_cached = atten_mdb;
            return 0;
        default:
            RETURN_INVAL("ch", "is not a valid TX channel");
    }
}

static int _set_tx_mute(struct bladerf *dev, bladerf_channel ch, bool state)
{
    struct bladerf2_board_data *board_data;
    struct ad9361_rf_phy *phy;
    uint32_t atten;
    uint32_t cached;
    int port;
    int status;

    const uint32_t MUTED_ATTEN = 89750;

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    board_data = dev->board_data;
    phy        = board_data->phy;
    port       = (ch >> 1);
    cached     = _get_tx_gain_cache(dev, ch);

    if (board_data->tx_mute[ch >> 1] == state) {
        log_warning("attempted to mute already-muted channel %d\n", ch);
        return 0;
    }

    if (state) {
        status = ad9361_get_tx_attenuation(phy, port, &cached);
        if (status != 0) {
            RETURN_ERROR_AD9361("failed to get current tx attenuation", status);
        }
        atten  = MUTED_ATTEN;
    } else {
        atten = cached;
    }

    status = _set_tx_gain_cache(dev, ch, cached);
    if (status != 0) {
        RETURN_ERROR_STATUS("failed to update tx gain cache", status);
    }

    log_debug("%s: %smuting TX%d (cached: %d)\n", __FUNCTION__,
              state ? "" : "un", port, cached);
    status = ad9361_set_tx_attenuation(phy, port, atten);
    if (status != 0) {
        RETURN_ERROR_AD9361("failed to set tx atten", status);
    }

    board_data->tx_mute[ch >> 1] = state;

    return 0;
}

static bool _check_total_sample_rate(struct bladerf *dev,
                                     const uint32_t *rffe_control_reg)
{
    int status;
    uint32_t reg;
    size_t i;

    bladerf_sample_rate rate_accum = 0;
    size_t active_channels         = 0;

    const bladerf_sample_rate MAX_SAMPLE_THROUGHPUT = 80000000;

    /* Read RFFE control register, if required */
    if (rffe_control_reg != NULL) {
        reg = *rffe_control_reg;
    } else {
        status = dev->backend->rffe_control_read(dev, &reg);
        if (status < 0) {
            return false;
        }
    }

    /* Accumulate sample rates for all channels */
    if (_is_rffe_dir_enabled(reg, BLADERF_RX)) {
        bladerf_sample_rate rx_rate;

        status = bladerf2_get_sample_rate(dev, BLADERF_CHANNEL_RX(0), &rx_rate);
        if (status < 0) {
            return false;
        }

        for (i = 0; i < bladerf_get_channel_count(dev, BLADERF_RX); ++i) {
            if (_is_rffe_ch_enabled(reg, BLADERF_CHANNEL_RX(i))) {
                rate_accum += rx_rate;
                ++active_channels;
            }
        }
    }

    if (_is_rffe_dir_enabled(reg, BLADERF_TX)) {
        bladerf_sample_rate tx_rate;

        status = bladerf2_get_sample_rate(dev, BLADERF_CHANNEL_TX(0), &tx_rate);
        if (status < 0) {
            return false;
        }

        for (i = 0; i < bladerf_get_channel_count(dev, BLADERF_TX); ++i) {
            if (_is_rffe_ch_enabled(reg, BLADERF_CHANNEL_TX(i))) {
                rate_accum += tx_rate;
                ++active_channels;
            }
        }
    }

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

/******************************************************************************/
/* Low-level Initialization */
/******************************************************************************/

static int bladerf2_initialize(struct bladerf *dev)
{
    struct bladerf2_board_data *board_data;
    struct ad9361_rf_phy *phy;
    struct bladerf_version required_fw_version, required_fpga_version;
    uint32_t reg;
    int status;

    const bladerf_frequency RESET_FREQUENCY = 70000000;

    /* Test for uninitialized dev struct */
    CHECK_BOARD_STATE(STATE_UNINITIALIZED);

    board_data = dev->board_data;

    /* Initialize board_data members */
    board_data->rxfir_orig          = BLADERF_RFIC_RXFIR_DEFAULT;
    board_data->txfir_orig          = BLADERF_RFIC_TXFIR_DEFAULT;
    board_data->low_samplerate_mode = false;

    /* Read FPGA version */
    status = dev->backend->get_fpga_version(dev, &board_data->fpga_version);
    if (status < 0) {
        RETURN_ERROR_STATUS("Failed to get FPGA version", status);
    }

    log_verbose("Read FPGA version: %s\n", board_data->fpga_version.describe);

    /* Determine FPGA capabilities */
    board_data->capabilities |=
        bladerf2_get_fpga_capabilities(&board_data->fpga_version);

    log_verbose("Capability mask after FPGA load: 0x%016" PRIx64 "\n",
                board_data->capabilities);

    /* If the FPGA version check fails, just warn, but don't error out.
     *
     * If an error code caused this function to bail out, it would prevent a
     * user from being able to unload and reflash a bitstream being
     * "autoloaded" from SPI flash. */
    status =
        version_check(&bladerf2_fw_compat_table, &bladerf2_fpga_compat_table,
                      &board_data->fw_version, &board_data->fpga_version,
                      &required_fw_version, &required_fpga_version);
    if (status < 0) {
#if LOGGING_ENABLED
        if (BLADERF_ERR_UPDATE_FPGA == status) {
            log_warning(
                "FPGA v%u.%u.%u was detected. Firmware v%u.%u.%u "
                "requires FPGA v%u.%u.%u or later. Please load a "
                "different FPGA version before continuing.\n\n",
                board_data->fpga_version.major, board_data->fpga_version.minor,
                board_data->fpga_version.patch, board_data->fw_version.major,
                board_data->fw_version.minor, board_data->fw_version.patch,
                required_fpga_version.major, required_fpga_version.minor,
                required_fpga_version.patch);
        } else if (BLADERF_ERR_UPDATE_FW == status) {
            log_warning(
                "FPGA v%u.%u.%u was detected, which requires firmware "
                "v%u.%u.%u or later. The device firmware is currently "
                "v%u.%u.%u. Please upgrade the device firmware before "
                "continuing.\n\n",
                board_data->fpga_version.major, board_data->fpga_version.minor,
                board_data->fpga_version.patch, required_fw_version.major,
                required_fw_version.minor, required_fw_version.patch,
                board_data->fw_version.major, board_data->fw_version.minor,
                board_data->fw_version.patch);
        }
#endif
    }

    /* Set FPGA packet protocol */
    status = dev->backend->set_fpga_protocol(dev, BACKEND_FPGA_PROTOCOL_NIOSII);
    if (status < 0) {
        RETURN_ERROR_STATUS("set_fpga_protocol", status);
    }

    /* Initialize RFFE control */
    status = dev->backend->rffe_control_write(
        dev, (1 << RFFE_CONTROL_ENABLE) | (1 << RFFE_CONTROL_TXNRX));
    if (status < 0) {
        RETURN_ERROR_STATUS("rffe_control_write initialization", status);
    }

    /* Initialize INA219 */
    /* For reasons unknown, this fails if done immediately after
     * ad9361_set_rx_fir_config when DEBUG is not defined. It shouldn't make
     * a difference, but it does. TODO: Investigate/fix this */
    status = ina219_init(dev, ina219_r_shunt);
    if (status < 0) {
        RETURN_ERROR_STATUS("ina219_init", status);
    }

    /* Initialize AD9361 */
    log_debug("%s: ad9361_init starting\n", __FUNCTION__);
    status = ad9361_init(&board_data->phy, &bladerf2_rfic_init_params, dev);
    log_debug("%s: ad9361_init complete, status = %d\n", __FUNCTION__, status);
    if (status < 0) {
        RETURN_ERROR_AD9361("ad9361_init", status);
    }

    if (NULL == board_data->phy || NULL == board_data->phy->pdata) {
        RETURN_ERROR_STATUS("ad9361_init struct initialization",
                            BLADERF_ERR_UNEXPECTED);
    }

    phy = board_data->phy;

    /* Force AD9361 to a non-default freq. This will entice it to do a proper
     * re-tuning when we set it back to the default freq later on. */
    status = ad9361_set_tx_lo_freq(board_data->phy, RESET_FREQUENCY);
    if (status < 0) {
        RETURN_ERROR_AD9361("ad9361_set_tx_lo_freq", status);
    }

    status = ad9361_set_rx_lo_freq(board_data->phy, RESET_FREQUENCY);
    if (status < 0) {
        RETURN_ERROR_AD9361("ad9361_set_rx_lo_freq", status);
    }

    /* Set up AD9361 FIR filters */
    /* TODO: permit specification of filter taps, for the truly brave */
    status = bladerf_set_rfic_rx_fir(dev, BLADERF_RFIC_RXFIR_DEFAULT);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf_set_rfic_rx_fir", status);
    }

    status = bladerf_set_rfic_tx_fir(dev, BLADERF_RFIC_TXFIR_DEFAULT);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf_set_rfic_tx_fir", status);
    }

    /* Disable AD9361 until we need it */
    status = dev->backend->rffe_control_read(dev, &reg);
    if (status < 0) {
        RETURN_ERROR_STATUS("rffe_control_read", status);
    }

    reg &= ~(1 << RFFE_CONTROL_TXNRX);
    reg &= ~(1 << RFFE_CONTROL_ENABLE);

    status = dev->backend->rffe_control_write(dev, reg);
    if (status < 0) {
        RETURN_ERROR_STATUS("rffe_control_write", status);
    }

    /* Set up band selection */
    status = bladerf2_select_band(dev, BLADERF_TX, phy->pdata->tx_synth_freq);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf2_select_band (TX)", status);
    }

    status = bladerf2_select_band(dev, BLADERF_RX, phy->pdata->rx_synth_freq);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf2_select_band (RX)", status);
    }

    /* Move AD9361 back to desired frequency */
    status = ad9361_set_rx_lo_freq(board_data->phy, phy->pdata->rx_synth_freq);
    if (status < 0) {
        RETURN_ERROR_AD9361("ad9361_set_rx_lo_freq", status);
    }

    status = ad9361_set_tx_lo_freq(board_data->phy, phy->pdata->tx_synth_freq);
    if (status < 0) {
        RETURN_ERROR_AD9361("ad9361_set_tx_lo_freq", status);
    }

    /* Mute TX channels */
    board_data->tx_mute[0] = false;
    board_data->tx_mute[1] = false;

    status = _set_tx_mute(dev, BLADERF_CHANNEL_TX(0), true);
    if (status < 0) {
        RETURN_ERROR_STATUS("_set_tx_mute(BLADERF_CHANNEL_TX(0))", status);
    }

    status = _set_tx_mute(dev, BLADERF_CHANNEL_TX(1), true);
    if (status < 0) {
        RETURN_ERROR_STATUS("_set_tx_mute(BLADERF_CHANNEL_TX(1))", status);
    }

    /* Initialize VCTCXO trim DAC to stored value */
    uint16_t *trimval = &(board_data->trimdac_stored_value);

    status = bladerf2_read_flash_vctcxo_trim(dev, trimval);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf2_read_flash_vctcxo_trim", status);
    }

    status = dev->backend->ad56x1_vctcxo_trim_dac_write(dev, *trimval);
    if (status < 0) {
        RETURN_ERROR_STATUS("ad56x1_vctcxo_trim_dac_write", status);
    }

    board_data->trim_source = TRIM_SOURCE_TRIM_DAC;

    /* Configure PLL */
    status = bladerf_set_pll_refclk(dev, BLADERF_REFIN_DEFAULT);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf_set_pll_refclk", status);
    }

    /* Reset current quick tune profile number */
    board_data->quick_tune_rx_profile = 0;
    board_data->quick_tune_tx_profile = 0;

    /* Update device state */
    board_data->state = STATE_INITIALIZED;

    log_debug("%s: complete\n", __FUNCTION__);

    return 0;
}


/******************************************************************************
 *                        Generic Board Functions                             *
 ******************************************************************************/

/******************************************************************************/
/* Matches */
/******************************************************************************/

static bool bladerf2_matches(struct bladerf *dev)
{
    uint16_t vid, pid;
    int status;

    if (NULL == dev || NULL == dev->backend) {
        RETURN_INVAL("dev", "not initialized");
    }

    status = dev->backend->get_vid_pid(dev, &vid, &pid);
    if (status < 0) {
        log_error("%s: get_vid_pid returned status %s\n", __FUNCTION__,
                  bladerf_strerror(status));
        return false;
    }

    if (USB_NUAND_VENDOR_ID == vid && USB_NUAND_BLADERF2_PRODUCT_ID == pid) {
        return true;
    }

    return false;
}


/******************************************************************************/
/* Open/close */
/******************************************************************************/

static int bladerf2_open(struct bladerf *dev, struct bladerf_devinfo *devinfo)
{
    struct bladerf2_board_data *board_data;
    struct bladerf_version required_fw_version;
    char *full_path;
    bladerf_dev_speed usb_speed;
    int ready;
    int status = 0;
    size_t i;

    const size_t max_retries = 30;

    if (NULL == dev || NULL == dev->backend) {
        RETURN_INVAL("dev", "not initialized");
    }

    /* Allocate board data */
    board_data = calloc(1, sizeof(struct bladerf2_board_data));
    if (NULL == board_data) {
        RETURN_ERROR_STATUS("calloc board_data", BLADERF_ERR_MEM);
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
        RETURN_ERROR_STATUS("get_fw_version", status);
    }

    log_verbose("Read Firmware version: %s\n", board_data->fw_version.describe);

    /* Determine firmware capabilities */
    board_data->capabilities |=
        bladerf2_get_fw_capabilities(&board_data->fw_version);
    log_verbose("Capability mask before FPGA load: 0x%016" PRIx64 "\n",
                board_data->capabilities);

    /* Update device state */
    board_data->state = STATE_FIRMWARE_LOADED;

    /* Wait until firmware is ready */
    for (i = 0; i < max_retries; i++) {
        ready = dev->backend->is_fw_ready(dev);
        if (ready != 1) {
            if (0 == i) {
                log_info("Waiting for device to become ready...\n");
            } else {
                log_debug("Retry %02u/%02u.\n", i + 1, max_retries);
            }
            usleep(1000000);
        } else {
            break;
        }
    }

    if (ready != 1) {
        RETURN_ERROR_STATUS("is_fw_ready", BLADERF_ERR_TIMEOUT);
    }

    /* Determine data message size */
    status = dev->backend->get_device_speed(dev, &usb_speed);
    if (status < 0) {
        RETURN_ERROR_STATUS("get_device_speed", status);
    }

    switch (usb_speed) {
        case BLADERF_DEVICE_SPEED_SUPER:
            board_data->msg_size = USB_MSG_SIZE_SS;
            break;
        case BLADERF_DEVICE_SPEED_HIGH:
            board_data->msg_size = USB_MSG_SIZE_HS;
            break;
        default:
            log_error("%s: unsupported device speed (%d)\n", __FUNCTION__,
                      usb_speed);
            return BLADERF_ERR_UNSUPPORTED;
    }

    /* Verify that we have a sufficent firmware version before continuing. */
    status = version_check_fw(&bladerf2_fw_compat_table,
                              &board_data->fw_version, &required_fw_version);
    if (status != 0) {
#ifdef LOGGING_ENABLED
        if (BLADERF_ERR_UPDATE_FW == status) {
            log_warning("Firmware v%u.%u.%u was detected. libbladeRF v%s "
                        "requires firmware v%u.%u.%u or later. An upgrade via "
                        "the bootloader is required.\n\n",
                        &board_data->fw_version.major,
                        &board_data->fw_version.minor,
                        &board_data->fw_version.patch, LIBBLADERF_VERSION,
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

    /* Get FPGA size */
    status = spi_flash_read_fpga_size(dev, &board_data->fpga_size);
    if (status < 0) {
        log_warning("Failed to get FPGA size %s\n", bladerf_strerror(status));
    }

    if (getenv("BLADERF_FORCE_FPGA_A9")) {
        log_info("BLADERF_FORCE_FPGA_A9 is set, assuming A9 FPGA\n");
        board_data->fpga_size = BLADERF_FPGA_A9;
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

    /* Check if FPGA is configured */
    status = dev->backend->is_fpga_configured(dev);
    if (status < 0) {
        RETURN_ERROR_STATUS("is_fpga_configured", status);
    } else if (1 == status) {
        board_data->state = STATE_FPGA_LOADED;
    } else if (status != 1 && BLADERF_FPGA_UNKNOWN == board_data->fpga_size) {
        log_warning("Unknown FPGA size. Skipping FPGA configuration...\n");
        log_warning("Skipping further initialization...\n");
        return 0;
    } else if (status != 1) {
        /* Try searching for an FPGA in the config search path */
        switch (board_data->fpga_size) {
            case BLADERF_FPGA_A4:
                full_path = file_find("hostedxA4.rbf");
                break;

            case BLADERF_FPGA_A9:
                full_path = file_find("hostedxA9.rbf");
                break;

            default:
                log_error("%s: invalid FPGA size: %d\n", __FUNCTION__,
                          board_data->fpga_size);
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
                RETURN_ERROR_STATUS("file_read_buffer", status);
            }

            status = dev->backend->load_fpga(dev, buf, buf_size);
            if (status != 0) {
                RETURN_ERROR_STATUS("load_fpga", status);
            }

            board_data->state = STATE_FPGA_LOADED;
        } else {
            log_warning("FPGA bitstream file not found.\n");
            log_warning("Skipping further initialization...\n");
            return 0;
        }
    }

    /* Initialize the board */
    status = bladerf2_initialize(dev);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf2_initialize", status);
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

static void bladerf2_close(struct bladerf *dev)
{
    if (dev != NULL) {
        struct bladerf2_board_data *board_data = dev->board_data;
        struct bladerf_flash_arch *flash_arch  = dev->flash_arch;

        if (board_data != NULL) {
            sync_deinit(&board_data->sync[BLADERF_CHANNEL_RX(0)]);
            sync_deinit(&board_data->sync[BLADERF_CHANNEL_TX(0)]);

            if (dev->backend->is_fpga_configured(dev) &&
                have_cap(board_data->capabilities,
                         BLADERF_CAP_SCHEDULED_RETUNE)) {
                /* Cancel scheduled retunes here to avoid the device retuning
                 * underneath the user should they open it again in the future.
                 *
                 * This is intended to help developers avoid a situation during
                 * debugging where they schedule "far" into the future, but hit
                 * a case where their program aborts or exits early. If we do
                 * not cancel these scheduled retunes, the device could start
                 * up and/or "unexpectedly" switch to a different frequency.
                 */
                dev->board->cancel_scheduled_retunes(dev, BLADERF_CHANNEL_RX(0));
                dev->board->cancel_scheduled_retunes(dev, BLADERF_CHANNEL_TX(0));
            }

            if (board_data->phy != NULL) {
                ad9361_deinit(board_data->phy);
                board_data->phy = NULL;
            }

            free(board_data);
            board_data = NULL;
        }

        if (flash_arch != NULL) {
            free(flash_arch);
            flash_arch = NULL;
        }
    }
}


/******************************************************************************/
/* Properties */
/******************************************************************************/

static bladerf_dev_speed bladerf2_device_speed(struct bladerf *dev)
{
    bladerf_dev_speed usb_speed;
    int status;

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    status = dev->backend->get_device_speed(dev, &usb_speed);
    if (status < 0) {
        log_error("%s: get_device_speed failed: %s\n", __FUNCTION__,
                  bladerf_strerror(status));
        return BLADERF_DEVICE_SPEED_UNKNOWN;
    }

    return usb_speed;
}

static int bladerf2_get_serial(struct bladerf *dev, char *serial)
{
    if (NULL == serial) {
        RETURN_INVAL("serial", "is null");
    }

    CHECK_BOARD_STATE(STATE_UNINITIALIZED);

    // TODO: don't use strcpy
    strcpy(serial, dev->ident.serial);

    return 0;
}

static int bladerf2_get_fpga_size(struct bladerf *dev, bladerf_fpga_size *size)
{
    struct bladerf2_board_data *board_data;

    if (NULL == size) {
        RETURN_INVAL("size", "is null");
    }

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    board_data = dev->board_data;

    *size = board_data->fpga_size;

    return 0;
}

static int bladerf2_get_flash_size(struct bladerf *dev,
                                   uint32_t *size,
                                   bool *is_guess)
{
    if (NULL == size) {
        RETURN_INVAL("size", "is null");
    }

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    *size     = dev->flash_arch->tsize_bytes;
    *is_guess = (dev->flash_arch->status != STATUS_SUCCESS);

    return 0;
}

static int bladerf2_is_fpga_configured(struct bladerf *dev)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    return dev->backend->is_fpga_configured(dev);
}

static int bladerf2_get_fpga_source(struct bladerf *dev,
                                    bladerf_fpga_source *source)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    struct bladerf2_board_data *board_data = dev->board_data;

    if (!have_cap(board_data->capabilities, BLADERF_CAP_FW_FPGA_SOURCE)) {
        log_debug("%s: not supported by firmware\n", __FUNCTION__);
        *source = BLADERF_FPGA_SOURCE_UNKNOWN;
        return BLADERF_ERR_UNSUPPORTED;
    }

    *source = dev->backend->get_fpga_source(dev);

    return 0;
}

static uint64_t bladerf2_get_capabilities(struct bladerf *dev)
{
    struct bladerf2_board_data *board_data;

    CHECK_BOARD_STATE(STATE_UNINITIALIZED);

    board_data = dev->board_data;

    return board_data->capabilities;
}

static size_t bladerf2_get_channel_count(struct bladerf *dev,
                                         bladerf_direction dir)
{
    return 2;
}

/******************************************************************************/
/* Versions */
/******************************************************************************/

static int bladerf2_get_fpga_version(struct bladerf *dev,
                                     struct bladerf_version *version)
{
    struct bladerf2_board_data *board_data;

    if (NULL == version) {
        RETURN_INVAL("version", "is null");
    }

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    board_data = dev->board_data;

    memcpy(version, &board_data->fpga_version, sizeof(*version));

    return 0;
}

static int bladerf2_get_fw_version(struct bladerf *dev,
                                   struct bladerf_version *version)
{
    struct bladerf2_board_data *board_data;

    if (NULL == version) {
        RETURN_INVAL("version", "is null");
    }

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    board_data = dev->board_data;

    memcpy(version, &board_data->fw_version, sizeof(*version));

    return 0;
}


/******************************************************************************/
/* Enable/disable */
/******************************************************************************/

static int perform_format_deconfig(struct bladerf *dev, bladerf_direction dir);

static int bladerf2_enable_module(struct bladerf *dev,
                                  bladerf_channel ch,
                                  bool enable)
{
    struct bladerf2_board_data *board_data;
    int status;

    bladerf_direction dir = BLADERF_CHANNEL_IS_TX(ch) ? BLADERF_TX : BLADERF_RX;

    uint32_t reg;       /* RFFE register value */
    uint32_t reg_old;   /* Original RFFE register value */
    int ch_bit;         /* RFFE MIMO channel bit */
    int dir_bit;        /* RFFE RX/TX enable bit */
    bool ch_pending;    /* Requested channel state differs */
    bool dir_enable;    /* Direction is enabled */
    bool dir_pending;   /* Requested direction state differs */
    bool backend_clear; /* Backend requires reset */

    bladerf_frequency freq = 0;

    static uint64_t lastrun = 0; /* nsec value at last run */

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;

    /* Look up the RFFE bits affecting this channel */
    ch_bit  = _get_rffe_control_bit_for_ch(ch);
    dir_bit = _get_rffe_control_bit_for_dir(dir);
    if (ch_bit < 0 || dir_bit < 0) {
        RETURN_ERROR_STATUS("_get_rffe_control_bit", BLADERF_ERR_INVAL);
    }

    /* Query the current frequency if necessary */
    if (enable) {
        status = bladerf2_get_frequency(dev, ch, &freq);
        if (status < 0) {
            RETURN_ERROR_STATUS("bladerf2_get_frequency", status);
        }
    }

    /**
     * If we are moving from 0 active channels to 1 active channel:
     *  Channel:    Setup SPDT, MIMO, TX Mute
     *  Direction:  Setup ENABLE/TXNRX, RFIC port
     *  Backend:    Enable
     *
     * If we are moving from 1 active channel to 0 active channels:
     *  Channel:    Teardown SPDT, MIMO, TX Mute
     *  Direction:  Teardown ENABLE/TXNRX, RFIC port, Sync
     *  Backend:    Disable
     *
     * If we are enabling an nth channel, where n > 1:
     *  Channel:    Setup SPDT, MIMO, TX Mute
     *  Direction:  no change
     *  Backend:    Clear
     *
     * If we are disabling an nth channel, where n > 1:
     *  Channel:    Teardown SPDT, MIMO, TX Mute
     *  Direction:  no change
     *  Backend:    no change
     */

    /* Read RFFE control register */
    status = dev->backend->rffe_control_read(dev, &reg);
    if (status < 0) {
        RETURN_ERROR_STATUS("rffe_control_read", status);
    }
    reg_old = reg;

    ch_pending = _is_rffe_ch_enabled(reg, ch) != enable;

    /* Channel Setup/Teardown */
    if (ch_pending) {
        /* Modify SPDT bits */
        status = _modify_spdt_bits_by_freq(&reg, ch, enable, freq);
        if (status < 0) {
            RETURN_ERROR_STATUS("_modify_spdt_bits_by_freq", status);
        }

        /* Modify MIMO channel enable bits */
        if (enable) {
            reg |= (1 << ch_bit);
        } else {
            reg &= ~(1 << ch_bit);
        }

        /* Set/unset TX mute */
        if (BLADERF_CHANNEL_IS_TX(ch)) {
            _set_tx_mute(dev, ch, !enable);
        }

        /* Warn the user if the sample rate isn't reasonable */
        if (enable) {
            _check_total_sample_rate(dev, &reg);
        }
    }

    dir_enable  = enable || _does_rffe_dir_have_enabled_ch(reg, dir);
    dir_pending = _is_rffe_dir_enabled(reg, dir) != dir_enable;

    /* Direction Setup/Teardown */
    if (dir_pending) {
        /* Modify ENABLE/TXNRX bits */
        if (dir_enable) {
            reg |= (1 << dir_bit);
        } else {
            reg &= ~(1 << dir_bit);
        }

        /* Select RFIC port */
        status = _set_ad9361_port_by_freq(dev, ch, dir_enable, freq);
        if (status < 0) {
            RETURN_ERROR_STATUS("_set_ad9361_port_by_freq", status);
        }

        /* Tear down sync interface if required */
        if (!dir_enable) {
            sync_deinit(&board_data->sync[dir]);
            perform_format_deconfig(dev, dir);
        }
    }

    /* Reset FIFO if we are enabling an additional RX channel */
    backend_clear = enable && !dir_pending && BLADERF_RX == dir;

    /* Debug logging */
    uint64_t nsec = wallclock_get_current_nsec();

    log_debug("%s: %s%d ch_en=%d ch_pend=%d dir_en=%d dir_pend=%d be_clr=%d "
              "reg=0x%08x->0x%08x nsec=%" PRIu64 " (delta: %" PRIu64 ")\n",
              __FUNCTION__, BLADERF_TX == dir ? "TX" : "RX", (ch >> 1) + 1,
              enable, ch_pending, dir_enable, dir_pending, backend_clear,
              reg_old, reg, nsec, nsec - lastrun);

    lastrun = nsec;

    /* Write RFFE control register */
    if (reg_old != reg) {
        status = dev->backend->rffe_control_write(dev, reg);
        if (status < 0) {
            RETURN_ERROR_STATUS("rffe_control_write", status);
        }
    } else {
        log_debug("%s: reg value unchanged? (%08x)\n", __FUNCTION__, reg);
    }

    /* Backend Setup/Teardown/Reset */
    if (dir_pending || backend_clear) {
        if (!dir_enable || backend_clear) {
            status = dev->backend->enable_module(dev, dir, false);
            if (status < 0) {
                RETURN_ERROR_STATUS("enable_module(false)", status);
            }
        }

        if (dir_enable || backend_clear) {
            status = dev->backend->enable_module(dev, dir, true);
            if (status < 0) {
                RETURN_ERROR_STATUS("enable_module(true)", status);
            }
        }
    }

    return 0;
}

/******************************************************************************/
/* Gain */
/******************************************************************************/

static int _get_gain_range(struct bladerf *dev,
                           bladerf_channel ch,
                           char const *stage,
                           struct bladerf_gain_range const **gainrange)
{
    struct bladerf_gain_range const *ranges = NULL;
    size_t ranges_len;
    bladerf_frequency frequency = 0;
    int status;
    size_t i;

    if (NULL == gainrange) {
        RETURN_INVAL("gainrange", "is null");
    }

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        ranges     = bladerf2_tx_gain_ranges;
        ranges_len = ARRAY_SIZE(bladerf2_tx_gain_ranges);
    } else {
        ranges     = bladerf2_rx_gain_ranges;
        ranges_len = ARRAY_SIZE(bladerf2_rx_gain_ranges);
    }

    status = bladerf2_get_frequency(dev, ch, &frequency);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf2_get_frequency", status);
    }

    for (i = 0; i < ranges_len; ++i) {
        // if the frequency range matches, and either:
        //  both the range name and the stage name are null, or
        //  neither name is null and the strings match
        // then we found our match
        struct bladerf_gain_range const *range = &(ranges[i]);
        struct bladerf_range const *rfreq      = &(range->frequency);

        if (_is_within_range(rfreq, frequency) &&
            ((NULL == range->name && NULL == stage) ||
             (range->name != NULL && stage != NULL &&
              (strcmp(range->name, stage) == 0)))) {
            *gainrange = range;
            return 0;
        }
    }

    return BLADERF_ERR_INVAL;
}

static int _get_gain_offset(struct bladerf *dev,
                            bladerf_channel ch,
                            float *offset)
{
    int status;
    struct bladerf_gain_range const *gainrange = NULL;

    status = _get_gain_range(dev, ch, NULL, &gainrange);
    if (status < 0) {
        RETURN_ERROR_STATUS("_get_gain_range", status);
    }

    *offset = gainrange->offset;

    return 0;
}

static int bladerf2_get_gain_stage_range(struct bladerf *dev,
                                         bladerf_channel ch,
                                         char const *stage,
                                         struct bladerf_range const **range)
{
    struct bladerf_gain_range const *gainrange = NULL;
    int status;

    if (NULL == range) {
        RETURN_INVAL("range", "is null");
    }

    status = _get_gain_range(dev, ch, stage, &gainrange);
    if (status < 0) {
        RETURN_ERROR_STATUS("_get_gain_range", status);
    }

    *range = &(gainrange->gain);

    return 0;
}

static int bladerf2_get_gain_range(struct bladerf *dev,
                                   bladerf_channel ch,
                                   struct bladerf_range const **range)
{
    return bladerf2_get_gain_stage_range(dev, ch, NULL, range);
}

static int bladerf2_set_gain_stage(struct bladerf *dev,
                                   bladerf_channel ch,
                                   char const *stage,
                                   int gain)
{
    struct bladerf2_board_data *board_data;
    struct ad9361_rf_phy *phy;
    uint8_t const ad9361_channel      = ch >> 1;
    struct bladerf_range const *range = NULL;

    int status;
    int val;

    if (NULL == stage) {
        RETURN_INVAL("stage", "is null");
    }

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;
    phy        = board_data->phy;

    status = bladerf2_get_gain_stage_range(dev, ch, stage, &range);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf2_get_gain_stage_range", status);
    }

    /* Scale/round/clamp as required */
    val = __scale_int(range, _clamp_to_range(range, gain));

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        if (strcmp(stage, "dsa") == 0) {
            if (board_data->tx_mute[ad9361_channel]) {
                /* TX not currently active, so cache the value */
                status = _set_tx_gain_cache(dev, ch, -val);
                if (status < 0) {
                    RETURN_ERROR_STATUS("_set_tx_gain_cache", status);
                }
            } else {
                /* TX is active, so apply immediately */
                status = ad9361_set_tx_attenuation(phy, ad9361_channel, -val);
                if (status < 0) {
                    RETURN_ERROR_AD9361("ad9361_set_tx_attenuation", status);
                }
            }

            return 0;
        }
    } else {
        if (strcmp(stage, "full") == 0) {
            status = ad9361_set_rx_rf_gain(phy, ad9361_channel, val);
            if (status < 0) {
                RETURN_ERROR_AD9361("ad9361_set_rx_rf_gain", status);
            }

            return 0;
        }
    }

    log_warning("%s: gain stage '%s' invalid\n", __FUNCTION__, stage);
    return 0;
}

static int bladerf2_set_gain(struct bladerf *dev, bladerf_channel ch, int gain)
{
    int status;
    float offset = 0.0f;

    status = _get_gain_offset(dev, ch, &offset);
    if (status < 0) {
        RETURN_ERROR_STATUS("_get_gain_offset", status);
    }

    gain -= __round_int(offset);

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        return bladerf2_set_gain_stage(dev, ch, "dsa", gain);
    } else {
        return bladerf2_set_gain_stage(dev, ch, "full", gain);
    }
}

static int bladerf2_get_gain_stage(struct bladerf *dev,
                                   bladerf_channel ch,
                                   char const *stage,
                                   int *gain)
{
    struct bladerf2_board_data *board_data;
    struct ad9361_rf_phy *phy;
    struct bladerf_range const *range = NULL;
    uint8_t const ad9361_channel      = ch >> 1;

    int status;

    if (NULL == stage) {
        RETURN_INVAL("stage", "is null");
    }

    if (NULL == gain) {
        RETURN_INVAL("gain", "is null");
    }

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;
    phy        = board_data->phy;

    status = bladerf2_get_gain_stage_range(dev, ch, stage, &range);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf2_get_gain_stage_range", status);
    }

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        if (strcmp(stage, "dsa") == 0) {
            uint32_t atten;

            if (board_data->tx_mute[ad9361_channel]) {
                /* TX is muted, get cached value */
                atten = _get_tx_gain_cache(dev, ch);
            } else {
                /* Get actual value from hardware */
                status = ad9361_get_tx_attenuation(phy, ad9361_channel, &atten);
                if (status < 0) {
                    RETURN_ERROR_AD9361("ad9361_get_tx_attenuation", status);
                }
            }

            /* Flip sign, unscale */
            *gain = -(__unscale_int(range, atten));
            return 0;
        }
    } else {
        struct rf_rx_gain rx_gain;

        status = ad9361_get_rx_gain(phy, ad9361_channel + 1, &rx_gain);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_get_rx_gain", status);
        }

        if (strcmp(stage, "full") == 0) {
            *gain = __unscale_int(range, rx_gain.gain_db);
            return 0;
        } else if (strcmp(stage, "digital") == 0) {
            *gain = __unscale_int(range, rx_gain.digital_gain);
            return 0;
        }
    }

    log_warning("%s: gain stage '%s' invalid\n", __FUNCTION__, stage);
    return 0;
}

static int bladerf2_get_gain(struct bladerf *dev, bladerf_channel ch, int *gain)
{
    int status;
    int val;
    float offset;

    if (NULL == gain) {
        RETURN_INVAL("gain", "is null");
    }

    status = _get_gain_offset(dev, ch, &offset);
    if (status < 0) {
        RETURN_ERROR_STATUS("_get_gain_offset", status);
    }

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        status = bladerf2_get_gain_stage(dev, ch, "dsa", &val);
    } else {
        status = bladerf2_get_gain_stage(dev, ch, "full", &val);
    }

    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf2_get_gain_stage", status);
    }

    *gain = __round_int(val + offset);

    return 0;
}

static int bladerf2_set_gain_mode(struct bladerf *dev,
                                  bladerf_channel ch,
                                  bladerf_gain_mode mode)
{
    struct bladerf2_board_data *board_data;
    uint8_t ad9361_channel;
    enum rf_gain_ctrl_mode gc_mode;
    struct bladerf_ad9361_gain_mode_map const *mode_map;
    size_t mode_map_len;
    int status;
    size_t i;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        RETURN_ERROR_STATUS("bladerf2_set_gain_mode(tx)",
                            BLADERF_ERR_UNSUPPORTED);
    }

    /* Channel conversion */
    switch (ch) {
        case BLADERF_CHANNEL_RX(0):
            ad9361_channel = 0;
            gc_mode        = bladerf2_rfic_init_params.gc_rx1_mode;
            break;

        case BLADERF_CHANNEL_RX(1):
            ad9361_channel = 1;
            gc_mode        = bladerf2_rfic_init_params.gc_rx2_mode;
            break;

        default:
            log_error("%s: unknown channel index (%d)\n", __FUNCTION__, ch);
            return BLADERF_ERR_UNSUPPORTED;
    }

    /* Mode conversion */
    if (mode != BLADERF_GAIN_DEFAULT) {
        mode_map     = bladerf2_rx_gain_mode_map;
        mode_map_len = ARRAY_SIZE(bladerf2_rx_gain_mode_map);

        for (i = 0; i < mode_map_len; ++i) {
            if (mode_map[i].brf_mode == mode) {
                gc_mode = mode_map[i].ad9361_mode;
                break;
            }
        }
    }

    /* Set the mode! */
    status = ad9361_set_rx_gain_control_mode(board_data->phy, ad9361_channel,
                                             gc_mode);
    if (status < 0) {
        RETURN_ERROR_AD9361("ad9361_set_rx_gain_control_mode", status);
    }

    return 0;
}

static int bladerf2_get_gain_mode(struct bladerf *dev,
                                  bladerf_channel ch,
                                  bladerf_gain_mode *mode)
{
    struct bladerf2_board_data *board_data;
    struct ad9361_rf_phy *phy;
    uint8_t ad9361_channel;
    uint8_t gc_mode;
    struct bladerf_ad9361_gain_mode_map const *mode_map;
    size_t mode_map_len;
    int status;
    size_t i;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;
    phy        = board_data->phy;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        RETURN_ERROR_STATUS("bladerf2_get_gain_mode(tx)",
                            BLADERF_ERR_UNSUPPORTED);
    }

    /* Channel conversion */
    switch (ch) {
        case BLADERF_CHANNEL_RX(0):
            ad9361_channel = 0;
            break;

        case BLADERF_CHANNEL_RX(1):
            ad9361_channel = 1;
            break;

        default:
            log_error("%s: unknown channel index (%d)\n", __FUNCTION__, ch);
            return BLADERF_ERR_UNSUPPORTED;
    }

    /* Get the gain */
    status = ad9361_get_rx_gain_control_mode(phy, ad9361_channel, &gc_mode);
    if (status < 0) {
        RETURN_ERROR_AD9361("ad9361_get_rx_gain_control_mode", status);
    }

    /* Mode conversion */
    if (mode != NULL) {
        *mode        = BLADERF_GAIN_DEFAULT;
        mode_map     = bladerf2_rx_gain_mode_map;
        mode_map_len = ARRAY_SIZE(bladerf2_rx_gain_mode_map);

        for (i = 0; i < mode_map_len; ++i) {
            if (mode_map[i].ad9361_mode == gc_mode) {
                *mode = mode_map[i].brf_mode;
                break;
            }
        }
    }

    return 0;
}

static int bladerf2_get_gain_modes(struct bladerf *dev,
                                   bladerf_channel ch,
                                   struct bladerf_gain_modes const **modes)
{
    struct bladerf_gain_modes const *mode_infos;
    unsigned int mode_infos_len;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        mode_infos     = NULL;
        mode_infos_len = 0;
    } else {
        mode_infos     = bladerf2_rx_gain_modes;
        mode_infos_len = ARRAY_SIZE(bladerf2_rx_gain_modes);
    }

    if (modes != NULL) {
        *modes = mode_infos;
    }

    return mode_infos_len;
}

static int bladerf2_get_gain_stages(struct bladerf *dev,
                                    bladerf_channel ch,
                                    char const **stages,
                                    size_t count)
{
    struct bladerf_gain_range const *ranges;
    unsigned int ranges_len;
    char const **names;

    size_t stage_count = 0;
    size_t i, j;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        ranges     = bladerf2_tx_gain_ranges;
        ranges_len = ARRAY_SIZE(bladerf2_tx_gain_ranges);
    } else {
        ranges     = bladerf2_rx_gain_ranges;
        ranges_len = ARRAY_SIZE(bladerf2_rx_gain_ranges);
    }

    names = calloc(ranges_len + 1, sizeof(char *));
    if (NULL == names) {
        RETURN_ERROR_STATUS("calloc names", BLADERF_ERR_MEM);
    }

    // Iterate through all the ranges...
    for (i = 0; i < ranges_len; ++i) {
        struct bladerf_gain_range const *range = &(ranges[i]);

        if (NULL == range->name) {
            // this is system gain, skip it
            continue;
        }

        // loop through the output array to make sure we record this one
        for (j = 0; j < ranges_len; ++j) {
            if (NULL == names[j]) {
                // Made it to the end of names without finding a match
                names[j] = range->name;
                ++stage_count;
                break;
            } else if (strcmp(range->name, names[j]) == 0) {
                // found a match, break
                break;
            }
        }
    }

    if (NULL != stages && 0 != count) {
        count = (stage_count < count) ? stage_count : count;

        for (i = 0; i < count; i++) {
            stages[i] = names[i];
        }
    }

    free((char **)names);
    return (int)stage_count;
}


/******************************************************************************/
/* Sample Rate */
/******************************************************************************/

static int bladerf2_get_sample_rate_range(struct bladerf *dev,
                                          bladerf_channel ch,
                                          const struct bladerf_range **range)
{
    if (NULL == range) {
        RETURN_INVAL("range", "is null");
    }

    *range = &bladerf2_sample_rate_range;

    return 0;
}

static int bladerf2_get_sample_rate(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bladerf_sample_rate *rate)
{
    struct bladerf2_board_data *board_data;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        status = ad9361_get_tx_sampling_freq(board_data->phy, rate);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_get_tx_sampling_freq", status);
        }
    } else {
        status = ad9361_get_rx_sampling_freq(board_data->phy, rate);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_get_rx_sampling_freq", status);
        }
    }

    return 0;
}

static int bladerf2_set_sample_rate(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bladerf_sample_rate rate,
                                    bladerf_sample_rate *actual)
{
    struct bladerf2_board_data *board_data;
    const struct bladerf_range *range = NULL;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;

    status = bladerf2_get_sample_rate_range(dev, ch, &range);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf2_get_sample_rate_range", status);
    }

    if (!_is_within_range(range, rate)) {
        return BLADERF_ERR_RANGE;
    }

    /* If the requested sample rate is outside of the native range, we must
     * adjust the FIR filtering. */
    if (_is_within_range(&bladerf2_sample_rate_range_4x, rate)) {
        bladerf_rfic_rxfir rxfir;
        bladerf_rfic_txfir txfir;

        status = bladerf_get_rfic_rx_fir(dev, &rxfir);
        if (status < 0) {
            RETURN_ERROR_STATUS("bladerf_get_rfic_rx_fir", status);
        }

        status = bladerf_get_rfic_tx_fir(dev, &txfir);
        if (status < 0) {
            RETURN_ERROR_STATUS("bladerf_get_rfic_tx_fir", status);
        }

        if (rxfir != BLADERF_RFIC_RXFIR_DEC4 ||
            txfir != BLADERF_RFIC_TXFIR_INT4 ||
            !board_data->low_samplerate_mode) {
            log_debug("enabling 4x decimation/interpolation filters\n");

            board_data->rxfir_orig = rxfir;
            status = bladerf_set_rfic_rx_fir(dev, BLADERF_RFIC_RXFIR_DEC4);
            if (status < 0) {
                RETURN_ERROR_STATUS("bladerf_set_rfic_rx_fir", status);
            }

            board_data->txfir_orig = txfir;
            status = bladerf_set_rfic_tx_fir(dev, BLADERF_RFIC_TXFIR_INT4);
            if (status < 0) {
                RETURN_ERROR_STATUS("bladerf_set_rfic_tx_fir", status);
            }

            board_data->low_samplerate_mode = true;
        }
    }

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        status = ad9361_set_tx_sampling_freq(board_data->phy, rate);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_set_tx_sampling_freq", status);
        }
    } else {
        status = ad9361_set_rx_sampling_freq(board_data->phy, rate);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_set_rx_sampling_freq", status);
        }
    }

    if (board_data->low_samplerate_mode &&
        !_is_within_range(&bladerf2_sample_rate_range_4x, rate)) {
        log_debug("disabling 4x decimation/interpolation filters\n");

        status = bladerf_set_rfic_rx_fir(dev, board_data->rxfir_orig);
        if (status < 0) {
            RETURN_ERROR_STATUS("bladerf_set_rfic_fir", status);
        }

        status = bladerf_set_rfic_tx_fir(dev, board_data->txfir_orig);
        if (status < 0) {
            RETURN_ERROR_STATUS("bladerf_set_rfic_fir", status);
        }

        board_data->low_samplerate_mode = false;
    }

    if (actual != NULL) {
        status = bladerf2_get_sample_rate(dev, ch, actual);
        if (status < 0) {
            RETURN_ERROR_STATUS("bladerf2_get_sample_rate", status);
        }
    }

    /* Warn the user if this isn't achievable */
    _check_total_sample_rate(dev, NULL);

    return 0;
}

static int bladerf2_get_rational_sample_rate(struct bladerf *dev,
                                             bladerf_channel ch,
                                             struct bladerf_rational_rate *rate)
{
    int status;
    bladerf_sample_rate integer_rate;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    status = bladerf2_get_sample_rate(dev, ch, &integer_rate);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf2_get_sample_rate", status);
    }

    if (rate != NULL) {
        rate->integer = integer_rate;
        rate->num     = 0;
        rate->den     = 1;
    }

    return 0;
}

static int bladerf2_set_rational_sample_rate(
    struct bladerf *dev,
    bladerf_channel ch,
    struct bladerf_rational_rate *rate,
    struct bladerf_rational_rate *actual)
{
    int status;
    bladerf_sample_rate integer_rate, actual_integer_rate;

    if (NULL == rate) {
        RETURN_INVAL("rate", "is null");
    }

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    integer_rate = (bladerf_sample_rate)(rate->integer + rate->num / rate->den);

    status =
        bladerf2_set_sample_rate(dev, ch, integer_rate, &actual_integer_rate);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf2_set_sample_rate", status);
    }

    if (actual != NULL) {
        status = bladerf2_get_rational_sample_rate(dev, ch, actual);
        if (status < 0) {
            RETURN_ERROR_STATUS("bladerf2_get_rational_sample_rate", status);
        }
    }

    return 0;
}


/******************************************************************************/
/* Bandwidth */
/******************************************************************************/

static int bladerf2_get_bandwidth_range(struct bladerf *dev,
                                        bladerf_channel ch,
                                        const struct bladerf_range **range)
{
    if (NULL == range) {
        RETURN_INVAL("range", "is null");
    }

    *range = &bladerf2_bandwidth_range;
    return 0;
}

static int bladerf2_get_bandwidth(struct bladerf *dev,
                                  bladerf_channel ch,
                                  unsigned int *bandwidth)
{
    struct bladerf2_board_data *board_data;
    int status;

    if (NULL == bandwidth) {
        RETURN_INVAL("bandwidth", "is null");
    }

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        status = ad9361_get_tx_rf_bandwidth(board_data->phy, bandwidth);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_get_tx_rf_bandwidth", status);
        }
    } else {
        status = ad9361_get_rx_rf_bandwidth(board_data->phy, bandwidth);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_get_rx_rf_bandwidth", status);
        }
    }

    return 0;
}

static int bladerf2_set_bandwidth(struct bladerf *dev,
                                  bladerf_channel ch,
                                  unsigned int bandwidth,
                                  unsigned int *actual)
{
    struct bladerf2_board_data *board_data;
    const struct bladerf_range *range = NULL;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;

    status = bladerf2_get_bandwidth_range(dev, ch, &range);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf2_get_bandwidth_range", status);
    }

    bandwidth = (unsigned int)_clamp_to_range(range, bandwidth);

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        status = ad9361_set_tx_rf_bandwidth(board_data->phy, bandwidth);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_set_tx_rf_bandwidth", status);
        }
    } else {
        status = ad9361_set_rx_rf_bandwidth(board_data->phy, bandwidth);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_set_rx_rf_bandwidth", status);
        }
    }

    if (actual != NULL) {
        return bladerf2_get_bandwidth(dev, ch, actual);
    }

    return 0;
}


/******************************************************************************/
/* Frequency */
/******************************************************************************/

static int bladerf2_get_frequency_range(struct bladerf *dev,
                                        bladerf_channel ch,
                                        const struct bladerf_range **range)
{
    if (NULL == range) {
        RETURN_INVAL("range", "is null");
    }

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        *range = &bladerf2_tx_frequency_range;
    } else {
        *range = &bladerf2_rx_frequency_range;
    }

    return 0;
}

static int bladerf2_select_band(struct bladerf *dev,
                                bladerf_channel ch,
                                bladerf_frequency frequency)
{
    int status;
    uint32_t reg;
    size_t i;

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    /* Read RFFE control register */
    status = dev->backend->rffe_control_read(dev, &reg);
    if (status < 0) {
        RETURN_ERROR_STATUS("rffe_control_read", status);
    }

    /* We have to do this for all the channels sharing the same LO. */
    for (i = 0; i < 2; ++i) {
        bladerf_channel bch = BLADERF_CHANNEL_IS_TX(ch) ? BLADERF_CHANNEL_TX(i)
                                                        : BLADERF_CHANNEL_RX(i);

        /* Is this channel enabled? */
        bool enable = _is_rffe_ch_enabled(reg, bch);

        /* Update SPDT bits accordingly */
        status = _modify_spdt_bits_by_freq(&reg, bch, enable, frequency);
        if (status < 0) {
            RETURN_ERROR_STATUS("_modify_spdt_bits_by_freq", status);
        }
    }

    status = dev->backend->rffe_control_write(dev, reg);
    if (status < 0) {
        RETURN_ERROR_STATUS("rffe_control_write", status);
    }

    /* Set AD9361 port */
    status = _set_ad9361_port_by_freq(dev, ch, _is_rffe_ch_enabled(reg, ch),
                                      frequency);
    if (status < 0) {
        RETURN_ERROR_STATUS("_set_ad9361_port_by_freq", status);
    }

    return 0;
}

static int bladerf2_set_frequency(struct bladerf *dev,
                                  bladerf_channel ch,
                                  bladerf_frequency frequency)
{
    struct bladerf2_board_data *board_data;
    const struct bladerf_range *range = NULL;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;

    status = bladerf2_get_frequency_range(dev, ch, &range);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf2_get_frequency_range", status);
    }

    if (!_is_within_range(range, frequency)) {
        return BLADERF_ERR_RANGE;
    }

    /* Set up band selection */
    status = bladerf2_select_band(dev, ch, frequency);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf2_select_band", status);
    }

    /* Change LO frequency */
    if (BLADERF_CHANNEL_IS_TX(ch)) {
        status = ad9361_set_tx_lo_freq(board_data->phy, frequency);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_set_tx_lo_freq", status);
        }
    } else {
        status = ad9361_set_rx_lo_freq(board_data->phy, frequency);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_set_rx_lo_freq", status);
        }
    }

    return 0;
}

static int bladerf2_get_frequency(struct bladerf *dev,
                                  bladerf_channel ch,
                                  bladerf_frequency *frequency)
{
    struct bladerf2_board_data *board_data;
    int status;
    bladerf_frequency lo_frequency;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        status = ad9361_get_tx_lo_freq(board_data->phy, &lo_frequency);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_get_tx_lo_freq", status);
        }
    } else {
        status = ad9361_get_rx_lo_freq(board_data->phy, &lo_frequency);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_get_rx_lo_freq", status);
        }
    }

    if (frequency != NULL) {
        *frequency = lo_frequency;
    }

    return 0;
}


/******************************************************************************/
/* RF ports */
/******************************************************************************/

static int bladerf2_set_rf_port(struct bladerf *dev,
                                bladerf_channel ch,
                                const char *port)
{
    struct bladerf2_board_data *board_data;
    const struct bladerf_ad9361_port_name_map *port_map;
    unsigned int port_map_len;
    uint32_t port_id = UINT32_MAX;
    int status;
    size_t i;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        port_map     = bladerf2_tx_port_map;
        port_map_len = ARRAY_SIZE(bladerf2_tx_port_map);
    } else {
        port_map     = bladerf2_rx_port_map;
        port_map_len = ARRAY_SIZE(bladerf2_rx_port_map);
    }

    for (i = 0; i < port_map_len; i++) {
        if (strcmp(port_map[i].name, port) == 0) {
            port_id = port_map[i].id;
            break;
        }
    }

    if (UINT32_MAX == port_id) {
        RETURN_INVAL("port", "is not valid");
    }

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        status = ad9361_set_tx_rf_port_output(board_data->phy, port_id);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_set_tx_rf_port_output", status);
        }
    } else {
        status = ad9361_set_rx_rf_port_input(board_data->phy, port_id);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_set_rx_rf_port_input", status);
        }
    }

    return 0;
}

static int bladerf2_get_rf_port(struct bladerf *dev,
                                bladerf_channel ch,
                                const char **port)
{
    struct bladerf2_board_data *board_data;
    const struct bladerf_ad9361_port_name_map *port_map;
    unsigned int port_map_len;
    uint32_t port_id;
    bool ok;
    int status;
    size_t i;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        port_map     = bladerf2_tx_port_map;
        port_map_len = ARRAY_SIZE(bladerf2_tx_port_map);
        status       = ad9361_get_tx_rf_port_output(board_data->phy, &port_id);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_get_tx_rf_port_output", status);
        }
    } else {
        port_map     = bladerf2_rx_port_map;
        port_map_len = ARRAY_SIZE(bladerf2_rx_port_map);
        status       = ad9361_get_rx_rf_port_input(board_data->phy, &port_id);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_get_rx_rf_port_input", status);
        }
    }

    if (port != NULL) {
        ok = false;
        for (i = 0; i < port_map_len; i++) {
            if (port_id == port_map[i].id) {
                *port = port_map[i].name;
                ok    = true;
                break;
            }
        }

        if (!ok) {
            *port = "unknown";
            log_error("%s: unexpected port_id %u\n", __FUNCTION__, port_id);
            return BLADERF_ERR_UNEXPECTED;
        }
    }

    return 0;
}

static int bladerf2_get_rf_ports(struct bladerf *dev,
                                 bladerf_channel ch,
                                 const char **ports,
                                 unsigned int count)
{
    const struct bladerf_ad9361_port_name_map *port_map;
    unsigned int port_map_len;
    size_t i;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        port_map     = bladerf2_tx_port_map;
        port_map_len = ARRAY_SIZE(bladerf2_tx_port_map);
    } else {
        port_map     = bladerf2_rx_port_map;
        port_map_len = ARRAY_SIZE(bladerf2_rx_port_map);
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

static int bladerf2_get_quick_tune(struct bladerf *dev,
                                   bladerf_channel ch,
                                   struct bladerf_quick_tune *quick_tune)
{
    const struct band_port_map *port_map;
    struct bladerf2_board_data *board_data;
    bladerf_frequency freq;
    int status;

    if (NULL == quick_tune) {
        RETURN_INVAL("quick_tune", "is null");
    }

    if (ch != BLADERF_CHANNEL_RX(0) && ch != BLADERF_CHANNEL_RX(1) &&
        ch != BLADERF_CHANNEL_TX(0) && ch != BLADERF_CHANNEL_TX(1)) {
        RETURN_INVAL_ARG("channel", ch, "is not valid");
    }

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;

    status = bladerf2_get_frequency(dev, ch, &freq);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf2_get_frequency", status);
    }

    port_map = _get_band_port_map_by_freq(ch, true, freq);

    if (BLADERF_CHANNEL_IS_TX(ch)) {

        if( board_data->quick_tune_tx_profile < NUM_BBP_FASTLOCK_PROFILES ) {
            /* Assign Nios and RFFE profile numbers */
            quick_tune->nios_profile = board_data->quick_tune_tx_profile++;
            log_verbose("Quick tune assigned Nios TX fast lock index: %u\n",
                        quick_tune->nios_profile);
            quick_tune->rffe_profile = quick_tune->nios_profile %
                NUM_RFFE_FASTLOCK_PROFILES;
            log_verbose("Quick tune assigned RFFE TX fast lock index: %u\n",
                        quick_tune->rffe_profile);
        } else {
            log_error("Reached maximum number of TX quick tune profiles.");
            return BLADERF_ERR_UNEXPECTED;
        }

        /* Create a fast lock profile in the RFIC */
        status = ad9361_tx_fastlock_store(board_data->phy,
                                          quick_tune->rffe_profile);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_tx_fastlock_store", status);
        }

        /* Save the fast lock profile to quick_tune structure */
        /*status = ad9361_tx_fastlock_save(board_data->phy,
                                         quick_tune->rffe_profile,
                                         &quick_tune->rffe_profile_data[0]);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_tx_fastlock_save", status);
        }*/

        /* Save a copy of the TX fast lock profile to the Nios */
        dev->backend->rffe_fastlock_save(dev, true, quick_tune->rffe_profile,
                                         quick_tune->nios_profile);

        /* Set the TX band */
        quick_tune->port = (port_map->ad9361_port << 6);

        /* Set the TX SPDTs */
        quick_tune->spdt = (port_map->spdt << 6) | (port_map->spdt << 4);

    } else {

        if( board_data->quick_tune_rx_profile < NUM_BBP_FASTLOCK_PROFILES ) {
            /* Assign Nios and RFFE profile numbers */
            quick_tune->nios_profile = board_data->quick_tune_rx_profile++;
            log_verbose("Quick tune assigned Nios RX fast lock index: %u\n",
                        quick_tune->nios_profile);
            quick_tune->rffe_profile = quick_tune->nios_profile %
                NUM_RFFE_FASTLOCK_PROFILES;
            log_verbose("Quick tune assigned RFFE RX fast lock index: %u\n",
                        quick_tune->rffe_profile);
        } else {
            log_error("Reached maximum number of RX quick tune profiles.");
            return BLADERF_ERR_UNEXPECTED;
        }

        /* Create a fast lock profile in the RFIC */
        status = ad9361_rx_fastlock_store(board_data->phy,
                                          quick_tune->rffe_profile);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_rx_fastlock_store", status);
        }

        /* Save the fast lock profile to quick_tune structure */
        /*status = ad9361_rx_fastlock_save(board_data->phy,
                                         quick_tune->rffe_profile,
                                         &quick_tune->rffe_profile_data[0]);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_rx_fastlock_save", status);
        }*/

        /* Save a copy of the RX fast lock profile to the Nios */
        dev->backend->rffe_fastlock_save(dev, false, quick_tune->rffe_profile,
                                         quick_tune->nios_profile);

        /* Set the RX bit */
        quick_tune->port = NIOS_PKT_RETUNE2_PORT_IS_RX_MASK;

        /* Set the RX band */
        if (port_map->ad9361_port < 3) {
            quick_tune->port |= (3 << (port_map->ad9361_port << 1));
        } else {
            quick_tune->port |= (1 << (port_map->ad9361_port - 3));
        }

        /* Set the RX SPDTs */
        quick_tune->spdt = (port_map->spdt << 2) | (port_map->spdt);
    }

    return 0;
}

static int bladerf2_schedule_retune(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bladerf_timestamp timestamp,
                                    bladerf_frequency frequency,
                                    struct bladerf_quick_tune *quick_tune)
{
    struct bladerf2_board_data *board_data = dev->board_data;

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
        log_error("Scheduled retunes require a quick tune parameter\n");
        return BLADERF_ERR_UNSUPPORTED;
    }

    return dev->backend->retune2(dev, ch, timestamp,
                                 quick_tune->nios_profile,
                                 quick_tune->rffe_profile,
                                 quick_tune->port,
                                 quick_tune->spdt);
}

static int bladerf2_cancel_scheduled_retunes(struct bladerf *dev,
                                             bladerf_channel ch)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    int status;

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    if (have_cap(board_data->capabilities, BLADERF_CAP_SCHEDULED_RETUNE)) {
        status = dev->backend->retune2(dev, ch, NIOS_PKT_RETUNE2_CLEAR_QUEUE,
                                       0, 0, 0, 0);
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

// clang-format off
static const struct {
    struct {
        uint16_t reg[2];        /* Low/High band */
        unsigned int shift;     /* Value scaling */
    } corr[4];
} ad9361_correction_reg_table[4] = {
    [BLADERF_CHANNEL_RX(0)].corr = {
        [BLADERF_CORR_DCOFF_I] = {
            FIELD_INIT(.reg, {0, 0}),  /* More complex look up */
            FIELD_INIT(.shift, 0),
        },
        [BLADERF_CORR_DCOFF_Q] = {
            FIELD_INIT(.reg, {0, 0}),  /* More complex look up */
            FIELD_INIT(.shift, 0),
        },
        [BLADERF_CORR_PHASE] = {
            FIELD_INIT(.reg, {  AD936X_REG_RX1_INPUT_A_PHASE_CORR,
                                AD936X_REG_RX1_INPUT_BC_PHASE_CORR  }),
            FIELD_INIT(.shift, 6),
        },
        [BLADERF_CORR_GAIN] = {
            FIELD_INIT(.reg, {  AD936X_REG_RX1_INPUT_A_GAIN_CORR,
                                AD936X_REG_RX1_INPUT_BC_PHASE_CORR  }),
            FIELD_INIT(.shift, 6),
        }
    },
    [BLADERF_CHANNEL_RX(1)].corr = {
        [BLADERF_CORR_DCOFF_I] = {
            FIELD_INIT(.reg, {0, 0}), /* More complex look up */
            FIELD_INIT(.shift, 0),
        },
        [BLADERF_CORR_DCOFF_Q] = {
            FIELD_INIT(.reg, {0, 0}), /* More complex look up */
            FIELD_INIT(.shift, 0),
        },
        [BLADERF_CORR_PHASE] = {
            FIELD_INIT(.reg, {  AD936X_REG_RX2_INPUT_A_PHASE_CORR,
                                AD936X_REG_RX2_INPUT_BC_PHASE_CORR  }),
            FIELD_INIT(.shift, 6),
        },
        [BLADERF_CORR_GAIN] = {
            FIELD_INIT(.reg, {  AD936X_REG_RX2_INPUT_A_GAIN_CORR,
                                AD936X_REG_RX2_INPUT_BC_PHASE_CORR  }),
            FIELD_INIT(.shift, 6),
        }
    },
    [BLADERF_CHANNEL_TX(0)].corr = {
        [BLADERF_CORR_DCOFF_I] = {
            FIELD_INIT(.reg, {  AD936X_REG_TX1_OUT_1_OFFSET_I,
                                AD936X_REG_TX1_OUT_2_OFFSET_I   }),
            FIELD_INIT(.shift, 5),
        },
        [BLADERF_CORR_DCOFF_Q] = {
            FIELD_INIT(.reg, {  AD936X_REG_TX1_OUT_1_OFFSET_Q,
                                AD936X_REG_TX1_OUT_2_OFFSET_Q   }),
            FIELD_INIT(.shift, 5),
        },
        [BLADERF_CORR_PHASE] = {
            FIELD_INIT(.reg, {  AD936X_REG_TX1_OUT_1_PHASE_CORR,
                                AD936X_REG_TX1_OUT_2_PHASE_CORR }),
            FIELD_INIT(.shift, 6),
        },
        [BLADERF_CORR_GAIN] = {
            FIELD_INIT(.reg, {  AD936X_REG_TX1_OUT_1_GAIN_CORR,
                                AD936X_REG_TX1_OUT_2_GAIN_CORR  }),
            FIELD_INIT(.shift, 6),
        }
    },
    [BLADERF_CHANNEL_TX(1)].corr = {
        [BLADERF_CORR_DCOFF_I] = {
            FIELD_INIT(.reg, {  AD936X_REG_TX2_OUT_1_OFFSET_I,
                                AD936X_REG_TX2_OUT_2_OFFSET_I   }),
            FIELD_INIT(.shift, 5),
        },
        [BLADERF_CORR_DCOFF_Q] = {
            FIELD_INIT(.reg, {  AD936X_REG_TX2_OUT_1_OFFSET_Q,
                                AD936X_REG_TX2_OUT_2_OFFSET_Q   }),
            FIELD_INIT(.shift, 5),
        },
        [BLADERF_CORR_PHASE] = {
            FIELD_INIT(.reg, {  AD936X_REG_TX2_OUT_1_PHASE_CORR,
                                AD936X_REG_TX2_OUT_2_PHASE_CORR }),
            FIELD_INIT(.shift, 6),
        },
        [BLADERF_CORR_GAIN] = {
            FIELD_INIT(.reg, {  AD936X_REG_TX2_OUT_1_GAIN_CORR,
                                AD936X_REG_TX2_OUT_2_GAIN_CORR  }),
            FIELD_INIT(.shift, 6),
        }
    },
};

static const struct {
    uint16_t reg_top;
    uint16_t reg_bot;
} ad9361_correction_rx_dcoff_reg_table[4][2][2] = {
    /* Channel 1 */
    [BLADERF_CHANNEL_RX(0)] = {
        /* A band */
        {
            /* I */
            {AD936X_REG_INPUT_A_OFFSETS_1, AD936X_REG_RX1_INPUT_A_OFFSETS},
            /* Q */
            {AD936X_REG_RX1_INPUT_A_OFFSETS, AD936X_REG_RX1_INPUT_A_Q_OFFSET},
        },
        /* B/C band */
        {
            /* I */
            {AD936X_REG_INPUT_BC_OFFSETS_1, AD936X_REG_RX1_INPUT_BC_OFFSETS},
            /* Q */
            {AD936X_REG_RX1_INPUT_BC_OFFSETS, AD936X_REG_RX1_INPUT_BC_Q_OFFSET},
        },
    },
    /* Channel 2 */
    [BLADERF_CHANNEL_RX(1)] = {
        /* A band */
        {
            /* I */
            {AD936X_REG_RX2_INPUT_A_I_OFFSET, AD936X_REG_RX2_INPUT_A_OFFSETS},
            /* Q */
            {AD936X_REG_RX2_INPUT_A_OFFSETS, AD936X_REG_INPUT_A_OFFSETS_1},
        },
        /* B/C band */
        {
            /* I */
            {AD936X_REG_RX2_INPUT_BC_I_OFFSET, AD936X_REG_RX2_INPUT_BC_OFFSETS},
            /* Q */
            {AD936X_REG_RX2_INPUT_BC_OFFSETS, AD936X_REG_INPUT_BC_OFFSETS_1},
        },
    },
};

static const int ad9361_correction_force_bit[2][4][2] = {
    [0] = {
        [BLADERF_CORR_DCOFF_I] = {2, 6},
        [BLADERF_CORR_DCOFF_Q] = {2, 6},
        [BLADERF_CORR_PHASE]   = {0, 4},
        [BLADERF_CORR_GAIN]    = {0, 4},
    },
    [1] = {
        [BLADERF_CORR_DCOFF_I] = {3, 7},
        [BLADERF_CORR_DCOFF_Q] = {3, 7},
        [BLADERF_CORR_PHASE]   = {1, 5},
        [BLADERF_CORR_GAIN]    = {1, 5},
    },
};
// clang-format on

static int bladerf2_get_correction(struct bladerf *dev,
                                   bladerf_channel ch,
                                   bladerf_correction corr,
                                   int16_t *value)
{
    struct bladerf2_board_data *board_data;
    int status;
    bool low_band;
    uint16_t reg, data;
    unsigned int shift;

    if (NULL == value) {
        RETURN_INVAL("value", "is null");
    }

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;

    /* Validate channel */
    if (ch != BLADERF_CHANNEL_RX(0) && ch != BLADERF_CHANNEL_RX(1) &&
        ch != BLADERF_CHANNEL_TX(0) && ch != BLADERF_CHANNEL_TX(1)) {
        RETURN_INVAL_ARG("channel", ch, "is not valid");
    }

    /* Validate correction */
    if (corr != BLADERF_CORR_DCOFF_I && corr != BLADERF_CORR_DCOFF_Q &&
        corr != BLADERF_CORR_PHASE && corr != BLADERF_CORR_GAIN) {
        RETURN_ERROR_STATUS("corr", BLADERF_ERR_UNSUPPORTED);
    }

    /* Look up band */
    if (BLADERF_CHANNEL_IS_TX(ch)) {
        uint32_t mode;

        status = ad9361_get_tx_rf_port_output(board_data->phy, &mode);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_get_tx_rf_port_output", status);
        }

        low_band = (mode == AD936X_TXA);
    } else {
        uint32_t mode;

        status = ad9361_get_rx_rf_port_input(board_data->phy, &mode);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_get_rx_rf_port_input", status);
        }

        /* Check if RX RF port mode is supported */
        if (mode != AD936X_A_BALANCED && mode != AD936X_B_BALANCED &&
            mode != AD936X_C_BALANCED) {
            RETURN_ERROR_STATUS("mode", BLADERF_ERR_UNSUPPORTED);
        }

        low_band = (mode == AD936X_A_BALANCED);
    }

    if ((corr == BLADERF_CORR_DCOFF_I || corr == BLADERF_CORR_DCOFF_Q) &&
        (ch & BLADERF_DIRECTION_MASK) == BLADERF_RX) {
        /* RX DC offset corrections are stuffed in a super convoluted way in
         * the register map. See AD9361 register map page 51. */
        bool is_q = (corr == BLADERF_CORR_DCOFF_Q);
        uint8_t data_top, data_bot;
        uint16_t data;

        /* Read top register */
        status = ad9361_spi_read(
            board_data->phy->spi,
            ad9361_correction_rx_dcoff_reg_table[ch][low_band][is_q].reg_top);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_spi_read(top)", status);
        }

        data_top = status;

        /* Read bottom register */
        status = ad9361_spi_read(
            board_data->phy->spi,
            ad9361_correction_rx_dcoff_reg_table[ch][low_band][is_q].reg_bot);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_spi_read(bottom)", status);
        }

        data_bot = status;

        if (ch == BLADERF_CHANNEL_RX(0)) {
            if (!is_q) {
                /*    top: | x x x x 9 8 7 6 | */
                /* bottom: | 5 4 3 2 1 0 x x | */
                data = ((data_top & 0xf) << 6) | (data_bot >> 2);
            } else {
                /*    top: | x x x x x x 9 8 | */
                /* bottom: | 7 6 5 4 3 2 1 0 | */
                data = ((data_top & 0x3) << 8) | data_bot;
            }
        } else {
            if (!is_q) {
                /*    top: | 9 8 7 6 5 4 3 2 | */
                /* bottom: | x x x x x x 1 0 | */
                data = (data_top << 2) | (data_bot & 0x3);
            } else {
                /*    top: | x x 9 8 7 6 5 4 | */
                /* bottom: | 3 2 1 0 x x x x | */
                data = (data_top << 4) | (data_bot >> 4);
            }
        }

        /* Scale 10-bit to 13-bit */
        data = data << 3;

        /* Sign extend value */
        *value = data | ((data & (1 << 12)) ? 0xf000 : 0x0000);
    } else {
        /* Look up correction register and value shift in table */
        reg   = ad9361_correction_reg_table[ch].corr[corr].reg[low_band];
        shift = ad9361_correction_reg_table[ch].corr[corr].shift;

        /* Read register and scale value */
        status = ad9361_spi_read(board_data->phy->spi, reg);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_spi_read(reg)", status);
        }

        /* Scale 8-bit to 12-bit/13-bit */
        data = status << shift;

        /* Sign extend value */
        if (shift == 5) {
            *value = data | ((data & (1 << 12)) ? 0xf000 : 0x0000);
        } else {
            *value = data | ((data & (1 << 13)) ? 0xc000 : 0x0000);
        }
    }

    return 0;
}

static int bladerf2_set_correction(struct bladerf *dev,
                                   bladerf_channel ch,
                                   bladerf_correction corr,
                                   int16_t value)
{
    struct bladerf2_board_data *board_data;
    int status;
    bool low_band;
    uint16_t reg, data;
    unsigned int shift;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;

    /* Validate channel */
    if (ch != BLADERF_CHANNEL_RX(0) && ch != BLADERF_CHANNEL_RX(1) &&
        ch != BLADERF_CHANNEL_TX(0) && ch != BLADERF_CHANNEL_TX(1)) {
        RETURN_INVAL_ARG("channel", ch, "is not valid");
    }

    /* Validate correction */
    if (corr != BLADERF_CORR_DCOFF_I && corr != BLADERF_CORR_DCOFF_Q &&
        corr != BLADERF_CORR_PHASE && corr != BLADERF_CORR_GAIN) {
        RETURN_ERROR_STATUS("corr", BLADERF_ERR_UNSUPPORTED);
    }

    /* Look up band */
    if (BLADERF_CHANNEL_IS_TX(ch)) {
        uint32_t mode;

        status = ad9361_get_tx_rf_port_output(board_data->phy, &mode);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_get_tx_rf_port_output", status);
        }

        low_band = (mode == AD936X_TXA);
    } else {
        uint32_t mode;

        status = ad9361_get_rx_rf_port_input(board_data->phy, &mode);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_get_rx_rf_port_input", status);
        }

        /* Check if RX RF port mode is supported */
        if (mode != AD936X_A_BALANCED && mode != AD936X_B_BALANCED &&
            mode != AD936X_C_BALANCED) {
            RETURN_ERROR_STATUS("mode", BLADERF_ERR_UNSUPPORTED);
        }

        low_band = (mode == AD936X_A_BALANCED);
    }

    if ((corr == BLADERF_CORR_DCOFF_I || corr == BLADERF_CORR_DCOFF_Q) &&
        (ch & BLADERF_DIRECTION_MASK) == BLADERF_RX) {
        /* RX DC offset corrections are stuffed in a super convoluted way in
         * the register map. See AD9361 register map page 51. */
        bool is_q = (corr == BLADERF_CORR_DCOFF_Q);
        uint8_t data_top, data_bot;

        /* Scale 13-bit to 10-bit */
        data = value >> 3;

        /* Read top register */
        status = ad9361_spi_read(
            board_data->phy->spi,
            ad9361_correction_rx_dcoff_reg_table[ch][low_band][is_q].reg_top);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_spi_read(top)", status);
        }

        data_top = status;

        /* Read bottom register */
        status = ad9361_spi_read(
            board_data->phy->spi,
            ad9361_correction_rx_dcoff_reg_table[ch][low_band][is_q].reg_bot);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_spi_read(bottom)", status);
        }

        data_bot = status;

        /* Modify registers */
        if (ch == BLADERF_CHANNEL_RX(0)) {
            if (!is_q) {
                /*    top: | x x x x 9 8 7 6 | */
                /* bottom: | 5 4 3 2 1 0 x x | */
                data_top = (data_top & 0xf0) | ((data >> 6) & 0x0f);
                data_bot = (data_bot & 0x03) | ((data & 0x3f) << 2);
            } else {
                /*    top: | x x x x x x 9 8 | */
                /* bottom: | 7 6 5 4 3 2 1 0 | */
                data_top = (data_top & 0xfc) | ((data >> 8) & 0x03);
                data_bot = data & 0xff;
            }
        } else {
            if (!is_q) {
                /*    top: | 9 8 7 6 5 4 3 2 | */
                /* bottom: | x x x x x x 1 0 | */
                data_top = (data >> 2) & 0xff;
                data_bot = (data_bot & 0xfc) | (data & 0x03);
            } else {
                /*    top: | x x 9 8 7 6 5 4 | */
                /* bottom: | 3 2 1 0 x x x x | */
                data_top = (data & 0xc0) | ((data >> 4) & 0x3f);
                data_bot = (data & 0x0f) | ((data & 0x0f) << 4);
            }
        }

        /* Write top register */
        status = ad9361_spi_write(
            board_data->phy->spi,
            ad9361_correction_rx_dcoff_reg_table[ch][low_band][is_q].reg_top,
            data_top);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_spi_write(top)", status);
        }

        /* Write bottom register */
        status = ad9361_spi_write(
            board_data->phy->spi,
            ad9361_correction_rx_dcoff_reg_table[ch][low_band][is_q].reg_bot,
            data_bot);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_spi_write(bottom)", status);
        }
    } else {
        /* Look up correction register and value shift in table */
        reg   = ad9361_correction_reg_table[ch].corr[corr].reg[low_band];
        shift = ad9361_correction_reg_table[ch].corr[corr].shift;

        /* Scale 12-bit/13-bit to 8-bit */
        data = (value >> shift) & 0xff;

        /* Write register */
        status = ad9361_spi_write(board_data->phy->spi, reg, data & 0xff);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_spi_write(reg)", status);
        }
    }

    reg = (BLADERF_CHANNEL_IS_TX(ch)) ? AD936X_REG_TX_FORCE_BITS
                                      : AD936X_REG_FORCE_BITS;

    /* Read force bit register */
    status = ad9361_spi_read(board_data->phy->spi, reg);
    if (status < 0) {
        RETURN_ERROR_AD9361("ad9361_spi_read(force)", status);
    }

    /* Modify register */
    data = status | (1 << ad9361_correction_force_bit[ch >> 1][corr][low_band]);

    /* Write force bit register */
    status = ad9361_spi_write(board_data->phy->spi, reg, data);
    if (status < 0) {
        RETURN_ERROR_AD9361("ad9361_spi_write(force)", status);
    }

    return 0;
}


/******************************************************************************/
/* Trigger */
/******************************************************************************/

static int bladerf2_trigger_init(struct bladerf *dev,
                                 bladerf_channel ch,
                                 bladerf_trigger_signal signal,
                                 struct bladerf_trigger *trigger)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return fpga_trigger_init(dev, ch, signal, trigger);
}

static int bladerf2_trigger_arm(struct bladerf *dev,
                                const struct bladerf_trigger *trigger,
                                bool arm,
                                uint64_t resv1,
                                uint64_t resv2)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return fpga_trigger_arm(dev, trigger, arm);
}

static int bladerf2_trigger_fire(struct bladerf *dev,
                                 const struct bladerf_trigger *trigger)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return fpga_trigger_fire(dev, trigger);
}

static int bladerf2_trigger_state(struct bladerf *dev,
                                  const struct bladerf_trigger *trigger,
                                  bool *is_armed,
                                  bool *has_fired,
                                  bool *fire_requested,
                                  uint64_t *resv1,
                                  uint64_t *resv2)
{
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    status =
        fpga_trigger_state(dev, trigger, is_armed, has_fired, fire_requested);

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
static int perform_format_config(struct bladerf *dev,
                                 bladerf_direction dir,
                                 bladerf_format format)
{
    int status;
    bool use_timestamps;
    bladerf_channel other;
    bool other_using_timestamps;
    uint32_t gpio_val;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    struct bladerf2_board_data *board_data = dev->board_data;

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

    status = dev->backend->config_gpio_read(dev, &gpio_val);
    if (status != 0) {
        return status;
    }

    if (use_timestamps) {
        gpio_val |= BLADERF_GPIO_TIMESTAMP;
    } else {
        gpio_val &= ~BLADERF_GPIO_TIMESTAMP;
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
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    struct bladerf2_board_data *board_data = dev->board_data;

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

static int bladerf2_init_stream(struct bladerf_stream **stream,
                                struct bladerf *dev,
                                bladerf_stream_cb callback,
                                void ***buffers,
                                size_t num_buffers,
                                bladerf_format format,
                                size_t samples_per_buffer,
                                size_t num_transfers,
                                void *user_data)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return async_init_stream(stream, dev, callback, buffers, num_buffers,
                             format, samples_per_buffer, num_transfers,
                             user_data);
}

static int bladerf2_stream(struct bladerf_stream *stream,
                           bladerf_channel_layout layout)
{
    bladerf_direction dir = layout & BLADERF_DIRECTION_MASK;
    int stream_status, fmt_status;

    switch (layout) {
        case BLADERF_RX_X1:
        case BLADERF_RX_X2:
        case BLADERF_TX_X1:
        case BLADERF_TX_X2:
            break;
        default:
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

static int bladerf2_submit_stream_buffer(struct bladerf_stream *stream,
                                         void *buffer,
                                         unsigned int timeout_ms,
                                         bool nonblock)
{
    return async_submit_stream_buffer(stream, buffer, timeout_ms, nonblock);
}

static void bladerf2_deinit_stream(struct bladerf_stream *stream)
{
    async_deinit_stream(stream);
}

static int bladerf2_set_stream_timeout(struct bladerf *dev,
                                       bladerf_direction dir,
                                       unsigned int timeout)
{
    struct bladerf2_board_data *board_data = dev->board_data;

    MUTEX_LOCK(&board_data->sync[dir].lock);
    board_data->sync[dir].stream_config.timeout_ms = timeout;
    MUTEX_UNLOCK(&board_data->sync[dir].lock);

    return 0;
}

static int bladerf2_get_stream_timeout(struct bladerf *dev,
                                       bladerf_direction dir,
                                       unsigned int *timeout)
{
    struct bladerf2_board_data *board_data = dev->board_data;

    MUTEX_LOCK(&board_data->sync[dir].lock);
    *timeout = board_data->sync[dir].stream_config.timeout_ms;
    MUTEX_UNLOCK(&board_data->sync[dir].lock);

    return 0;
}

static int bladerf2_sync_config(struct bladerf *dev,
                                bladerf_channel_layout layout,
                                bladerf_format format,
                                unsigned int num_buffers,
                                unsigned int buffer_size,
                                unsigned int num_transfers,
                                unsigned int stream_timeout)
{
    struct bladerf2_board_data *board_data;
    bladerf_direction dir = layout & BLADERF_DIRECTION_MASK;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;

    switch (layout) {
        case BLADERF_RX_X1:
        case BLADERF_RX_X2:
        case BLADERF_TX_X1:
        case BLADERF_TX_X2:
            break;
        default:
            return -EINVAL;
    }

    status = perform_format_config(dev, dir, format);
    if (status == 0) {
        status = sync_init(&board_data->sync[dir], dev, layout, format,
                           num_buffers, buffer_size, board_data->msg_size,
                           num_transfers, stream_timeout);
        if (status != 0) {
            perform_format_deconfig(dev, dir);
        }
    }

    return status;
}

static int bladerf2_sync_tx(struct bladerf *dev,
                            void const *samples,
                            unsigned int num_samples,
                            struct bladerf_metadata *metadata,
                            unsigned int timeout_ms)
{
    struct bladerf2_board_data *board_data;
    int status;

    if (NULL == dev || NULL == dev->board_data) {
        RETURN_INVAL("dev", "not initialized");
    }

    board_data = dev->board_data;

    if (!board_data->sync[BLADERF_TX].initialized) {
        RETURN_INVAL("sync tx", "not initialized");
    }

    status = sync_tx(&board_data->sync[BLADERF_TX], samples, num_samples,
                     metadata, timeout_ms);

    return status;
}

static int bladerf2_sync_rx(struct bladerf *dev,
                            void *samples,
                            unsigned int num_samples,
                            struct bladerf_metadata *metadata,
                            unsigned int timeout_ms)
{
    struct bladerf2_board_data *board_data;
    int status;

    if (NULL == dev || NULL == dev->board_data) {
        RETURN_INVAL("dev", "not initialized");
    }

    board_data = dev->board_data;

    if (!board_data->sync[BLADERF_RX].initialized) {
        RETURN_INVAL("sync rx", "not initialized");
    }

    status = sync_rx(&board_data->sync[BLADERF_RX], samples, num_samples,
                     metadata, timeout_ms);

    return status;
}

static int bladerf2_get_timestamp(struct bladerf *dev,
                                  bladerf_direction dir,
                                  bladerf_timestamp *value)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return dev->backend->get_timestamp(dev, dir, value);
}


/******************************************************************************/
/* FPGA/Firmware Loading/Flashing */
/******************************************************************************/

/* We do not build FPGAs with compression enabled. Therfore, they
 * will always have a fixed file size.
 */
#define FPGA_SIZE_XA4 (2632660)
#define FPGA_SIZE_XA9 (12858972)

static bool is_valid_fpga_size(bladerf_fpga_size fpga, size_t len)
{
    static const char env_override[] = "BLADERF_SKIP_FPGA_SIZE_CHECK";
    bool valid;
    size_t expected;

    switch (fpga) {
        case BLADERF_FPGA_A4:
            expected = FPGA_SIZE_XA4;
            break;

        case BLADERF_FPGA_A9:
            expected = FPGA_SIZE_XA9;
            break;

        default:
            expected = 0;
            break;
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
        } else if (len > BLADERF_FLASH_BYTE_LEN_FPGA) {
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

static int bladerf2_load_fpga(struct bladerf *dev,
                              const uint8_t *buf,
                              size_t length)
{
    struct bladerf2_board_data *board_data;
    int status;

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    board_data = dev->board_data;

    if (!is_valid_fpga_size(board_data->fpga_size, length)) {
        RETURN_INVAL("fpga file", "incorrect file size");
    }

    MUTEX_LOCK(&dev->lock);

    status = dev->backend->load_fpga(dev, buf, length);
    if (status != 0) {
        MUTEX_UNLOCK(&dev->lock);
        RETURN_ERROR_STATUS("load_fpga", status);
    }

    /* Update device state */
    board_data->state = STATE_FPGA_LOADED;

    MUTEX_UNLOCK(&dev->lock);

    status = bladerf2_initialize(dev);
    if (status != 0) {
        RETURN_ERROR_STATUS("bladerf2_initialize", status);
    }

    return 0;
}

static int bladerf2_flash_fpga(struct bladerf *dev,
                               const uint8_t *buf,
                               size_t length)
{
    struct bladerf2_board_data *board_data;

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    board_data = dev->board_data;

    if (!is_valid_fpga_size(board_data->fpga_size, length)) {
        RETURN_INVAL("fpga file", "incorrect file size");
    }

    return spi_flash_write_fpga_bitstream(dev, buf, length);
}

static int bladerf2_erase_stored_fpga(struct bladerf *dev)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    return spi_flash_erase_fpga(dev);
}

static bool is_valid_fw_size(size_t len)
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

static int bladerf2_flash_firmware(struct bladerf *dev,
                                   const uint8_t *buf,
                                   size_t length)
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
        log_info("Define BLADERF_SKIP_FW_SIZE_CHECK in your environment "
                 "to skip this check.\n");
        RETURN_INVAL_ARG("firmware size", length, "is not valid");
    }

    return spi_flash_write_fx3_fw(dev, buf, length);
}

static int bladerf2_device_reset(struct bladerf *dev)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    return dev->backend->device_reset(dev);
}


/******************************************************************************/
/* Tuning mode */
/******************************************************************************/

static int bladerf2_set_tuning_mode(struct bladerf *dev,
                                    bladerf_tuning_mode mode)
{
    struct bladerf2_board_data *board_data = dev->board_data;

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
        default:
            assert(!"Invalid tuning mode.");
            return BLADERF_ERR_INVAL;
    }

    board_data->tuning_mode = mode;

    return 0;
}

static int bladerf2_get_tuning_mode(struct bladerf *dev,
                                    bladerf_tuning_mode *mode)
{
    struct bladerf2_board_data *board_data = dev->board_data;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    *mode = board_data->tuning_mode;

    return 0;
}


/******************************************************************************/
/* Loopback */
/******************************************************************************/

static int bladerf2_get_loopback_modes(
    struct bladerf *dev, struct bladerf_loopback_modes const **modes)
{
    if (modes != NULL) {
        *modes = bladerf2_loopback_modes;
    }

    return ARRAY_SIZE(bladerf2_loopback_modes);
}

static int bladerf2_set_loopback(struct bladerf *dev, bladerf_loopback l)
{
    struct bladerf2_board_data *board_data;
    int32_t bist_loopback  = 0;
    bool firmware_loopback = false;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;

    switch (l) {
        case BLADERF_LB_NONE:
            break;
        case BLADERF_LB_FIRMWARE:
            firmware_loopback = true;
            break;
        case BLADERF_LB_RFIC_BIST:
            bist_loopback = 1;
            break;
        default:
            log_error("%s: unknown loopback mode (%d)\n", __FUNCTION__, l);
            return BLADERF_ERR_UNEXPECTED;
    }

    /* Set digital loopback state */
    status = ad9361_bist_loopback(board_data->phy, bist_loopback);
    if (status < 0) {
        RETURN_ERROR_AD9361("ad9361_bist_loopback", status);
    }

    /* Set firmware loopback state */
    status = dev->backend->set_firmware_loopback(dev, firmware_loopback);
    if (status < 0) {
        RETURN_ERROR_STATUS("set_firmware_loopback", status);
    }

    return 0;
}

static int bladerf2_get_loopback(struct bladerf *dev, bladerf_loopback *l)
{
    struct bladerf2_board_data *board_data;
    int status;
    bool fw_loopback;
    int32_t ad9361_loopback;

    if (NULL == l) {
        RETURN_INVAL("l", "is null");
    }

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;

    /* Read firwmare loopback */
    status = dev->backend->get_firmware_loopback(dev, &fw_loopback);
    if (status < 0) {
        RETURN_ERROR_STATUS("get_firmware_loopback", status);
    }

    if (fw_loopback) {
        *l = BLADERF_LB_FIRMWARE;
        return 0;
    }

    /* Read AD9361 bist loopback */
    /* Note: this returns void */
    ad9361_get_bist_loopback(board_data->phy, &ad9361_loopback);

    if (ad9361_loopback == 1) {
        *l = BLADERF_LB_RFIC_BIST;
        return 0;
    }

    *l = BLADERF_LB_NONE;

    return 0;
}


/******************************************************************************/
/* Sample RX FPGA Mux */
/******************************************************************************/

static int bladerf2_set_rx_mux(struct bladerf *dev, bladerf_rx_mux mode)
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
            rx_mux_val = ((uint32_t)mode) << BLADERF_GPIO_RX_MUX_SHIFT;
            break;

        default:
            log_debug("Invalid RX mux mode setting passed to %s(): %d\n", mode,
                      __FUNCTION__);
            RETURN_INVAL_ARG("bladerf_rx_mux", mode, "is invalid");
    }

    status = dev->backend->config_gpio_read(dev, &config_gpio);
    if (status != 0) {
        RETURN_ERROR_STATUS("config_gpio_read", status);
    }

    /* Clear out and assign the associated RX mux bits */
    config_gpio &= ~BLADERF_GPIO_RX_MUX_MASK;
    config_gpio |= rx_mux_val;

    status = dev->backend->config_gpio_write(dev, config_gpio);
    if (status != 0) {
        RETURN_ERROR_STATUS("config_gpio_write", status);
    }

    return 0;
}

static int bladerf2_get_rx_mux(struct bladerf *dev, bladerf_rx_mux *mode)
{
    bladerf_rx_mux val;
    uint32_t config_gpio;
    int status;

    if (NULL == mode) {
        RETURN_INVAL("mode", "is null");
    }

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    status = dev->backend->config_gpio_read(dev, &config_gpio);
    if (status != 0) {
        RETURN_ERROR_STATUS("config_gpio_read", status);
    }

    /* Extract RX mux bits */
    config_gpio &= BLADERF_GPIO_RX_MUX_MASK;
    config_gpio >>= BLADERF_GPIO_RX_MUX_SHIFT;
    val = config_gpio;

    /* Ensure it's a valid/supported value */
    switch (val) {
        case BLADERF_RX_MUX_BASEBAND:
        case BLADERF_RX_MUX_12BIT_COUNTER:
        case BLADERF_RX_MUX_32BIT_COUNTER:
        case BLADERF_RX_MUX_DIGITAL_LOOPBACK:
            *mode = val;
            break;

        default:
            *mode  = BLADERF_RX_MUX_INVALID;
            status = BLADERF_ERR_UNEXPECTED;
            log_debug("Invalid rx mux mode %d read from config gpio\n", val);
    }

    return status;
}


/******************************************************************************/
/* Low-level VCTCXO Tamer Mode */
/******************************************************************************/

static int bladerf2_set_vctcxo_tamer_mode(struct bladerf *dev,
                                          bladerf_vctcxo_tamer_mode mode)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_get_vctcxo_tamer_mode(struct bladerf *dev,
                                          bladerf_vctcxo_tamer_mode *mode)
{
    return BLADERF_ERR_UNSUPPORTED;
}

/******************************************************************************/
/* Low-level VCTCXO Trim DAC access */
/******************************************************************************/

static int bladerf2_get_trim_dac_enable(struct bladerf *dev, bool *enable)
{
    int status;
    uint16_t trim;

    if (NULL == enable) {
        RETURN_INVAL("enable", "is null");
    }

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    // Read current trim DAC setting
    status = dev->backend->ad56x1_vctcxo_trim_dac_read(dev, &trim);
    if (status < 0) {
        RETURN_ERROR_STATUS("ad56x1_vctcxo_trim_dac_read", status);
    }

    // Determine if it's enabled...
    *enable = (TRIMDAC_EN_ACTIVE == (trim >> TRIMDAC_EN));

    log_debug("trim DAC is %s\n", (*enable ? "enabled" : "disabled"));

    if ((trim >> TRIMDAC_EN) != TRIMDAC_EN_ACTIVE &&
        (trim >> TRIMDAC_EN) != TRIMDAC_EN_HIGHZ) {
        log_warning("unknown trim DAC state: 0x%x\n", (trim >> TRIMDAC_EN));
    }

    return 0;
}

static int bladerf2_set_trim_dac_enable(struct bladerf *dev, bool enable)
{
    int status;
    uint16_t trim;
    bool current_state;
    struct bladerf2_board_data *board_data;

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    board_data = dev->board_data;

    // See if we have anything to do
    status = bladerf2_get_trim_dac_enable(dev, &current_state);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf2_get_trim_dac_enable", status);
    }

    if (enable == current_state) {
        log_debug("trim DAC already %s, nothing to do\n",
                  enable ? "enabled" : "disabled");
        return 0;
    }

    // Read current trim DAC setting
    status = dev->backend->ad56x1_vctcxo_trim_dac_read(dev, &trim);
    if (status < 0) {
        RETURN_ERROR_STATUS("ad56x1_vctcxo_trim_dac_read", status);
    }

    // Set the trim DAC to high z if applicable
    if (!enable && trim != (TRIMDAC_EN_HIGHZ << TRIMDAC_EN)) {
        board_data->trimdac_last_value = trim;
        log_debug("saving current trim DAC value: 0x%04x\n", trim);
        trim = TRIMDAC_EN_HIGHZ << TRIMDAC_EN;
    } else if (enable && trim == (TRIMDAC_EN_HIGHZ << TRIMDAC_EN)) {
        trim = board_data->trimdac_last_value;
        log_debug("restoring old trim DAC value: 0x%04x\n", trim);
    }

    // Write back the trim DAC setting
    status = dev->backend->ad56x1_vctcxo_trim_dac_write(dev, trim);
    if (status < 0) {
        RETURN_ERROR_STATUS("ad56x1_vctcxo_trim_dac_write", status);
    }

    // Update our state flag
    board_data->trim_source = enable ? TRIM_SOURCE_TRIM_DAC : TRIM_SOURCE_NONE;

    return 0;
}

/**
 * @brief      Read the VCTCXO trim value from the SPI flash
 *
 * Retrieves the factory VCTCXO value from the SPI flash. This function
 * should not be used while sample streaming is in progress.
 *
 * @param      dev   Device handle
 * @param      trim  Pointer to populate with the trim value
 *
 * @return     0 on success, value from \ref RETCODES list on failure
 */
static int bladerf2_read_flash_vctcxo_trim(struct bladerf *dev, uint16_t *trim)
{
    int status;

    if (NULL == trim) {
        RETURN_INVAL("trim", "is null");
    }

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    status = spi_flash_read_vctcxo_trim(dev, trim);
    if (status < 0) {
        log_warning("Failed to get VCTCXO trim value: %s\n",
                    bladerf_strerror(status));
        log_debug("Defaulting DAC trim to 0x1ffc.\n");
        *trim = 0x1ffc;
        return 0;
    }

    return status;
}

static int bladerf2_get_vctcxo_trim(struct bladerf *dev, uint16_t *trim)
{
    struct bladerf2_board_data *board_data;

    if (NULL == trim) {
        RETURN_INVAL("trim", "is null");
    }

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    board_data = dev->board_data;

    *trim = board_data->trimdac_stored_value;

    return 0;
}

static int bladerf2_trim_dac_read(struct bladerf *dev, uint16_t *trim)
{
    if (NULL == trim) {
        RETURN_INVAL("trim", "is null");
    }

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    return dev->backend->ad56x1_vctcxo_trim_dac_read(dev, trim);
}

static int bladerf2_trim_dac_write(struct bladerf *dev, uint16_t trim)
{
    struct bladerf2_board_data *board_data;
    bool enable;
    uint16_t trim_control;
    uint16_t trim_value;
    int status;

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    board_data = dev->board_data;

    trim_control = (trim >> TRIMDAC_EN) & TRIMDAC_EN_MASK;
    trim_value   = trim & TRIMDAC_MASK;

    log_debug("requested trim 0x%04x (control 0x%01x value 0x%04x)\n", trim,
              trim_control, trim_value);

    // Is the trimdac enabled?
    status = bladerf2_get_trim_dac_enable(dev, &enable);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf2_get_trim_dac_enable", status);
    }

    // If the trimdac is not enabled, save this value for later but don't
    // apply it.
    if (!enable && trim_control != TRIMDAC_EN_HIGHZ) {
        log_warning("trim DAC is disabled. New value will be saved until "
                    "trim DAC is enabled\n");
        board_data->trimdac_last_value = trim_value;
        return 0;
    }

    return dev->backend->ad56x1_vctcxo_trim_dac_write(dev, trim);
}

/******************************************************************************/
/* Low-level Trigger control access */
/******************************************************************************/

static int bladerf2_read_trigger(struct bladerf *dev,
                                 bladerf_channel ch,
                                 bladerf_trigger_signal trigger,
                                 uint8_t *val)
{
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    return fpga_trigger_read(dev, ch, trigger, val);
}

static int bladerf2_write_trigger(struct bladerf *dev,
                                  bladerf_channel ch,
                                  bladerf_trigger_signal trigger,
                                  uint8_t val)
{
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    return fpga_trigger_write(dev, ch, trigger, val);
}


/******************************************************************************/
/* Low-level Configuration GPIO access */
/******************************************************************************/

static int bladerf2_config_gpio_read(struct bladerf *dev, uint32_t *val)
{
    if (NULL == val) {
        RETURN_INVAL("val", "is null");
    }

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    return dev->backend->config_gpio_read(dev, val);
}

static int bladerf2_config_gpio_write(struct bladerf *dev, uint32_t val)
{
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    return dev->backend->config_gpio_write(dev, val);
}


/******************************************************************************/
/* Low-level SPI Flash access */
/******************************************************************************/

static int bladerf2_erase_flash(struct bladerf *dev,
                                uint32_t erase_block,
                                uint32_t count)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    return spi_flash_erase(dev, erase_block, count);
}

static int bladerf2_read_flash(struct bladerf *dev,
                               uint8_t *buf,
                               uint32_t page,
                               uint32_t count)
{
    if (NULL == buf) {
        RETURN_INVAL("buf", "is null");
    }

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    return spi_flash_read(dev, buf, page, count);
}

static int bladerf2_write_flash(struct bladerf *dev,
                                const uint8_t *buf,
                                uint32_t page,
                                uint32_t count)
{
    if (NULL == buf) {
        RETURN_INVAL("buf", "is null");
    }

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    return spi_flash_write(dev, buf, page, count);
}


/******************************************************************************/
/* Expansion support */
/******************************************************************************/

static int bladerf2_expansion_attach(struct bladerf *dev, bladerf_xb xb)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_expansion_get_attached(struct bladerf *dev, bladerf_xb *xb)
{
    if (NULL == xb) {
        RETURN_INVAL("xb", "is null");
    }

    *xb = BLADERF_XB_NONE;

    return 0;
}


/******************************************************************************/
/* Board binding */
/******************************************************************************/

const struct board_fns bladerf2_board_fns = {
    FIELD_INIT(.matches, bladerf2_matches),
    FIELD_INIT(.open, bladerf2_open),
    FIELD_INIT(.close, bladerf2_close),
    FIELD_INIT(.device_speed, bladerf2_device_speed),
    FIELD_INIT(.get_serial, bladerf2_get_serial),
    FIELD_INIT(.get_fpga_size, bladerf2_get_fpga_size),
    FIELD_INIT(.get_flash_size, bladerf2_get_flash_size),
    FIELD_INIT(.is_fpga_configured, bladerf2_is_fpga_configured),
    FIELD_INIT(.get_fpga_source, bladerf2_get_fpga_source),
    FIELD_INIT(.get_capabilities, bladerf2_get_capabilities),
    FIELD_INIT(.get_channel_count, bladerf2_get_channel_count),
    FIELD_INIT(.get_fpga_version, bladerf2_get_fpga_version),
    FIELD_INIT(.get_fw_version, bladerf2_get_fw_version),
    FIELD_INIT(.set_gain, bladerf2_set_gain),
    FIELD_INIT(.get_gain, bladerf2_get_gain),
    FIELD_INIT(.set_gain_mode, bladerf2_set_gain_mode),
    FIELD_INIT(.get_gain_mode, bladerf2_get_gain_mode),
    FIELD_INIT(.get_gain_modes, bladerf2_get_gain_modes),
    FIELD_INIT(.get_gain_range, bladerf2_get_gain_range),
    FIELD_INIT(.set_gain_stage, bladerf2_set_gain_stage),
    FIELD_INIT(.get_gain_stage, bladerf2_get_gain_stage),
    FIELD_INIT(.get_gain_stage_range, bladerf2_get_gain_stage_range),
    FIELD_INIT(.get_gain_stages, bladerf2_get_gain_stages),
    FIELD_INIT(.set_sample_rate, bladerf2_set_sample_rate),
    FIELD_INIT(.set_rational_sample_rate, bladerf2_set_rational_sample_rate),
    FIELD_INIT(.get_sample_rate, bladerf2_get_sample_rate),
    FIELD_INIT(.get_sample_rate_range, bladerf2_get_sample_rate_range),
    FIELD_INIT(.get_rational_sample_rate, bladerf2_get_rational_sample_rate),
    FIELD_INIT(.set_bandwidth, bladerf2_set_bandwidth),
    FIELD_INIT(.get_bandwidth, bladerf2_get_bandwidth),
    FIELD_INIT(.get_bandwidth_range, bladerf2_get_bandwidth_range),
    FIELD_INIT(.get_frequency, bladerf2_get_frequency),
    FIELD_INIT(.set_frequency, bladerf2_set_frequency),
    FIELD_INIT(.get_frequency_range, bladerf2_get_frequency_range),
    FIELD_INIT(.select_band, bladerf2_select_band),
    FIELD_INIT(.set_rf_port, bladerf2_set_rf_port),
    FIELD_INIT(.get_rf_port, bladerf2_get_rf_port),
    FIELD_INIT(.get_rf_ports, bladerf2_get_rf_ports),
    FIELD_INIT(.get_quick_tune, bladerf2_get_quick_tune),
    FIELD_INIT(.schedule_retune, bladerf2_schedule_retune),
    FIELD_INIT(.cancel_scheduled_retunes, bladerf2_cancel_scheduled_retunes),
    FIELD_INIT(.get_correction, bladerf2_get_correction),
    FIELD_INIT(.set_correction, bladerf2_set_correction),
    FIELD_INIT(.trigger_init, bladerf2_trigger_init),
    FIELD_INIT(.trigger_arm, bladerf2_trigger_arm),
    FIELD_INIT(.trigger_fire, bladerf2_trigger_fire),
    FIELD_INIT(.trigger_state, bladerf2_trigger_state),
    FIELD_INIT(.enable_module, bladerf2_enable_module),
    FIELD_INIT(.init_stream, bladerf2_init_stream),
    FIELD_INIT(.stream, bladerf2_stream),
    FIELD_INIT(.submit_stream_buffer, bladerf2_submit_stream_buffer),
    FIELD_INIT(.deinit_stream, bladerf2_deinit_stream),
    FIELD_INIT(.set_stream_timeout, bladerf2_set_stream_timeout),
    FIELD_INIT(.get_stream_timeout, bladerf2_get_stream_timeout),
    FIELD_INIT(.sync_config, bladerf2_sync_config),
    FIELD_INIT(.sync_tx, bladerf2_sync_tx),
    FIELD_INIT(.sync_rx, bladerf2_sync_rx),
    FIELD_INIT(.get_timestamp, bladerf2_get_timestamp),
    FIELD_INIT(.load_fpga, bladerf2_load_fpga),
    FIELD_INIT(.flash_fpga, bladerf2_flash_fpga),
    FIELD_INIT(.erase_stored_fpga, bladerf2_erase_stored_fpga),
    FIELD_INIT(.flash_firmware, bladerf2_flash_firmware),
    FIELD_INIT(.device_reset, bladerf2_device_reset),
    FIELD_INIT(.set_tuning_mode, bladerf2_set_tuning_mode),
    FIELD_INIT(.get_tuning_mode, bladerf2_get_tuning_mode),
    FIELD_INIT(.get_loopback_modes, bladerf2_get_loopback_modes),
    FIELD_INIT(.set_loopback, bladerf2_set_loopback),
    FIELD_INIT(.get_loopback, bladerf2_get_loopback),
    FIELD_INIT(.get_rx_mux, bladerf2_get_rx_mux),
    FIELD_INIT(.set_rx_mux, bladerf2_set_rx_mux),
    FIELD_INIT(.set_vctcxo_tamer_mode, bladerf2_set_vctcxo_tamer_mode),
    FIELD_INIT(.get_vctcxo_tamer_mode, bladerf2_get_vctcxo_tamer_mode),
    FIELD_INIT(.get_vctcxo_trim, bladerf2_get_vctcxo_trim),
    FIELD_INIT(.trim_dac_read, bladerf2_trim_dac_read),
    FIELD_INIT(.trim_dac_write, bladerf2_trim_dac_write),
    FIELD_INIT(.read_trigger, bladerf2_read_trigger),
    FIELD_INIT(.write_trigger, bladerf2_write_trigger),
    FIELD_INIT(.config_gpio_read, bladerf2_config_gpio_read),
    FIELD_INIT(.config_gpio_write, bladerf2_config_gpio_write),
    FIELD_INIT(.erase_flash, bladerf2_erase_flash),
    FIELD_INIT(.read_flash, bladerf2_read_flash),
    FIELD_INIT(.write_flash, bladerf2_write_flash),
    FIELD_INIT(.expansion_attach, bladerf2_expansion_attach),
    FIELD_INIT(.expansion_get_attached, bladerf2_expansion_get_attached),
    FIELD_INIT(.name, "bladerf2"),
};


/******************************************************************************
 ******************************************************************************
 *                         bladeRF2-specific Functions *
 ******************************************************************************
 ******************************************************************************/

/******************************************************************************/
/* Bias Tee Control */
/******************************************************************************/

int bladerf_get_bias_tee(struct bladerf *dev, bladerf_channel ch, bool *enable)
{
    uint32_t reg;
    int status;
    uint32_t shift;

    if (NULL == dev) {
        RETURN_INVAL("dev", "not initialized");
    }

    if (dev->board != &bladerf2_board_fns) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    if (NULL == enable) {
        RETURN_INVAL("enable", "is null");
    }

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_FPGA_LOADED);

    shift = BLADERF_CHANNEL_IS_TX(ch) ? RFFE_CONTROL_TX_BIAS_EN
                                      : RFFE_CONTROL_RX_BIAS_EN;

    /* Read RFFE control register */
    status = dev->backend->rffe_control_read(dev, &reg);
    if (status < 0) {
        RETURN_ERROR_STATUS("rffe_control_read", status);
    }

    /* Check register value */
    *enable = (reg >> shift) & 0x1;

    MUTEX_UNLOCK(&dev->lock);

    return 0;
}

int bladerf_set_bias_tee(struct bladerf *dev, bladerf_channel ch, bool enable)
{
    uint32_t reg;
    int status;
    uint32_t shift;

    if (NULL == dev) {
        RETURN_INVAL("dev", "not initialized");
    }

    if (dev->board != &bladerf2_board_fns) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_FPGA_LOADED);

    shift = BLADERF_CHANNEL_IS_TX(ch) ? RFFE_CONTROL_TX_BIAS_EN
                                      : RFFE_CONTROL_RX_BIAS_EN;

    /* Read RFFE control register */
    status = dev->backend->rffe_control_read(dev, &reg);
    if (status < 0) {
        RETURN_ERROR_STATUS("rffe_control_read", status);
    }

    /* Clear register value */
    reg &= ~(1 << shift);

    /* Set register value */
    if (enable) {
        reg |= (1 << shift);
    }

    /* Write RFFE control register */
    log_debug("%s: rffe_control_write %08x\n", __FUNCTION__, reg);
    status = dev->backend->rffe_control_write(dev, reg);
    if (status < 0) {
        RETURN_ERROR_STATUS("rffe_control_write", status);
    }

    MUTEX_UNLOCK(&dev->lock);

    return 0;
}

/******************************************************************************/
/* Low level RFIC Accessors */
/******************************************************************************/

int bladerf_get_rfic_register(struct bladerf *dev,
                              uint16_t address,
                              uint8_t *val)
{
    int status;
    uint64_t data;

    if (NULL == dev) {
        RETURN_INVAL("dev", "not initialized");
    }

    if (dev->board != &bladerf2_board_fns) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    if (NULL == val) {
        RETURN_INVAL("val", "is null");
    }

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_FPGA_LOADED);

    address = AD936X_READ | AD936X_CNT(1) | address;

    status = dev->backend->ad9361_spi_read(dev, address, &data);
    if (status < 0) {
        MUTEX_UNLOCK(&dev->lock);
        RETURN_ERROR_AD9361("ad9361_spi_read", status);
    }

    *val = (data >> 56) & 0xff;

    MUTEX_UNLOCK(&dev->lock);

    return 0;
}

int bladerf_set_rfic_register(struct bladerf *dev,
                              uint16_t address,
                              uint8_t val)
{
    int status;
    uint64_t data;

    if (NULL == dev) {
        RETURN_INVAL("dev", "not initialized");
    }

    if (dev->board != &bladerf2_board_fns) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_FPGA_LOADED);

    address = AD936X_WRITE | AD936X_CNT(1) | address;

    data = (((uint64_t)val) << 56);

    status = dev->backend->ad9361_spi_write(dev, address, data);
    if (status < 0) {
        MUTEX_UNLOCK(&dev->lock);
        RETURN_ERROR_AD9361("ad9361_spi_write", status);
    }

    MUTEX_UNLOCK(&dev->lock);

    return 0;
}

int bladerf_get_rfic_temperature(struct bladerf *dev, float *val)
{
    struct bladerf2_board_data *board_data;
    struct ad9361_rf_phy *phy;

    if (NULL == dev) {
        RETURN_INVAL("dev", "not initialized");
    }

    if (dev->board != &bladerf2_board_fns) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    board_data = dev->board_data;
    phy        = board_data->phy;

    *val = ad9361_get_temp(phy) / 1000.0F;

    return 0;
}

int bladerf_get_rfic_rssi(struct bladerf *dev,
                          bladerf_channel ch,
                          int *pre_rssi,
                          int *sym_rssi)
{
    struct bladerf2_board_data *board_data;
    struct ad9361_rf_phy *phy;
    int status;
    int pre, sym;

    if (NULL == dev) {
        RETURN_INVAL("dev", "not initialized");
    }

    if (dev->board != &bladerf2_board_fns) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_FPGA_LOADED);

    board_data = dev->board_data;
    phy        = board_data->phy;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        uint32_t rssi = 0;

        status = ad9361_get_tx_rssi(phy, ch >> 1, &rssi);
        if (status < 0) {
            MUTEX_UNLOCK(&dev->lock);
            RETURN_ERROR_AD9361("ad9361_get_tx_rssi", status);
        }

        pre = __round_int(rssi / 1000.0);
        sym = __round_int(rssi / 1000.0);
    } else {
        struct rf_rssi rssi;

        status = ad9361_get_rx_rssi(phy, ch >> 1, &rssi);
        if (status < 0) {
            MUTEX_UNLOCK(&dev->lock);
            RETURN_ERROR_AD9361("ad9361_get_rx_rssi", status);
        }

        pre = __round_int(rssi.preamble / (float)rssi.multiplier);
        sym = __round_int(rssi.symbol / (float)rssi.multiplier);
    }

    if (NULL != pre_rssi) {
        *pre_rssi = -pre;
    }

    if (NULL != sym_rssi) {
        *sym_rssi = -sym;
    }

    MUTEX_UNLOCK(&dev->lock);

    return 0;
}

int bladerf_get_rfic_ctrl_out(struct bladerf *dev, uint8_t *ctrl_out)
{
    int status;
    uint32_t reg;

    if (NULL == dev) {
        RETURN_INVAL("dev", "not initialized");
    }

    if (dev->board != &bladerf2_board_fns) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    if (NULL == ctrl_out) {
        RETURN_INVAL("ctrl_out", "is null");
    }

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    /* Read RFFE control register */
    status = dev->backend->rffe_control_read(dev, &reg);
    if (status < 0) {
        RETURN_ERROR_STATUS("rffe_control_read", status);
    }

    *ctrl_out = (uint8_t)((reg >> RFFE_CONTROL_CTRL_OUT) & 0xFF);

    return 0;
}

int bladerf_get_rfic_rx_fir(struct bladerf *dev, bladerf_rfic_rxfir *rxfir)
{
    struct bladerf2_board_data *board_data;

    if (NULL == dev) {
        RETURN_INVAL("dev", "not initialized");
    }

    if (dev->board != &bladerf2_board_fns) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    board_data = dev->board_data;

    *rxfir = board_data->rxfir;

    return 0;
}

int bladerf_set_rfic_rx_fir(struct bladerf *dev, bladerf_rfic_rxfir rxfir)
{
    struct bladerf2_board_data *board_data;
    struct ad9361_rf_phy *phy;
    AD9361_RXFIRConfig *fir_config = NULL;
    uint8_t enable;
    int status;

    if (NULL == dev) {
        RETURN_INVAL("dev", "not initialized");
    }

    if (dev->board != &bladerf2_board_fns) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    board_data = dev->board_data;
    phy        = board_data->phy;

    if (BLADERF_RFIC_RXFIR_CUSTOM == rxfir) {
        log_warning("custom FIR not implemented, assuming default\n");
        rxfir = BLADERF_RFIC_RXFIR_DEFAULT;
    }

    switch (rxfir) {
        case BLADERF_RFIC_RXFIR_BYPASS:
            fir_config = &bladerf2_rfic_rx_fir_config;
            enable     = 0;
            break;
        case BLADERF_RFIC_RXFIR_DEC1:
            fir_config = &bladerf2_rfic_rx_fir_config;
            enable     = 1;
            break;
        case BLADERF_RFIC_RXFIR_DEC2:
            fir_config = &bladerf2_rfic_rx_fir_config_dec2;
            enable     = 1;
            break;
        case BLADERF_RFIC_RXFIR_DEC4:
            fir_config = &bladerf2_rfic_rx_fir_config_dec4;
            enable     = 1;
            break;
        default:
            assert(!"Bug: unhandled rxfir selection");
            return BLADERF_ERR_UNEXPECTED;
    }

    status = ad9361_set_rx_fir_config(phy, *fir_config);
    if (status < 0) {
        RETURN_ERROR_AD9361("ad9361_set_rx_fir_config", status);
    }

    status = ad9361_set_rx_fir_en_dis(phy, enable);
    if (status < 0) {
        RETURN_ERROR_AD9361("ad9361_set_rx_fir_en_dis", status);
    }

    board_data->rxfir = rxfir;

    return 0;
}

int bladerf_get_rfic_tx_fir(struct bladerf *dev, bladerf_rfic_txfir *txfir)
{
    struct bladerf2_board_data *board_data;

    if (NULL == dev) {
        RETURN_INVAL("dev", "not initialized");
    }

    if (dev->board != &bladerf2_board_fns) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    board_data = dev->board_data;

    *txfir = board_data->txfir;

    return 0;
}

int bladerf_set_rfic_tx_fir(struct bladerf *dev, bladerf_rfic_txfir txfir)
{
    struct bladerf2_board_data *board_data;
    struct ad9361_rf_phy *phy;
    AD9361_TXFIRConfig *fir_config = NULL;
    uint8_t enable;
    int status;

    if (NULL == dev) {
        RETURN_INVAL("dev", "not initialized");
    }

    if (dev->board != &bladerf2_board_fns) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    board_data = dev->board_data;
    phy        = board_data->phy;

    if (BLADERF_RFIC_TXFIR_CUSTOM == txfir) {
        log_warning("custom FIR not implemented, assuming default\n");
        txfir = BLADERF_RFIC_TXFIR_DEFAULT;
    }

    switch (txfir) {
        case BLADERF_RFIC_TXFIR_BYPASS:
            fir_config = &bladerf2_rfic_tx_fir_config;
            enable     = 0;
            break;
        case BLADERF_RFIC_TXFIR_INT1:
            fir_config = &bladerf2_rfic_tx_fir_config;
            enable     = 1;
            break;
        case BLADERF_RFIC_TXFIR_INT2:
            fir_config = &bladerf2_rfic_tx_fir_config_int2;
            enable     = 1;
            break;
        case BLADERF_RFIC_TXFIR_INT4:
            fir_config = &bladerf2_rfic_tx_fir_config_int4;
            enable     = 1;
            break;
        default:
            assert(!"Bug: unhandled txfir selection");
            return BLADERF_ERR_UNEXPECTED;
    }

    status = ad9361_set_tx_fir_config(phy, *fir_config);
    if (status < 0) {
        RETURN_ERROR_AD9361("ad9361_set_rx_fir_config", status);
    }

    status = ad9361_set_tx_fir_en_dis(phy, enable);
    if (status < 0) {
        RETURN_ERROR_AD9361("ad9361_set_rx_fir_en_dis", status);
    }

    board_data->txfir = txfir;

    return 0;
}

/******************************************************************************/
/* Low level PLL Accessors */
/******************************************************************************/

static int bladerf_pll_configure(struct bladerf *dev, uint16_t R, uint16_t N)
{
    int status;
    uint32_t init_array[3];
    bool is_enabled;
    uint8_t i;

    if (NULL == dev) {
        RETURN_INVAL("dev", "not initialized");
    }

    if (dev->board != &bladerf2_board_fns) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    if (R < 1 || R > 16383) {
        RETURN_INVAL("R", "outside range [1,16383]");
    }

    if (N < 1 || N > 8191) {
        RETURN_INVAL("N", "outside range [1,8191]");
    }

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    /* Get the present state... */
    status = bladerf_get_pll_enable(dev, &is_enabled);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf_get_pll_enable", status);
    }

    /* Enable the chip if applicable */
    if (!is_enabled) {
        bladerf_set_pll_enable(dev, true);
        if (status < 0) {
            RETURN_ERROR_STATUS("bladerf_set_pll_enable", status);
        }
    }

    /* Register 0: Reference Counter Latch */
    init_array[0] = 0;
    /* R Counter: */
    init_array[0] |= (R & ((1 << 14) - 1)) << 2;
    /* Hardcoded values: */
    /* Anti-backlash: 00 (2.9 ns) */
    /* Lock detect precision: 0 (three cycles) */

    /* Register 1: N Counter Latch */
    init_array[1] = 1;
    /* N Counter: */
    init_array[1] |= (N & ((1 << 13) - 1)) << 8;
    /* Hardcoded values: */
    /* CP Gain: 0 (Setting 1) */

    /* Register 2: Function Latch */
    init_array[2] = 2;
    /* Hardcoded values: */
    /* Counter operation: 0 (Normal) */
    /* Power down control: 00 (Normal) */
    /* Muxout control: 0x1 (digital lock detect) */
    init_array[2] |= (1 & ((1 << 3) - 1)) << 4;
    /* PD Polarity: 1 (positive) */
    init_array[2] |= 1 << 7;
    /* CP three-state: 0 (normal) */
    /* Fastlock Mode: 00 (disabled) */
    /* Timer Counter Control: 0000 (3 PFD cycles) */
    /* Current Setting 1: 111 (5 mA) */
    init_array[2] |= 0x7 << 15;
    /* Current Setting 2: 111 (5 mA) */
    init_array[2] |= 0x7 << 18;

    /* Write the values to the chip */
    for (i = 0; i < ARRAY_SIZE(init_array); ++i) {
        log_debug("reg %x gets 0x%08x\n", i, init_array[i]);
        status = bladerf_set_pll_register(dev, i, init_array[i]);
        if (status < 0) {
            RETURN_ERROR_STATUS("bladerf_set_pll_register", status);
        }
    }

    /* Re-disable the chip if applicable */
    if (!is_enabled) {
        bladerf_set_pll_enable(dev, false);
        if (status < 0) {
            RETURN_ERROR_STATUS("bladerf_set_pll_enable", status);
        }
    }

    return 0;
}

static int bladerf_pll_calculate_ratio(bladerf_frequency ref_freq,
                                       bladerf_frequency clock_freq,
                                       uint16_t *R,
                                       uint16_t *N)
{
    size_t const Rmax = 16383;
    double const tol  = 0.00001;
    double target     = (double)clock_freq / (double)ref_freq;
    uint16_t R_try, N_try;

    struct bladerf_range const clock_frequency_range = {
        FIELD_INIT(.min, 5000000),
        FIELD_INIT(.max, 400000000),
        FIELD_INIT(.step, 1),
        FIELD_INIT(.scale, 1),
    };

    if (NULL == R) {
        RETURN_INVAL("R", "is null");
    }

    if (NULL == N) {
        RETURN_INVAL("N", "is null");
    }

    if (!_is_within_range(&bladerf2_pll_refclk_range, ref_freq)) {
        return BLADERF_ERR_RANGE;
    }

    if (!_is_within_range(&clock_frequency_range, clock_freq)) {
        return BLADERF_ERR_RANGE;
    }

    for (R_try = 1; R_try < Rmax; ++R_try) {
        N_try = (uint16_t)(target * R_try + 0.5);

        if (N_try > 8191) {
            continue;
        }

        double ratio = (double)N_try / (double)R_try;
        double delta = (ratio > target) ? (ratio - target) : (target - ratio);

        if (delta < tol) {
            *R = R_try;
            *N = N_try;

            return 0;
        }
    }

    RETURN_INVAL("requested ratio", "not achievable");
}

int bladerf_get_pll_lock_state(struct bladerf *dev, bool *locked)
{
    int status;
    uint32_t reg;

    if (NULL == dev) {
        RETURN_INVAL("dev", "not initialized");
    }

    if (dev->board != &bladerf2_board_fns) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    if (NULL == locked) {
        RETURN_INVAL("locked", "is null");
    }

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    /* Read RFFE control register */
    status = dev->backend->rffe_control_read(dev, &reg);
    if (status < 0) {
        RETURN_ERROR_STATUS("rffe_control_read", status);
    }

    *locked = (reg >> RFFE_CONTROL_ADF_MUXOUT) & 0x1;

    return 0;
}

int bladerf_get_pll_enable(struct bladerf *dev, bool *enabled)
{
    int status;
    uint32_t data;

    if (NULL == dev) {
        RETURN_INVAL("dev", "not initialized");
    }

    if (dev->board != &bladerf2_board_fns) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    if (NULL == enabled) {
        RETURN_INVAL("enabled", "is null");
    }

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_FPGA_LOADED);

    status = dev->backend->config_gpio_read(dev, &data);
    if (status < 0) {
        MUTEX_UNLOCK(&dev->lock);
        RETURN_ERROR_STATUS("config_gpio_read", status);
    }

    *enabled = (data >> CFG_GPIO_PLL_EN) & 0x01;

    MUTEX_UNLOCK(&dev->lock);

    return 0;
}

int bladerf_set_pll_enable(struct bladerf *dev, bool enable)
{
    int status;
    uint32_t data;
    struct bladerf2_board_data *board_data;

    if (NULL == dev) {
        RETURN_INVAL("dev", "not initialized");
    }

    if (dev->board != &bladerf2_board_fns) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    board_data = dev->board_data;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_FPGA_LOADED);

    // Disable the trim DAC when we're using the PLL
    if (enable) {
        status = bladerf2_set_trim_dac_enable(dev, false);
        if (status < 0) {
            RETURN_ERROR_STATUS("bladerf2_set_trim_dac_enable", status);
        }
    }

    // Read current config GPIO value
    status = dev->backend->config_gpio_read(dev, &data);
    if (status < 0) {
        MUTEX_UNLOCK(&dev->lock);
        RETURN_ERROR_STATUS("config_gpio_read", status);
    }

    // Set the PLL enable bit accordingly
    data &= ~(1 << CFG_GPIO_PLL_EN);
    data |= ((enable ? 1 : 0) << CFG_GPIO_PLL_EN);

    // Write back the config GPIO
    status = dev->backend->config_gpio_write(dev, data);
    if (status < 0) {
        MUTEX_UNLOCK(&dev->lock);
        RETURN_ERROR_STATUS("config_gpio_write", status);
    }

    // Update our state flag
    board_data->trim_source = enable ? TRIM_SOURCE_PLL : TRIM_SOURCE_NONE;

    // Enable the trim DAC if we're done with the PLL
    if (!enable) {
        status = bladerf2_set_trim_dac_enable(dev, true);
        if (status < 0) {
            RETURN_ERROR_STATUS("bladerf2_set_trim_dac_enable", status);
        }
    }

    MUTEX_UNLOCK(&dev->lock);

    return 0;
}

int bladerf_get_pll_refclk_range(struct bladerf *dev,
                                 const struct bladerf_range **range)
{
    if (NULL == range) {
        RETURN_INVAL("range", "is null");
    }

    *range = &bladerf2_pll_refclk_range;

    return 0;
}

int bladerf_get_pll_refclk(struct bladerf *dev, bladerf_frequency *frequency)
{
    int status;
    uint32_t reg;
    uint16_t R, N;

    uint8_t const R_LATCH_REG   = 0;
    size_t const R_LATCH_SHIFT  = 2;
    uint32_t const R_LATCH_MASK = 0x3fff;

    uint8_t const N_LATCH_REG   = 1;
    size_t const N_LATCH_SHIFT  = 8;
    uint32_t const N_LATCH_MASK = 0x1fff;

    if (NULL == dev) {
        RETURN_INVAL("dev", "not initialized");
    }

    if (dev->board != &bladerf2_board_fns) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    // Get the current R value (latch 0, bits 2-15)
    status = bladerf_get_pll_register(dev, R_LATCH_REG, &reg);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf_get_pll_register", status);
    }
    R = (reg >> R_LATCH_SHIFT) & R_LATCH_MASK;

    // Get the current N value (latch 1, bits 8-20)
    status = bladerf_get_pll_register(dev, N_LATCH_REG, &reg);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf_get_pll_register", status);
    }
    N = (reg >> N_LATCH_SHIFT) & N_LATCH_MASK;

    // We assume the system clock frequency is BLADERF_VCTCXO_FREQUENCY.
    // If it isn't, do your own math
    *frequency = R * BLADERF_VCTCXO_FREQUENCY / N;

    return 0;
}

int bladerf_set_pll_refclk(struct bladerf *dev, bladerf_frequency frequency)
{
    int status;
    uint16_t R, N;

    if (NULL == dev) {
        RETURN_INVAL("dev", "not initialized");
    }

    if (dev->board != &bladerf2_board_fns) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    // We assume the system clock frequency is BLADERF_VCTCXO_FREQUENCY.
    // If it isn't, do your own math
    status = bladerf_pll_calculate_ratio(frequency, BLADERF_VCTCXO_FREQUENCY,
                                         &R, &N);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf_pll_calculate_ratio", status);
    }

    status = bladerf_pll_configure(dev, R, N);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf_pll_configure", status);
    }

    return 0;
}

int bladerf_get_pll_register(struct bladerf *dev,
                             uint8_t address,
                             uint32_t *val)
{
    int status;
    uint32_t data;

    if (NULL == dev) {
        RETURN_INVAL("dev", "not initialized");
    }

    if (dev->board != &bladerf2_board_fns) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    if (NULL == val) {
        RETURN_INVAL("val", "is null");
    }

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_FPGA_LOADED);

    address &= 0x03;

    status = dev->backend->adf400x_read(dev, address, &data);
    if (status < 0) {
        MUTEX_UNLOCK(&dev->lock);
        RETURN_ERROR_STATUS("adf400x_read", status);
    }

    *val = data;

    MUTEX_UNLOCK(&dev->lock);

    return 0;
}

int bladerf_set_pll_register(struct bladerf *dev, uint8_t address, uint32_t val)
{
    int status;
    uint32_t data;

    if (NULL == dev) {
        RETURN_INVAL("dev", "not initialized");
    }

    if (dev->board != &bladerf2_board_fns) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_FPGA_LOADED);

    address &= 0x03;

    data = val;

    status = dev->backend->adf400x_write(dev, address, data);
    if (status < 0) {
        MUTEX_UNLOCK(&dev->lock);
        RETURN_ERROR_STATUS("adf400x_write", status);
    }

    MUTEX_UNLOCK(&dev->lock);

    return 0;
}

/******************************************************************************/
/* Low level Power Source Accessors */
/******************************************************************************/

int bladerf_get_power_source(struct bladerf *dev, bladerf_power_sources *src)
{
    int status;
    uint32_t data;

    if (NULL == dev) {
        RETURN_INVAL("dev", "not initialized");
    }

    if (dev->board != &bladerf2_board_fns) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    if (NULL == src) {
        RETURN_INVAL("src", "is null");
    }

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_FPGA_LOADED);

    status = dev->backend->config_gpio_read(dev, &data);
    if (status < 0) {
        MUTEX_UNLOCK(&dev->lock);
        RETURN_ERROR_STATUS("config_gpio_read", status);
    }

    if ((data >> CFG_GPIO_POWERSOURCE) & 0x01) {
        *src = BLADERF_PS_USB_VBUS;
    } else {
        *src = BLADERF_PS_DC;
    }

    MUTEX_UNLOCK(&dev->lock);

    return 0;
}

/******************************************************************************/
/* Low level clock source selection accessors */
/******************************************************************************/

int bladerf_get_clock_select(struct bladerf *dev, bladerf_clock_select *sel)
{
    int status;
    uint32_t gpio;

    if (NULL == dev) {
        RETURN_INVAL("dev", "not initialized");
    }

    if (dev->board != &bladerf2_board_fns) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    if (NULL == sel) {
        RETURN_INVAL("sel", "is null");
    }

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_FPGA_LOADED);

    status = dev->backend->config_gpio_read(dev, &gpio);
    if (status < 0) {
        MUTEX_UNLOCK(&dev->lock);
        RETURN_ERROR_STATUS("config_gpio_read", status);
    }

    if ((gpio & (1 << CFG_GPIO_CLOCK_SELECT)) == 0x0) {
        *sel = CLOCK_SELECT_ONBOARD;
    } else {
        *sel = CLOCK_SELECT_EXTERNAL;
    }

    MUTEX_UNLOCK(&dev->lock);

    return 0;
}

int bladerf_set_clock_select(struct bladerf *dev, bladerf_clock_select sel)
{
    int status;
    uint32_t gpio;

    if (NULL == dev) {
        RETURN_INVAL("dev", "not initialized");
    }

    if (dev->board != &bladerf2_board_fns) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_FPGA_LOADED);

    status = dev->backend->config_gpio_read(dev, &gpio);
    if (status < 0) {
        MUTEX_UNLOCK(&dev->lock);
        RETURN_ERROR_STATUS("config_gpio_read", status);
    }

    // Set the clock select bit(s) accordingly
    switch (sel) {
        case CLOCK_SELECT_ONBOARD:
            gpio &= ~(1 << CFG_GPIO_CLOCK_SELECT);
            break;
        case CLOCK_SELECT_EXTERNAL:
            gpio |= (1 << CFG_GPIO_CLOCK_SELECT);
            break;
        default:
            break;
    }

    // Write back the config GPIO
    status = dev->backend->config_gpio_write(dev, gpio);
    if (status < 0) {
        MUTEX_UNLOCK(&dev->lock);
        RETURN_ERROR_STATUS("config_gpio_write", status);
    }

    MUTEX_UNLOCK(&dev->lock);

    return 0;
}

/******************************************************************************/
/* Low level clock buffer output accessors */
/******************************************************************************/

int bladerf_get_clock_output(struct bladerf *dev, bool *state)
{
    int status;
    uint32_t gpio;

    if (NULL == dev) {
        RETURN_INVAL("dev", "not initialized");
    }

    if (dev->board != &bladerf2_board_fns) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    if (NULL == state) {
        RETURN_INVAL("state", "is null");
    }

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_FPGA_LOADED);

    status = dev->backend->config_gpio_read(dev, &gpio);
    if (status < 0) {
        MUTEX_UNLOCK(&dev->lock);
        RETURN_ERROR_STATUS("config_gpio_read", status);
    }

    *state = ((gpio & (1 << CFG_GPIO_CLOCK_OUTPUT)) != 0x0);

    MUTEX_UNLOCK(&dev->lock);

    return 0;
}

int bladerf_set_clock_output(struct bladerf *dev, bool enable)
{
    int status;
    uint32_t gpio;

    if (NULL == dev) {
        RETURN_INVAL("dev", "not initialized");
    }

    if (dev->board != &bladerf2_board_fns) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_FPGA_LOADED);

    status = dev->backend->config_gpio_read(dev, &gpio);
    if (status < 0) {
        MUTEX_UNLOCK(&dev->lock);
        RETURN_ERROR_STATUS("config_gpio_read", status);
    }

    // Set or clear the clock output enable bit
    gpio &= ~(1 << CFG_GPIO_CLOCK_OUTPUT);
    gpio |= ((enable ? 1 : 0) << CFG_GPIO_CLOCK_OUTPUT);

    // Write back the config GPIO
    status = dev->backend->config_gpio_write(dev, gpio);
    if (status < 0) {
        MUTEX_UNLOCK(&dev->lock);
        RETURN_ERROR_STATUS("config_gpio_write", status);
    }

    MUTEX_UNLOCK(&dev->lock);

    return 0;
}

/******************************************************************************/
/* Low level INA219 Accessors */
/******************************************************************************/

int bladerf_get_pmic_register(struct bladerf *dev,
                              bladerf_pmic_register reg,
                              void *val)
{
    int status = 0;

    if (NULL == dev) {
        RETURN_INVAL("dev", "not initialized");
    }

    if (dev->board != &bladerf2_board_fns) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_FPGA_LOADED);

    switch (reg) {
        case BLADERF_PMIC_CONFIGURATION:
        case BLADERF_PMIC_CALIBRATION:
            return BLADERF_ERR_UNSUPPORTED;
        case BLADERF_PMIC_VOLTAGE_SHUNT:
            status = ina219_read_shunt_voltage(dev, (float *)val);
            break;
        case BLADERF_PMIC_VOLTAGE_BUS:
            status = ina219_read_bus_voltage(dev, (float *)val);
            break;
        case BLADERF_PMIC_POWER:
            status = ina219_read_power(dev, (float *)val);
            break;
        case BLADERF_PMIC_CURRENT:
            status = ina219_read_current(dev, (float *)val);
            break;
    }

    if (status < 0) {
        MUTEX_UNLOCK(&dev->lock);
        RETURN_ERROR_STATUS("ina219_read", status);
    }

    MUTEX_UNLOCK(&dev->lock);

    return 0;
}

/******************************************************************************/
/* Low level RF Switch Accessors */
/******************************************************************************/

int bladerf_get_rf_switch_config(struct bladerf *dev,
                                 bladerf_rf_switch_config *config)
{
    struct bladerf2_board_data *board_data;
    struct ad9361_rf_phy *phy;
    uint32_t val, reg;
    int status;

    if (NULL == dev) {
        RETURN_INVAL("dev", "not initialized");
    }

    if (dev->board != &bladerf2_board_fns) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    if (NULL == config) {
        RETURN_INVAL("config", "is null");
    }

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_FPGA_LOADED);

    board_data = dev->board_data;
    phy        = board_data->phy;

    /* Get AD9361 status */
    status = ad9361_get_tx_rf_port_output(phy, &val);
    if (status < 0) {
        MUTEX_UNLOCK(&dev->lock);
        RETURN_ERROR_STATUS("ad9361_get_tx_rf_port_output", status);
    }

    config->tx1_rfic_port = val;
    config->tx2_rfic_port = val;

    status = ad9361_get_rx_rf_port_input(phy, &val);
    if (status < 0) {
        MUTEX_UNLOCK(&dev->lock);
        RETURN_ERROR_STATUS("ad9361_get_rx_rf_port_input", status);
    }

    config->rx1_rfic_port = val;
    config->rx2_rfic_port = val;

    /* Read RFFE control register */
    status = dev->backend->rffe_control_read(dev, &reg);
    if (status < 0) {
        MUTEX_UNLOCK(&dev->lock);
        RETURN_ERROR_STATUS("rffe_control_read", status);
    }

    config->rx1_spdt_port =
        (reg >> RFFE_CONTROL_RX_SPDT_1) & RFFE_CONTROL_SPDT_MASK;
    config->rx2_spdt_port =
        (reg >> RFFE_CONTROL_RX_SPDT_2) & RFFE_CONTROL_SPDT_MASK;
    config->tx1_spdt_port =
        (reg >> RFFE_CONTROL_TX_SPDT_1) & RFFE_CONTROL_SPDT_MASK;
    config->tx2_spdt_port =
        (reg >> RFFE_CONTROL_TX_SPDT_2) & RFFE_CONTROL_SPDT_MASK;

    MUTEX_UNLOCK(&dev->lock);

    return 0;
}
