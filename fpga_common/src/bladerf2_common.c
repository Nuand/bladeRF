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

#ifdef BLADERF_NIOS_BUILD
#include "devices.h"
#endif  // BLADERF_NIOS_BUILD

/* Avoid building this in low-memory situations */
#if !defined(BLADERF_NIOS_BUILD) || defined(BLADERF_NIOS_LIBAD936X)

#include "bladerf2_common.h"

/**
 * Value of the current sense resistor (R132)
 */
float const ina219_r_shunt = 0.001F;

int errno_ad9361_to_bladerf(int err)
{
    if (err >= 0) {
        return 0;
    }

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

struct band_port_map const *_get_band_port_map_by_freq(bladerf_channel ch,
                                                       bladerf_frequency freq)
{
    struct band_port_map const *port_map;
    size_t port_map_len;
    int64_t freqi = (int64_t)freq;
    size_t i;

    /* Select the band->port map for RX vs TX */
    if (BLADERF_CHANNEL_IS_TX(ch)) {
        port_map     = bladerf2_tx_band_port_map;
        port_map_len = ARRAY_SIZE(bladerf2_tx_band_port_map);
    } else {
        port_map     = bladerf2_rx_band_port_map;
        port_map_len = ARRAY_SIZE(bladerf2_rx_band_port_map);
    }

    if (NULL == port_map) {
        return NULL;
    }

    /* Search through the band->port map for the desired band */
    for (i = 0; i < port_map_len; i++) {
        if (is_within_range(&port_map[i].frequency, freqi)) {
            return &port_map[i];
        }
    }

    /* Wasn't found, return a null ptr */
    return NULL;
}

int _modify_spdt_bits_by_freq(uint32_t *reg,
                              bladerf_channel ch,
                              bool enabled,
                              bladerf_frequency freq)
{
    struct band_port_map const *port_map;
    uint32_t shift;

    if (NULL == reg) {
        return BLADERF_ERR_INVAL;
    }

    /* Look up the port configuration for this frequency */
    port_map = _get_band_port_map_by_freq(ch, enabled ? freq : 0);

    if (NULL == port_map) {
        return BLADERF_ERR_INVAL;
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
            return BLADERF_ERR_INVAL;
    }

    *reg &= ~(RFFE_CONTROL_SPDT_MASK << shift);
    *reg |= port_map->spdt << shift;

    return 0;
}

int _get_rffe_control_bit_for_dir(bladerf_direction dir)
{
    switch (dir) {
        case BLADERF_RX:
            return RFFE_CONTROL_ENABLE;
        case BLADERF_TX:
            return RFFE_CONTROL_TXNRX;
        default:
            return UINT32_MAX;
    }
}

int _get_rffe_control_bit_for_ch(bladerf_channel ch)
{
    switch (ch) {
        case BLADERF_CHANNEL_RX(0):
            return RFFE_CONTROL_MIMO_RX_EN_0;
        case BLADERF_CHANNEL_RX(1):
            return RFFE_CONTROL_MIMO_RX_EN_1;
        case BLADERF_CHANNEL_TX(0):
            return RFFE_CONTROL_MIMO_TX_EN_0;
        case BLADERF_CHANNEL_TX(1):
            return RFFE_CONTROL_MIMO_TX_EN_1;
        default:
            return UINT32_MAX;
    }
}

bool _rffe_ch_enabled(uint32_t reg, bladerf_channel ch)
{
    return (reg >> _get_rffe_control_bit_for_ch(ch)) & 0x1;
}

bool _rffe_dir_enabled(uint32_t reg, bladerf_direction dir)
{
    return (reg >> _get_rffe_control_bit_for_dir(dir)) & 0x1;
}

bool _rffe_dir_otherwise_enabled(uint32_t reg, bladerf_channel ch)
{
    switch (ch) {
        case BLADERF_CHANNEL_RX(0):
            return _rffe_ch_enabled(reg, BLADERF_CHANNEL_RX(1));
        case BLADERF_CHANNEL_RX(1):
            return _rffe_ch_enabled(reg, BLADERF_CHANNEL_RX(0));
        case BLADERF_CHANNEL_TX(0):
            return _rffe_ch_enabled(reg, BLADERF_CHANNEL_TX(1));
        case BLADERF_CHANNEL_TX(1):
            return _rffe_ch_enabled(reg, BLADERF_CHANNEL_TX(0));
    }

    return false;
}

#endif  // !defined(BLADERF_NIOS_BUILD) || defined(BLADERF_NIOS_LIBAD936X)
