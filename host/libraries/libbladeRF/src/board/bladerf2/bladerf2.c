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

#include "../bladerf1/flash.h"
#include "board/board.h"
#include "capabilities.h"
#include "compatibility.h"

#include "driver/fpga_trigger.h"
#include "driver/fx3_fw.h"
#include "driver/ina219.h"
#include "driver/spi_flash.h"
#include "driver/thirdparty/adi/ad9361_api.h"

#include "backend/backend_config.h"
#include "backend/usb/usb.h"

#include "streaming/async.h"
#include "streaming/sync.h"

#include "devinfo.h"
#include "helpers/file.h"
#include "helpers/version.h"
#include "version.h"


/******************************************************************************
 *                          bladeRF2 board state                              *
 ******************************************************************************/

enum bladerf2_vctcxo_trim_source {
    TRIM_SOURCE_NONE,
    TRIM_SOURCE_TRIM_DAC,
    TRIM_SOURCE_ADF4002,
    TRIM_SOURCE_AD9361_AUXDAC
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
    uint16_t trimdac_value;
};

/* Macro for logging and returning an error status. This should be used for
 * errors defined in the \ref RETCODES list. */
#define RETURN_ERROR_STATUS_ARG(_what, _arg, _status)                  \
    {                                                                  \
        log_error("%s: %s %s failed: %s\n", __FUNCTION__, _what, _arg, \
                  bladerf_strerror(_status));                          \
        return _status;                                                \
    }

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


/******************************************************************************/
/* Constants */
/******************************************************************************/

enum bladerf2_band { BAND_SHUTDOWN, BAND_LOW, BAND_HIGH };

struct bladerf_ad9361_gain_mode_map {
    bladerf_gain_mode brf_mode;
    enum rf_gain_ctrl_mode ad9361_mode;
};

struct bladerf_gain_range {
    struct bladerf_range frequency;
    struct bladerf_range gain;
};

struct bladerf_gain_stage_info {
    const char *name;
    struct bladerf_range range;
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

static const uint64_t BLADERF_VCTCXO_FREQUENCY = 38.4e6;
static const uint64_t BLADERF_REFIN_DEFAULT    = 10.0e6;

// clang-format off
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
#define RFFE_CONTROL_SPDT_MASK      0x3
#define RFFE_CONTROL_SPDT_SHUTDOWN  0x0  // no connection
#define RFFE_CONTROL_SPDT_LOWBAND   0x2  // RF1 <-> RF3
#define RFFE_CONTROL_SPDT_HIGHBAND  0x1  // RF1 <-> RF2

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

/* Overall RX gain range */
/* Reference: ad9361.c, ad9361_gt_tableindex and ad9361_init_gain_tables */
static const struct bladerf_gain_range bladerf2_rx_gain_ranges[] = {
    {
        FIELD_INIT(.frequency, {
            FIELD_INIT(.min,    0),
            FIELD_INIT(.max,    1300e6),
            FIELD_INIT(.step,   1),
            FIELD_INIT(.scale,  1),
        }),
        FIELD_INIT(.gain, {
            FIELD_INIT(.min,    1),
            FIELD_INIT(.max,    77),
            FIELD_INIT(.step,   1),
            FIELD_INIT(.scale,  1),
        })
    },
    {
        FIELD_INIT(.frequency, {
            FIELD_INIT(.min,    1300e6),
            FIELD_INIT(.max,    4000e6),
            FIELD_INIT(.step,   1),
            FIELD_INIT(.scale,  1),
        }),
        FIELD_INIT(.gain, {
            FIELD_INIT(.min,    -4),
            FIELD_INIT(.max,    71),
            FIELD_INIT(.step,   1),
            FIELD_INIT(.scale,  1),
        })
    },
    {
        FIELD_INIT(.frequency, {
            FIELD_INIT(.min,    4000e6),
            FIELD_INIT(.max,    6000e6),
            FIELD_INIT(.step,   1),
            FIELD_INIT(.scale,  1),
        }),
        FIELD_INIT(.gain, {
            FIELD_INIT(.min,    -10),
            FIELD_INIT(.max,    62),
            FIELD_INIT(.step,   1),
            FIELD_INIT(.scale,  1),
        })
    }
};

/* Overall TX gain range */
static const struct bladerf_range bladerf2_tx_gain_range = {
    FIELD_INIT(.min,    -89750),
    FIELD_INIT(.max,    0),
    FIELD_INIT(.step,   250),
    FIELD_INIT(.scale,  0.001),
};

/* RX gain modes */
static const struct bladerf_gain_modes bladerf2_rx_gain_modes[] = {
    {
        FIELD_INIT(.name, "default"),
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

/* RX gain stages */
static const struct bladerf_gain_stage_info bladerf2_rx_gain_stages[] = {
    {
        FIELD_INIT(.name, "full"),
        FIELD_INIT(.range, {
            FIELD_INIT(.min,    -10),
            FIELD_INIT(.max,    77),
            FIELD_INIT(.step,   1),
            FIELD_INIT(.scale,  1),
        }),
    },
    {
        FIELD_INIT(.name, "digital"),
        FIELD_INIT(.range, {
            FIELD_INIT(.min,    0),
            FIELD_INIT(.max,    31),
            FIELD_INIT(.step,   1),
            FIELD_INIT(.scale,  1),
        }),
    },
};

/* TX gain stages */
static const struct bladerf_gain_stage_info bladerf2_tx_gain_stages[] = {
    {
        FIELD_INIT(.name, "dsa"),
        FIELD_INIT(.range, {
            FIELD_INIT(.min,    -89750),
            FIELD_INIT(.max,    0),
            FIELD_INIT(.step,   250),
            FIELD_INIT(.scale,  0.001),
        }),
    },
};

/* Sample Rate Range */
static const struct bladerf_range bladerf2_sample_rate_range = {
    FIELD_INIT(.min,    2083334),
    FIELD_INIT(.max,    61440000),
    FIELD_INIT(.step,   1),
    FIELD_INIT(.scale,  1),
};

/* Bandwidth Range */
static const struct bladerf_range bladerf2_bandwidth_range = {
    FIELD_INIT(.min,    200e3),
    FIELD_INIT(.max,    56e6),
    FIELD_INIT(.step,   1),
    FIELD_INIT(.scale,  1),
};

/* Frequency Ranges */
static const struct bladerf_range bladerf2_rx_frequency_range = {
    FIELD_INIT(.min,    70e6),
    FIELD_INIT(.max,    6000e6),
    FIELD_INIT(.step,   2),
    FIELD_INIT(.scale,  1),
};

static const struct bladerf_range bladerf2_tx_frequency_range = {
    FIELD_INIT(.min,    70e6),
    FIELD_INIT(.max,    6000e6),
    FIELD_INIT(.step,   2),
    FIELD_INIT(.scale,  1),
};

static const struct bladerf_range bladerf2_adf4002_refclk_range = {
    FIELD_INIT(.min, 5e6),
    FIELD_INIT(.max, 300e6),
    FIELD_INIT(.step, 1),
    FIELD_INIT(.scale, 1),
};

/* RF Ports */
static const struct bladerf_ad9361_port_name_map bladerf2_rx_port_map[] = {
    {   FIELD_INIT(.name, "A_BALANCED"),    FIELD_INIT(.id, A_BALANCED),    },
    {   FIELD_INIT(.name, "B_BALANCED"),    FIELD_INIT(.id, B_BALANCED),    },
    {   FIELD_INIT(.name, "C_BALANCED"),    FIELD_INIT(.id, C_BALANCED),    },
    {   FIELD_INIT(.name, "A_N"),           FIELD_INIT(.id, A_N),           },
    {   FIELD_INIT(.name, "A_P"),           FIELD_INIT(.id, A_P),           },
    {   FIELD_INIT(.name, "B_N"),           FIELD_INIT(.id, B_N),           },
    {   FIELD_INIT(.name, "B_P"),           FIELD_INIT(.id, B_P),           },
    {   FIELD_INIT(.name, "C_N"),           FIELD_INIT(.id, C_N),           },
    {   FIELD_INIT(.name, "C_P"),           FIELD_INIT(.id, C_P),           },
    {   FIELD_INIT(.name, "TX_MON1"),       FIELD_INIT(.id, TX_MON1),       },
    {   FIELD_INIT(.name, "TX_MON2"),       FIELD_INIT(.id, TX_MON2),       },
    {   FIELD_INIT(.name, "TX_MON1_2"),     FIELD_INIT(.id, TX_MON1_2),     },
};

static const struct bladerf_ad9361_port_name_map bladerf2_tx_port_map[] = {
    {   FIELD_INIT(.name, "TXA"),           FIELD_INIT(.id, TXA),           },
    {   FIELD_INIT(.name, "TXB"),           FIELD_INIT(.id, TXB),           },
};

static const struct range_band_map bladerf2_rx_range_band_map[] = {
    {
        FIELD_INIT(.band, BAND_LOW),
        FIELD_INIT(.range, {
            FIELD_INIT(.min,    70e6),
            FIELD_INIT(.max,    3000e6),
            FIELD_INIT(.step,   2),
            FIELD_INIT(.scale,  1)
        }),
    },
    {
        FIELD_INIT(.band, BAND_HIGH),
        FIELD_INIT(.range, {
            FIELD_INIT(.min,    3000e6),
            FIELD_INIT(.max,    6000e6),
            FIELD_INIT(.step,   2),
            FIELD_INIT(.scale,  1)
        }),
    },
};

static const struct range_band_map bladerf2_tx_range_band_map[] = {
    {
        FIELD_INIT(.band, BAND_LOW),
        FIELD_INIT(.range, {
            FIELD_INIT(.min,    46.875e6),
            FIELD_INIT(.max,    3000e6),
            FIELD_INIT(.step,   2),
            FIELD_INIT(.scale,  1)
        }),
    },
    {
        FIELD_INIT(.band, BAND_HIGH),
        FIELD_INIT(.range, {
            FIELD_INIT(.min,    3000e6),
            FIELD_INIT(.max,    6000e6),
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
        FIELD_INIT(.ad9361_port,    B_BALANCED),
    },
    {
        FIELD_INIT(.band,           BAND_HIGH),
        FIELD_INIT(.spdt,           RFFE_CONTROL_SPDT_HIGHBAND),
        FIELD_INIT(.ad9361_port,    A_BALANCED),
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
        FIELD_INIT(.ad9361_port,    TXB),
    },
    {
        FIELD_INIT(.band,           BAND_HIGH),
        FIELD_INIT(.spdt,           RFFE_CONTROL_SPDT_HIGHBAND),
        FIELD_INIT(.ad9361_port,    TXA),
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
        FIELD_INIT(.mode, BLADERF_LB_AD9361_BIST)
    },
};

// clang-format on

/******************************************************************************/
/* Forward declarations */
/******************************************************************************/

static int bladerf2_select_band(struct bladerf *dev,
                                bladerf_channel ch,
                                uint64_t frequency);
static int bladerf2_get_frequency(struct bladerf *dev,
                                  bladerf_channel ch,
                                  uint64_t *frequency);
static int bladerf2_get_vctcxo_trim(struct bladerf *dev, uint16_t *trim);


/******************************************************************************/
/* Externs */
/******************************************************************************/

extern AD9361_InitParam ad9361_init_params;
extern AD9361_RXFIRConfig ad9361_init_rx_fir_config;
extern AD9361_TXFIRConfig ad9361_init_tx_fir_config;
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

    return ((value / range->scale) >= range->min &&
            (value / range->scale) <= range->max);
}

static int64_t _clamp_to_range(struct bladerf_range const *range, int64_t value)
{
    if (NULL == range) {
        log_error("%s: range is null\n", __FUNCTION__);
        return value;
    }

    if ((value / range->scale) < range->min) {
        log_warning("%s: %" PRIi64 " below range [%" PRIi64 ",%" PRIi64 "]\n",
                    __FUNCTION__, value, range->min, range->max);
        value = range->min * range->scale;
    }

    if ((value / range->scale) > range->max) {
        log_warning("%s: %" PRIi64 " above range [%" PRIi64 ",%" PRIi64 "]\n",
                    __FUNCTION__, value, range->min, range->max);
        value = range->max * range->scale;
    }

    return value;
}

static enum bladerf2_band _get_band_by_frequency(bladerf_channel ch,
                                                 uint64_t frequency)
{
    const struct range_band_map *band_map;
    size_t band_map_len;
    int64_t freqi = (int64_t)frequency;

    /* Select RX vs TX */
    if (BLADERF_CHANNEL_IS_TX(ch)) {
        band_map     = bladerf2_tx_range_band_map;
        band_map_len = ARRAY_SIZE(bladerf2_tx_range_band_map);
    } else {
        band_map     = bladerf2_rx_range_band_map;
        band_map_len = ARRAY_SIZE(bladerf2_rx_range_band_map);
    }

    /* Determine the band for the given frequency */
    for (size_t i = 0; i < band_map_len; ++i) {
        if (_is_within_range(&band_map[i].range, freqi)) {
            return band_map[i].band;
        }
    }

    /* Not a valid frequency */
    log_warning("%s: frequency %" PRIu64 " not found in band map\n",
                __FUNCTION__, frequency);
    return BAND_SHUTDOWN;
}

static const struct band_port_map *_get_band_port_map(bladerf_channel ch,
                                                      bool enabled,
                                                      uint64_t frequency)
{
    enum bladerf2_band band;
    const struct band_port_map *port_map;
    size_t port_map_len;

    /* Determine which band to use */
    band = enabled ? _get_band_by_frequency(ch, frequency) : BAND_SHUTDOWN;

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
    for (size_t i = 0; i < port_map_len; i++) {
        if (port_map[i].band == band) {
            return &port_map[i];
        }
    }

    /* Wasn't found, return a null ptr */
    log_warning("%s: frequency %" PRIu64 " not found in port map\n",
                __FUNCTION__, frequency);
    return NULL;
}

static int _set_spdt_bits(uint32_t *reg,
                          bladerf_channel ch,
                          bool enabled,
                          uint64_t frequency)
{
    const struct band_port_map *port_map;
    uint32_t shift;

    if (NULL == reg) {
        RETURN_INVAL("reg", "is null");
    }

    /* Look up the port configuration for this frequency */
    port_map = _get_band_port_map(ch, enabled, frequency);

    if (NULL == port_map) {
        RETURN_INVAL("_get_band_port_map", "returned null");
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

static int _set_ad9361_port(struct bladerf *dev,
                            bladerf_channel ch,
                            uint64_t frequency)
{
    const struct band_port_map *port_map;
    struct bladerf2_board_data *board_data;
    struct ad9361_rf_phy *phy;
    int status;

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    board_data = dev->board_data;
    phy        = board_data->phy;

    /* Look up the port configuration for this frequency */
    port_map = _get_band_port_map(ch, true, frequency);

    if (NULL == port_map) {
        RETURN_INVAL("_get_band_port_map", "returned null");
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

static bool _is_rffe_channel_enabled(uint32_t reg, bladerf_channel ch)
{
    /* Given a register read from the RFFE, determine if ch is enabled */
    switch (ch) {
        case BLADERF_CHANNEL_RX(0):
            return ((reg >> RFFE_CONTROL_ENABLE) & 0x1) &&
                   ((reg >> RFFE_CONTROL_MIMO_RX_EN_0) & 0x1);
        case BLADERF_CHANNEL_RX(1):
            return ((reg >> RFFE_CONTROL_ENABLE) & 0x1) &&
                   ((reg >> RFFE_CONTROL_MIMO_RX_EN_1) & 0x1);
        case BLADERF_CHANNEL_TX(0):
            return ((reg >> RFFE_CONTROL_TXNRX) & 0x1) &&
                   ((reg >> RFFE_CONTROL_MIMO_TX_EN_0) & 0x1);
        case BLADERF_CHANNEL_TX(1):
            return ((reg >> RFFE_CONTROL_TXNRX) & 0x1) &&
                   ((reg >> RFFE_CONTROL_MIMO_TX_EN_1) & 0x1);
        default:
            log_error("%s: unknown channel index %d\n", __FUNCTION__, ch);
            return false;
    }
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

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    board_data = dev->board_data;
    phy        = board_data->phy;
    port       = (ch >> 1) + 1;
    cached     = _get_tx_gain_cache(dev, ch);

    if (board_data->tx_mute[ch >> 1] == state) {
        log_warning("attempted to mute already-muted channel %d\n", ch);
        return 0;
    }

    if (state) {
        cached = ad9361_get_tx_atten(phy, port);
        atten  = 89750;
    } else {
        atten = cached;
    }

    status = _set_tx_gain_cache(dev, ch, cached);
    if (status != 0) {
        RETURN_ERROR_STATUS("failed to update tx gain cache", status);
    }

    log_debug("%s: %smuting TX%d (cached: %d)\n", __FUNCTION__,
              state ? "" : "un", port, cached);
    status = ad9361_set_tx_atten(phy, atten, (port == 1), (port == 2), true);
    if (status != 0) {
        RETURN_ERROR_AD9361("failed to set tx atten", status);
    }

    board_data->tx_mute[ch >> 1] = state;

    return 0;
}

static bool _is_total_sample_rate_achievable(struct bladerf *dev)
{
    int status;
    uint32_t reg, rx_rate, tx_rate;
    float throughput_accum                 = 0;
    size_t active_channels                 = 0;
    float const MAX_SAMPLE_THROUGHPUT      = 80e6;
    struct bladerf2_board_data *board_data = dev->board_data;

    /* Read RFFE control register */
    status = dev->backend->rffe_control_read(dev, &reg);
    if (status < 0) {
        return false;
    }

    /* Get RX and TX sample rates */
    status = ad9361_get_rx_sampling_freq(board_data->phy, &rx_rate);
    if (status < 0) {
        return false;
    }
    status = ad9361_get_tx_sampling_freq(board_data->phy, &tx_rate);
    if (status < 0) {
        return false;
    }

    /* Accumulate sample rates for all channels */
    if ((reg >> RFFE_CONTROL_MIMO_RX_EN_0) & 0x1) {
        throughput_accum += rx_rate;
        ++active_channels;
    }

    if ((reg >> RFFE_CONTROL_MIMO_RX_EN_1) & 0x1) {
        throughput_accum += rx_rate;
        ++active_channels;
    }

    if ((reg >> RFFE_CONTROL_MIMO_TX_EN_0) & 0x1) {
        throughput_accum += tx_rate;
        ++active_channels;
    }

    if ((reg >> RFFE_CONTROL_MIMO_TX_EN_1) & 0x1) {
        throughput_accum += tx_rate;
        ++active_channels;
    }

    if (throughput_accum > MAX_SAMPLE_THROUGHPUT) {
        log_warning("The total sample throughput for the %d active channel%s, "
                    "%g Msps, is greater than the recommended maximum sample "
                    "throughput, %g Msps. You may experience dropped samples "
                    "unless the sample rate is reduced, or some channels are "
                    "deactivated.\n",
                    active_channels, (active_channels == 1 ? "" : "s"),
                    throughput_accum / 1e6, MAX_SAMPLE_THROUGHPUT / 1e6);
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

    /* Test for uninitialized dev struct */
    CHECK_BOARD_STATE(STATE_UNINITIALIZED);

    board_data = dev->board_data;

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
    status = ad9361_init(&board_data->phy, &ad9361_init_params, dev);
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
    status = ad9361_set_tx_lo_freq(board_data->phy, 70e6);
    if (status < 0) {
        RETURN_ERROR_AD9361("ad9361_set_tx_lo_freq", status);
    }

    status = ad9361_set_rx_lo_freq(board_data->phy, 70e6);
    if (status < 0) {
        RETURN_ERROR_AD9361("ad9361_set_rx_lo_freq", status);
    }

    /* Set up AD9361 FIR filters */
    status = ad9361_set_tx_fir_config(phy, ad9361_init_tx_fir_config);
    if (status < 0) {
        RETURN_ERROR_AD9361("ad9361_set_tx_fir_config", status);
    }

    status = ad9361_set_rx_fir_config(phy, ad9361_init_rx_fir_config);
    if (status < 0) {
        RETURN_ERROR_AD9361("ad9361_set_rx_fir_config", status);
    }

    /* Enable RX FIR filter */
    status = ad9361_set_rx_fir_en_dis(phy, 1);
    if (status < 0) {
        RETURN_ERROR_AD9361("ad9361_set_rx_fir_en_dis", status);
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
    uint16_t *trimval = &(board_data->trimdac_value);

    status = bladerf2_get_vctcxo_trim(dev, trimval);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf2_get_vctcxo_trim", status);
    }

    status = dev->backend->ad56x1_vctcxo_trim_dac_write(dev, *trimval);
    if (status < 0) {
        RETURN_ERROR_STATUS("ad56x1_vctcxo_trim_dac_write", status);
    }

    board_data->trim_source = TRIM_SOURCE_TRIM_DAC;

    /* Configure ADF4002 */
    status = bladerf_adf4002_set_refclk(dev, BLADERF_REFIN_DEFAULT);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf_adf4002_set_refclk", status);
    }

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

    /* Initialize board data */
    board_data->fpga_version.describe = board_data->fpga_version_str;
    board_data->fw_version.describe   = board_data->fw_version_str;

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
    for (size_t i = 0; i < max_retries; i++) {
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
            RETURN_ERROR_STATUS_ARG("Got unsupported device speed", usb_speed,
                                    BLADERF_ERR_UNEXPECTED);
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

    /* Get FPGA size */
    /* TODO: Actually get FPGA size from flash */
    board_data->fpga_size = BLADERF_FPGA_A4;

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
                RETURN_ERROR_STATUS_ARG("Mapping FPGA size",
                                        board_data->fpga_size,
                                        BLADERF_ERR_UNEXPECTED);
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

    return 0;
}

static void bladerf2_close(struct bladerf *dev)
{
    if (dev != NULL) {
        struct bladerf2_board_data *board_data = dev->board_data;

        if (board_data != NULL) {
            if (board_data->phy != NULL) {
                ad9361_deinit(board_data->phy);
                board_data->phy = NULL;
            }
            free(board_data);
            board_data = NULL;
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

static int bladerf2_is_fpga_configured(struct bladerf *dev)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    return dev->backend->is_fpga_configured(dev);
}

static uint64_t bladerf2_get_capabilities(struct bladerf *dev)
{
    struct bladerf2_board_data *board_data;

    CHECK_BOARD_STATE(STATE_UNINITIALIZED);

    board_data = dev->board_data;

    return board_data->capabilities;
}

static size_t bladerf2_get_channel_count(struct bladerf *dev, bool tx)
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

static int bladerf2_enable_module(struct bladerf *dev,
                                  bladerf_channel ch,
                                  bool enable)
{
    struct bladerf2_board_data *board_data;
    uint32_t reg, ch_bit, dir_bit;
    uint64_t freq = 0;
    int status;
    bool ch_changing, dir_changing, dir_enable;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;

    /* Read RFFE control register */
    status = dev->backend->rffe_control_read(dev, &reg);
    if (status < 0) {
        RETURN_ERROR_STATUS("rffe_control_read", status);
    }

    log_debug("%s: rffe_control_read %08x\n", __FUNCTION__, reg);

    /* Figure out what we need to do */
    switch (ch) {
        case BLADERF_CHANNEL_RX(0):
            dir_enable = enable || ((reg >> RFFE_CONTROL_MIMO_RX_EN_1) & 0x1);
            ch_bit     = RFFE_CONTROL_MIMO_RX_EN_0;
            dir_bit    = RFFE_CONTROL_ENABLE;
            break;
        case BLADERF_CHANNEL_RX(1):
            dir_enable = enable || ((reg >> RFFE_CONTROL_MIMO_RX_EN_0) & 0x1);
            ch_bit     = RFFE_CONTROL_MIMO_RX_EN_1;
            dir_bit    = RFFE_CONTROL_ENABLE;
            break;
        case BLADERF_CHANNEL_TX(0):
            dir_enable = enable || ((reg >> RFFE_CONTROL_MIMO_TX_EN_1) & 0x1);
            ch_bit     = RFFE_CONTROL_MIMO_TX_EN_0;
            dir_bit    = RFFE_CONTROL_TXNRX;
            break;
        case BLADERF_CHANNEL_TX(1):
            dir_enable = enable || ((reg >> RFFE_CONTROL_MIMO_TX_EN_0) & 0x1);
            ch_bit     = RFFE_CONTROL_MIMO_TX_EN_1;
            dir_bit    = RFFE_CONTROL_TXNRX;
            break;
        default:
            RETURN_ERROR_STATUS("bladerf2_enable_module", BLADERF_ERR_INVAL);
    }

    ch_changing  = ((reg >> ch_bit) & 0x1) != enable;
    dir_changing = ((reg >> dir_bit) & 0x1) != dir_enable;

    /* Get current frequency */
    status = bladerf2_get_frequency(dev, ch, &freq);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf2_get_frequency", status);
    }

    /* Set the AD9361 port accordingly */
    if (ch_changing && enable) {
        status = bladerf2_select_band(dev, ch, freq);
        if (status < 0) {
            RETURN_ERROR_STATUS("bladerf2_select_band", status);
        }
    }

    /* If this is the last channel in a direction, stop synchronous interface */
    if (dir_changing && !dir_enable) {
        sync_deinit(&board_data->sync[BLADERF_CHANNEL_IS_TX(ch) ? BLADERF_TX
                                                                : BLADERF_RX]);
    }

    /* Mute unused TX channels */
    if (ch_changing && BLADERF_CHANNEL_IS_TX(ch)) {
        _set_tx_mute(dev, ch, !enable);
    }

    /* Modify MIMO channel enable bits */
    if (enable) {
        reg |= (1 << ch_bit);
        log_debug("%s: %s%d enable\n", __FUNCTION__,
                  BLADERF_CHANNEL_IS_TX(ch) ? "TX" : "RX", (ch >> 1) + 1);
    } else {
        reg &= ~(1 << ch_bit);
        log_debug("%s: %s%d disable\n", __FUNCTION__,
                  BLADERF_CHANNEL_IS_TX(ch) ? "TX" : "RX", (ch >> 1) + 1);
    }

    /* Modify ENABLE/TXNRX bits */
    if (dir_enable) {
        reg |= (1 << dir_bit);
        log_debug("%s: %s module enable\n", __FUNCTION__,
                  BLADERF_CHANNEL_IS_TX(ch) ? "TX" : "RX");
    } else {
        reg &= ~(1 << dir_bit);
        log_debug("%s: %s module disable\n", __FUNCTION__,
                  BLADERF_CHANNEL_IS_TX(ch) ? "TX" : "RX");
    }

    /* Modify SPDT bits */
    status = _set_spdt_bits(&reg, ch, enable, freq);
    if (status < 0) {
        RETURN_ERROR_STATUS("_set_spdt_bits", status);
    }

    /* Write RFFE control register */
    log_debug("%s: rffe_control_write %08x\n", __FUNCTION__, reg);
    status = dev->backend->rffe_control_write(dev, reg);
    if (status < 0) {
        RETURN_ERROR_STATUS("rffe_control_write", status);
    }

    /* Enable module through backend */
    if (dir_changing) {
        status = dev->backend->enable_module(
            dev, BLADERF_CHANNEL_IS_TX(ch) ? BLADERF_TX : BLADERF_RX,
            dir_enable);
        if (status < 0) {
            RETURN_ERROR_STATUS("enable_module", status);
        }
    }

    /* Warn the user if this isn't achievable */
    _is_total_sample_rate_achievable(dev);

    return 0;
}


/******************************************************************************/
/* Gain */
/******************************************************************************/

static int bladerf2_get_gain_range(struct bladerf *dev,
                                   bladerf_channel ch,
                                   struct bladerf_range *range)
{
    if (NULL == range) {
        RETURN_INVAL("range", "is null");
    }

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        *range = bladerf2_tx_gain_range;
    } else {
        const struct bladerf_gain_range *ranges;
        size_t ranges_len;
        uint64_t frequency = 0;
        int status;

        ranges     = bladerf2_rx_gain_ranges;
        ranges_len = ARRAY_SIZE(bladerf2_rx_gain_ranges);

        status = bladerf2_get_frequency(dev, ch, &frequency);
        if (status < 0) {
            RETURN_ERROR_STATUS("bladerf2_get_frequency", status);
        }

        for (size_t i = 0; i < ranges_len; i++) {
            if (_is_within_range(&ranges[i].frequency, frequency)) {
                *range = ranges[i].gain;
                return 0;
            }
        }

        return BLADERF_ERR_RANGE;
    }

    return 0;
}

static int bladerf2_set_gain(struct bladerf *dev, bladerf_channel ch, int gain)
{
    struct bladerf2_board_data *board_data;
    struct bladerf_range range;
    int val;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;

    status = bladerf2_get_gain_range(dev, ch, &range);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf2_get_gain_range", status);
    }

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        val = -_clamp_to_range(&range, gain) / range.scale;

        if (board_data->tx_mute[ch >> 1]) {
            status = _set_tx_gain_cache(dev, ch, val);
        } else {
            status = ad9361_set_tx_attenuation(board_data->phy, ch >> 1, val);
            if (status < 0) {
                RETURN_ERROR_AD9361("ad9361_set_tx_attenuation", status);
            }
        }
    } else {
        val = _clamp_to_range(&range, gain) / range.scale;

        status = ad9361_set_rx_rf_gain(board_data->phy, ch >> 1, val);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_set_rx_rf_gain", status);
        }
    }

    return 0;
}

static int bladerf2_get_gain(struct bladerf *dev, bladerf_channel ch, int *gain)
{
    struct bladerf2_board_data *board_data;
    struct bladerf_range range;
    uint8_t ad_ch = ch >> 1;
    int status;

    if (NULL == gain) {
        RETURN_INVAL("gain", "is null");
    }

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;

    status = bladerf2_get_gain_range(dev, ch, &range);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf2_get_gain_range", status);
    }

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        uint32_t atten;

        if (board_data->tx_mute[ad_ch]) {
            /* TX is muted, return cached value */
            atten = _get_tx_gain_cache(dev, ch);
        } else {
            /* Get actual value from hardware */
            status = ad9361_get_tx_attenuation(board_data->phy, ad_ch, &atten);
            if (status < 0) {
                RETURN_ERROR_AD9361("ad9361_get_tx_attenuation", status);
            }
        }

        /* Flip sign and scale */
        *gain = -(atten * range.scale);
    } else {
        int32_t rawgain;

        status = ad9361_get_rx_rf_gain(board_data->phy, ad_ch, &rawgain);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_get_rx_rf_gain", status);
        }

        /* Scale */
        *gain = rawgain * range.scale;
    }

    return 0;
}

static int bladerf2_set_gain_mode(struct bladerf *dev,
                                  bladerf_channel ch,
                                  bladerf_gain_mode mode)
{
    struct bladerf2_board_data *board_data;
    int status;
    uint8_t ad9361_channel;
    const struct bladerf_ad9361_gain_mode_map *mode_map;
    size_t mode_map_len;
    enum rf_gain_ctrl_mode gc_mode;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        // TODO: undefined!
        RETURN_ERROR_STATUS("bladerf2_set_gain_mode(tx)",
                            BLADERF_ERR_UNSUPPORTED);
    } else {
        mode_map     = bladerf2_rx_gain_mode_map;
        mode_map_len = ARRAY_SIZE(bladerf2_rx_gain_mode_map);
    }

    /* Channel conversion */
    if (ch == BLADERF_CHANNEL_RX(0)) {
        ad9361_channel = 0;
        gc_mode        = ad9361_init_params.gc_rx1_mode;
    } else if (ch == BLADERF_CHANNEL_RX(1)) {
        ad9361_channel = 1;
        gc_mode        = ad9361_init_params.gc_rx2_mode;
    } else {
        RETURN_ERROR_STATUS_ARG("channel", ch, BLADERF_ERR_UNSUPPORTED);
    }

    /* Mode conversion */
    if (mode != BLADERF_GAIN_DEFAULT) {
        for (size_t i = 0; i < mode_map_len; ++i) {
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
    uint8_t gc_mode;
    uint8_t channel;
    const struct bladerf_ad9361_gain_mode_map *mode_map;
    size_t mode_map_len;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;
    phy        = board_data->phy;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        // TODO: undefined!
        return BLADERF_ERR_UNSUPPORTED;
    } else {
        mode_map     = bladerf2_rx_gain_mode_map;
        mode_map_len = ARRAY_SIZE(bladerf2_rx_gain_mode_map);
    }

    /* Channel conversion */
    if (ch == BLADERF_CHANNEL_RX(0)) {
        channel = 0;
    } else if (ch == BLADERF_CHANNEL_RX(1)) {
        channel = 1;
    } else {
        RETURN_ERROR_STATUS_ARG("channel", ch, BLADERF_ERR_UNSUPPORTED);
    }

    /* Get the gain */
    status = ad9361_get_rx_gain_control_mode(phy, channel, &gc_mode);
    if (status < 0) {
        RETURN_ERROR_AD9361("ad9361_get_rx_gain_control_mode", status);
    }

    /* Mode conversion */
    if (mode != NULL) {
        *mode = BLADERF_GAIN_DEFAULT;

        for (size_t i = 0; i < mode_map_len; ++i) {
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

static int bladerf2_get_gain_stage_range(struct bladerf *dev,
                                         bladerf_channel ch,
                                         const char *stage,
                                         struct bladerf_range *range)
{
    const struct bladerf_gain_stage_info *stage_infos;
    unsigned int stage_infos_len;

    if (NULL == stage) {
        RETURN_INVAL("stage", "is null");
    }

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        stage_infos     = bladerf2_tx_gain_stages;
        stage_infos_len = ARRAY_SIZE(bladerf2_tx_gain_stages);
    } else {
        stage_infos     = bladerf2_rx_gain_stages;
        stage_infos_len = ARRAY_SIZE(bladerf2_rx_gain_stages);
    }

    for (size_t i = 0; i < stage_infos_len; i++) {
        if (strcmp(stage_infos[i].name, stage) == 0) {
            if (NULL != range) {
                *range = stage_infos[i].range;
            }
            return 0;
        }
    }

    return BLADERF_ERR_INVAL;
}

static int bladerf2_set_gain_stage(struct bladerf *dev,
                                   bladerf_channel ch,
                                   const char *stage,
                                   int gain)
{
    if (NULL == stage) {
        RETURN_INVAL("stage", "is null");
    }

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        if (strcmp(stage, "dsa") == 0) {
            return dev->board->set_gain(dev, ch, gain);
        }
    } else {
        if (strcmp(stage, "full") == 0) {
            return dev->board->set_gain(dev, ch, gain);
        } else if (strcmp(stage, "digital") == 0) {
            log_warning("%s: gain stage '%s' unsupported\n", __FUNCTION__,
                        stage);
            return 0;
        }
    }

    log_warning("%s: gain stage '%s' invalid\n", __FUNCTION__, stage);
    return 0;
}

static int bladerf2_get_gain_stage(struct bladerf *dev,
                                   bladerf_channel ch,
                                   const char *stage,
                                   int *gain)
{
    struct bladerf2_board_data *board_data;
    int status;

    if (NULL == stage) {
        RETURN_INVAL("stage", "is null");
    }

    if (NULL == gain) {
        RETURN_INVAL("gain", "is null");
    }

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        if (strcmp(stage, "dsa") == 0) {
            return dev->board->get_gain(dev, ch, gain);
        }
    } else {
        struct rf_rx_gain rx_gain;

        status = ad9361_get_rx_gain(board_data->phy, (ch >> 1) + 1, &rx_gain);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_get_rx_gain", status);
        }

        if (strcmp(stage, "full") == 0) {
            *gain = rx_gain.gain_db;
            return 0;
        } else if (strcmp(stage, "digital") == 0) {
            *gain = rx_gain.digital_gain;
            return 0;
        }
    }

    log_warning("%s: gain stage '%s' invalid\n", __FUNCTION__, stage);
    return 0;
}

static int bladerf2_get_gain_stages(struct bladerf *dev,
                                    bladerf_channel ch,
                                    const char **stages,
                                    unsigned int count)
{
    const struct bladerf_gain_stage_info *stage_infos;
    unsigned int stage_infos_len;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        stage_infos     = bladerf2_tx_gain_stages;
        stage_infos_len = ARRAY_SIZE(bladerf2_tx_gain_stages);
    } else {
        stage_infos     = bladerf2_rx_gain_stages;
        stage_infos_len = ARRAY_SIZE(bladerf2_rx_gain_stages);
    }

    if (NULL == stages || 0 == count) {
        return stage_infos_len;
    }

    count = (stage_infos_len < count) ? stage_infos_len : count;

    for (size_t i = 0; i < count; i++) {
        stages[i] = stage_infos[i].name;
    }

    return stage_infos_len;
}


/******************************************************************************/
/* Sample Rate */
/******************************************************************************/

static int bladerf2_get_sample_rate_range(struct bladerf *dev,
                                          bladerf_channel ch,
                                          struct bladerf_range *range)
{
    if (NULL == range) {
        RETURN_INVAL("range", "is null");
    }

    *range = bladerf2_sample_rate_range;

    return 0;
}

static int bladerf2_get_sample_rate(struct bladerf *dev,
                                    bladerf_channel ch,
                                    unsigned int *rate)
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
                                    unsigned int rate,
                                    unsigned int *actual)
{
    struct bladerf2_board_data *board_data;
    struct bladerf_range range;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;

    status = bladerf2_get_sample_rate_range(dev, ch, &range);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf2_get_sample_rate_range", status);
    }

    if (!_is_within_range(&range, rate)) {
        return BLADERF_ERR_RANGE;
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

    if (actual != NULL) {
        return bladerf2_get_sample_rate(dev, ch, actual);
    }

    /* Warn the user if this isn't achievable */
    _is_total_sample_rate_achievable(dev);

    return 0;
}

static int bladerf2_get_rational_sample_rate(struct bladerf *dev,
                                             bladerf_channel ch,
                                             struct bladerf_rational_rate *rate)
{
    int status;
    unsigned int integer_rate;

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
    unsigned int integer_rate, actual_integer_rate;

    if (NULL == rate) {
        RETURN_INVAL("rate", "is null");
    }

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    integer_rate = rate->integer + rate->num / rate->den;

    status =
        bladerf2_set_sample_rate(dev, ch, integer_rate, &actual_integer_rate);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf2_set_sample_ratel", status);
    }

    if (actual != NULL) {
        return bladerf2_get_rational_sample_rate(dev, ch, actual);
    }

    return 0;
}


/******************************************************************************/
/* Bandwidth */
/******************************************************************************/

static int bladerf2_get_bandwidth_range(struct bladerf *dev,
                                        bladerf_channel ch,
                                        struct bladerf_range *range)
{
    if (NULL == range) {
        RETURN_INVAL("range", "is null");
    }

    *range = bladerf2_bandwidth_range;
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
    struct bladerf_range range;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;

    status = bladerf2_get_bandwidth_range(dev, ch, &range);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf2_get_bandwidth_range", status);
    }

    bandwidth = _clamp_to_range(&range, bandwidth);

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
                                        struct bladerf_range *range)
{
    if (NULL == range) {
        RETURN_INVAL("range", "is null");
    }

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        *range = bladerf2_tx_frequency_range;
    } else {
        *range = bladerf2_rx_frequency_range;
    }

    return 0;
}

static int bladerf2_select_band(struct bladerf *dev,
                                bladerf_channel ch,
                                uint64_t frequency)
{
    int status;
    uint32_t reg;

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    /* Read RFFE control register */
    status = dev->backend->rffe_control_read(dev, &reg);
    if (status < 0) {
        RETURN_ERROR_STATUS("rffe_control_read", status);
    }

    /* We have to do this for all the channels sharing the same LO. */
    for (size_t bandchan = 0; bandchan < 2; ++bandchan) {
        bladerf_channel bch = BLADERF_CHANNEL_IS_TX(ch)
                                  ? BLADERF_CHANNEL_TX(bandchan)
                                  : BLADERF_CHANNEL_RX(bandchan);

        /* Is this channel enabled? */
        bool enable = _is_rffe_channel_enabled(reg, bch);

        /* Update SPDT bits accordingly */
        status = _set_spdt_bits(&reg, bch, enable, frequency);
        if (status < 0) {
            RETURN_ERROR_STATUS("_set_spdt_bits", status);
        }
    }

    status = dev->backend->rffe_control_write(dev, reg);
    if (status < 0) {
        RETURN_ERROR_STATUS("rffe_control_write", status);
    }

    /* Set AD9361 port */
    status = _set_ad9361_port(dev, ch, frequency);
    if (status < 0) {
        RETURN_ERROR_STATUS("_set_ad9361_port", status);
    }

    return 0;
}

static int bladerf2_set_frequency(struct bladerf *dev,
                                  bladerf_channel ch,
                                  uint64_t frequency)
{
    struct bladerf2_board_data *board_data;
    struct bladerf_range range;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;

    status = bladerf2_get_frequency_range(dev, ch, &range);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf2_get_frequency_range", status);
    }

    if (!_is_within_range(&range, frequency)) {
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
                                  uint64_t *frequency)
{
    struct bladerf2_board_data *board_data;
    int status;
    uint64_t lo_frequency;

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

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        port_map     = bladerf2_tx_port_map;
        port_map_len = ARRAY_SIZE(bladerf2_tx_port_map);
    } else {
        port_map     = bladerf2_rx_port_map;
        port_map_len = ARRAY_SIZE(bladerf2_rx_port_map);
    }

    for (size_t i = 0; i < port_map_len; i++) {
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
        for (size_t i = 0; i < port_map_len; i++) {
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

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        port_map     = bladerf2_tx_port_map;
        port_map_len = ARRAY_SIZE(bladerf2_tx_port_map);
    } else {
        port_map     = bladerf2_rx_port_map;
        port_map_len = ARRAY_SIZE(bladerf2_rx_port_map);
    }

    if (ports != NULL) {
        count = (port_map_len < count) ? port_map_len : count;

        for (size_t i = 0; i < count; i++) {
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
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_schedule_retune(struct bladerf *dev,
                                    bladerf_channel ch,
                                    uint64_t timestamp,
                                    uint64_t frequency,
                                    struct bladerf_quick_tune *quick_tune)

{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_cancel_scheduled_retunes(struct bladerf *dev,
                                             bladerf_channel ch)
{
    return BLADERF_ERR_UNSUPPORTED;
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
            FIELD_INIT(.reg, {REG_RX1_INPUT_A_PHASE_CORR, REG_RX1_INPUT_BC_PHASE_CORR}),
            FIELD_INIT(.shift, 6),
        },
        [BLADERF_CORR_GAIN] = {
            FIELD_INIT(.reg, {REG_RX1_INPUT_A_GAIN_CORR, REG_RX1_INPUT_BC_PHASE_CORR}),
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
            FIELD_INIT(.reg, {REG_RX2_INPUT_A_PHASE_CORR, REG_RX2_INPUT_BC_PHASE_CORR}),
            FIELD_INIT(.shift, 6),
        },
        [BLADERF_CORR_GAIN] = {
            FIELD_INIT(.reg, {REG_RX2_INPUT_A_GAIN_CORR, REG_RX2_INPUT_BC_PHASE_CORR}),
            FIELD_INIT(.shift, 6),
        }
    },
    [BLADERF_CHANNEL_TX(0)].corr = {
        [BLADERF_CORR_DCOFF_I] = {
            FIELD_INIT(.reg, {REG_TX1_OUT_1_OFFSET_I, REG_TX1_OUT_2_OFFSET_I}),
            FIELD_INIT(.shift, 5),
        },
        [BLADERF_CORR_DCOFF_Q] = {
            FIELD_INIT(.reg, {REG_TX1_OUT_1_OFFSET_Q, REG_TX1_OUT_2_OFFSET_Q}),
            FIELD_INIT(.shift, 5),
        },
        [BLADERF_CORR_PHASE] = {
            FIELD_INIT(.reg, {REG_TX1_OUT_1_PHASE_CORR, REG_TX1_OUT_2_PHASE_CORR}),
            FIELD_INIT(.shift, 6),
        },
        [BLADERF_CORR_GAIN] = {
            FIELD_INIT(.reg, {REG_TX1_OUT_1_GAIN_CORR, REG_TX1_OUT_2_GAIN_CORR}),
            FIELD_INIT(.shift, 6),
        }
    },
    [BLADERF_CHANNEL_TX(1)].corr = {
        [BLADERF_CORR_DCOFF_I] = {
            FIELD_INIT(.reg, {REG_TX2_OUT_1_OFFSET_I, REG_TX2_OUT_2_OFFSET_I}),
            FIELD_INIT(.shift, 5),
        },
        [BLADERF_CORR_DCOFF_Q] = {
            FIELD_INIT(.reg, {REG_TX2_OUT_1_OFFSET_Q, REG_TX2_OUT_2_OFFSET_Q}),
            FIELD_INIT(.shift, 5),
        },
        [BLADERF_CORR_PHASE] = {
            FIELD_INIT(.reg, {REG_TX2_OUT_1_PHASE_CORR, REG_TX2_OUT_2_PHASE_CORR}),
            FIELD_INIT(.shift, 6),
        },
        [BLADERF_CORR_GAIN] = {
            FIELD_INIT(.reg, {REG_TX2_OUT_1_GAIN_CORR, REG_TX2_OUT_2_GAIN_CORR}),
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
            {REG_INPUT_A_OFFSETS_1, REG_RX1_INPUT_A_OFFSETS},
            /* Q */
            {REG_RX1_INPUT_A_OFFSETS, REG_RX1_INPUT_A_Q_OFFSET},
        },
        /* B/C band */
        {
            /* I */
            {REG_INPUT_BC_OFFSETS_1, REG_RX1_INPUT_BC_OFFSETS},
            /* Q */
            {REG_RX1_INPUT_BC_OFFSETS, REG_RX1_INPUT_BC_Q_OFFSET},
        },
    },
    /* Channel 2 */
    [BLADERF_CHANNEL_RX(1)] = {
        /* A band */
        {
            /* I */
            {REG_RX2_INPUT_A_I_OFFSET, REG_RX2_INPUT_A_OFFSETS},
            /* Q */
            {REG_RX2_INPUT_A_OFFSETS, REG_INPUT_A_OFFSETS_1},
        },
        /* B/C band */
        {
            /* I */
            {REG_RX2_INPUT_BC_I_OFFSET, REG_RX2_INPUT_BC_OFFSETS},
            /* Q */
            {REG_RX2_INPUT_BC_OFFSETS, REG_INPUT_BC_OFFSETS_1},
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

        low_band = (mode == TXA);
    } else {
        uint32_t mode;

        status = ad9361_get_rx_rf_port_input(board_data->phy, &mode);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_get_rx_rf_port_input", status);
        }

        /* Check if RX RF port mode is supported */
        if (mode != A_BALANCED && mode != B_BALANCED && mode != C_BALANCED) {
            RETURN_ERROR_STATUS("mode", BLADERF_ERR_UNSUPPORTED);
        }

        low_band = (mode == A_BALANCED);
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

        low_band = (mode == TXA);
    } else {
        uint32_t mode;

        status = ad9361_get_rx_rf_port_input(board_data->phy, &mode);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_get_rx_rf_port_input", status);
        }

        /* Check if RX RF port mode is supported */
        if (mode != A_BALANCED && mode != B_BALANCED && mode != C_BALANCED) {
            RETURN_ERROR_STATUS("mode", BLADERF_ERR_UNSUPPORTED);
        }

        low_band = (mode == A_BALANCED);
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

    reg = (BLADERF_CHANNEL_IS_TX(ch)) ? REG_TX_FORCE_BITS : REG_FORCE_BITS;

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

    return fpga_trigger_fire(dev, trigger);
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
    /* FIXME use layout to configure for MIMO here */

    return async_run_stream(stream, layout & BLADERF_DIRECTION_MASK);
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
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_get_stream_timeout(struct bladerf *dev,
                                       bladerf_direction dir,
                                       unsigned int *timeout)
{
    return BLADERF_ERR_UNSUPPORTED;
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

    /* FIXME use layout to configure for MIMO here */

    status = sync_init(&board_data->sync[dir], dev, layout, format, num_buffers,
                       buffer_size, board_data->msg_size, num_transfers,
                       stream_timeout);

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
                                  uint64_t *value)
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
// clang-format off
#define FPGA_SIZE_XA4   (2632660)
#define FPGA_SIZE_XA9   (12858972)
// clang-format on

static bool is_valid_fpga_size(bladerf_fpga_size fpga, size_t len)
{
    static const char env_override[] = "BLADERF_SKIP_FPGA_SIZE_CHECK";
    bool valid;

    switch (fpga) {
        case BLADERF_FPGA_A4:
            valid = (len == FPGA_SIZE_XA4);
            break;

        case BLADERF_FPGA_A9:
            valid = (len == FPGA_SIZE_XA9);
            break;

        default:
            log_debug("Unknown FPGA type (%d). Using relaxed size criteria.\n",
                      fpga);

            if (len < (1 * 1024 * 1024)) {
                valid = false;
            } else if (len > BLADERF_FLASH_BYTE_LEN_FPGA) {
                valid = false;
            } else {
                valid = true;
            }
            break;
    }

    /* Provide a means to override this check. This is intended to allow
     * folks who know what they're doing to work around this quickly without
     * needing to make a code change. (e.g., someone building a custom FPGA
     * image that enables compressoin) */
    if (getenv(env_override)) {
        log_info("Overriding FPGA size check per %s\n", env_override);
        valid = true;
    }

    if (!valid) {
        log_warning("Detected potentially incorrect FPGA file.\n");

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
        RETURN_INVAL_ARG("fpga size", board_data->fpga_size, "is not valid");
    }

    MUTEX_LOCK(&dev->lock);

    status = dev->backend->load_fpga(dev, buf, length);
    if (status != 0) {
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
        RETURN_INVAL_ARG("fpga size", board_data->fpga_size, "is not valid");
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
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_get_tuning_mode(struct bladerf *dev,
                                    bladerf_tuning_mode *mode)
{
    return BLADERF_ERR_UNSUPPORTED;
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
        case BLADERF_LB_AD9361_BIST:
            bist_loopback = 1;
            break;
        default:
            RETURN_ERROR_STATUS_ARG("decoding loopback mode", l,
                                    BLADERF_ERR_UNSUPPORTED);
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
        *l = BLADERF_LB_AD9361_BIST;
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
    *enable = (0 == (trim >> 14));

    log_debug("trim DAC is %s\n", (*enable ? "enabled" : "disabled"));

    if ((trim >> 14) != 0 && (trim >> 14) != 3) {
        log_warning("unknown trim DAC state: 0x%x\n", (trim >> 14));
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
    if (!enable && trim != (3 << 14)) {
        board_data->trimdac_value = trim;
        log_debug("saving current trim DAC value: 0x%04x\n", trim);
        trim = 3 << 14;
    } else if (enable && trim == (3 << 14)) {
        trim = board_data->trimdac_value;
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

static int bladerf2_get_vctcxo_trim(struct bladerf *dev, uint16_t *trim)
{
    if (NULL == trim) {
        RETURN_INVAL("trim", "is null");
    }

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    /* FIXME fetch factory value from SPI flash */
    *trim = 0x1ffc;

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
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

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
    FIELD_INIT(.is_fpga_configured, bladerf2_is_fpga_configured),
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
/* Low level AD9361 Accessors */
/******************************************************************************/

int bladerf_ad9361_read(struct bladerf *dev, uint16_t address, uint8_t *val)
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

    address = AD_READ | AD_CNT(1) | address;

    status = dev->backend->ad9361_spi_read(dev, address, &data);
    if (status < 0) {
        MUTEX_UNLOCK(&dev->lock);
        RETURN_ERROR_AD9361("ad9361_spi_read", status);
    }

    *val = (data >> 56) & 0xff;

    MUTEX_UNLOCK(&dev->lock);

    return 0;
}

int bladerf_ad9361_write(struct bladerf *dev, uint16_t address, uint8_t val)
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

    address = AD_WRITE | AD_CNT(1) | address;

    data = (((uint64_t)val) << 56);

    status = dev->backend->ad9361_spi_write(dev, address, data);
    if (status < 0) {
        MUTEX_UNLOCK(&dev->lock);
        RETURN_ERROR_AD9361("ad9361_spi_write", status);
    }

    MUTEX_UNLOCK(&dev->lock);

    return 0;
}

int bladerf_ad9361_temperature(struct bladerf *dev, float *val)
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

    *val = ad9361_get_temp(phy) / 1000.0;

    return 0;
}

int bladerf_ad9361_get_rssi(struct bladerf *dev,
                            bladerf_channel ch,
                            int *pre_rssi,
                            int *sym_rssi)
{
    struct bladerf2_board_data *board_data;
    struct ad9361_rf_phy *phy;
    int status;
    int32_t pre, sym;

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

        pre = rssi / 1000.0;
        sym = rssi / 1000.0;
    } else {
        struct rf_rssi rssi;

        status = ad9361_get_rx_rssi(phy, ch >> 1, &rssi);
        if (status < 0) {
            MUTEX_UNLOCK(&dev->lock);
            RETURN_ERROR_AD9361("ad9361_get_rx_rssi", status);
        }

        pre = rssi.preamble / rssi.multiplier;
        sym = rssi.symbol / rssi.multiplier;
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

/******************************************************************************/
/* Low level ADF4002 Accessors */
/******************************************************************************/

static int bladerf_adf4002_configure(struct bladerf *dev,
                                     uint16_t R,
                                     uint16_t N)
{
    int status;
    uint32_t init_array[3];
    bool is_enabled;

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
    status = bladerf_adf4002_get_enable(dev, &is_enabled);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf_adf4002_get_enable", status);
    }

    /* Enable the chip if applicable */
    if (!is_enabled) {
        bladerf_adf4002_set_enable(dev, true);
        if (status < 0) {
            RETURN_ERROR_STATUS("bladerf_adf4002_set_enable", status);
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
    for (size_t i = 0; i < ARRAY_SIZE(init_array); ++i) {
        log_debug("reg %x gets 0x%08x\n", i, init_array[i]);
        status = bladerf_adf4002_write(dev, i, init_array[i]);
        if (status < 0) {
            RETURN_ERROR_STATUS("bladerf_adf4002_write", status);
        }
    }

    /* Re-disable the chip if applicable */
    if (!is_enabled) {
        bladerf_adf4002_set_enable(dev, false);
        if (status < 0) {
            RETURN_ERROR_STATUS("bladerf_adf4002_set_enable", status);
        }
    }

    return 0;
}

static int bladerf_adf4002_calculate_ratio(uint64_t ref_freq,
                                           uint64_t clock_freq,
                                           uint16_t *R,
                                           uint16_t *N)
{
    size_t const Rmax = 16383;
    double const tol  = 0.00001;
    double target     = (double)clock_freq / (double)ref_freq;

    struct bladerf_range const clock_frequency_range = {
        FIELD_INIT(.min, 5e6), FIELD_INIT(.max, 400e6), FIELD_INIT(.step, 1),
        FIELD_INIT(.scale, 1),
    };

    if (NULL == R) {
        RETURN_INVAL("R", "is null");
    }

    if (NULL == N) {
        RETURN_INVAL("N", "is null");
    }

    if (!_is_within_range(&bladerf2_adf4002_refclk_range, ref_freq)) {
        return BLADERF_ERR_RANGE;
    }

    if (!_is_within_range(&clock_frequency_range, clock_freq)) {
        return BLADERF_ERR_RANGE;
    }

    for (uint16_t R_try = 1; R_try < Rmax; ++R_try) {
        uint16_t N_try = (uint16_t)(target * R_try + 0.5);

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

int bladerf_adf4002_get_locked(struct bladerf *dev, bool *locked)
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

int bladerf_adf4002_get_enable(struct bladerf *dev, bool *enabled)
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

    *enabled = (data >> 11) & 0x01;

    MUTEX_UNLOCK(&dev->lock);

    return 0;
}

int bladerf_adf4002_set_enable(struct bladerf *dev, bool enable)
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

    // Disable the trim DAC when we're using the ADF4002
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

    // Set the ADF4002 enable bit accordingly
    data &= ~(1 << 11);
    data |= ((enable ? 1 : 0) << 11);

    // Write back the config GPIO
    status = dev->backend->config_gpio_write(dev, data);
    if (status < 0) {
        MUTEX_UNLOCK(&dev->lock);
        RETURN_ERROR_STATUS("config_gpio_write", status);
    }

    // Update our state flag
    board_data->trim_source = enable ? TRIM_SOURCE_ADF4002 : TRIM_SOURCE_NONE;

    // Enable the trim DAC if we're done with the ADF4002
    if (!enable) {
        status = bladerf2_set_trim_dac_enable(dev, true);
        if (status < 0) {
            RETURN_ERROR_STATUS("bladerf2_set_trim_dac_enable", status);
        }
    }

    MUTEX_UNLOCK(&dev->lock);

    return 0;
}

int bladerf_adf4002_get_refclk_range(struct bladerf *dev,
                                     struct bladerf_range *range)
{
    if (NULL == range) {
        RETURN_INVAL("range", "is null");
    }

    *range = bladerf2_adf4002_refclk_range;

    return 0;
}

int bladerf_adf4002_get_refclk(struct bladerf *dev, uint64_t *frequency)
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
    status = bladerf_adf4002_read(dev, R_LATCH_REG, &reg);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf_adf4002_read", status);
    }
    R = (reg >> R_LATCH_SHIFT) & R_LATCH_MASK;

    // Get the current N value (latch 1, bits 8-20)
    status = bladerf_adf4002_read(dev, N_LATCH_REG, &reg);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf_adf4002_read", status);
    }
    N = (reg >> N_LATCH_SHIFT) & N_LATCH_MASK;

    // We assume the system clock frequency is BLADERF_VCTCXO_FREQUENCY.
    // If it isn't, do your own math
    *frequency = R * BLADERF_VCTCXO_FREQUENCY / N;

    return 0;
}

int bladerf_adf4002_set_refclk(struct bladerf *dev, uint64_t frequency)
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
    status = bladerf_adf4002_calculate_ratio(frequency,
                                             BLADERF_VCTCXO_FREQUENCY, &R, &N);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf_adf4002_calculate_ratio", status);
    }

    status = bladerf_adf4002_configure(dev, R, N);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf_adf4002_configure", status);
    }

    return 0;
}

int bladerf_adf4002_read(struct bladerf *dev, uint8_t address, uint32_t *val)
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

int bladerf_adf4002_write(struct bladerf *dev, uint8_t address, uint32_t val)
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

    if ((data >> 0) & 0x01) {
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

    if ((gpio & (1 << 18)) == 0x0) {
        *sel = CLOCK_SELECT_VCTCXO;
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
        case CLOCK_SELECT_VCTCXO:
            gpio &= ~(1 << 18);
            break;
        case CLOCK_SELECT_EXTERNAL:
            gpio |= (1 << 18);
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
/* Low level SI53304 clock output accessors */
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

    *state = ((gpio & (1 << 17)) != 0x0);

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
    gpio &= ~(1 << 17);
    gpio |= ((enable ? 1 : 0) << 17);

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

int bladerf_ina219_read(struct bladerf *dev,
                        bladerf_ina219_register reg,
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
        case BLADERF_INA219_CONFIGURATION:
        case BLADERF_INA219_CALIBRATION:
            return BLADERF_ERR_UNSUPPORTED;
        case BLADERF_INA219_VOLTAGE_SHUNT:
            status = ina219_read_shunt_voltage(dev, (float *)val);
            break;
        case BLADERF_INA219_VOLTAGE_BUS:
            status = ina219_read_bus_voltage(dev, (float *)val);
            break;
        case BLADERF_INA219_POWER:
            status = ina219_read_power(dev, (float *)val);
            break;
        case BLADERF_INA219_CURRENT:
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
