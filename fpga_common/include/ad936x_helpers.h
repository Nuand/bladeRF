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

#ifndef FPGA_COMMON_AD936X_HELPERS_H_
#define FPGA_COMMON_AD936X_HELPERS_H_

#ifdef BLADERF_NIOS_BUILD
#include "devices.h"
#endif  // BLADERF_NIOS_BUILD

#if !defined(BLADERF_NIOS_BUILD) || defined(BLADERF_NIOS_LIBAD936X)

#if !defined(BLADERF_NIOS_BUILD) && !defined(BLADERF_NIOS_PC_SIMULATION)
#include <libbladeRF.h>
#else
#include "libbladeRF_nios_compat.h"
#endif

#include "ad936x.h"

/**
 * @brief       Retrieve current value in TX attenuation cache.
 *
 * @param       phy     RFIC handle
 * @param[in]   ch      Channel
 *
 * @return      Cached attenuation value
 */
uint32_t txmute_get_cached(struct ad9361_rf_phy *phy, bladerf_channel ch);

/**
 * @brief       Save a new value to the TX attenuation cache.
 *
 * @param       phy     RFIC handle
 * @param[in]   ch      Channel
 * @param[in]   atten   Attenuation
 *
 * @return      0 on success, value from \ref RETCODES list on failure
 */
int txmute_set_cached(struct ad9361_rf_phy *phy,
                      bladerf_channel ch,
                      uint32_t atten);

/**
 * @brief       Get the transmit mute state
 *
 * @param       phy     RFIC handle
 * @param[in]   ch      Channel
 * @param[out]  state   Mute state: true for muted, false for unmuted
 *
 * @return      0 on success, value from \ref RETCODES list on failure
 */
int txmute_get(struct ad9361_rf_phy *phy, bladerf_channel ch, bool *state);

/**
 * @brief       Sets the transmit mute.
 *
 * If muted, the TX attenuation will be set to maximum to reduce leakage
 * as much as possible.
 *
 * When unmuted, TX attenuation will be restored to its previous value.
 *
 * @param       phy     RFIC handle
 * @param[in]   ch      Channel
 * @param[in]   state   Mute state: true for muted, false for unmuted
 *
 * @return      0 on success, value from \ref RETCODES list on failure
 */
int txmute_set(struct ad9361_rf_phy *phy, bladerf_channel ch, bool state);

/**
 * @brief       Set AD9361 RFIC RF port
 *
 * @param       phy     RFIC handle
 * @param[in]   ch      Channel
 * @param[in]   enabled True if the channel is enabled, false otherwise
 * @param[in]   freq    Frequency
 *
 * @return      0 on success, value from \ref RETCODES list on failure
 */
int set_ad9361_port_by_freq(struct ad9361_rf_phy *phy,
                            bladerf_channel ch,
                            bool enabled,
                            bladerf_frequency freq);

/**
 * @brief       Translate bladerf_gain_mode to rf_gain_ctrl_mode
 *
 * @param[in]   gainmode    The libbladeRF gainmode
 * @param[out]  ok          True if return value is valid, false otherwise
 *
 * @return      rf_gain_ctrl_mode
 */
enum rf_gain_ctrl_mode gainmode_bladerf_to_ad9361(bladerf_gain_mode gainmode,
                                                  bool *ok);

/**
 * @brief       Translate rf_gain_ctrl_mode to bladerf_gain_mode
 *
 * @param[in]   gainmode    The RFIC gainmode
 * @param[out]  ok          True if return value is valid, false otherwise
 *
 * @return      bladerf_gain_mode
 */
bladerf_gain_mode gainmode_ad9361_to_bladerf(enum rf_gain_ctrl_mode gainmode,
                                             bool *ok);

#endif  // !defined(BLADERF_NIOS_BUILD) || defined(BLADERF_NIOS_LIBAD936X)
#endif  // FPGA_COMMON_AD936X_HELPERS_H_
