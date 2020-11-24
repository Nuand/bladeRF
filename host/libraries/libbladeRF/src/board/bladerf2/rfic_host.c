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

#include <libbladeRF.h>

#include "ad936x_helpers.h"
#include "bladerf2_common.h"
#include "board/board.h"
#include "common.h"
#include "helpers/wallclock.h"
#include "iterators.h"
#include "log.h"

// #define BLADERF_HOSTED_C_DEBUG


/******************************************************************************/
/* Initialization */
/******************************************************************************/

static bool _rfic_host_is_present(struct bladerf *dev)
{
    return true;
}

static bool _rfic_host_is_initialized(struct bladerf *dev)
{
    if (NULL == dev || NULL == dev->board_data) {
        return false;
    }

    struct bladerf2_board_data *board_data = dev->board_data;

    return (NULL != board_data->phy);
}

static int _rfic_host_get_init_state(struct bladerf *dev,
                                     bladerf_rfic_init_state *state)
{
    if (_rfic_host_is_initialized(dev)) {
        *state = BLADERF_RFIC_INIT_STATE_ON;
    } else {
        *state = BLADERF_RFIC_INIT_STATE_OFF;
    }

    return 0;
}

static bool _rfic_host_is_standby(struct bladerf *dev)
{
    return false;
}

static int _rfic_host_initialize(struct bladerf *dev)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = NULL;
    struct controller_fns const *rfic      = board_data->rfic;
    uint32_t reg;
    bladerf_direction dir;
    bladerf_channel ch;
    size_t i;
    uint32_t config_gpio;

    log_debug("%s: initializating\n", __FUNCTION__);

    /* Initialize RFFE control */
    CHECK_STATUS(dev->backend->rffe_control_write(
        dev, (1 << RFFE_CONTROL_ENABLE) | (1 << RFFE_CONTROL_TXNRX)));


    CHECK_STATUS(dev->backend->config_gpio_read(dev, &config_gpio));

    board_data->rfic_init_params = (config_gpio & BLADERF_GPIO_PACKET_CORE_PRESENT) ?
                (void *)&bladerf2_rfic_init_params_fastagc_burst :
                (void *)&bladerf2_rfic_init_params;

    /* Initialize AD9361 */
    CHECK_AD936X(ad9361_init(&phy, (AD9361_InitParam *)board_data->rfic_init_params, dev));

    if (NULL == phy || NULL == phy->pdata) {
        RETURN_ERROR_STATUS("ad9361_init struct initialization",
                            BLADERF_ERR_UNEXPECTED);
    }

    log_verbose("%s: ad9361 initialized @ %p\n", __FUNCTION__, phy);

    board_data->phy = phy;

    /* Force AD9361 to a non-default freq. This will entice it to do a
     * proper re-tuning when we set it back to the default freq later on. */
    FOR_EACH_DIRECTION(dir)
    {
        FOR_EACH_CHANNEL(dir, 1, i, ch)
        {
            CHECK_STATUS(rfic->set_frequency(dev, ch, RESET_FREQUENCY));
        }
    }

    /* Set up AD9361 FIR filters */
    /* TODO: permit specification of filter taps, for the truly brave */
    CHECK_STATUS(rfic->set_filter(dev, BLADERF_CHANNEL_RX(0),
                                  BLADERF_RFIC_RXFIR_DEFAULT, 0));
    CHECK_STATUS(rfic->set_filter(dev, BLADERF_CHANNEL_TX(0), 0,
                                  BLADERF_RFIC_TXFIR_DEFAULT));

    /* Clear RFFE control */
    CHECK_STATUS(dev->backend->rffe_control_read(dev, &reg));
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
    CHECK_STATUS(dev->backend->rffe_control_write(dev, reg));

    /* Move AD9361 back to desired frequency */
    CHECK_STATUS(rfic->set_frequency(dev, BLADERF_CHANNEL_RX(0),
                                     phy->pdata->rx_synth_freq));
    CHECK_STATUS(rfic->set_frequency(dev, BLADERF_CHANNEL_TX(0),
                                     phy->pdata->tx_synth_freq));

    /* Mute TX channels */
    FOR_EACH_CHANNEL(BLADERF_TX, dev->board->get_channel_count(dev, BLADERF_TX),
                     i, ch)
    {
        CHECK_STATUS(rfic->set_txmute(dev, ch, true));
    }

    log_debug("%s: initialization complete\n", __FUNCTION__);

    return 0;
}

static int _rfic_host_deinitialize(struct bladerf *dev)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    uint32_t reg;

    log_debug("%s: deinitializing\n", __FUNCTION__);

    /* Clear RFFE control */
    CHECK_STATUS(dev->backend->rffe_control_read(dev, &reg));
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
    CHECK_STATUS(dev->backend->rffe_control_write(dev, reg));

    if (NULL != board_data->phy) {
        CHECK_STATUS(ad9361_deinit(board_data->phy));
        board_data->phy = NULL;
    }

    return 0;
}

static int _rfic_host_standby(struct bladerf *dev)
{
    return _rfic_host_deinitialize(dev);
}


/******************************************************************************/
/* Enable */
/******************************************************************************/

static int _rfic_host_enable_module(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bool enable)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;
    struct controller_fns const *rfic      = board_data->rfic;
    bladerf_direction dir = BLADERF_CHANNEL_IS_TX(ch) ? BLADERF_TX : BLADERF_RX;
    bladerf_frequency freq = 0;
    uint32_t reg;       /* RFFE register value */
    uint32_t reg_old;   /* Original RFFE register value */
    int ch_bit;         /* RFFE MIMO channel bit */
    int dir_bit;        /* RFFE RX/TX enable bit */
    bool ch_pending;    /* Requested channel state differs */
    bool dir_enable;    /* Direction is enabled */
    bool dir_pending;   /* Requested direction state differs */
    bool backend_clear; /* Backend requires reset */

    /* Look up the RFFE bits affecting this channel */
    ch_bit  = _get_rffe_control_bit_for_ch(ch);
    dir_bit = _get_rffe_control_bit_for_dir(dir);
    if (ch_bit < 0 || dir_bit < 0) {
        RETURN_ERROR_STATUS("_get_rffe_control_bit", BLADERF_ERR_INVAL);
    }

    /* Query the current frequency if necessary */
    if (enable) {
        CHECK_STATUS(rfic->get_frequency(dev, ch, &freq));
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
    CHECK_STATUS(dev->backend->rffe_control_read(dev, &reg));
    reg_old    = reg;
    ch_pending = _rffe_ch_enabled(reg, ch) != enable;

    /* Channel Setup/Teardown */
    if (ch_pending) {
        /* Modify SPDT bits */
        CHECK_STATUS(_modify_spdt_bits_by_freq(&reg, ch, enable, freq));

        /* Modify MIMO channel enable bits */
        if (enable) {
            reg |= (1 << ch_bit);
        } else {
            reg &= ~(1 << ch_bit);
        }

        /* Set/unset TX mute */
        if (BLADERF_CHANNEL_IS_TX(ch)) {
            txmute_set(phy, ch, !enable);
        }
    }

    dir_enable  = enable || does_rffe_dir_have_enabled_ch(reg, dir);
    dir_pending = _rffe_dir_enabled(reg, dir) != dir_enable;

    /* Direction Setup/Teardown */
    if (dir_pending) {
        /* Modify ENABLE/TXNRX bits */
        if (dir_enable) {
            reg |= (1 << dir_bit);
        } else {
            reg &= ~(1 << dir_bit);
        }

        /* Select RFIC port */
        CHECK_STATUS(set_ad9361_port_by_freq(phy, ch, dir_enable, freq));

        /* Tear down sync interface if required */
        if (!dir_enable) {
            sync_deinit(&board_data->sync[dir]);
            perform_format_deconfig(dev, dir);
        }
    }

    /* Reset FIFO if we are enabling an additional RX channel */
    backend_clear = enable && !dir_pending && BLADERF_RX == dir;

    /* Write RFFE control register */
    if (reg_old != reg) {
        CHECK_STATUS(dev->backend->rffe_control_write(dev, reg));
    } else {
        log_debug("%s: reg value unchanged? (%08x)\n", __FUNCTION__, reg);
    }

    /* Backend Setup/Teardown/Reset */
    if (dir_pending || backend_clear) {
        if (!dir_enable || backend_clear) {
            CHECK_STATUS(dev->backend->enable_module(dev, dir, false));
        }

        if (dir_enable || backend_clear) {
            CHECK_STATUS(dev->backend->enable_module(dev, dir, true));
        }
    }

    /* Warn the user if the sample rate isn't reasonable */
    if (ch_pending && enable) {
        check_total_sample_rate(dev);
    }

#ifdef BLADERF_HOSTED_C_DEBUG
    /* Debug logging */
    static uint64_t lastrun = 0; /* nsec value at last run */
    uint64_t nsec           = wallclock_get_current_nsec();

    log_debug("%s: %s%d ch_en=%d ch_pend=%d dir_en=%d dir_pend=%d be_clr=%d "
              "reg=0x%08x->0x%08x nsec=%" PRIu64 " (delta: %" PRIu64 ")\n",
              __FUNCTION__, BLADERF_TX == dir ? "TX" : "RX", (ch >> 1) + 1,
              enable, ch_pending, dir_enable, dir_pending, backend_clear,
              reg_old, reg, nsec, nsec - lastrun);

    lastrun = nsec;
#endif

    return 0;
}


/******************************************************************************/
/* Sample rate */
/******************************************************************************/

static int _rfic_host_get_sample_rate(struct bladerf *dev,
                                      bladerf_channel ch,
                                      bladerf_sample_rate *rate)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        CHECK_AD936X(ad9361_get_tx_sampling_freq(phy, rate));
    } else {
        CHECK_AD936X(ad9361_get_rx_sampling_freq(phy, rate));
    }

    return 0;
}

static int _rfic_host_set_sample_rate(struct bladerf *dev,
                                      bladerf_channel ch,
                                      bladerf_sample_rate rate)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        CHECK_AD936X(ad9361_set_tx_sampling_freq(phy, rate));
    } else {
        CHECK_AD936X(ad9361_set_rx_sampling_freq(phy, rate));
    }

    return 0;
}


/******************************************************************************/
/* Frequency */
/******************************************************************************/

static int _rfic_host_get_frequency(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bladerf_frequency *frequency)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;
    bladerf_frequency lo_frequency;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        CHECK_AD936X(ad9361_get_tx_lo_freq(phy, &lo_frequency));
    } else {
        CHECK_AD936X(ad9361_get_rx_lo_freq(phy, &lo_frequency));
    }

    *frequency = lo_frequency;

    return 0;
}

static int _rfic_host_set_frequency(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bladerf_frequency frequency)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;
    struct controller_fns const *rfic      = board_data->rfic;
    struct bladerf_range const *range      = NULL;

    CHECK_STATUS(dev->board->get_frequency_range(dev, ch, &range));

    if (!is_within_range(range, frequency)) {
        return BLADERF_ERR_RANGE;
    }

    /* Set up band selection */
    CHECK_STATUS(rfic->select_band(dev, ch, frequency));

    /* Change LO frequency */
    if (BLADERF_CHANNEL_IS_TX(ch)) {
        CHECK_AD936X(ad9361_set_tx_lo_freq(phy, frequency));
    } else {
        CHECK_AD936X(ad9361_set_rx_lo_freq(phy, frequency));
    }

    return 0;
}

static int _rfic_host_select_band(struct bladerf *dev,
                                  bladerf_channel ch,
                                  bladerf_frequency frequency)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;
    uint32_t reg;
    size_t i;

    /* Read RFFE control register */
    CHECK_STATUS(dev->backend->rffe_control_read(dev, &reg));

    /* Modify the SPDT bits. */
    /* We have to do this for all the channels sharing the same LO. */
    for (i = 0; i < 2; ++i) {
        bladerf_channel bch = BLADERF_CHANNEL_IS_TX(ch) ? BLADERF_CHANNEL_TX(i)
                                                        : BLADERF_CHANNEL_RX(i);
        CHECK_STATUS(_modify_spdt_bits_by_freq(
            &reg, bch, _rffe_ch_enabled(reg, bch), frequency));
    }

    /* Write RFFE control register */
    CHECK_STATUS(dev->backend->rffe_control_write(dev, reg));

    /* Set AD9361 port */
    CHECK_STATUS(
        set_ad9361_port_by_freq(phy, ch, _rffe_ch_enabled(reg, ch), frequency));

    return 0;
}


/******************************************************************************/
/* Bandwidth */
/******************************************************************************/

static int _rfic_host_get_bandwidth(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bladerf_bandwidth *bandwidth)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        CHECK_AD936X(ad9361_get_tx_rf_bandwidth(phy, bandwidth));
    } else {
        CHECK_AD936X(ad9361_get_rx_rf_bandwidth(phy, bandwidth));
    }

    return 0;
}

static int _rfic_host_set_bandwidth(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bladerf_bandwidth bandwidth,
                                    bladerf_bandwidth *actual)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;
    struct controller_fns const *rfic      = board_data->rfic;
    struct bladerf_range const *range      = NULL;

    CHECK_STATUS(dev->board->get_bandwidth_range(dev, ch, &range));

    bandwidth = (unsigned int)clamp_to_range(range, bandwidth);

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        CHECK_AD936X(ad9361_set_tx_rf_bandwidth(phy, bandwidth));
    } else {
        CHECK_AD936X(ad9361_set_rx_rf_bandwidth(phy, bandwidth));
    }

    if (actual != NULL) {
        return rfic->get_bandwidth(dev, ch, actual);
    }

    return 0;
}


/******************************************************************************/
/* Gain mode */
/******************************************************************************/

static int _rfic_host_get_gain_mode(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bladerf_gain_mode *mode)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;
    uint8_t const rfic_ch                  = ch >> 1;
    uint8_t gc_mode;
    bool ok;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        log_warning("%s: automatic gain control not valid for TX channels\n",
                    __FUNCTION__);
        *mode = BLADERF_GAIN_DEFAULT;
        return 0;
    }

    /* Get the gain */
    CHECK_AD936X(ad9361_get_rx_gain_control_mode(phy, rfic_ch, &gc_mode));

    /* Mode conversion */
    if (mode != NULL) {
        *mode = gainmode_ad9361_to_bladerf(gc_mode, &ok);
        if (!ok) {
            RETURN_INVAL("mode", "is not valid");
        }
    }

    return 0;
}

static int _rfic_host_set_gain_mode(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bladerf_gain_mode mode)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;
    uint8_t const rfic_ch                  = ch >> 1;
    enum rf_gain_ctrl_mode gc_mode;
    bool ok;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        log_warning("%s: automatic gain control not valid for TX channels\n",
                    __FUNCTION__);
        return 0;
    }

    /* Channel conversion */
    switch (ch) {
        case BLADERF_CHANNEL_RX(0):
            gc_mode = ((AD9361_InitParam *)board_data->rfic_init_params)->gc_rx1_mode;
            break;

        case BLADERF_CHANNEL_RX(1):
            gc_mode = ((AD9361_InitParam *)board_data->rfic_init_params)->gc_rx2_mode;
            break;

        default:
            log_error("%s: unknown channel index (%d)\n", __FUNCTION__, ch);
            return BLADERF_ERR_UNSUPPORTED;
    }

    /* Mode conversion */
    if (mode != BLADERF_GAIN_DEFAULT) {
        gc_mode = gainmode_bladerf_to_ad9361(mode, &ok);
        if (!ok) {
            RETURN_INVAL("mode", "is not valid");
        }
    }

    /* Set the mode! */
    CHECK_AD936X(ad9361_set_rx_gain_control_mode(phy, rfic_ch, gc_mode));

    return 0;
}


/******************************************************************************/
/* Gain */
/* These functions handle offset */
/******************************************************************************/

static int _rfic_host_get_gain(struct bladerf *dev,
                               bladerf_channel ch,
                               int *gain)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;
    struct controller_fns const *rfic      = board_data->rfic;
    int val;
    float offset;

    CHECK_STATUS(get_gain_offset(dev, ch, &offset));

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        bool muted;

        CHECK_STATUS(txmute_get(phy, ch, &muted));

        if (muted) {
            struct bladerf_range const *range = NULL;

            CHECK_STATUS(
                dev->board->get_gain_stage_range(dev, ch, "dsa", &range));

            val = -__unscale_int(range, txmute_get_cached(phy, ch));
        } else {
            CHECK_STATUS(rfic->get_gain_stage(dev, ch, "dsa", &val));
        }
    } else {
        CHECK_STATUS(rfic->get_gain_stage(dev, ch, "full", &val));
    }

    *gain = __round_int(val + offset);

    return 0;
}

static int _rfic_host_set_gain(struct bladerf *dev,
                               bladerf_channel ch,
                               int gain)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;
    struct controller_fns const *rfic      = board_data->rfic;
    int val;
    float offset;

    CHECK_STATUS(get_gain_offset(dev, ch, &offset));

    val = gain - offset;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        bool muted;

        CHECK_STATUS(txmute_get(phy, ch, &muted));

        if (muted) {
            struct bladerf_range const *range = NULL;

            CHECK_STATUS(
                dev->board->get_gain_stage_range(dev, ch, "dsa", &range));

            CHECK_STATUS(txmute_set_cached(phy, ch, -__scale_int(range, val)));
        } else {
            CHECK_STATUS(rfic->set_gain_stage(dev, ch, "dsa", val));
        }
    } else {
        CHECK_STATUS(rfic->set_gain_stage(dev, ch, "full", val));
    }

    return 0;
}


/******************************************************************************/
/* Gain stage */
/* These functions handle scaling */
/******************************************************************************/

static int _rfic_host_get_gain_stage(struct bladerf *dev,
                                     bladerf_channel ch,
                                     char const *stage,
                                     int *gain)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;
    struct bladerf_range const *range      = NULL;
    uint8_t const rfic_ch                  = ch >> 1;
    int val;

    CHECK_STATUS(dev->board->get_gain_stage_range(dev, ch, stage, &range));

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        if (strcmp(stage, "dsa") == 0) {
            uint32_t atten;

            CHECK_AD936X(ad9361_get_tx_attenuation(phy, rfic_ch, &atten));

            val = -atten;
        } else {
            log_error("%s: gain stage '%s' invalid\n", __FUNCTION__, stage);
            return BLADERF_ERR_INVAL;
        }
    } else {
        struct rf_rx_gain rx_gain;
        CHECK_AD936X(ad9361_get_rx_gain(phy, rfic_ch + 1, &rx_gain));

        if (strcmp(stage, "full") == 0) {
            val = rx_gain.gain_db;
        } else if (strcmp(stage, "digital") == 0) {
            val = rx_gain.digital_gain;
        } else {
            log_error("%s: gain stage '%s' invalid\n", __FUNCTION__, stage);
            return BLADERF_ERR_INVAL;
        }
    }

    *gain = __unscale_int(range, val);

    return 0;
}

static int _rfic_host_set_gain_stage(struct bladerf *dev,
                                     bladerf_channel ch,
                                     char const *stage,
                                     int gain)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;
    struct bladerf_range const *range      = NULL;
    uint8_t const rfic_ch                  = ch >> 1;
    int val;

    CHECK_STATUS(dev->board->get_gain_stage_range(dev, ch, stage, &range));

    /* Scale/round/clamp as required */
    if (BLADERF_CHANNEL_IS_TX(ch) && gain < -89) {
        val = -89750;
    } else {
        val = __scale_int(range, clamp_to_range(range, gain));
    }

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        if (strcmp(stage, "dsa") == 0) {
            CHECK_AD936X(ad9361_set_tx_attenuation(phy, rfic_ch, -val));
        } else {
            log_warning("%s: gain stage '%s' invalid\n", __FUNCTION__, stage);
            return BLADERF_ERR_INVAL;
        }
    } else {
        if (strcmp(stage, "full") == 0) {
            CHECK_AD936X(ad9361_set_rx_rf_gain(phy, rfic_ch, val));
        } else {
            log_warning("%s: gain stage '%s' invalid\n", __FUNCTION__, stage);
            return BLADERF_ERR_INVAL;
        }
    }

    return 0;
}


/******************************************************************************/
/* RSSI */
/******************************************************************************/

static int _rfic_host_get_rssi(struct bladerf *dev,
                               bladerf_channel ch,
                               int *pre_rssi,
                               int *sym_rssi)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;
    uint8_t const rfic_ch                  = ch >> 1;

    int pre, sym;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        uint32_t rssi = 0;

        CHECK_AD936X(ad9361_get_tx_rssi(phy, rfic_ch, &rssi));

        pre = __round_int(rssi / 1000.0);
        sym = __round_int(rssi / 1000.0);
    } else {
        struct rf_rssi rssi;

        CHECK_AD936X(ad9361_get_rx_rssi(phy, rfic_ch, &rssi));

        pre = __round_int(rssi.preamble / (float)rssi.multiplier);
        sym = __round_int(rssi.symbol / (float)rssi.multiplier);
    }

    if (NULL != pre_rssi) {
        *pre_rssi = -pre;
    }

    if (NULL != sym_rssi) {
        *sym_rssi = -sym;
    }

    return 0;
}


/******************************************************************************/
/* Filter */
/******************************************************************************/

static int _rfic_host_get_filter(struct bladerf *dev,
                                 bladerf_channel ch,
                                 bladerf_rfic_rxfir *rxfir,
                                 bladerf_rfic_txfir *txfir)
{
    struct bladerf2_board_data *board_data = dev->board_data;

    if (NULL != rxfir) {
        *rxfir = board_data->rxfir;
    }

    if (NULL != txfir) {
        *txfir = board_data->txfir;
    }

    return 0;
}

static int _rfic_host_set_filter(struct bladerf *dev,
                                 bladerf_channel ch,
                                 bladerf_rfic_rxfir rxfir,
                                 bladerf_rfic_txfir txfir)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        AD9361_TXFIRConfig *fir_config = NULL;
        uint8_t enable;

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
                MUTEX_UNLOCK(&dev->lock);
                assert(!"Bug: unhandled txfir selection");
                return BLADERF_ERR_UNEXPECTED;
        }

        CHECK_AD936X(ad9361_set_tx_fir_config(phy, *fir_config));
        CHECK_AD936X(ad9361_set_tx_fir_en_dis(phy, enable));

        board_data->txfir = txfir;
    } else {
        AD9361_RXFIRConfig *fir_config = NULL;
        uint8_t enable;

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
                MUTEX_UNLOCK(&dev->lock);
                assert(!"Bug: unhandled rxfir selection");
                return BLADERF_ERR_UNEXPECTED;
        }

        CHECK_AD936X(ad9361_set_rx_fir_config(phy, *fir_config));
        CHECK_AD936X(ad9361_set_rx_fir_en_dis(phy, enable));

        board_data->rxfir = rxfir;
    }

    return 0;
}


/******************************************************************************/
/* TX Mute */
/******************************************************************************/

static int _rfic_host_get_txmute(struct bladerf *dev,
                                 bladerf_channel ch,
                                 bool *state)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        return txmute_get(phy, ch, state);
    }

    return BLADERF_ERR_UNSUPPORTED;
}

static int _rfic_host_set_txmute(struct bladerf *dev,
                                 bladerf_channel ch,
                                 bool state)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        return txmute_set(phy, ch, state);
    }

    return BLADERF_ERR_UNSUPPORTED;
}


/******************************************************************************/
/* Fastlock */
/******************************************************************************/

static int _rfic_host_store_fastlock_profile(struct bladerf *dev,
                                             bladerf_channel ch,
                                             uint32_t profile)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        CHECK_AD936X(ad9361_tx_fastlock_store(phy, profile));
    } else {
        CHECK_AD936X(ad9361_rx_fastlock_store(phy, profile));
    }

    return 0;
}


/******************************************************************************/
/* Function pointers */
/******************************************************************************/

struct controller_fns const rfic_host_control = {
    FIELD_INIT(.is_present, _rfic_host_is_present),
    FIELD_INIT(.is_standby, _rfic_host_is_standby),
    FIELD_INIT(.is_initialized, _rfic_host_is_initialized),
    FIELD_INIT(.get_init_state, _rfic_host_get_init_state),

    FIELD_INIT(.initialize, _rfic_host_initialize),
    FIELD_INIT(.standby, _rfic_host_standby),
    FIELD_INIT(.deinitialize, _rfic_host_deinitialize),

    FIELD_INIT(.enable_module, _rfic_host_enable_module),

    FIELD_INIT(.get_sample_rate, _rfic_host_get_sample_rate),
    FIELD_INIT(.set_sample_rate, _rfic_host_set_sample_rate),

    FIELD_INIT(.get_frequency, _rfic_host_get_frequency),
    FIELD_INIT(.set_frequency, _rfic_host_set_frequency),
    FIELD_INIT(.select_band, _rfic_host_select_band),

    FIELD_INIT(.get_bandwidth, _rfic_host_get_bandwidth),
    FIELD_INIT(.set_bandwidth, _rfic_host_set_bandwidth),

    FIELD_INIT(.get_gain_mode, _rfic_host_get_gain_mode),
    FIELD_INIT(.set_gain_mode, _rfic_host_set_gain_mode),

    FIELD_INIT(.get_gain, _rfic_host_get_gain),
    FIELD_INIT(.set_gain, _rfic_host_set_gain),

    FIELD_INIT(.get_gain_stage, _rfic_host_get_gain_stage),
    FIELD_INIT(.set_gain_stage, _rfic_host_set_gain_stage),

    FIELD_INIT(.get_rssi, _rfic_host_get_rssi),

    FIELD_INIT(.get_filter, _rfic_host_get_filter),
    FIELD_INIT(.set_filter, _rfic_host_set_filter),

    FIELD_INIT(.get_txmute, _rfic_host_get_txmute),
    FIELD_INIT(.set_txmute, _rfic_host_set_txmute),

    FIELD_INIT(.store_fastlock_profile, _rfic_host_store_fastlock_profile),

    FIELD_INIT(.command_mode, RFIC_COMMAND_HOST),
};
