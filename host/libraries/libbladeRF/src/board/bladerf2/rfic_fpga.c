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

#include <libbladeRF.h>

#include "board/board.h"
#include "common.h"
#include "conversions.h"
#include "iterators.h"
#include "log.h"

// #define BLADERF_HEADLESS_C_DEBUG


/******************************************************************************/
/* Declarations */
/******************************************************************************/

/**
 * @brief       RFIC Command Read
 *
 * Executes a command using the FPGA-based RFIC interface, returning the reply
 * value in *data.
 *
 * @param       dev     Device handle
 * @param[in]   ch      Channel to act upon; use ::BLADERF_CHANNEL_INVALID for
 *                      non-channel-specific commands
 * @param[in]   cmd     Command
 * @param[out]  data    Pointer for response data
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
static int _rfic_cmd_read(struct bladerf *dev,
                          bladerf_channel ch,
                          bladerf_rfic_command cmd,
                          uint64_t *data);

/**
 * @brief       RFIC Command Write
 *
 * Executes a command using the FPGA-based RFIC interface, using the supplied
 * data value as an argument. Waits until the command has been executed before
 * returning.
 *
 * @param       dev     Device handle
 * @param[in]   ch      Channel to act upon; use ::BLADERF_CHANNEL_INVALID for
 *                      non-channel-specific commands
 * @param[in]   cmd     Command
 * @param[in]   data    Argument for command
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
static int _rfic_cmd_write(struct bladerf *dev,
                           bladerf_channel ch,
                           bladerf_rfic_command cmd,
                           uint64_t data);


/******************************************************************************/
/* Helpers */
/******************************************************************************/

/* Build RFIC address from bladerf_rfic_command and bladerf_channel */
#define RFIC_ADDRESS(cmd, ch) ((cmd & 0xFF) + ((ch & 0xF) << 8))

static int _rfic_fpga_get_status(
    struct bladerf *dev, struct bladerf_rfic_status_register *rfic_status)
{
    uint64_t sreg = 0;
    int status;

    status = _rfic_cmd_read(dev, BLADERF_CHANNEL_INVALID,
                            BLADERF_RFIC_COMMAND_STATUS, &sreg);

    rfic_status->rfic_initialized   = ((sreg >> 0) & 0x1);
    rfic_status->write_queue_length = ((sreg >> 8) & 0xFF);

    return status;
}

static int _rfic_fpga_get_status_wqlen(struct bladerf *dev)
{
    struct bladerf_rfic_status_register rfic_status;
    int status;

    status = _rfic_fpga_get_status(dev, &rfic_status);
    if (status < 0) {
        return status;
    }

#ifdef BLADERF_HEADLESS_C_DEBUG
    if (rfic_status.write_queue_length > 0) {
        log_verbose("%s: queue len = %d\n", __FUNCTION__,
                    rfic_status.write_queue_length);
    }
#endif

    return (int)rfic_status.write_queue_length;
}

static int _rfic_fpga_spinwait(struct bladerf *dev)
{
    size_t const TRIES       = 30;
    unsigned int const DELAY = 100;
    size_t count             = 0;
    int jobs;

    /* Poll the CPU and spin until the job has been completed. */
    do {
        jobs = _rfic_fpga_get_status_wqlen(dev);
        if (0 != jobs) {
            usleep(DELAY);
        }
    } while ((0 != jobs) && (++count < TRIES));

    /* If it's simply taking too long to dequeue the command, status will
     * have the number of items in the queue. Bonk this down to a timeout. */
    if (jobs > 0) {
        jobs = BLADERF_ERR_TIMEOUT;
    }

    return jobs;
}


/******************************************************************************/
/* Low level RFIC Accessors */
/******************************************************************************/

static int _rfic_cmd_read(struct bladerf *dev,
                          bladerf_channel ch,
                          bladerf_rfic_command cmd,
                          uint64_t *data)
{
    return dev->backend->rfic_command_read(dev, RFIC_ADDRESS(cmd, ch), data);
}

static int _rfic_cmd_write(struct bladerf *dev,
                           bladerf_channel ch,
                           bladerf_rfic_command cmd,
                           uint64_t data)
{
    /* Perform the write command. */
    CHECK_STATUS(
        dev->backend->rfic_command_write(dev, RFIC_ADDRESS(cmd, ch), data));

    /* Block until the job has been completed. */
    return _rfic_fpga_spinwait(dev);
}


/******************************************************************************/
/* Initialization */
/******************************************************************************/

static bool _rfic_fpga_is_present(struct bladerf *dev)
{
    uint64_t sreg = 0;
    int status;

    status = _rfic_cmd_read(dev, BLADERF_CHANNEL_INVALID,
                            BLADERF_RFIC_COMMAND_STATUS, &sreg);
    if (status < 0) {
        return false;
    }

    return true;
}

static int _rfic_fpga_get_init_state(struct bladerf *dev,
                                     bladerf_rfic_init_state *state)
{
    uint64_t data;

    CHECK_STATUS(_rfic_cmd_read(dev, BLADERF_CHANNEL_INVALID,
                                BLADERF_RFIC_COMMAND_INIT, &data));

    *state = (bladerf_rfic_init_state)data;

    return 0;
}

static bool _rfic_fpga_is_initialized(struct bladerf *dev)
{
    bladerf_rfic_init_state state;
    int status;

    status = _rfic_fpga_get_init_state(dev, &state);
    if (status < 0) {
        log_error("%s: failed to get RFIC initialization state: %s\n",
                  __FUNCTION__, bladerf_strerror(status));
        return false;
    }

    return BLADERF_RFIC_INIT_STATE_ON == state;
}

static bool _rfic_fpga_is_standby(struct bladerf *dev)
{
    bladerf_rfic_init_state state;
    int status;

    status = _rfic_fpga_get_init_state(dev, &state);
    if (status < 0) {
        log_error("%s: failed to get RFIC initialization state: %s\n",
                  __FUNCTION__, bladerf_strerror(status));
        return false;
    }

    return BLADERF_RFIC_INIT_STATE_STANDBY == state;
}

static int _rfic_fpga_initialize(struct bladerf *dev)
{
    log_debug("%s: initializing\n", __FUNCTION__);

    return _rfic_cmd_write(dev, BLADERF_CHANNEL_INVALID,
                           BLADERF_RFIC_COMMAND_INIT,
                           BLADERF_RFIC_INIT_STATE_ON);
}

static int _rfic_fpga_standby(struct bladerf *dev)
{
    log_debug("%s: entering standby mode\n", __FUNCTION__);

    return _rfic_cmd_write(dev, BLADERF_CHANNEL_INVALID,
                           BLADERF_RFIC_COMMAND_INIT,
                           BLADERF_RFIC_INIT_STATE_STANDBY);
}

static int _rfic_fpga_deinitialize(struct bladerf *dev)
{
    log_debug("%s: deinitializing\n", __FUNCTION__);

    return _rfic_cmd_write(dev, BLADERF_CHANNEL_INVALID,
                           BLADERF_RFIC_COMMAND_INIT,
                           BLADERF_RFIC_INIT_STATE_OFF);
}


/******************************************************************************/
/* Enable */
/******************************************************************************/

static int _rfic_fpga_enable_module(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bool ch_enable)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    struct controller_fns const *rfic      = board_data->rfic;
    bladerf_direction dir = BLADERF_CHANNEL_IS_TX(ch) ? BLADERF_TX : BLADERF_RX;
    uint32_t reg;     /* RFFE register value */
    bool ch_enabled;  /* Channel: initial state */
    bool ch_pending;  /* Channel: target state is not initial state */
    bool dir_enabled; /* Direction: initial state */
    bool dir_enable;  /* Direction: target state */
    bool dir_pending; /* Direction: target state is not initial state */
    bool be_toggle;   /* Backend: toggle off/on to reset backend FIFO */
    bool be_teardown; /* Backend: disable backend module */
    bool be_setup;    /* Backend: enable backend module */

    /* Read RFFE control register */
    CHECK_STATUS(dev->backend->rffe_control_read(dev, &reg));

#ifdef BLADERF_HEADLESS_C_DEBUG
    uint32_t reg_old = reg;
#endif

    /* Calculate initial and target states */
    ch_enabled  = _rffe_ch_enabled(reg, ch);
    dir_enabled = _rffe_dir_enabled(reg, dir);
    dir_enable  = ch_enable || _rffe_dir_otherwise_enabled(reg, ch);
    ch_pending  = ch_enabled != ch_enable;
    dir_pending = dir_enabled != dir_enable;
    be_toggle   = !BLADERF_CHANNEL_IS_TX(ch) && ch_enable && !dir_pending;
    be_setup    = be_toggle || (dir_pending && dir_enable);
    be_teardown = be_toggle || (dir_pending && !dir_enable);

    /* Perform Direction Teardown */
    if (dir_pending && !dir_enable) {
        sync_deinit(&board_data->sync[dir]);
        perform_format_deconfig(dev, dir);
    }

    /* Perform Channel Setup/Teardown */
    if (ch_pending) {
        /* Set/unset TX mute */
        if (BLADERF_CHANNEL_IS_TX(ch)) {
            CHECK_STATUS(rfic->set_txmute(dev, ch, !ch_enable));
        }

        /* Execute RFIC enable command. */
        CHECK_STATUS(_rfic_cmd_write(dev, ch, BLADERF_RFIC_COMMAND_ENABLE,
                                     ch_enable ? 1 : 0));
    }

    /* Perform backend teardown */
    if (be_teardown) {
        CHECK_STATUS(dev->backend->enable_module(dev, dir, false));
    }

    /* Perform backend setup */
    if (be_setup) {
        CHECK_STATUS(dev->backend->enable_module(dev, dir, true));
    }

    /* Warn the user if the sample rate isn't reasonable */
    if (ch_pending && ch_enable) {
        check_total_sample_rate(dev);
    }

#ifdef BLADERF_HEADLESS_C_DEBUG
    /* Debug output... */
    if (BLADERF_LOG_LEVEL_VERBOSE == log_get_verbosity()) {
        CHECK_STATUS(dev->backend->rffe_control_read(dev, &reg));

        log_verbose(
            "%s: %s ch[en=%d->%d pend=%d] dir[en=%d->%d pend=%d] be[clr=%d "
            "su=%d td=%d] reg=0x%08x->0x%08x\n",
            __FUNCTION__, channel2str(ch), ch_enabled, ch_enable, ch_pending,
            dir_enabled, dir_enable, dir_pending, be_toggle, be_setup,
            be_teardown, reg_old, reg);
    }
#endif

    return 0;
}


/******************************************************************************/
/* Sample rate */
/******************************************************************************/

static int _rfic_fpga_get_sample_rate(struct bladerf *dev,
                                      bladerf_channel ch,
                                      bladerf_sample_rate *rate)
{
    uint64_t readval;

    CHECK_STATUS(
        _rfic_cmd_read(dev, ch, BLADERF_RFIC_COMMAND_SAMPLERATE, &readval));

    *rate = (uint32_t)readval;

    return 0;
}

static int _rfic_fpga_set_sample_rate(struct bladerf *dev,
                                      bladerf_channel ch,
                                      bladerf_sample_rate rate)
{
    return _rfic_cmd_write(dev, ch, BLADERF_RFIC_COMMAND_SAMPLERATE, rate);
}


/******************************************************************************/
/* Frequency */
/******************************************************************************/

static int _rfic_fpga_get_frequency(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bladerf_frequency *frequency)
{
    bladerf_frequency freq;

    CHECK_STATUS(
        _rfic_cmd_read(dev, ch, BLADERF_RFIC_COMMAND_FREQUENCY, &freq));

    *frequency = freq;

    return 0;
}

static int _rfic_fpga_set_frequency(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bladerf_frequency frequency)
{
    struct bladerf_range const *range = NULL;

    CHECK_STATUS(dev->board->get_frequency_range(dev, ch, &range));

    if (!is_within_range(range, frequency)) {
        return BLADERF_ERR_RANGE;
    }

    return _rfic_cmd_write(dev, ch, BLADERF_RFIC_COMMAND_FREQUENCY, frequency);
}

static int _rfic_fpga_select_band(struct bladerf *dev,
                                  bladerf_channel ch,
                                  bladerf_frequency frequency)
{
    // This functionality is unnecessary with the headless architecture.
    return 0;
}


/******************************************************************************/
/* Bandwidth */
/******************************************************************************/

static int _rfic_fpga_get_bandwidth(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bladerf_bandwidth *bandwidth)
{
    uint64_t readval;

    CHECK_STATUS(
        _rfic_cmd_read(dev, ch, BLADERF_RFIC_COMMAND_BANDWIDTH, &readval));

    *bandwidth = (uint32_t)readval;

    return 0;
}

static int _rfic_fpga_set_bandwidth(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bladerf_bandwidth bandwidth,
                                    bladerf_bandwidth *actual)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    struct controller_fns const *rfic      = board_data->rfic;
    struct bladerf_range const *range      = NULL;

    CHECK_STATUS(dev->board->get_bandwidth_range(dev, ch, &range));

    if (!is_within_range(range, bandwidth)) {
        return BLADERF_ERR_RANGE;
    }

    CHECK_STATUS(
        _rfic_cmd_write(dev, ch, BLADERF_RFIC_COMMAND_BANDWIDTH, bandwidth));

    if (actual != NULL) {
        return rfic->get_bandwidth(dev, ch, actual);
    }

    return 0;
}


/******************************************************************************/
/* Gain */
/* These functions handle offset */
/******************************************************************************/

static int _rfic_fpga_get_gain(struct bladerf *dev,
                               bladerf_channel ch,
                               int *gain)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    struct controller_fns const *rfic      = board_data->rfic;
    char const *stage = BLADERF_CHANNEL_IS_TX(ch) ? "dsa" : "full";
    int val;
    float offset;

    CHECK_STATUS(get_gain_offset(dev, ch, &offset));

    CHECK_STATUS(rfic->get_gain_stage(dev, ch, stage, &val));

    *gain = __round_int(val + offset);

    return 0;
}

static int _rfic_fpga_set_gain(struct bladerf *dev,
                               bladerf_channel ch,
                               int gain)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    struct controller_fns const *rfic      = board_data->rfic;
    char const *stage = BLADERF_CHANNEL_IS_TX(ch) ? "dsa" : "full";
    float offset;

    CHECK_STATUS(get_gain_offset(dev, ch, &offset));

    return rfic->set_gain_stage(dev, ch, stage, __round_int(gain - offset));
}


/******************************************************************************/
/* Gain mode */
/******************************************************************************/

static int _rfic_fpga_get_gain_mode(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bladerf_gain_mode *mode)
{
    uint64_t readval;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        log_warning("%s: automatic gain control not valid for TX channels\n",
                    __FUNCTION__);
        *mode = BLADERF_GAIN_DEFAULT;
        return 0;
    }

    CHECK_STATUS(
        _rfic_cmd_read(dev, ch, BLADERF_RFIC_COMMAND_GAINMODE, &readval));

    *mode = (bladerf_gain_mode)readval;

    return 0;
}

static int _rfic_fpga_set_gain_mode(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bladerf_gain_mode mode)
{
    if (BLADERF_CHANNEL_IS_TX(ch)) {
        log_warning("%s: automatic gain control not valid for TX channels\n",
                    __FUNCTION__);
        return 0;
    }

    return _rfic_cmd_write(dev, ch, BLADERF_RFIC_COMMAND_GAINMODE, mode);
}


/******************************************************************************/
/* Gain stage */
/* These functions handle scaling */
/******************************************************************************/

static int _rfic_fpga_get_gain_stage(struct bladerf *dev,
                                     bladerf_channel ch,
                                     char const *stage,
                                     int *gain)
{
    struct bladerf_range const *range = NULL;
    uint64_t val;

    if ((BLADERF_CHANNEL_IS_TX(ch) && strcmp(stage, "dsa") != 0) ||
        (!BLADERF_CHANNEL_IS_TX(ch) && strcmp(stage, "full") != 0)) {
        log_error("%s: unknown gain stage '%s'\n", __FUNCTION__, stage);
        return BLADERF_ERR_INVAL;
    }

    CHECK_STATUS(dev->board->get_gain_stage_range(dev, ch, stage, &range));

    CHECK_STATUS(_rfic_cmd_read(dev, ch, BLADERF_RFIC_COMMAND_GAIN, &val));

    *gain = __unscale_int(range, val);

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        *gain = -(*gain);
    }

    return 0;
}

static int _rfic_fpga_set_gain_stage(struct bladerf *dev,
                                     bladerf_channel ch,
                                     char const *stage,
                                     int gain)
{
    struct bladerf_range const *range = NULL;
    int64_t val;

    if ((BLADERF_CHANNEL_IS_TX(ch) && strcmp(stage, "dsa") != 0) ||
        (!BLADERF_CHANNEL_IS_TX(ch) && strcmp(stage, "full") != 0)) {
        log_error("%s: unknown gain stage '%s'\n", __FUNCTION__, stage);
        return BLADERF_ERR_INVAL;
    }

    CHECK_STATUS(dev->board->get_gain_stage_range(dev, ch, stage, &range));

    if (BLADERF_CHANNEL_IS_TX(ch) && gain < -89) {
        val = -89750;
    } else {
        val = __scale_int64(range, clamp_to_range(range, gain));
    }

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        val = -val;
    }

    return _rfic_cmd_write(dev, ch, BLADERF_RFIC_COMMAND_GAIN, (uint64_t)val);
}


/******************************************************************************/
/* RSSI */
/******************************************************************************/

static int _rfic_fpga_get_rssi(struct bladerf *dev,
                               bladerf_channel ch,
                               int *pre_rssi,
                               int *sym_rssi)
{
    uint64_t readval;
    int16_t mult, pre, sym;

    CHECK_STATUS(_rfic_cmd_read(dev, ch, BLADERF_RFIC_COMMAND_RSSI, &readval));

    mult = (int16_t)((readval >> BLADERF_RFIC_RSSI_MULT_SHIFT) &
                     BLADERF_RFIC_RSSI_MULT_MASK);
    pre  = (int16_t)((readval >> BLADERF_RFIC_RSSI_PRE_SHIFT) &
                    BLADERF_RFIC_RSSI_PRE_MASK);
    sym  = (int16_t)((readval >> BLADERF_RFIC_RSSI_SYM_SHIFT) &
                    BLADERF_RFIC_RSSI_SYM_MASK);

    *pre_rssi = __round_int(pre / (float)mult);
    *sym_rssi = __round_int(sym / (float)mult);

    return 0;
}


/******************************************************************************/
/* Filter */
/******************************************************************************/

static int _rfic_fpga_get_filter(struct bladerf *dev,
                                 bladerf_channel ch,
                                 bladerf_rfic_rxfir *rxfir,
                                 bladerf_rfic_txfir *txfir)
{
    uint64_t readval;

    CHECK_STATUS(
        _rfic_cmd_read(dev, ch, BLADERF_RFIC_COMMAND_FILTER, &readval));

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        if (NULL != txfir) {
            *txfir = (bladerf_rfic_txfir)readval;
        } else {
            return BLADERF_ERR_INVAL;
        }
    } else {
        if (NULL != rxfir) {
            *rxfir = (bladerf_rfic_rxfir)readval;
        } else {
            return BLADERF_ERR_INVAL;
        }
    }

    return 0;
}

static int _rfic_fpga_set_filter(struct bladerf *dev,
                                 bladerf_channel ch,
                                 bladerf_rfic_rxfir rxfir,
                                 bladerf_rfic_txfir txfir)
{
    uint64_t data;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        data = (uint64_t)txfir;
    } else {
        data = (uint64_t)rxfir;
    }

    return _rfic_cmd_write(dev, ch, BLADERF_RFIC_COMMAND_FILTER, data);
}


/******************************************************************************/
/* TX Mute */
/******************************************************************************/

static int _rfic_fpga_get_txmute(struct bladerf *dev,
                                 bladerf_channel ch,
                                 bool *state)
{
    if (BLADERF_CHANNEL_IS_TX(ch)) {
        uint64_t readval;

        CHECK_STATUS(
            _rfic_cmd_read(dev, ch, BLADERF_RFIC_COMMAND_TXMUTE, &readval));

        return (readval > 0);
    }

    return BLADERF_ERR_UNSUPPORTED;
}

static int _rfic_fpga_set_txmute(struct bladerf *dev,
                                 bladerf_channel ch,
                                 bool state)
{
    if (BLADERF_CHANNEL_IS_TX(ch)) {
        return _rfic_cmd_write(dev, ch, BLADERF_RFIC_COMMAND_TXMUTE,
                               state ? 1 : 0);
    }

    return BLADERF_ERR_UNSUPPORTED;
}


/******************************************************************************/
/* Fastlock */
/******************************************************************************/

static int _rfic_fpga_store_fastlock_profile(struct bladerf *dev,
                                             bladerf_channel ch,
                                             uint32_t profile)
{
    return _rfic_cmd_write(dev, ch, BLADERF_RFIC_COMMAND_FASTLOCK, profile);
}


/******************************************************************************/
/* Function pointers */
/******************************************************************************/

struct controller_fns const rfic_fpga_control = {
    FIELD_INIT(.is_present, _rfic_fpga_is_present),
    FIELD_INIT(.is_standby, _rfic_fpga_is_standby),
    FIELD_INIT(.is_initialized, _rfic_fpga_is_initialized),
    FIELD_INIT(.get_init_state, _rfic_fpga_get_init_state),

    FIELD_INIT(.initialize, _rfic_fpga_initialize),
    FIELD_INIT(.standby, _rfic_fpga_standby),
    FIELD_INIT(.deinitialize, _rfic_fpga_deinitialize),

    FIELD_INIT(.enable_module, _rfic_fpga_enable_module),

    FIELD_INIT(.get_sample_rate, _rfic_fpga_get_sample_rate),
    FIELD_INIT(.set_sample_rate, _rfic_fpga_set_sample_rate),

    FIELD_INIT(.get_frequency, _rfic_fpga_get_frequency),
    FIELD_INIT(.set_frequency, _rfic_fpga_set_frequency),
    FIELD_INIT(.select_band, _rfic_fpga_select_band),

    FIELD_INIT(.get_bandwidth, _rfic_fpga_get_bandwidth),
    FIELD_INIT(.set_bandwidth, _rfic_fpga_set_bandwidth),

    FIELD_INIT(.get_gain_mode, _rfic_fpga_get_gain_mode),
    FIELD_INIT(.set_gain_mode, _rfic_fpga_set_gain_mode),

    FIELD_INIT(.get_gain, _rfic_fpga_get_gain),
    FIELD_INIT(.set_gain, _rfic_fpga_set_gain),

    FIELD_INIT(.get_gain_stage, _rfic_fpga_get_gain_stage),
    FIELD_INIT(.set_gain_stage, _rfic_fpga_set_gain_stage),

    FIELD_INIT(.get_rssi, _rfic_fpga_get_rssi),

    FIELD_INIT(.get_filter, _rfic_fpga_get_filter),
    FIELD_INIT(.set_filter, _rfic_fpga_set_filter),

    FIELD_INIT(.get_txmute, _rfic_fpga_get_txmute),
    FIELD_INIT(.set_txmute, _rfic_fpga_set_txmute),

    FIELD_INIT(.store_fastlock_profile, _rfic_fpga_store_fastlock_profile),

    FIELD_INIT(.command_mode, RFIC_COMMAND_FPGA),
};
