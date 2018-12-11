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

#ifndef BLADERF_NIOS_PC_SIMULATION

#include "bladerf2_common.h"
#include "debug.h"
#include "devices.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef BLADERF_NIOS_LIBAD936X

#include "ad936x.h"
#include "ad936x_helpers.h"
#include "devices_rfic.h"
#include "pkt_retune2.h"

extern AD9361_InitParam bladerf2_rfic_init_params;
extern AD9361_RXFIRConfig bladerf2_rfic_rx_fir_config;
extern AD9361_TXFIRConfig bladerf2_rfic_tx_fir_config;
extern AD9361_RXFIRConfig bladerf2_rfic_rx_fir_config_dec2;
extern AD9361_TXFIRConfig bladerf2_rfic_tx_fir_config_int2;
extern AD9361_RXFIRConfig bladerf2_rfic_rx_fir_config_dec4;
extern AD9361_TXFIRConfig bladerf2_rfic_tx_fir_config_int4;

static struct rfic_state state;

/* Uncomment to enable detailed debugging */
// #define BLADERF_RFIC_LOTS_OF_DEBUG


/******************************************************************************/
/* Helpers */
/******************************************************************************/

static bool _rfic_cmd_wr_frequency(bladerf_channel channel, uint64_t frequency);
static bool _rfic_cmd_wr_filter(bladerf_channel channel, uint64_t data);
static bool _rfic_cmd_wr_samplerate(bladerf_channel channel, uint64_t rate);

#define CHECK_BOOL(_fn)        \
    do {                       \
        int s = _fn;           \
        if (s < 0) {           \
            RFIC_ERR(#_fn, s); \
            return false;      \
        }                      \
    } while (0)

static inline bool _is_initialized(struct ad9361_rf_phy *phy)
{
    return BLADERF_RFIC_INIT_STATE_ON == state.init_state;
}

static void _rfic_clear_rffe_control()
{
    uint32_t reg;

    reg = rffe_csr_read();
    reg &= ~(1 << RFFE_CONTROL_TXNRX);
    reg &= ~(1 << RFFE_CONTROL_ENABLE);
    reg &= ~(1 << RFFE_CONTROL_RX_SPDT_1);
    reg &= ~(1 << RFFE_CONTROL_RX_SPDT_2);
    reg &= ~(1 << RFFE_CONTROL_TX_SPDT_1);
    reg &= ~(1 << RFFE_CONTROL_TX_SPDT_2);
    reg &= ~(1 << RFFE_CONTROL_MIMO_RX_EN_0);
    reg &= ~(1 << RFFE_CONTROL_MIMO_TX_EN_0);
    reg &= ~(1 << RFFE_CONTROL_MIMO_RX_EN_1);
    reg &= ~(1 << RFFE_CONTROL_MIMO_TX_EN_1);
    rffe_csr_write(reg);
}

static bool _rfic_deinitialize()
{
    uint32_t reg;

    /* Unset RFFE bits controlling RFIC */
    _rfic_clear_rffe_control();

    if (NULL != state.phy) {
        /* Deinitialize AD9361 */
        CHECK_BOOL(ad9361_deinit(state.phy));
        state.phy = NULL;
    } else {
        DBG("%s: state.phy already null?\n", __FUNCTION__);
    }

    reg = rffe_csr_read();
    reg &= ~(1 << RFFE_CONTROL_RESET_N);
    rffe_csr_write(reg);

    state.init_state = BLADERF_RFIC_INIT_STATE_OFF;

    DBG("*** RFIC Control Deinitialized ***\n");

    return true;
}

static bool _rfic_initialize()
{
    AD9361_InitParam *init_param = &bladerf2_rfic_init_params;
    bladerf_direction dir;
    bladerf_channel ch;
    uint32_t reg;
    size_t i;

    /* Unset RFFE bits controlling RFIC */
    _rfic_clear_rffe_control();

    if (NULL == state.phy) {
        /* Initialize AD9361 */
        state.init_state = BLADERF_RFIC_INIT_STATE_OFF;

        DBG("--- Initializing AD9361 ---\n");

        reg = rffe_csr_read();
        reg &= ~(1 << RFFE_CONTROL_RESET_N);
        rffe_csr_write(reg);

        usleep(1000);

        reg = rffe_csr_read();
        reg |= 1 << RFFE_CONTROL_RESET_N;
        rffe_csr_write(reg);

        CHECK_BOOL(ad9361_init(&state.phy, init_param, NULL));
    }

    if (NULL == state.phy) {
        /* Oh no */
        DBG("%s: ad9361_init failed silently\n", __FUNCTION__);
        state.init_state = BLADERF_RFIC_INIT_STATE_OFF;
        return false;
    }

    /* Per-module initialization */
    FOR_EACH_DIRECTION(dir)
    {
        FOR_EACH_CHANNEL(dir, 1, i, ch)
        {
            if (BLADERF_RFIC_INIT_STATE_OFF == state.init_state) {
                bladerf_frequency init_freq;
                bladerf_sample_rate init_samprate;

                if (BLADERF_CHANNEL_IS_TX(ch)) {
                    init_freq     = init_param->tx_synthesizer_frequency_hz;
                    init_samprate = init_param->tx_path_clock_frequencies[5];
                } else {
                    init_freq     = init_param->rx_synthesizer_frequency_hz;
                    init_samprate = init_param->rx_path_clock_frequencies[5];
                }

                // why? i don't know
                init_freq >>= 32;

                CHECK_BOOL(_rfic_cmd_wr_frequency(ch, RESET_FREQUENCY));

                CHECK_BOOL(_rfic_cmd_wr_filter(
                    ch, (BLADERF_TX == dir) ? BLADERF_RFIC_TXFIR_DEFAULT
                                            : BLADERF_RFIC_RXFIR_DEFAULT));

                CHECK_BOOL(_rfic_cmd_wr_samplerate(ch, init_samprate));

                CHECK_BOOL(_rfic_cmd_wr_frequency(ch, init_freq));
            }
        }
    }

    /* Per-channel initialization */
    FOR_EACH_CHANNEL(BLADERF_TX, 2, i, ch)
    {
        if (BLADERF_RFIC_INIT_STATE_OFF == state.init_state) {
            CHECK_BOOL(txmute_set_cached(
                state.phy, BLADERF_CHANNEL_TX(i),
                bladerf2_rfic_init_params.tx_attenuation_mdB));
            CHECK_BOOL(txmute_set(state.phy, BLADERF_CHANNEL_TX(i), false));
        }

        if (BLADERF_RFIC_INIT_STATE_STANDBY == state.init_state) {
            CHECK_BOOL(txmute_set(state.phy, BLADERF_CHANNEL_TX(i),
                                  (state.tx_mute_state[i])));
        }
    }

    state.init_state = BLADERF_RFIC_INIT_STATE_ON;

    DBG("*** RFIC Control Initialized ***\n");

    return true;
}

static bool _rfic_standby()
{
    AD9361_InitParam *init_param = &bladerf2_rfic_init_params;
    size_t i;
    bladerf_direction dir;

    /* Unset RFFE bits controlling RFIC */
    _rfic_clear_rffe_control();

    if (NULL == state.phy) {
        /* We weren't initialized to begin with, apparently. */
        state.init_state = BLADERF_RFIC_INIT_STATE_OFF;
    } else {
        /* Cache current txmute state and put it aside */
        for (i = 0; i < 2; ++i) {
            CHECK_BOOL(txmute_get(state.phy, BLADERF_CHANNEL_TX(i),
                                  &state.tx_mute_state[i]));
            CHECK_BOOL(txmute_set(state.phy, BLADERF_CHANNEL_TX(i), true));
        }

        /* Reset frequency if it's invalid */
        FOR_EACH_DIRECTION(dir)
        {
            if (state.frequency_invalid[dir]) {
                if (BLADERF_TX == dir) {
                    CHECK_BOOL(_rfic_cmd_wr_frequency(
                        BLADERF_CHANNEL_TX(0),
                        init_param->tx_synthesizer_frequency_hz));
                } else {
                    CHECK_BOOL(_rfic_cmd_wr_frequency(
                        BLADERF_CHANNEL_RX(0),
                        init_param->rx_synthesizer_frequency_hz));
                }
            }
        }
    }

    state.init_state = BLADERF_RFIC_INIT_STATE_STANDBY;

    DBG("*** RFIC Control Standby ***\n");

    return true;
}


/******************************************************************************/
/* Command implementations */
/******************************************************************************/

static bool _rfic_cmd_rd_status(bladerf_channel channel, uint64_t *data)
{
    *data = ((_is_initialized(state.phy) & BLADERF_RFIC_STATUS_INIT_MASK)
             << BLADERF_RFIC_STATUS_INIT_SHIFT) |

            ((state.write_queue.count & BLADERF_RFIC_STATUS_WQLEN_MASK)
             << BLADERF_RFIC_STATUS_WQLEN_SHIFT) |

            ((state.write_queue.last_rv & BLADERF_RFIC_STATUS_WQSUCCESS_MASK)
             << BLADERF_RFIC_STATUS_WQSUCCESS_SHIFT);

    return true;
}

/**
 * @brief      Initialize the RFIC
 *
 * @param      data     0 to de-initialize, > 0 to initialize
 *
 * @return     true if successful, false if not
 */
static bool _rfic_cmd_wr_init(bladerf_channel channel, uint64_t data)
{
    bladerf_rfic_init_state new_state = (bladerf_rfic_init_state)data;

    if (state.init_state == new_state) {
        DBG("%s: already in state %x\n", __FUNCTION__, state.init_state);
        return true;
    }

    switch (new_state) {
        case BLADERF_RFIC_INIT_STATE_OFF:
            return _rfic_deinitialize();

        case BLADERF_RFIC_INIT_STATE_ON:
            return _rfic_initialize();

        case BLADERF_RFIC_INIT_STATE_STANDBY:
            return _rfic_standby();
    }

    return false;
}

static bool _rfic_cmd_rd_init(bladerf_channel channel, uint64_t *data)
{
    *data = (uint64_t)state.init_state;
    return true;
}

/**
 * @brief      Set the enabled status for a given channel
 *
 * @param[in]  ch    Channel
 * @param[in]  data  0 to disable, >= 1 to enable
 *
 * @return     true if successful, false otherwise
 */
static bool _rfic_cmd_wr_enable(bladerf_channel ch, uint64_t data)
{
    bladerf_direction dir = BLADERF_CHANNEL_IS_TX(ch) ? BLADERF_TX : BLADERF_RX;
    uint32_t reg;     /* RFFE register value */
    bool ch_enabled;  /* Channel: initial state */
    bool ch_enable;   /* Channel: target state */
    bool ch_pending;  /* Channel: target state is not initial state */
    bool dir_enabled; /* Direction: initial state */
    bool dir_enable;  /* Direction: target state */
    bool dir_pending; /* Direction: target state is not initial state */

    bladerf_channel subch;
    bladerf_frequency freq = 0;

    /* Verify that this is not a wildcard channel */
    if (_rfic_chan_is_system(ch)) {
        return false;
    }

    /* Read RFFE control register */
    reg = rffe_csr_read();

#ifdef BLADERF_RFIC_LOTS_OF_DEBUG
    uint32_t reg_old = reg;
#endif

    /* Pre-compute useful values */
    /* Calculate initial and target states */
    ch_enabled  = _rffe_ch_enabled(reg, ch);
    dir_enabled = _rffe_dir_enabled(reg, dir);
    ch_enable   = (data > 0);
    dir_enable  = ch_enable || _rffe_dir_otherwise_enabled(reg, ch);
    ch_pending  = ch_enabled != ch_enable;
    dir_pending = dir_enabled != dir_enable;

    /* Query the current frequency if we are enabling this channel */
    if (ch_enable && (ch_pending || dir_pending)) {
        if (BLADERF_CHANNEL_IS_TX(ch)) {
            CHECK_BOOL(ad9361_get_tx_lo_freq(state.phy, &freq));
        } else {
            CHECK_BOOL(ad9361_get_rx_lo_freq(state.phy, &freq));
        }
    }

    /* Channel Setup/Teardown */
    if (ch_pending) {
        /* Modify SPDT bits */
        CHECK_BOOL(_modify_spdt_bits_by_freq(&reg, ch, ch_enable, freq));

        /* Modify MIMO channel enable bits */
        if (ch_enable) {
            reg |= (1 << _get_rffe_control_bit_for_ch(ch));
        } else {
            reg &= ~(1 << _get_rffe_control_bit_for_ch(ch));
        }
    }

    /* Direction Setup/Teardown */
    if (dir_pending) {
        struct band_port_map const *port_map;

        /* Modify ENABLE/TXNRX bits */
        if (dir_enable) {
            reg |= (1 << _get_rffe_control_bit_for_dir(dir));
        } else {
            size_t i;

            FOR_EACH_CHANNEL(dir, 2, i, subch)
            {
                CHECK_BOOL(_modify_spdt_bits_by_freq(&reg, subch, false, 0));
            }

            reg &= ~(1 << _get_rffe_control_bit_for_dir(dir));
        }

        /* Select RFIC port */
        /* Look up the port configuration for this frequency */
        port_map = _get_band_port_map_by_freq(ch, ch_enable ? freq : 0);
        if (NULL == port_map) {
            return false;
        }

        /* Set the AD9361 port accordingly */
        if (BLADERF_CHANNEL_IS_TX(ch)) {
            CHECK_BOOL(
                ad9361_set_tx_rf_port_output(state.phy, port_map->rfic_port));
        } else {
            CHECK_BOOL(
                ad9361_set_rx_rf_port_input(state.phy, port_map->rfic_port));
        }
    }

#ifdef BLADERF_RFIC_LOTS_OF_DEBUG
    DBG("%s: ch=%x dir=%x ch_en=%x ch_pend=%x dir_en=%x dir_pend=%x "
        "reg=%x->%x\n",
        __FUNCTION__, ch, dir, ch_enable, ch_pending, dir_enable, dir_pending,
        reg_old, reg);
#endif

    /* Write RFFE control register */
    rffe_csr_write(reg);

    return true;
}

/**
 * @brief      Get the enabled status for a given channel
 *
 * @param[in]  ch    Channel
 * @param[out] data  Pointer to return val. 0 if disabled, 1 if enabled
 *
 * @return     true if successful, false otherwise
 */
static bool _rfic_cmd_rd_enable(bladerf_channel ch, uint64_t *data)
{
    if (_rfic_chan_is_system(ch)) {
        return false;
    }

    *data = _rffe_ch_enabled(rffe_csr_read(), ch) ? 1 : 0;

    return true;
}

/**
 * @brief      Set sample rate for a given channel
 *
 * @param[in]  channel    The channel
 * @param[in]  rate       The rate
 *
 * @return     true if successful, false if not
 */
static bool _rfic_cmd_wr_samplerate(bladerf_channel channel, uint64_t rate)
{
    if (_rfic_chan_is_system(channel)) {
        return false;
    }

    if (BLADERF_CHANNEL_IS_TX(channel)) {
        CHECK_BOOL(ad9361_set_tx_sampling_freq(state.phy, rate));
    } else {
        CHECK_BOOL(ad9361_set_rx_sampling_freq(state.phy, rate));
    }

    return true;
}

/**
 * @brief      Get sample rate for a given channel
 *
 * @param[in]  channel    The channel
 * @param[out] rate       The sample rate
 *
 * @return     true if successful, false if not
 */
static bool _rfic_cmd_rd_samplerate(bladerf_channel channel, uint64_t *rate)
{
    uint32_t readval;

    if (_rfic_chan_is_system(channel)) {
        return false;
    };

    if (BLADERF_CHANNEL_IS_TX(channel)) {
        CHECK_BOOL(ad9361_get_tx_sampling_freq(state.phy, &readval));
    } else {
        CHECK_BOOL(ad9361_get_rx_sampling_freq(state.phy, &readval));
    }

    *rate = (uint64_t)readval;

    return true;
}

/**
 * @brief      Set LO frequency for a given channel
 *
 * @param[in]  channel    The channel
 * @param[in]  frequency  The frequency
 *
 * @return     true if successful, false if not
 */
static bool _rfic_cmd_wr_frequency(bladerf_channel channel, uint64_t frequency)
{
    if (_rfic_chan_is_system(channel)) {
        return false;
    }

    uint32_t reg;

    reg = rffe_csr_read();

    /* We have to do this for all the channels sharing the same LO. */
    for (size_t i = 0; i < 2; ++i) {
        bladerf_channel bch = BLADERF_CHANNEL_IS_TX(channel)
                                  ? BLADERF_CHANNEL_TX(i)
                                  : BLADERF_CHANNEL_RX(i);

        /* Is this channel enabled? */
        bool enable = _rffe_ch_enabled(reg, bch);

        /* Update SPDT bits accordingly */
        CHECK_BOOL(_modify_spdt_bits_by_freq(&reg, bch, enable, frequency));
    }

    rffe_csr_write(reg);

    CHECK_BOOL(set_ad9361_port_by_freq(
        state.phy, channel, _rffe_ch_enabled(reg, channel), frequency));

    if (BLADERF_CHANNEL_IS_TX(channel)) {
        CHECK_BOOL(ad9361_set_tx_lo_freq(state.phy, frequency));
        state.frequency_invalid[BLADERF_TX] = false;
    } else {
        CHECK_BOOL(ad9361_set_rx_lo_freq(state.phy, frequency));
        state.frequency_invalid[BLADERF_RX] = false;
    }

    return true;
}

void rfic_invalidate_frequency(bladerf_module module)
{
    state.frequency_invalid[module] = true;
}

/**
 * @brief      Get LO frequency for a given channel
 *
 * @param[in]  channel    The channel
 * @param[out] frequency  The frequency
 *
 * @return     true if successful, false if not
 */
static bool _rfic_cmd_rd_frequency(bladerf_channel channel, uint64_t *frequency)
{
    uint64_t readval;

    if (_rfic_chan_is_system(channel)) {
        return false;
    };

    if (BLADERF_CHANNEL_IS_TX(channel)) {
        if (state.frequency_invalid[BLADERF_TX]) {
            return false;
        }
        CHECK_BOOL(ad9361_get_tx_lo_freq(state.phy, &readval));
    } else {
        if (state.frequency_invalid[BLADERF_RX]) {
            return false;
        }
        CHECK_BOOL(ad9361_get_rx_lo_freq(state.phy, &readval));
    }

    *frequency = readval;

    return true;
}

static bool _rfic_cmd_wr_bandwidth(bladerf_channel channel, uint64_t bandwidth)
{
    if (_rfic_chan_is_system(channel)) {
        return false;
    }

    if (BLADERF_CHANNEL_IS_TX(channel)) {
        CHECK_BOOL(ad9361_set_tx_rf_bandwidth(state.phy, bandwidth));
    } else {
        CHECK_BOOL(ad9361_set_rx_rf_bandwidth(state.phy, bandwidth));
    }

    return true;
}

static bool _rfic_cmd_rd_bandwidth(bladerf_channel channel, uint64_t *bandwidth)
{
    uint32_t readval;

    if (_rfic_chan_is_system(channel)) {
        return false;
    };

    if (BLADERF_CHANNEL_IS_TX(channel)) {
        CHECK_BOOL(ad9361_get_tx_rf_bandwidth(state.phy, &readval));
    } else {
        CHECK_BOOL(ad9361_get_rx_rf_bandwidth(state.phy, &readval));
    }

    *bandwidth = (uint64_t)readval;

    return true;
}

static bool _rfic_cmd_wr_gainmode(bladerf_channel channel, uint64_t gainmode)
{
    uint8_t gmode         = (uint32_t)(gainmode & 0xF);
    uint8_t const rfic_ch = channel >> 1;
    enum rf_gain_ctrl_mode gc_mode;

    /* Determine default */
    switch (channel) {
        case BLADERF_CHANNEL_RX(0):
            gc_mode = bladerf2_rx_gain_mode_default[0];
            break;

        case BLADERF_CHANNEL_RX(1):
            gc_mode = bladerf2_rx_gain_mode_default[1];
            break;

        default:
            return false;
    }

    /* Mode conversion */
    switch (gmode) {
        case BLADERF_GAIN_DEFAULT:
            break;
        case BLADERF_GAIN_MGC:
            gc_mode = RF_GAIN_MGC;
            break;
        case BLADERF_GAIN_FASTATTACK_AGC:
            gc_mode = RF_GAIN_FASTATTACK_AGC;
            break;
        case BLADERF_GAIN_SLOWATTACK_AGC:
            gc_mode = RF_GAIN_SLOWATTACK_AGC;
            break;
        case BLADERF_GAIN_HYBRID_AGC:
            gc_mode = RF_GAIN_HYBRID_AGC;
            break;
        default:
            return false;
    }

    /* Set the mode! */
    CHECK_BOOL(ad9361_set_rx_gain_control_mode(state.phy, rfic_ch, gc_mode));

    return true;
}

static bool _rfic_cmd_rd_gainmode(bladerf_channel channel, uint64_t *gainmode)
{
    uint8_t const rfic_ch = channel >> 1;
    uint8_t gc_mode;

    if (BLADERF_CHANNEL_IS_TX(channel)) {
        return false;
    }

    /* Get the gain mode */
    CHECK_BOOL(ad9361_get_rx_gain_control_mode(state.phy, rfic_ch, &gc_mode));

    /* Mode conversion */
    switch (gc_mode) {
        case RF_GAIN_MGC:
            *gainmode = BLADERF_GAIN_MGC;
            break;
        case RF_GAIN_FASTATTACK_AGC:
            *gainmode = BLADERF_GAIN_FASTATTACK_AGC;
            break;
        case RF_GAIN_SLOWATTACK_AGC:
            *gainmode = BLADERF_GAIN_SLOWATTACK_AGC;
            break;
        case RF_GAIN_HYBRID_AGC:
            *gainmode = BLADERF_GAIN_HYBRID_AGC;
            break;
        default:
            return false;
    }

    return true;
}

static bool _rfic_cmd_wr_gain(bladerf_channel channel, uint64_t data)
{
    /* expects values that are ready to be passed down to the device.
     * e.g.:
     *  TX: mdB attenuation (-10 dB gain -> data = 10000)
     *  RX: dB gain (10 dB gain -> data = 10)
     * Values may be negative, and the uint64_t will be casted appropriately
     */

    uint8_t const rfic_ch = channel >> 1;

    if (BLADERF_CHANNEL_IS_TX(channel)) {
        bool muted;

        CHECK_BOOL(txmute_get(state.phy, channel, &muted));

        if (muted) {
            CHECK_BOOL(txmute_set_cached(state.phy, channel, (uint32_t)data));
        } else {
            CHECK_BOOL(
                ad9361_set_tx_attenuation(state.phy, rfic_ch, (uint32_t)data));
        }
    } else {
        CHECK_BOOL(ad9361_set_rx_rf_gain(state.phy, rfic_ch, (int32_t)data));
    }

    return true;
}

static bool _rfic_cmd_rd_gain(bladerf_channel channel, uint64_t *data)
{
    /* returns values that are direct from the device.
     * e.g.:
     *  TX: mdB attenuation (data = 10000 -> -10 dB gain)
     *  RX: dB gain (data = 10 -> 10 dB gain)
     * Values may be negative, and will be casted to uint64_t
     */

    uint8_t const rfic_ch = channel >> 1;

    if (BLADERF_CHANNEL_IS_TX(channel)) {
        uint32_t atten;
        bool muted;

        CHECK_BOOL(txmute_get(state.phy, channel, &muted));

        if (muted) {
            atten = txmute_get_cached(state.phy, channel);
        } else {
            CHECK_BOOL(ad9361_get_tx_attenuation(state.phy, rfic_ch, &atten));
        }

        *data = (uint64_t)atten;
    } else {
        struct rf_rx_gain rx_gain;

        CHECK_BOOL(ad9361_get_rx_gain(state.phy, rfic_ch + 1, &rx_gain));

        *data = (uint64_t)rx_gain.gain_db;
    }

    return true;
}

static bool _rfic_cmd_rd_rssi(bladerf_channel channel, uint64_t *data)
{
    uint8_t const rfic_ch = channel >> 1;
    int16_t pre, sym, mul;

    if (BLADERF_CHANNEL_IS_TX(channel)) {
        uint32_t rssi = 0;

        CHECK_BOOL(ad9361_get_tx_rssi(state.phy, rfic_ch, &rssi));

        mul = -1000;
        pre = rssi;
        sym = rssi;
    } else {
        struct rf_rssi rssi;

        CHECK_BOOL(ad9361_get_rx_rssi(state.phy, rfic_ch, &rssi));

        mul = -rssi.multiplier;
        pre = rssi.preamble;
        sym = rssi.symbol;
    }

    *data = ((uint64_t)((uint16_t)mul & BLADERF_RFIC_RSSI_MULT_MASK)
             << BLADERF_RFIC_RSSI_MULT_SHIFT);

    *data |= ((uint64_t)((uint16_t)pre & BLADERF_RFIC_RSSI_PRE_MASK)
              << BLADERF_RFIC_RSSI_PRE_SHIFT);

    *data |= ((uint64_t)((uint16_t)sym & BLADERF_RFIC_RSSI_SYM_MASK)
              << BLADERF_RFIC_RSSI_SYM_SHIFT);

    return true;
}

static bool _rfic_cmd_wr_filter(bladerf_channel channel, uint64_t data)
{
    if (BLADERF_CHANNEL_IS_TX(channel)) {
        bladerf_rfic_txfir txfir       = (bladerf_rfic_txfir)data;
        AD9361_TXFIRConfig *fir_config = NULL;
        uint8_t enable;

        if (BLADERF_RFIC_TXFIR_CUSTOM == txfir) {
            // custom FIR not implemented, assuming default
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
                return false;
        }

        CHECK_BOOL(ad9361_set_tx_fir_config(state.phy, *fir_config));
        CHECK_BOOL(ad9361_set_tx_fir_en_dis(state.phy, enable));

        state.txfir = txfir;

        return true;
    } else {
        bladerf_rfic_rxfir rxfir       = (bladerf_rfic_rxfir)data;
        AD9361_RXFIRConfig *fir_config = NULL;
        uint8_t enable;

        if (BLADERF_RFIC_RXFIR_CUSTOM == rxfir) {
            // custom FIR not implemented, assuming default
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
                return false;
        }

        CHECK_BOOL(ad9361_set_rx_fir_config(state.phy, *fir_config));
        CHECK_BOOL(ad9361_set_rx_fir_en_dis(state.phy, enable));

        state.rxfir = rxfir;

        return true;
    }

    return false;
}

static bool _rfic_cmd_rd_filter(bladerf_channel channel, uint64_t *data)
{
    if (BLADERF_CHANNEL_IS_TX(channel)) {
        *data = (uint64_t)state.txfir;
    } else {
        *data = (uint64_t)state.rxfir;
    }

    return true;
}

static bool _rfic_cmd_wr_txmute(bladerf_channel channel, uint64_t data)
{
    if (BLADERF_CHANNEL_IS_TX(channel)) {
        bool val = (data > 0);

        CHECK_BOOL(txmute_set(state.phy, channel, val));

        return true;
    }

    return false;
}

static bool _rfic_cmd_rd_txmute(bladerf_channel channel, uint64_t *data)
{
    if (BLADERF_CHANNEL_IS_TX(channel)) {
        bool val;

        CHECK_BOOL(txmute_get(state.phy, channel, &val));

        *data = val ? 1 : 0;

        return true;
    }

    return false;
}

static bool _rfic_cmd_wr_fastlock(bladerf_channel channel, uint64_t data)
{
    uint32_t profile = (uint32_t)data;

    if (BLADERF_CHANNEL_IS_TX(channel)) {
        CHECK_BOOL(ad9361_tx_fastlock_store(state.phy, profile));
    } else {
        CHECK_BOOL(ad9361_rx_fastlock_store(state.phy, profile));
    }

    return true;
}


/******************************************************************************/
/* Dispatchers */
/******************************************************************************/

struct rfic_command_fns {
    uint8_t command;
    bool (*write)(bladerf_channel channel, uint64_t data);
    bool (*read)(bladerf_channel channel, uint64_t *data);
};

struct rfic_command_fns const funcs[] = {
    // clang-format off
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_STATUS),
        FIELD_INIT(.write, NULL),
        FIELD_INIT(.read, _rfic_cmd_rd_status),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_INIT),
        FIELD_INIT(.write, _rfic_cmd_wr_init),
        FIELD_INIT(.read, _rfic_cmd_rd_init),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_ENABLE),
        FIELD_INIT(.write, _rfic_cmd_wr_enable),
        FIELD_INIT(.read, _rfic_cmd_rd_enable),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_SAMPLERATE),
        FIELD_INIT(.write, _rfic_cmd_wr_samplerate),
        FIELD_INIT(.read, _rfic_cmd_rd_samplerate),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_FREQUENCY),
        FIELD_INIT(.write, _rfic_cmd_wr_frequency),
        FIELD_INIT(.read, _rfic_cmd_rd_frequency),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_BANDWIDTH),
        FIELD_INIT(.write, _rfic_cmd_wr_bandwidth),
        FIELD_INIT(.read, _rfic_cmd_rd_bandwidth),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_GAINMODE),
        FIELD_INIT(.write, _rfic_cmd_wr_gainmode),
        FIELD_INIT(.read, _rfic_cmd_rd_gainmode),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_GAIN),
        FIELD_INIT(.write, _rfic_cmd_wr_gain),
        FIELD_INIT(.read, _rfic_cmd_rd_gain),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_RSSI),
        FIELD_INIT(.write, NULL),
        FIELD_INIT(.read, _rfic_cmd_rd_rssi),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_FILTER),
        FIELD_INIT(.write, _rfic_cmd_wr_filter),
        FIELD_INIT(.read, _rfic_cmd_rd_filter),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_TXMUTE),
        FIELD_INIT(.write, _rfic_cmd_wr_txmute),
        FIELD_INIT(.read, _rfic_cmd_rd_txmute),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_FASTLOCK),
        FIELD_INIT(.write, _rfic_cmd_wr_fastlock),
        FIELD_INIT(.read, NULL),
    },
    // clang-format on
};

static struct rfic_command_fns const *_get_cmd_ptr(uint8_t command)
{
    for (size_t i = 0; i < ARRAY_SIZE(funcs); ++i) {
        if (command == funcs[i].command) {
            return &funcs[i];
        }
    }

    return NULL;
}

/* Write queue worker */
static void rfic_command_work_wq(struct rfic_queue *q)
{
    struct rfic_queue_entry *e = rfic_queue_peek(q);

    if (NULL == e) {
        return;
    }

#ifndef BLADERF_RFIC_LOTS_OF_DEBUG
    if (ENTRY_STATE_COMPLETE == e->state)
#endif
    {
        RFIC_DBG("WRW", _rfic_cmdstr(e->cmd), _rfic_chanstr(e->ch),
                 _rfic_statestr(e->state), -1, e->value);
    }

    switch (e->state) {
        case ENTRY_STATE_NEW: {
            e->state = ENTRY_STATE_RUNNING;
            break;
        }

        case ENTRY_STATE_RUNNING: {
            struct rfic_command_fns const *f = _get_cmd_ptr(e->cmd);

            if (NULL == f || NULL == f->write) {
                e->rv = 0xFE;
                break;
            }

            e->rv    = f->write(e->ch, e->value);
            e->state = ENTRY_STATE_COMPLETE;

            break;
        }

        case ENTRY_STATE_COMPLETE: {
            /* Drop the item from the queue */
            q->last_rv = e->rv;
            rfic_dequeue(q, NULL);
            break;
        }

        default:
            break;
    }
}

/* Write command dispatcher */
bool rfic_command_write(uint16_t addr, uint64_t data)
{
    struct rfic_command_fns const *f = _get_cmd_ptr(_rfic_unpack_cmd(addr));
    bool rv;

    if (NULL == f || NULL == f->write) {
        rv = false;
    } else {
        rv = rfic_enqueue(&state.write_queue, _rfic_unpack_chan(addr),
                          _rfic_unpack_cmd(addr), data) != COMMAND_QUEUE_FULL;
    }

#ifdef BLADERF_RFIC_LOTS_OF_DEBUG
    RFIC_DBG("WR ", _rfic_cmdstr(_rfic_unpack_cmd(addr)),
             _rfic_chanstr(_rfic_unpack_chan(addr)), rv ? "ENQ" : "BAD", addr,
             data);
#endif

    return rv;
}

/* Read command dispatcher */
bool rfic_command_read(uint16_t addr, uint64_t *data)
{
    uint8_t cmd                      = _rfic_unpack_cmd(addr);
    uint8_t ch                       = _rfic_unpack_chan(addr);
    struct rfic_command_fns const *f = _get_cmd_ptr(cmd);
    bool rv                          = false;

    // Canary
    *data = 0xDEADBEEF;

    if (NULL == f || NULL == f->read) {
        rv = false;
    } else {
        rv = f->read(ch, data);
    }

    RFIC_DBG("RD ", _rfic_cmdstr(cmd), _rfic_chanstr(ch), rv ? "OK " : "BAD",
             addr, *data);

    return rv;
}

void rfic_command_init(void)
{
    rfic_queue_reset(&state.write_queue);

    return;
}

void rfic_command_work(void)
{
    rfic_command_work_wq(&state.write_queue);

    return;
}

#endif  // BLADERF_NIOS_LIBAD936X
#endif  // !BLADERF_NIOS_PC_SIMULATION
