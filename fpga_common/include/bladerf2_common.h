/* This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (c) 2018 Nuand LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef FPGA_COMMON_BLADERF2_COMMON_H_
#define FPGA_COMMON_BLADERF2_COMMON_H_

#include <errno.h>

#if !defined(BLADERF_NIOS_BUILD) && !defined(BLADERF_NIOS_PC_SIMULATION)
#include <libbladeRF.h>
#else
#include "libbladeRF_nios_compat.h"
#endif  // !defined(BLADERF_NIOS_BUILD) && !defined(BLADERF_NIOS_PC_SIMULATION)

#include "ad936x.h"
#include "host_config.h"
#include "nios_pkt_retune2.h"
#include "range.h"

/**
 * Number of modules (directions) present. 1 RX, 1 TX = 2.
 */
#define NUM_MODULES 2

/**
 * Frequency of the system VCTCXO, in Hz.
 */
static bladerf_frequency const BLADERF_VCTCXO_FREQUENCY = 38400000;

/**
 * Default reference input frequency, in Hz.
 */
static bladerf_frequency const BLADERF_REFIN_DEFAULT = 10000000;

/**
 * RFIC reset frequency (arbitrary)
 */
static bladerf_frequency const RESET_FREQUENCY = 70000000;

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
// clang-format on

/**
 * RF front end bands
 */
enum bladerf2_band {
    BAND_SHUTDOWN, /**< Inactive */
    BAND_LOW,      /**< Low-band */
    BAND_HIGH,     /**< High-band */
};

/**
 * Mapping between libbladeRF gain modes and RFIC gain modes.
 */
struct bladerf_rfic_gain_mode_map {
    bladerf_gain_mode brf_mode;       /**< libbladeRF gain mode */
    enum rf_gain_ctrl_mode rfic_mode; /**< RFIC gain mode */
};

/**
 * Mapping between frequency ranges and gain ranges.
 */
struct bladerf_gain_range {
    char const *name;               /**< Gain stage name */
    struct bladerf_range frequency; /**< Frequency range */
    struct bladerf_range gain;      /**< Applicable stage gain range */
    float offset; /**< Offset in dB, for mapping dB gain to absolute dBm. */
};

/**
 * Mapping between string names and RFIC port identifiers.
 */
struct bladerf_rfic_port_name_map {
    char const *name; /**< Port name */
    uint32_t id;      /**< Port ID */
};

/**
 * Mapping between RF front end bands, freqencies, and physical hardware
 * configurations
 */
struct band_port_map {
    struct bladerf_range const frequency; /**< Frequency range */
    enum bladerf2_band band;              /**< RF front end band */
    uint32_t spdt;                        /**< RF switch configuration */
    uint32_t rfic_port;                   /**< RFIC port configuration */
};

/**
 * @brief   Round a value into an int
 *
 * @param   x     Value to round
 *
 * @return  int
 */
#define __round_int(x) (x >= 0 ? (int)(x + 0.5) : (int)(x - 0.5))

/**
 * @brief   Round a value into an int64
 *
 * @param   x     Value to round
 *
 * @return  int64
 */
#define __round_int64(x) (x >= 0 ? (int64_t)(x + 0.5) : (int64_t)(x - 0.5))

/**
 * Subcommands for the BLADERF_RFIC_COMMAND_INIT RFIC command.
 */
typedef enum {
    BLADERF_RFIC_INIT_STATE_OFF = 0, /** Non-initialized state */
    BLADERF_RFIC_INIT_STATE_ON,      /** Initialized ("open") */
    BLADERF_RFIC_INIT_STATE_STANDBY, /** Standby ("closed") */
} bladerf_rfic_init_state;

/**
 * Commands available with the FPGA-based RFIC interface.
 *
 * There is an 8-bit address space (0x00 to 0xFF) available. Nuand will not
 * assign values between 0x80 and 0xFF, so they may be used for custom
 * applications.
 */
typedef enum {
    /** Query the status register. (Read)
     *
     * Pass ::BLADERF_CHANNEL_INVALID as the `ch` parameter.
     *
     * Return structure:
     *    +================+========================+
     *    |      Bit(s)    |         Value          |
     *    +================+========================+
     *    |      63:16     | Reserved. Set to 0.    |
     *    +----------------+------------------------+
     *    |      15:8      | count of items in      |
     *    |                | write queue            |
     *    +----------------+------------------------+
     *    |        0       | 1 if initialized, 0    |
     *    |                | otherwise              |
     *    +----------------+------------------------+
     */
    BLADERF_RFIC_COMMAND_STATUS = 0x00,

    /** Initialize the RFIC. (Read/Write)
     *
     * Pass ::BLADERF_CHANNEL_INVALID as the `ch` parameter.
     *
     * Pass/expect a ::bladerf_rfic_init_state value as the `data` parameter.
     */
    BLADERF_RFIC_COMMAND_INIT = 0x01,

    /** Enable/disable a channel. (Read/Write)
     *
     * Set `data` to `true` to enable the channel, or `false` to disable it.
     */
    BLADERF_RFIC_COMMAND_ENABLE = 0x02,

    /** Sample rate for a channel. (Read/Write)
     *
     * Value in samples per second.
     */
    BLADERF_RFIC_COMMAND_SAMPLERATE = 0x03,

    /** Center frequency for a channel. (Read/Write)
     *
     * Value in Hz. Read or write.
     */
    BLADERF_RFIC_COMMAND_FREQUENCY = 0x04,

    /** Bandwidth for a channel. (Read/Write)
     *
     * Value in Hz.
     */
    BLADERF_RFIC_COMMAND_BANDWIDTH = 0x05,

    /** Gain mode for a channel. (Read/Write)
     *
     * Pass a ::bladerf_gain_mode value as the `data` parameter.
     */
    BLADERF_RFIC_COMMAND_GAINMODE = 0x06,

    /** Overall gain for a channel. (Read/Write)
     *
     * Value in dB.
     */
    BLADERF_RFIC_COMMAND_GAIN = 0x07,

    /** RSSI (received signal strength indication) for a channel. (Read)
     *
     * Value in dB.
     */
    BLADERF_RFIC_COMMAND_RSSI = 0x08,

    /** FIR filter setting for a channel. (Read/Write)
     *
     * RX channels should pass a ::bladerf_rfic_rxfir value, TX channels should
     * pass a ::bladerf_rfic_txfir value.
     */
    BLADERF_RFIC_COMMAND_FILTER = 0x09,

    /** TX Mute setting for a channel. (Read/Write)
     *
     * 1 indicates TX mute is enabled, 0 indicates it is not.
     */
    BLADERF_RFIC_COMMAND_TXMUTE = 0x0A,

    /** Store Fastlock profile. (Write)
     *
     * Stores the current tuning into a fastlock profile, for later recall
     */
    BLADERF_RFIC_COMMAND_FASTLOCK = 0x0B,

    /** User-defined functionality (placeholder 1) */
    BLADERF_RFIC_COMMAND_USER_001 = 0x80,

    /** User-defined functionality (placeholder 128) */
    BLADERF_RFIC_COMMAND_USER_128 = 0xFF,

    /* Do not add additional commands beyond 0xFF */
} bladerf_rfic_command;

/** NIOS_PKT_16x64_RFIC_STATUS return structure
 *
 *  +===============+===================================================+
 *  |      Bit(s)   |         Value                                     |
 *  +===============+===================================================+
 *  |      63:16    | Reserved. Set to 0.                               |
 *  +---------------+---------------------------------------------------+
 *  |      15:8     | count of items in write queue                     |
 *  +---------------+---------------------------------------------------+
 *  |        1      | 1 if the last job executed in the write queue was |
 *  |               | successful, 0 otherwise                           |
 *  +---------------+---------------------------------------------------+
 *  |        0      | 1 if initialized, 0 otherwise                     |
 *  +---------------+---------------------------------------------------+
 */
// clang-format off
#define BLADERF_RFIC_STATUS_INIT_SHIFT       0
#define BLADERF_RFIC_STATUS_INIT_MASK        0x1
#define BLADERF_RFIC_STATUS_WQSUCCESS_SHIFT  1
#define BLADERF_RFIC_STATUS_WQSUCCESS_MASK   0x1
#define BLADERF_RFIC_STATUS_WQLEN_SHIFT      8
#define BLADERF_RFIC_STATUS_WQLEN_MASK       0xff

#define BLADERF_RFIC_RSSI_MULT_SHIFT         32
#define BLADERF_RFIC_RSSI_MULT_MASK          0xFFFF
#define BLADERF_RFIC_RSSI_PRE_SHIFT          16
#define BLADERF_RFIC_RSSI_PRE_MASK           0xFFFF
#define BLADERF_RFIC_RSSI_SYM_SHIFT          0
#define BLADERF_RFIC_RSSI_SYM_MASK           0xFFFF
// clang-format on


/******************************************************************************/
/* Constants */
/******************************************************************************/
// clang-format off

/* Gain mode mappings */
static struct bladerf_rfic_gain_mode_map const bladerf2_rx_gain_mode_map[] = {
    {   
        FIELD_INIT(.brf_mode, BLADERF_GAIN_MGC),
        FIELD_INIT(.rfic_mode, RF_GAIN_MGC)
    },
    {   
        FIELD_INIT(.brf_mode, BLADERF_GAIN_FASTATTACK_AGC),
        FIELD_INIT(.rfic_mode, RF_GAIN_FASTATTACK_AGC)
    },
    {   
        FIELD_INIT(.brf_mode, BLADERF_GAIN_SLOWATTACK_AGC),
        FIELD_INIT(.rfic_mode, RF_GAIN_SLOWATTACK_AGC)
    },
    {   
        FIELD_INIT(.brf_mode, BLADERF_GAIN_HYBRID_AGC),
        FIELD_INIT(.rfic_mode, RF_GAIN_HYBRID_AGC)
    },
};

/* RX gain ranges */
/* Reference: ad9361.c, ad9361_gt_tableindex and ad9361_init_gain_tables */
static struct bladerf_gain_range const bladerf2_rx_gain_ranges[] = {
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

/* Overall TX gain range */
static struct bladerf_gain_range const bladerf2_tx_gain_ranges[] = {
    {
        /* TX gain offset: 60 dB system gain ~= 0 dBm output */
        FIELD_INIT(.name, NULL),
        FIELD_INIT(.frequency, {
            FIELD_INIT(.min,    47000000UL),
            FIELD_INIT(.max,    6000000000UL),
            FIELD_INIT(.step,   1),
            FIELD_INIT(.scale,  1),
        }),
        FIELD_INIT(.gain, {
            FIELD_INIT(.min,    __round_int64(1000*(-89.750 + 66.0))),
            FIELD_INIT(.max,    __round_int64(1000*(0 + 66.0))),
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
static struct bladerf_gain_modes const bladerf2_rx_gain_modes[] = {
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

/* Default RX gain control modes */
static enum rf_gain_ctrl_mode const bladerf2_rx_gain_mode_default[2] = {
    RF_GAIN_SLOWATTACK_AGC, RF_GAIN_SLOWATTACK_AGC
};

/* Sample Rate Range */
static struct bladerf_range const bladerf2_sample_rate_range = {
    FIELD_INIT(.min,    520834),
    FIELD_INIT(.max,    61440000),
    FIELD_INIT(.step,   1),
    FIELD_INIT(.scale,  1),
};

/* Sample rates requiring a 4x interpolation/decimation */
static struct bladerf_range const bladerf2_sample_rate_range_4x = {
    FIELD_INIT(.min,    520834),
    FIELD_INIT(.max,    2083334),
    FIELD_INIT(.step,   1),
    FIELD_INIT(.scale,  1),
};

/* Bandwidth Range */
static struct bladerf_range const bladerf2_bandwidth_range = {
    FIELD_INIT(.min,    200000),
    FIELD_INIT(.max,    56000000),
    FIELD_INIT(.step,   1),
    FIELD_INIT(.scale,  1),
};

/* Frequency Ranges */
static struct bladerf_range const bladerf2_rx_frequency_range = {
    FIELD_INIT(.min,    70000000),
    FIELD_INIT(.max,    6000000000),
    FIELD_INIT(.step,   2),
    FIELD_INIT(.scale,  1),
};

static struct bladerf_range const bladerf2_tx_frequency_range = {
    FIELD_INIT(.min,    47000000),
    FIELD_INIT(.max,    6000000000),
    FIELD_INIT(.step,   2),
    FIELD_INIT(.scale,  1),
};


/* RF Ports */
static struct bladerf_rfic_port_name_map const bladerf2_rx_port_map[] = {
    { FIELD_INIT(.name, "A_BALANCED"),  FIELD_INIT(.id, AD936X_A_BALANCED), },
    { FIELD_INIT(.name, "B_BALANCED"),  FIELD_INIT(.id, AD936X_B_BALANCED), },
    { FIELD_INIT(.name, "C_BALANCED"),  FIELD_INIT(.id, AD936X_C_BALANCED), },
    { FIELD_INIT(.name, "A_N"),         FIELD_INIT(.id, AD936X_A_N),        },
    { FIELD_INIT(.name, "A_P"),         FIELD_INIT(.id, AD936X_A_P),        },
    { FIELD_INIT(.name, "B_N"),         FIELD_INIT(.id, AD936X_B_N),        },
    { FIELD_INIT(.name, "B_P"),         FIELD_INIT(.id, AD936X_B_P),        },
    { FIELD_INIT(.name, "C_N"),         FIELD_INIT(.id, AD936X_C_N),        },
    { FIELD_INIT(.name, "C_P"),         FIELD_INIT(.id, AD936X_C_P),        },
    { FIELD_INIT(.name, "TX_MON1"),     FIELD_INIT(.id, AD936X_TX_MON1),    },
    { FIELD_INIT(.name, "TX_MON2"),     FIELD_INIT(.id, AD936X_TX_MON2),    },
    { FIELD_INIT(.name, "TX_MON1_2"),   FIELD_INIT(.id, AD936X_TX_MON1_2),  },
};

static struct bladerf_rfic_port_name_map const bladerf2_tx_port_map[] = {
    { FIELD_INIT(.name, "TXA"),         FIELD_INIT(.id, AD936X_TXA),        },
    { FIELD_INIT(.name, "TXB"),         FIELD_INIT(.id, AD936X_TXB),        },
};

static struct band_port_map const bladerf2_rx_band_port_map[] = {
    {
        FIELD_INIT(.frequency, {
            FIELD_INIT(.min,        0),
            FIELD_INIT(.max,        0),
            FIELD_INIT(.step,       1),
            FIELD_INIT(.scale,      1),
        }),
        FIELD_INIT(.band,           BAND_SHUTDOWN),
        FIELD_INIT(.spdt,           RFFE_CONTROL_SPDT_SHUTDOWN),
        FIELD_INIT(.rfic_port,      0),
    },
    {
        FIELD_INIT(.frequency, {
            FIELD_INIT(.min,        70000000UL),
            FIELD_INIT(.max,        3000000000UL),
            FIELD_INIT(.step,       2),
            FIELD_INIT(.scale,      1),
        }),
        FIELD_INIT(.band,           BAND_LOW),
        FIELD_INIT(.spdt,           RFFE_CONTROL_SPDT_LOWBAND),
        FIELD_INIT(.rfic_port,      AD936X_B_BALANCED),
    },
    {
        FIELD_INIT(.frequency, {
            FIELD_INIT(.min,        3000000000UL),
            FIELD_INIT(.max,        6000000000UL),
            FIELD_INIT(.step,       2),
            FIELD_INIT(.scale,      1),
        }),
        FIELD_INIT(.band,           BAND_HIGH),
        FIELD_INIT(.spdt,           RFFE_CONTROL_SPDT_HIGHBAND),
        FIELD_INIT(.rfic_port,      AD936X_A_BALANCED),
    },
};

static struct band_port_map const bladerf2_tx_band_port_map[] = {
    {
        FIELD_INIT(.frequency, {
            FIELD_INIT(.min,        0),
            FIELD_INIT(.max,        0),
            FIELD_INIT(.step,       1),
            FIELD_INIT(.scale,      1),
        }),
        FIELD_INIT(.band,           BAND_SHUTDOWN),
        FIELD_INIT(.spdt,           RFFE_CONTROL_SPDT_SHUTDOWN),
        FIELD_INIT(.rfic_port,      0),
    },
    {
        FIELD_INIT(.frequency, {
            FIELD_INIT(.min,        46875000UL),
            FIELD_INIT(.max,        3000000000UL),
            FIELD_INIT(.step,       2),
            FIELD_INIT(.scale,      1),
        }),
        FIELD_INIT(.band,           BAND_LOW),
        FIELD_INIT(.spdt,           RFFE_CONTROL_SPDT_LOWBAND),
        FIELD_INIT(.rfic_port,      AD936X_TXB),
    },
    {
        FIELD_INIT(.frequency, {
            FIELD_INIT(.min,        3000000000UL),
            FIELD_INIT(.max,        6000000000UL),
            FIELD_INIT(.step,       2),
            FIELD_INIT(.scale,      1),
        }),
        FIELD_INIT(.band,           BAND_HIGH),
        FIELD_INIT(.spdt,           RFFE_CONTROL_SPDT_HIGHBAND),
        FIELD_INIT(.rfic_port,      AD936X_TXA),
    },
};

// clang-format on


/******************************************************************************/
/* Helpers */
/******************************************************************************/

/**
 * @brief      Translate libad936x error codes to libbladeRF error codes
 *
 * @param[in]  err   The error
 *
 * @return     value from \ref RETCODES list, or 0 if err is >= 0
 */
int errno_ad9361_to_bladerf(int err);

/**
 * @brief      Gets the band port map by frequency.
 *
 * @param[in]  ch    Channel
 * @param[in]  freq  Frequency. Use 0 for the "disabled" state.
 *
 * @return     pointer to band_port_map
 */
struct band_port_map const *_get_band_port_map_by_freq(bladerf_channel ch,
                                                       bladerf_frequency freq);

/**
 * @brief      Modifies reg to configure the RF switch SPDT bits
 *
 * @param      reg      RFFE control register ptr
 * @param[in]  ch       Channel
 * @param[in]  enabled  True if the channel is enabled, False otherwise
 * @param[in]  freq     Frequency
 *
 * @return     0 on success, value from \ref RETCODES list on failure
 */
int _modify_spdt_bits_by_freq(uint32_t *reg,
                              bladerf_channel ch,
                              bool enabled,
                              bladerf_frequency freq);

/**
 * @brief      Look up the RFFE control register bit for a bladerf_direction
 *
 * @param[in]  dir   Direction
 *
 * @return     Bit index
 */
int _get_rffe_control_bit_for_dir(bladerf_direction dir);

/**
 * @brief      Look up the RFFE control register bit for a bladerf_channel
 *
 * @param[in]  ch    Channel
 *
 * @return     Bit index
 */
int _get_rffe_control_bit_for_ch(bladerf_channel ch);

/**
 * @brief      Determine if a channel is active
 *
 * @param[in]  reg   RFFE control register
 * @param[in]  ch    Channel
 *
 * @return     true if active, false otherwise
 */
bool _rffe_ch_enabled(uint32_t reg, bladerf_channel ch);

/**
 * @brief      Determine if any channel in a direction is active
 *
 * @param[in]  reg   RFFE control register
 * @param[in]  dir   Direction
 *
 * @return     true if any channel is active, false otherwise
 */
bool _rffe_dir_enabled(uint32_t reg, bladerf_direction dir);

/**
 * @brief      Determine if any *other* channel in a direction is active
 *
 * @param[in]  reg   RFFE control register
 * @param[in]  ch    Channel
 *
 * @return     true if any channel in the same direction as ch is active, false
 *             otherwise
 */
bool _rffe_dir_otherwise_enabled(uint32_t reg, bladerf_channel ch);

#endif  // FPGA_COMMON_BLADERF2_COMMON_H_
