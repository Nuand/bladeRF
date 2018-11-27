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

#ifdef BLADERF_NIOS_BUILD
#include "devices.h"
#endif  // BLADERF_NIOS_BUILD

/* Avoid building this in low-memory situations */
#if !defined(BLADERF_NIOS_BUILD) || defined(BLADERF_NIOS_LIBAD936X)

#if !defined(BLADERF_NIOS_BUILD) && !defined(BLADERF_NIOS_PC_SIMULATION)
#include "log.h"
#endif

#include "ad936x_helpers.h"
#include "bladerf2_common.h"

static bool tx_mute_state[2] = { false };

uint32_t txmute_get_cached(struct ad9361_rf_phy *phy, bladerf_channel ch)
{
    switch (ch) {
        case BLADERF_CHANNEL_TX(0):
            return phy->tx1_atten_cached;
        case BLADERF_CHANNEL_TX(1):
            return phy->tx2_atten_cached;
        default:
            return 0;
    }
}

int txmute_set_cached(struct ad9361_rf_phy *phy,
                      bladerf_channel ch,
                      uint32_t atten)
{
    switch (ch) {
        case BLADERF_CHANNEL_TX(0):
            phy->tx1_atten_cached = atten;
            return 0;
        case BLADERF_CHANNEL_TX(1):
            phy->tx2_atten_cached = atten;
            return 0;
        default:
            return BLADERF_ERR_INVAL;
    }
}

int txmute_get(struct ad9361_rf_phy *phy, bladerf_channel ch, bool *state)
{
    int rfic_ch = (ch >> 1);

    *state = tx_mute_state[rfic_ch];

    return 0;
}

int txmute_set(struct ad9361_rf_phy *phy, bladerf_channel ch, bool state)
{
    int rfic_ch                = (ch >> 1);
    uint32_t const MUTED_ATTEN = 89750;
    uint32_t atten, cached;
    int status;

    if (tx_mute_state[rfic_ch] == state) {
        // short circuit if there's no change
        return 0;
    }

    if (state) {
        // mute: save the existing value before muting
        uint32_t readval;

        status = ad9361_get_tx_attenuation(phy, rfic_ch, &readval);
        if (status < 0) {
            return errno_ad9361_to_bladerf(status);
        }

        cached = readval;
        atten  = MUTED_ATTEN;
    } else {
        // unmute: restore the saved value
        cached = txmute_get_cached(phy, ch);
        atten  = cached;
    }

    status = ad9361_set_tx_attenuation(phy, rfic_ch, atten);
    if (status < 0) {
        return errno_ad9361_to_bladerf(status);
    }

    status = txmute_set_cached(phy, ch, cached);
    if (status < 0) {
        return status;
    }

    tx_mute_state[rfic_ch] = state;

    return 0;
}

int set_ad9361_port_by_freq(struct ad9361_rf_phy *phy,
                            bladerf_channel ch,
                            bool enabled,
                            bladerf_frequency freq)
{
    struct band_port_map const *port_map = NULL;
    int status;

    /* Look up the port configuration for this frequency */
    port_map = _get_band_port_map_by_freq(ch, enabled ? freq : 0);

    if (NULL == port_map) {
        return BLADERF_ERR_INVAL;
    }

    /* Set the AD9361 port accordingly */
    if (BLADERF_CHANNEL_IS_TX(ch)) {
        status = ad9361_set_tx_rf_port_output(phy, port_map->rfic_port);
    } else {
        status = ad9361_set_rx_rf_port_input(phy, port_map->rfic_port);
    }

    return errno_ad9361_to_bladerf(status);
}

enum rf_gain_ctrl_mode gainmode_bladerf_to_ad9361(bladerf_gain_mode gainmode,
                                                  bool *ok)
{
    struct bladerf_rfic_gain_mode_map const *mode_map;
    size_t mode_map_len;
    size_t i;

    mode_map     = bladerf2_rx_gain_mode_map;
    mode_map_len = ARRAY_SIZE(bladerf2_rx_gain_mode_map);

    if (NULL != ok) {
        *ok = false;
    }

    for (i = 0; i < mode_map_len; ++i) {
        if (mode_map[i].brf_mode == gainmode) {
            if (NULL != ok) {
                *ok = true;
            }
            return mode_map[i].rfic_mode;
        }
    }

    return 0;
};

bladerf_gain_mode gainmode_ad9361_to_bladerf(enum rf_gain_ctrl_mode gainmode,
                                             bool *ok)
{
    struct bladerf_rfic_gain_mode_map const *mode_map;
    size_t mode_map_len;
    size_t i;

    mode_map     = bladerf2_rx_gain_mode_map;
    mode_map_len = ARRAY_SIZE(bladerf2_rx_gain_mode_map);

    if (NULL != ok) {
        *ok = false;
    }

    for (i = 0; i < mode_map_len; ++i) {
        if (mode_map[i].rfic_mode == gainmode) {
            if (NULL != ok) {
                *ok = true;
            }
            return mode_map[i].brf_mode;
        }
    }

    return 0;
}

#endif  // !defined(BLADERF_NIOS_BUILD) || defined(BLADERF_NIOS_LIBAD936X)
