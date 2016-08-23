/**
 * @file
 * @brief   FIR filter coefficients for low-pass channel filter
 *
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2016 Nuand LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef RX_CHANNEL_FILTER_H_
#define RX_CHANNEL_FILTER_H_

#include <stdlib.h>

/**
 * 31st Order Equiripple Low-Pass Filter
 * Pass band: Fs / 16
 * Stop band: Fs / 6
 *
 * MATLAB command:
 *      taps = firpm(31, [0 1/8 1/3 1], [1 1 0 0]);
 *                      OR
 *      taps = remez(31, [0 1/8 1/3 1], [1 1 0 0]);
 *
 * For FSK implementation with mod_index=pi/2, SPS=8, this filter was designed to:
 *      - Pass the signal's main lobe
 *      - Transition in the signal's first side lobe
 *      - Provide >= 60 dB of rejection at and beyond the second side lobe
 */
static const float rx_ch_filter[] = {
  -0.001419485011355f,
  -0.002008975606412f,
  -0.001265131815619f,
   0.001949590314393f,
   0.006879213394692f,
   0.010409452708224f,
   0.008177393348267f,
  -0.002501030091427f,
  -0.019324528756445f,
  -0.033658544676913f,
  -0.032963705706846f,
  -0.006721606720265f,
   0.046859333762514f,
   0.117272556273597f,
   0.184063253582650f,
   0.224762721634701f,
   0.224762721634701f,
   0.184063253582650f,
   0.117272556273597f,
   0.046859333762514f,
  -0.006721606720265f,
  -0.032963705706846f,
  -0.033658544676913f,
  -0.019324528756445f,
  -0.002501030091427f,
   0.008177393348267f,
   0.010409452708224f,
   0.006879213394692f,
   0.001949590314393f,
  -0.001265131815619f,
  -0.002008975606412f,
  -0.001419485011355f,
};

static const size_t
    rx_ch_filter_len = sizeof(rx_ch_filter) / sizeof(rx_ch_filter[0]);

#endif
