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

#ifndef BLADERF2_COMMON_H_
#define BLADERF2_COMMON_H_

#if !defined(BLADERF_NIOS_BUILD) && !defined(BLADERF_NIOS_PC_SIMULATION)
#include "log.h"
#endif

#include "bladerf2_common.h"
#include "helpers/version.h"
#include "streaming/sync.h"


/******************************************************************************/
/* Types */
/******************************************************************************/

enum bladerf2_vctcxo_trim_source {
    TRIM_SOURCE_NONE,
    TRIM_SOURCE_TRIM_DAC,
    TRIM_SOURCE_PLL,
    TRIM_SOURCE_AUXDAC
};

enum bladerf2_rfic_command_mode {
    RFIC_COMMAND_HOST, /**< Host-based control */
    RFIC_COMMAND_FPGA, /**< FPGA-based control */
};

struct controller_fns {
    bool (*is_present)(struct bladerf *dev);
    bool (*is_initialized)(struct bladerf *dev);
    bool (*is_standby)(struct bladerf *dev);
    int (*get_init_state)(struct bladerf *dev, bladerf_rfic_init_state *state);

    int (*initialize)(struct bladerf *dev);
    int (*standby)(struct bladerf *dev);
    int (*deinitialize)(struct bladerf *dev);

    int (*enable_module)(struct bladerf *dev, bladerf_channel ch, bool enable);

    int (*get_sample_rate)(struct bladerf *dev,
                           bladerf_channel ch,
                           bladerf_sample_rate *rate);
    int (*set_sample_rate)(struct bladerf *dev,
                           bladerf_channel ch,
                           bladerf_sample_rate rate);

    int (*get_frequency)(struct bladerf *dev,
                         bladerf_channel ch,
                         bladerf_frequency *frequency);
    int (*set_frequency)(struct bladerf *dev,
                         bladerf_channel ch,
                         bladerf_frequency frequency);
    int (*select_band)(struct bladerf *dev,
                       bladerf_channel ch,
                       bladerf_frequency frequency);

    int (*get_bandwidth)(struct bladerf *dev,
                         bladerf_channel ch,
                         bladerf_bandwidth *bandwidth);
    int (*set_bandwidth)(struct bladerf *dev,
                         bladerf_channel ch,
                         bladerf_bandwidth bandwidth,
                         bladerf_bandwidth *actual);

    int (*get_gain_mode)(struct bladerf *dev,
                         bladerf_channel ch,
                         bladerf_gain_mode *mode);
    int (*set_gain_mode)(struct bladerf *dev,
                         bladerf_channel ch,
                         bladerf_gain_mode mode);

    int (*get_gain)(struct bladerf *dev, bladerf_channel ch, int *gain);
    int (*set_gain)(struct bladerf *dev, bladerf_channel ch, int gain);

    int (*get_gain_stage)(struct bladerf *dev,
                          bladerf_channel ch,
                          char const *stage,
                          int *gain);
    int (*set_gain_stage)(struct bladerf *dev,
                          bladerf_channel ch,
                          char const *stage,
                          int gain);

    int (*get_rssi)(struct bladerf *dev,
                    bladerf_channel ch,
                    int *pre_rssi,
                    int *sym_rssi);

    int (*get_filter)(struct bladerf *dev,
                      bladerf_channel ch,
                      bladerf_rfic_rxfir *rxfir,
                      bladerf_rfic_txfir *txfir);
    int (*set_filter)(struct bladerf *dev,
                      bladerf_channel ch,
                      bladerf_rfic_rxfir rxfir,
                      bladerf_rfic_txfir txfir);

    int (*get_txmute)(struct bladerf *dev, bladerf_channel ch, bool *state);
    int (*set_txmute)(struct bladerf *dev, bladerf_channel ch, bool state);

    int (*store_fastlock_profile)(struct bladerf *dev,
                                  bladerf_channel ch,
                                  uint32_t profile);

    enum bladerf2_rfic_command_mode const command_mode;
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

    /* RFIC configuration parameters */
    void *rfic_init_params;

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

    /* VCTCXO trim state */
    enum bladerf2_vctcxo_trim_source trim_source;
    uint16_t trimdac_last_value;   /**< saved running value */
    uint16_t trimdac_stored_value; /**< cached value read from SPI flash */

    /* Quick Tune Profile Status */
    uint16_t quick_tune_tx_profile;
    uint16_t quick_tune_rx_profile;

    /* RFIC backend command handling */
    struct controller_fns const *rfic;

    /* RFIC FIR Filter status */
    bladerf_rfic_rxfir rxfir;
    bladerf_rfic_txfir txfir;

    /* If true, RFIC control will be fully de-initialized on close, instead of
     * just put into a standby state. */
    bool rfic_reset_on_close;
};

struct bladerf_rfic_status_register {
    bool rfic_initialized;
    size_t write_queue_length;
};


/******************************************************************************/
/* Externs */
/******************************************************************************/

extern AD9361_InitParam bladerf2_rfic_init_params;
extern AD9361_InitParam bladerf2_rfic_init_params_fastagc_burst;
extern AD9361_RXFIRConfig bladerf2_rfic_rx_fir_config;
extern AD9361_TXFIRConfig bladerf2_rfic_tx_fir_config;
extern AD9361_RXFIRConfig bladerf2_rfic_rx_fir_config_dec2;
extern AD9361_TXFIRConfig bladerf2_rfic_tx_fir_config_int2;
extern AD9361_RXFIRConfig bladerf2_rfic_rx_fir_config_dec4;
extern AD9361_TXFIRConfig bladerf2_rfic_tx_fir_config_int4;
extern const float ina219_r_shunt;


/******************************************************************************/
/* Constants */
/******************************************************************************/

extern char const *bladerf2_state_to_string[4];


/******************************************************************************/
/* Macros */
/******************************************************************************/

/* Macro for logging and returning an error status. This should be used for
 * errors defined in the \ref RETCODES list. */
#define RETURN_ERROR_STATUS(_what, _status)                   \
    do {                                                      \
        log_error("%s: %s failed: %s\n", __FUNCTION__, _what, \
                  bladerf_strerror(_status));                 \
        return _status;                                       \
    } while (0)

/* Macro for converting, logging, and returning libad9361 error codes. */
#define RETURN_ERROR_AD9361(_what, _status)                           \
    do {                                                              \
        RETURN_ERROR_STATUS(_what, errno_ad9361_to_bladerf(_status)); \
    } while (0)

/* Macro for logging and returning ::BLADERF_ERR_INVAL */
#define RETURN_INVAL_ARG(_what, _arg, _why)                               \
    do {                                                                  \
        log_error("%s: %s '%s' invalid: %s\n", __FUNCTION__, _what, #_arg, \
                  _why);                                                  \
        return BLADERF_ERR_INVAL;                                         \
    } while (0)

#define RETURN_INVAL(_what, _why)                                     \
    do {                                                              \
        log_error("%s: %s invalid: %s\n", __FUNCTION__, _what, _why); \
        return BLADERF_ERR_INVAL;                                     \
    } while (0)

/**
 * @brief   Null test
 *
 * @param   _var  The variable to check
 *
 * @return  RETURN_INVAL if _var is null, continues otherwise
 */
#define NULL_CHECK(_var)                    \
    do {                                    \
        if (NULL == _var) {                 \
            RETURN_INVAL(#_var, "is null"); \
        }                                   \
    } while (0)

/**
 * @brief   Null test, with mutex unlock on failure
 *
 * @param   _var  The variable to check
 *
 * @return  RETURN_INVAL if _var is null, continues otherwise
 */
#define NULL_CHECK_LOCKED(_var)             \
    do {                                    \
        NULL_CHECK(dev);                    \
                                            \
        if (NULL == _var) {                 \
            MUTEX_UNLOCK(__lock);           \
            RETURN_INVAL(#_var, "is null"); \
        }                                   \
    } while (0)

/**
 * @brief   Board state check
 *
 * @param   _state  Minimum sufficient board state
 *
 * @return  BLADERF_ERR_NOT_INIT if board's state is less than _state, continues
 *          otherwise
 */
#define CHECK_BOARD_STATE(_state)                                         \
    do {                                                                  \
        NULL_CHECK(dev);                                                  \
        NULL_CHECK(dev->board);                                           \
                                                                          \
        struct bladerf2_board_data *_bd = dev->board_data;                \
                                                                          \
        if (_bd->state < _state) {                                        \
            log_error("%s: Board state insufficient for operation "       \
                      "(current \"%s\", requires \"%s\").\n",             \
                      __FUNCTION__, bladerf2_state_to_string[_bd->state], \
                      bladerf2_state_to_string[_state]);                  \
                                                                          \
            return BLADERF_ERR_NOT_INIT;                                  \
        }                                                                 \
    } while (0)

/**
 * @brief   Test if board is a bladeRF 2
 *
 * @param   _dev  Device handle
 *
 * @return  BLADERF_ERR_UNSUPPORTED if board is not a bladeRF 2, continues
 *          otherwise
 */
#define CHECK_BOARD_IS_BLADERF2(_dev)                                        \
    do {                                                                     \
        NULL_CHECK(_dev);                                                    \
        NULL_CHECK(_dev->board);                                             \
                                                                             \
        if (_dev->board != &bladerf2_board_fns) {                            \
            log_error("%s: Board type \"%s\" not supported\n", __FUNCTION__, \
                      _dev->board->name);                                    \
            return BLADERF_ERR_UNSUPPORTED;                                  \
        }                                                                    \
    } while (0)

/**
 * @brief   Call a function and return early if it fails
 *
 * @param   _fn   The function
 *
 * @return  function return value if less than zero; continues otherwise
 */
#define CHECK_STATUS(_fn)                  \
    do {                                   \
        int _s = _fn;                      \
        if (_s < 0) {                      \
            RETURN_ERROR_STATUS(#_fn, _s); \
        }                                  \
    } while (0)

/**
 * @brief   Call a function and, if it fails, unlock the mutex and return
 *
 * @param   _fn   The function
 *
 * @return  function return value if less than zero; continues otherwise
 */
#define CHECK_STATUS_LOCKED(_fn)           \
    do {                                   \
        int _s = _fn;                      \
        if (_s < 0) {                      \
            MUTEX_UNLOCK(__lock);          \
            RETURN_ERROR_STATUS(#_fn, _s); \
        }                                  \
    } while (0)

/**
 * @brief   Call a function and return early if it fails, with error translation
 *          from AD936x to bladeRF return codes
 *
 * @param   _fn   The function
 *
 * @return  function return value if less than zero; continues otherwise
 */
#define CHECK_AD936X(_fn)                  \
    do {                                   \
        int _s = _fn;                      \
        if (_s < 0) {                      \
            RETURN_ERROR_AD9361(#_fn, _s); \
        }                                  \
    } while (0)

/**
 * @brief   Call a function and, if it fails, unlock the mutex and return, with
 *          error translation from AD936x to bladeRF return codes
 *
 * @param   _fn   The function
 *
 * @return  function return value if less than zero; continues otherwise
 */
#define CHECK_AD936X_LOCKED(_fn)           \
    do {                                   \
        int _s = _fn;                      \
        if (_s < 0) {                      \
            MUTEX_UNLOCK(__lock);          \
            RETURN_ERROR_AD9361(#_fn, _s); \
        }                                  \
    } while (0)

/**
 * @brief   Execute a command block with a mutex lock
 *
 * @note    Variables declared within the including scope must be declared
 *          one-per-line.
 *
 * @param   _lock    Lock to hold
 * @param   _thing   Block to execute
 */
#define WITH_MUTEX(_lock, _thing) \
    do {                          \
        MUTEX *__lock = _lock;    \
                                  \
        MUTEX_LOCK(__lock);       \
        _thing;                   \
        MUTEX_UNLOCK(__lock);     \
    } while (0)

/**
 * @brief   Execute command block, conditional to _mode
 *
 * @param   _dev     Device handle
 * @param   _mode    Command mode
 * @param   _thing   Block to do if it happens
 */
#define IF_COMMAND_MODE(_dev, _mode, _thing)               \
    do {                                                   \
        NULL_CHECK(_dev);                                  \
        NULL_CHECK(_dev->board_data);                      \
                                                           \
        struct bladerf2_board_data *bd = _dev->board_data; \
                                                           \
        if (bd->rfic->command_mode == _mode) {             \
            _thing;                                        \
        };                                                 \
    } while (0)


/******************************************************************************/
/* Functions */
/******************************************************************************/

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
int perform_format_config(struct bladerf *dev,
                          bladerf_direction dir,
                          bladerf_format format);

/**
 * Deconfigure and update any state pertaining what a format that a stream
 * direction is no longer using.
 *
 * @param       dev     Device handle
 * @param[in]   dir     Direction that is currently being deconfigured
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int perform_format_deconfig(struct bladerf *dev, bladerf_direction dir);

bool is_valid_fpga_size(struct bladerf *dev,
                        bladerf_fpga_size fpga,
                        size_t len);

bool is_valid_fw_size(size_t len);

bladerf_tuning_mode default_tuning_mode(struct bladerf *dev);

bool check_total_sample_rate(struct bladerf *dev);

bool does_rffe_dir_have_enabled_ch(uint32_t reg, bladerf_direction dir);

int get_gain_offset(struct bladerf *dev, bladerf_channel ch, float *offset);

#endif  // BLADERF2_COMMON_H_
