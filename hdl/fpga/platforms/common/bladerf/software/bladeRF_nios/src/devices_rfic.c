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
static bool _rfic_cmd_wr_filter(bladerf_channel channel, uint32_t data);
static bool _rfic_cmd_wr_samplerate(bladerf_channel channel, uint32_t rate);

/**
 * @brief      Error-catching wrapper
 *
 * @param      _fn   The function to wrap
 *
 * @return     false on error (_fn() < 0), continues otherwise
 */
#define CHECK_BOOL(_fn)         \
    do {                        \
        int _s = _fn;           \
        if (_s < 0) {           \
            RFIC_ERR(#_fn, _s); \
            return false;       \
        }                       \
    } while (0)

/**
 * @brief      Test for initialization status
 *
 * @return     True if initialized, False otherwise.
 */
static inline bool _is_initialized()
{
    return BLADERF_RFIC_INIT_STATE_ON == state.init_state;
}

/**
 * @brief      Clear RFFE control bits
 */
static void _clear_rffe_ctrl()
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

/**
 * @brief      Controls the RFIC hard reset bit
 *
 * @param[in]  state  true to reset the device, false to un-reset device
 */
static void _reset_rfic(bool state)
{
    uint32_t reg;

    reg = rffe_csr_read();

    if (state) {
        reg &= ~(1 << RFFE_CONTROL_RESET_N);
    } else {
        reg |= 1 << RFFE_CONTROL_RESET_N;
    }

    rffe_csr_write(reg);
}

/**
 * @brief      Deinitializes RFIC and RFFE
 *
 * @return     true if successful, false if not
 */
static bool _rfic_deinitialize()
{
    /* Unset RFFE bits controlling RFIC */
    _clear_rffe_ctrl();

    /* Deinitialize AD9361 */
    if (NULL != state.phy) {
        CHECK_BOOL(ad9361_deinit(state.phy));
        state.phy = NULL;
    }

    /* Put RFIC into hardware reset */
    _reset_rfic(true);

    /* Update state */
    state.init_state = BLADERF_RFIC_INIT_STATE_OFF;

    DBG("*** RFIC Control Deinitialized ***\n");

    return true;
}

/**
 * @brief      Initializes RFIC and RFFE
 *
 * @return     true if successful, false if not
 */
static bool _rfic_initialize()
{
    AD9361_InitParam *init_param = &bladerf2_rfic_init_params;
    bladerf_direction dir;
    bladerf_channel ch;
    bladerf_frequency init_freq;
    bladerf_sample_rate init_samprate;
    size_t i;

    /* Unset RFFE bits controlling RFIC */
    _clear_rffe_ctrl();

    /* Initialize AD9361 if it isn't initialized */
    if (NULL == state.phy) {
        state.init_state = BLADERF_RFIC_INIT_STATE_OFF;

        DBG("--- Initializing AD9361 ---\n");

        /* Hard-reset the RFIC */
        _reset_rfic(true);
        usleep(1000);
        _reset_rfic(false);

        CHECK_BOOL(ad9361_init(&state.phy, init_param, NULL));

        if (NULL == state.phy || NULL == state.phy->pdata) {
            /* Oh no */
            DBG("%s: ad9361_init failed silently\n", __FUNCTION__);
            ad9361_deinit(state.phy);
            state.phy = NULL;
            return false;
        }
    }

    /* Per-module initialization */
    FOR_EACH_DIRECTION(dir)
    {
        if (BLADERF_RFIC_INIT_STATE_OFF == state.init_state) {
            ch = CHANNEL_IDX(dir, 0);

            if (BLADERF_CHANNEL_IS_TX(ch)) {
                init_freq     = init_param->tx_synthesizer_frequency_hz;
                init_samprate = init_param->tx_path_clock_frequencies[5];
            } else {
                init_freq     = init_param->rx_synthesizer_frequency_hz;
                init_samprate = init_param->rx_path_clock_frequencies[5];
            }

            /* Switching the frequency twice seems essential for calibration
             * to be successful. */
            CHECK_BOOL(_rfic_cmd_wr_frequency(ch, RESET_FREQUENCY));

            CHECK_BOOL(_rfic_cmd_wr_filter(
                ch, (BLADERF_TX == dir) ? BLADERF_RFIC_TXFIR_DEFAULT
                                        : BLADERF_RFIC_RXFIR_DEFAULT));

            CHECK_BOOL(_rfic_cmd_wr_samplerate(ch, init_samprate));

            CHECK_BOOL(_rfic_cmd_wr_frequency(ch, init_freq));
        }
    }

    /* Per-channel initialization */
    FOR_EACH_CHANNEL(BLADERF_TX, 2, i, ch)
    {
        switch (state.init_state) {
            case BLADERF_RFIC_INIT_STATE_OFF:
                /* Initialize TX Mute */
                CHECK_BOOL(txmute_set_cached(state.phy, ch,
                                             init_param->tx_attenuation_mdB));
                CHECK_BOOL(txmute_set(state.phy, ch, false));
                break;

            case BLADERF_RFIC_INIT_STATE_STANDBY:
                /* Restore saved TX Mute state */
                CHECK_BOOL(txmute_set(state.phy, ch, (state.tx_mute_state[i])));
                break;

            default:
                break;
        }
    }

    /* Update state */
    state.init_state = BLADERF_RFIC_INIT_STATE_ON;

    DBG("*** RFIC Control Initialized ***\n");

    return true;
}

/**
 * @brief      Puts RFIC and RFFE into standby ("warm de-initialization")
 *
 * @return     true if successful, false if not
 */
static bool _rfic_standby()
{
    AD9361_InitParam *init_param = &bladerf2_rfic_init_params;
    bladerf_channel ch;
    size_t i;

    /* If we aren't initialized, just go directly to deinitialization */
    if (NULL == state.phy) {
        return _rfic_deinitialize();
    }

    /* Unset RFFE bits controlling RFIC */
    _clear_rffe_ctrl();

    /* Cache current txmute state and put it aside */
    FOR_EACH_CHANNEL(BLADERF_TX, 2, i, ch)
    {
        CHECK_BOOL(txmute_get(state.phy, ch, &state.tx_mute_state[i]));
        CHECK_BOOL(txmute_set(state.phy, ch, true));
    }

    /* Reset frequencies if they're invalid */
    if (state.frequency_invalid[BLADERF_TX]) {
        CHECK_BOOL(_rfic_cmd_wr_frequency(
            BLADERF_CHANNEL_TX(0), init_param->tx_synthesizer_frequency_hz));
    }

    if (state.frequency_invalid[BLADERF_RX]) {
        CHECK_BOOL(_rfic_cmd_wr_frequency(
            BLADERF_CHANNEL_RX(0), init_param->rx_synthesizer_frequency_hz));
    }

    /* Update state */
    state.init_state = BLADERF_RFIC_INIT_STATE_STANDBY;

    DBG("*** RFIC Control Standby ***\n");

    return true;
}


/******************************************************************************/
/* Command implementations */
/******************************************************************************/

/**
 * @brief       Read system status register
 *
 * @param[out]  status  Status register value
 *
 * @return      true if successful, false if not
 */
static bool _rfic_cmd_rd_status(bladerf_channel channel, uint64_t *status)
{
    *status = ((_is_initialized() & BLADERF_RFIC_STATUS_INIT_MASK)
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
 * @param[in]  init_state   bladerf_rfic_init_state
 *
 * @return     true if successful, false if not
 */
static bool _rfic_cmd_wr_init(bladerf_channel channel, uint32_t init_state)
{
    bladerf_rfic_init_state new_state = (bladerf_rfic_init_state)init_state;

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

/**
 * @brief       Query RFIC initialization state
 *
 * @param[out]  init_state  bladerf_rfic_init_state value, casted to uint32_t
 *
 * @return      true if successful, false if not
 */
static bool _rfic_cmd_rd_init(bladerf_channel channel, uint32_t *init_state)
{
    *init_state = (uint32_t)state.init_state;

    return true;
}

/**
 * @brief       Set the enabled status for a given channel
 *
 * @param[in]   channel  Channel
 * @param[in]   enable     0 to disable, >= 1 to enable
 *
 * @return      true if successful, false otherwise
 */
static bool _rfic_cmd_wr_enable(bladerf_channel channel, uint32_t enable)
{
    bladerf_direction direction =
        BLADERF_CHANNEL_IS_TX(channel) ? BLADERF_TX : BLADERF_RX;
    uint32_t reg;     /* RFFE register value */
    bool ch_enabled;  /* Channel: initial state */
    bool ch_enable;   /* Channel: target state */
    bool ch_pending;  /* Channel: target state is not initial state */
    bool dir_enabled; /* Direction: initial state */
    bool dir_enable;  /* Direction: target state */
    bool dir_pending; /* Direction: target state is not initial state */

    struct band_port_map const *port_map;
    bladerf_channel subch;
    bladerf_frequency freq = 0;
    size_t i;

    /* Read RFFE control register */
    reg = rffe_csr_read();

#ifdef BLADERF_RFIC_LOTS_OF_DEBUG
    uint32_t reg_old = reg;
#endif

    /* Pre-compute useful values */
    /* Calculate initial and target states */
    ch_enabled  = _rffe_ch_enabled(reg, channel);
    dir_enabled = _rffe_dir_enabled(reg, direction);
    ch_enable   = (enable > 0);
    dir_enable  = ch_enable || _rffe_dir_otherwise_enabled(reg, channel);
    ch_pending  = ch_enabled != ch_enable;
    dir_pending = dir_enabled != dir_enable;

    if (!ch_pending && !dir_pending) {
        DBG("%s: nothing to do?\n", __FUNCTION__);
        return true;
    }

    /* Query the current frequency if we'll need it */
    if (ch_enable) {
        if (BLADERF_CHANNEL_IS_TX(channel)) {
            CHECK_BOOL(ad9361_get_tx_lo_freq(state.phy, &freq));
        } else {
            CHECK_BOOL(ad9361_get_rx_lo_freq(state.phy, &freq));
        }
    }

    /* Channel Setup/Teardown */
    if (ch_pending) {
        /* Modify SPDT bits */
        CHECK_BOOL(_modify_spdt_bits_by_freq(&reg, channel, ch_enable, freq));

        /* Modify MIMO channel enable bits */
        if (ch_enable) {
            reg |= (1 << _get_rffe_control_bit_for_ch(channel));
        } else {
            reg &= ~(1 << _get_rffe_control_bit_for_ch(channel));
        }
    }

    /* Direction Setup/Teardown */
    if (dir_pending) {
        /* Modify ENABLE/TXNRX bits */
        if (dir_enable) {
            reg |= (1 << _get_rffe_control_bit_for_dir(direction));
        } else {
            FOR_EACH_CHANNEL(direction, 2, i, subch)
            {
                CHECK_BOOL(_modify_spdt_bits_by_freq(&reg, subch, false, 0));
            }

            reg &= ~(1 << _get_rffe_control_bit_for_dir(direction));
        }

        /* Select RFIC port */
        /* Look up the port configuration for this frequency */
        port_map = _get_band_port_map_by_freq(channel, ch_enable ? freq : 0);
        if (NULL == port_map) {
            return false;
        }

        /* Set the AD9361 port accordingly */
        if (BLADERF_CHANNEL_IS_TX(channel)) {
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
        __FUNCTION__, channel, direction, ch_enable, ch_pending, dir_enable,
        dir_pending, reg_old, reg);
#endif

    /* Write RFFE control register */
    rffe_csr_write(reg);

    return true;
}

/**
 * @brief      Get the enabled status for a given channel
 *
 * @param[in]  channel  The channel
 * @param[out] enabled  0 if disabled, 1 if enabled
 *
 * @return     true if successful, false otherwise
 */
static bool _rfic_cmd_rd_enable(bladerf_channel channel, uint32_t *enabled)
{
    *enabled = _rffe_ch_enabled(rffe_csr_read(), channel) ? 1 : 0;

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
static bool _rfic_cmd_wr_samplerate(bladerf_channel channel, uint32_t rate)
{
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
static bool _rfic_cmd_rd_samplerate(bladerf_channel channel, uint32_t *rate)
{
    if (BLADERF_CHANNEL_IS_TX(channel)) {
        CHECK_BOOL(ad9361_get_tx_sampling_freq(state.phy, rate));
    } else {
        CHECK_BOOL(ad9361_get_rx_sampling_freq(state.phy, rate));
    }

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
    bladerf_direction dir =
        BLADERF_CHANNEL_IS_TX(channel) ? BLADERF_TX : BLADERF_RX;
    bladerf_channel other_ch;
    uint32_t reg;
    size_t i;

    reg = rffe_csr_read();

    /* We have to do this for all the channels sharing the same LO. */
    FOR_EACH_CHANNEL(dir, 2, i, other_ch)
    {
        /* Is this channel enabled? */
        bool enable = _rffe_ch_enabled(reg, other_ch);

        /* Update SPDT bits accordingly */
        CHECK_BOOL(
            _modify_spdt_bits_by_freq(&reg, other_ch, enable, frequency));
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

/**
 * @brief      Flag the current frequency state as invalid
 *
 * This should be set if doing things that bypass the RFIC state structure,
 * such as direct-SPI fastlock.
 *
 * @param[in]  module  The module (BLADERF_RX or BLADERF_TX)
 */
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
    if (BLADERF_CHANNEL_IS_TX(channel)) {
        if (state.frequency_invalid[BLADERF_TX]) {
            return false;
        }
        CHECK_BOOL(ad9361_get_tx_lo_freq(state.phy, frequency));
    } else {
        if (state.frequency_invalid[BLADERF_RX]) {
            return false;
        }
        CHECK_BOOL(ad9361_get_rx_lo_freq(state.phy, frequency));
    }

    return true;
}

/**
 * @brief      Set bandwidth
 *
 * @param[in]  channel    The channel
 * @param[in]  bandwidth  The bandwidth
 *
 * @return     true if successful, false if not
 */
static bool _rfic_cmd_wr_bandwidth(bladerf_channel channel, uint32_t bandwidth)
{
    if (BLADERF_CHANNEL_IS_TX(channel)) {
        CHECK_BOOL(ad9361_set_tx_rf_bandwidth(state.phy, bandwidth));
    } else {
        CHECK_BOOL(ad9361_set_rx_rf_bandwidth(state.phy, bandwidth));
    }

    return true;
}

/**
 * @brief      Get bandwidth
 *
 * @param[in]  channel    The channel
 * @param[out] bandwidth  The bandwidth
 *
 * @return     true if successful, false if not
 */
static bool _rfic_cmd_rd_bandwidth(bladerf_channel channel, uint32_t *bandwidth)
{
    if (BLADERF_CHANNEL_IS_TX(channel)) {
        CHECK_BOOL(ad9361_get_tx_rf_bandwidth(state.phy, bandwidth));
    } else {
        CHECK_BOOL(ad9361_get_rx_rf_bandwidth(state.phy, bandwidth));
    }

    return true;
}

/**
 * @brief      Set gainmode
 *
 * @param[in]  channel    The channel
 * @param[in]  gainmode   The gainmode
 *
 * @return     true if successful, false if not
 */
static bool _rfic_cmd_wr_gainmode(bladerf_channel channel, uint32_t gainmode)
{
    uint8_t const rfic_ch = channel >> 1;
    enum rf_gain_ctrl_mode gc_mode;

    /* Mode conversion */
    switch (gainmode) {
        case BLADERF_GAIN_DEFAULT:
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

/**
 * @brief      Get gainmode
 *
 * @param[in]  channel   The channel
 * @param[out] gainmode  The gainmode
 *
 * @return     true if successful, false if not
 */
static bool _rfic_cmd_rd_gainmode(bladerf_channel channel, uint32_t *gainmode)
{
    uint8_t const rfic_ch = channel >> 1;
    uint8_t gc_mode;

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

/**
 * @brief      Set gain
 *
 * expects values that are ready to be passed down to the device.
 * e.g.:
 *  TX: mdB attenuation (-10 dB gain -> gain = 10000)
 *  RX: dB gain (10 dB gain -> gain = 10)
 * Values may be negative, and the uint32_t will be casted appropriately
 *
 * @param[in]  channel    The channel
 * @param[in]  gain       The gain
 *
 * @return     true if successful, false if not
 */
static bool _rfic_cmd_wr_gain(bladerf_channel channel, uint32_t gain)
{
    uint8_t const rfic_ch = channel >> 1;

    if (BLADERF_CHANNEL_IS_TX(channel)) {
        bool muted;

        CHECK_BOOL(txmute_get(state.phy, channel, &muted));

        if (muted) {
            CHECK_BOOL(txmute_set_cached(state.phy, channel, gain));
        } else {
            CHECK_BOOL(ad9361_set_tx_attenuation(state.phy, rfic_ch, gain));
        }
    } else {
        CHECK_BOOL(ad9361_set_rx_rf_gain(state.phy, rfic_ch, (int32_t)gain));
    }

    return true;
}

/**
 * @brief       Get gain
 *
 * returns values that are direct from the device.
 * e.g.:
 *  TX: mdB attenuation (data = 10000 -> -10 dB gain)
 *  RX: dB gain (data = 10 -> 10 dB gain)
 * Values may be negative, and will be casted to uint32_t
 *
 *
 * @param[in]   channel    The channel
 * @param[out]  gain       The gain
 *
 * @return      true if successful, false if not
 */
static bool _rfic_cmd_rd_gain(bladerf_channel channel, uint32_t *gain)
{
    uint8_t const rfic_ch = channel >> 1;

    if (BLADERF_CHANNEL_IS_TX(channel)) {
        bool muted;

        CHECK_BOOL(txmute_get(state.phy, channel, &muted));

        if (muted) {
            *gain = txmute_get_cached(state.phy, channel);
        } else {
            CHECK_BOOL(ad9361_get_tx_attenuation(state.phy, rfic_ch, gain));
        }
    } else {
        struct rf_rx_gain rx_gain;

        CHECK_BOOL(ad9361_get_rx_gain(state.phy, rfic_ch + 1, &rx_gain));

        *gain = (uint32_t)rx_gain.gain_db;
    }

    return true;
}

/**
 * @brief       Get RSSI reading
 *
 * @param[in]   channel   The channel
 * @param[out]  rssi_val  RSSI, packed
 *
 * @return      true if successful, false if not
 */
static bool _rfic_cmd_rd_rssi(bladerf_channel channel, uint64_t *rssi_val)
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

    *rssi_val = ((uint64_t)((uint16_t)mul & BLADERF_RFIC_RSSI_MULT_MASK)
                 << BLADERF_RFIC_RSSI_MULT_SHIFT) |

                ((uint64_t)((uint16_t)pre & BLADERF_RFIC_RSSI_PRE_MASK)
                 << BLADERF_RFIC_RSSI_PRE_SHIFT) |

                ((uint64_t)((uint16_t)sym & BLADERF_RFIC_RSSI_SYM_MASK)
                 << BLADERF_RFIC_RSSI_SYM_SHIFT);

    return true;
}

/**
 * @brief       Set RFIC FIR filter
 *
 * @param[in]   channel  The channel
 * @param[in]   filter   The filter enum value
 *
 * @return      true if successful, false if not
 */
static bool _rfic_cmd_wr_filter(bladerf_channel channel, uint32_t filter)
{
    if (BLADERF_CHANNEL_IS_TX(channel)) {
        bladerf_rfic_txfir txfir       = (bladerf_rfic_txfir)filter;
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
        bladerf_rfic_rxfir rxfir       = (bladerf_rfic_rxfir)filter;
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

/**
 * @brief       Get current FIR filter setting
 *
 * @param[in]   channel  The channel
 * @param[out]  filter   The filter state
 *
 * @return      true if successful, false if not
 */
static bool _rfic_cmd_rd_filter(bladerf_channel channel, uint32_t *filter)
{
    if (BLADERF_CHANNEL_IS_TX(channel)) {
        *filter = (uint32_t)state.txfir;
    } else {
        *filter = (uint32_t)state.rxfir;
    }

    return true;
}

/**
 * @brief       Set/unset TX mute
 *
 * @param[in]   channel  The channel
 * @param[in]   mute     Requested mute status
 *
 * @return      true if successful, false if not
 */
static bool _rfic_cmd_wr_txmute(bladerf_channel channel, uint32_t mute)
{
    bool val = (mute > 0);

    CHECK_BOOL(txmute_set(state.phy, channel, val));

    return true;
}

/**
 * @brief       Get current TX mute status
 *
 * @param[in]   channel  The channel
 * @param[out]  mute     Current mute status
 *
 * @return      true if successful, false if not
 */
static bool _rfic_cmd_rd_txmute(bladerf_channel channel, uint32_t *mute)
{
    bool val;

    CHECK_BOOL(txmute_get(state.phy, channel, &val));

    *mute = val ? 1 : 0;

    return true;
}

/**
 * @brief       Store current fastlock parameters into profile
 *
 * @param[in]   channel  The channel
 * @param[in]   profile  The profile number
 *
 * @return      true if successful, false if not
 */
static bool _rfic_cmd_wr_fastlock(bladerf_channel channel, uint32_t profile)
{
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

typedef uint8_t command_bitmask;

#define RFIC_CMD_INIT_REQD 1   /**< Command requires initialized state */
#define RFIC_CMD_CHAN_SYSTEM 2 /**< Command valid for wildcard channels */
#define RFIC_CMD_CHAN_TX 4     /**< Command valid for TX channels */
#define RFIC_CMD_CHAN_RX 8     /**< Command valid for RX channels */

/**
 * @brief      Function pointers for RFIC command dispatching
 */
struct rfic_command_fns {
    bool (*write64)(bladerf_channel channel, uint64_t data);
    bool (*write32)(bladerf_channel channel, uint32_t data);
    bool (*read64)(bladerf_channel channel, uint64_t *data);
    bool (*read32)(bladerf_channel channel, uint32_t *data);
    command_bitmask bitmask;
    uint8_t command;
};

/**
 * Function pointers for command dispatching
 */
struct rfic_command_fns const funcs[] = {
    // clang-format off
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_STATUS),
        FIELD_INIT(.read64, _rfic_cmd_rd_status),
        FIELD_INIT(.bitmask, RFIC_CMD_CHAN_SYSTEM),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_INIT),
        FIELD_INIT(.write32, _rfic_cmd_wr_init),
        FIELD_INIT(.read32, _rfic_cmd_rd_init),
        FIELD_INIT(.bitmask, RFIC_CMD_CHAN_SYSTEM),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_ENABLE),
        FIELD_INIT(.write32, _rfic_cmd_wr_enable),
        FIELD_INIT(.read32, _rfic_cmd_rd_enable),
        FIELD_INIT(.bitmask,
            RFIC_CMD_INIT_REQD | RFIC_CMD_CHAN_TX | RFIC_CMD_CHAN_RX),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_SAMPLERATE),
        FIELD_INIT(.write32, _rfic_cmd_wr_samplerate),
        FIELD_INIT(.read32, _rfic_cmd_rd_samplerate),
        FIELD_INIT(.bitmask,
            RFIC_CMD_INIT_REQD | RFIC_CMD_CHAN_TX | RFIC_CMD_CHAN_RX),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_FREQUENCY),
        FIELD_INIT(.write64, _rfic_cmd_wr_frequency),
        FIELD_INIT(.read64, _rfic_cmd_rd_frequency),
        FIELD_INIT(.bitmask,
            RFIC_CMD_INIT_REQD | RFIC_CMD_CHAN_TX | RFIC_CMD_CHAN_RX),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_BANDWIDTH),
        FIELD_INIT(.write32, _rfic_cmd_wr_bandwidth),
        FIELD_INIT(.read32, _rfic_cmd_rd_bandwidth),
        FIELD_INIT(.bitmask,
            RFIC_CMD_INIT_REQD | RFIC_CMD_CHAN_TX | RFIC_CMD_CHAN_RX),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_GAINMODE),
        FIELD_INIT(.write32, _rfic_cmd_wr_gainmode),
        FIELD_INIT(.read32, _rfic_cmd_rd_gainmode),
        FIELD_INIT(.bitmask, RFIC_CMD_INIT_REQD | RFIC_CMD_CHAN_RX),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_GAIN),
        FIELD_INIT(.write32, _rfic_cmd_wr_gain),
        FIELD_INIT(.read32, _rfic_cmd_rd_gain),
        FIELD_INIT(.bitmask,
            RFIC_CMD_INIT_REQD | RFIC_CMD_CHAN_TX | RFIC_CMD_CHAN_RX),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_RSSI),
        FIELD_INIT(.read64, _rfic_cmd_rd_rssi),
        FIELD_INIT(.bitmask,
            RFIC_CMD_INIT_REQD | RFIC_CMD_CHAN_TX | RFIC_CMD_CHAN_RX),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_FILTER),
        FIELD_INIT(.write32, _rfic_cmd_wr_filter),
        FIELD_INIT(.read32, _rfic_cmd_rd_filter),
        FIELD_INIT(.bitmask,
            RFIC_CMD_INIT_REQD | RFIC_CMD_CHAN_TX | RFIC_CMD_CHAN_RX),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_TXMUTE),
        FIELD_INIT(.write32, _rfic_cmd_wr_txmute),
        FIELD_INIT(.read32, _rfic_cmd_rd_txmute),
        FIELD_INIT(.bitmask, RFIC_CMD_INIT_REQD | RFIC_CMD_CHAN_TX),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_FASTLOCK),
        FIELD_INIT(.write32, _rfic_cmd_wr_fastlock),
        FIELD_INIT(.bitmask,
            RFIC_CMD_INIT_REQD | RFIC_CMD_CHAN_TX | RFIC_CMD_CHAN_RX),
    },
    // clang-format on
};

/**
 * @brief      Retrieve command pointer for a given command
 *
 * @param[in]  command  The command
 *
 * @return     The command pointer.
 */
static struct rfic_command_fns const *_get_cmd_ptr(uint8_t command)
{
    for (size_t i = 0; i < ARRAY_SIZE(funcs); ++i) {
        if (command == funcs[i].command) {
            return &funcs[i];
        }
    }

    return NULL;
}

/**
 * @brief      Determines if bitmask contains a given bit.
 *
 * @param[in]  bitmask  The bitmask
 * @param[in]  bit      The bit
 *
 * @return     True if bitmask has bit, False otherwise.
 */
static bool _has_bit(command_bitmask bitmask, command_bitmask bit)
{
    return ((bitmask & bit) > 0);
}

/**
 * @brief      Check validity of command pointer in current state
 *
 * @param      cmd_ptr  The command pointer
 * @param[in]  channel  The channel
 *
 * @return     True if the current state is valid, False otherwise.
 */
static bool _check_validity(struct rfic_command_fns const *cmd_ptr,
                            bladerf_channel channel)
{
    bool is_system = _rfic_chan_is_system(channel);
    bool is_tx     = !is_system && BLADERF_CHANNEL_IS_TX(channel);
    bool is_rx     = !is_system && !BLADERF_CHANNEL_IS_TX(channel);

    /* System channel check */
    if (is_system && !_has_bit(cmd_ptr->bitmask, RFIC_CMD_CHAN_SYSTEM)) {
        DBG("%s: operation not valid for wildcard channel\n", __FUNCTION__);
        return false;
    }

    /* Initialization check */
    if (!_is_initialized() && _has_bit(cmd_ptr->bitmask, RFIC_CMD_INIT_REQD)) {
        DBG("%s: operation not valid in uninitialized state\n", __FUNCTION__);
        return false;
    }

    /* TX channel check */
    if (is_tx && !_has_bit(cmd_ptr->bitmask, RFIC_CMD_CHAN_TX)) {
        DBG("%s: operation not valid for TX channels\n", __FUNCTION__);
        return false;
    }

    /* RX channel check */
    if (is_rx && !_has_bit(cmd_ptr->bitmask, RFIC_CMD_CHAN_RX)) {
        DBG("%s: operation not valid for RX channels\n", __FUNCTION__);
        return false;
    }

    /* All tests passed */
    return true;
}

/**
 * @brief      Write queue worker
 *
 * Sets e->rv to 0xFE if there is no write handler for the command.
 *
 * @param      q     rfic_queue pointer
 */
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

            e->rv    = 0xFE;
            e->state = ENTRY_STATE_COMPLETE;

            if (NULL != f->write64) {
                e->rv = f->write64(e->ch, e->value);
            } else if (NULL != f->write32) {
                e->rv = f->write32(e->ch, e->value);
            }

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

bool rfic_command_write(uint16_t addr, uint64_t data)
{
    uint8_t cmd                      = _rfic_unpack_cmd(addr);
    uint8_t const ch                 = _rfic_unpack_chan(addr);
    struct rfic_command_fns const *f = _get_cmd_ptr(cmd);
    bool rv                          = false;

    if (NULL != f && (NULL != f->write64 || NULL != f->write32) &&
        _check_validity(f, ch)) {
        rv = rfic_enqueue(&state.write_queue, ch, cmd, data) !=
             COMMAND_QUEUE_FULL;
    }

    RFIC_DBG("WR ", _rfic_cmdstr(cmd), _rfic_chanstr(ch), rv ? "ENQ" : "BAD",
             addr, data);

    return rv;
}

bool rfic_command_write_immed(bladerf_rfic_command cmd,
                              bladerf_channel ch,
                              uint64_t data)
{
    struct rfic_command_fns const *f = _get_cmd_ptr(cmd);

    bool rv = false;

    if (NULL == f || !_check_validity(f, ch)) {
        rv = false;
    } else if (NULL != f->write64) {
        rv = f->write64(ch, data);
    } else if (NULL != f->write32) {
        rv = f->write32(ch, (uint32_t)data);
    }

    RFIC_DBG("WR!", _rfic_cmdstr(cmd), _rfic_chanstr(ch), rv ? "OK " : "BAD",
             addr, data);

    return rv;
}

bool rfic_command_read(uint16_t addr, uint64_t *data)
{
    return rfic_command_read_immed(_rfic_unpack_cmd(addr),
                                   _rfic_unpack_chan(addr), data);
}

bool rfic_command_read_immed(bladerf_rfic_command cmd,
                             bladerf_channel ch,
                             uint64_t *data)
{
    struct rfic_command_fns const *f = _get_cmd_ptr(cmd);
    bool rv                          = false;

    // Canary
    *data = 0xDEADBEEF0B4DF00D;

    if (NULL == f || !_check_validity(f, ch)) {
        rv = false;
    } else if (NULL != f->read64) {
        rv = f->read64(ch, data);
    } else if (NULL != f->read32) {
        uint32_t data32;

        rv = f->read32(ch, &data32);

        if (rv) {
            *data = (uint64_t)data32;
        }
    }

    RFIC_DBG("RD!", _rfic_cmdstr(cmd), _rfic_chanstr(ch), rv ? "OK " : "BAD",
             addr, *data);

    return rv;
}

/**
 * @brief      Startup initialization for RFIC command
 */
void rfic_command_init(void)
{
    rfic_queue_reset(&state.write_queue);
}

/**
 * @brief      Worker wrapper for RFIC command
 */
void rfic_command_work(void)
{
    rfic_command_work_wq(&state.write_queue);
}

#endif  // BLADERF_NIOS_LIBAD936X
#endif  // !BLADERF_NIOS_PC_SIMULATION
