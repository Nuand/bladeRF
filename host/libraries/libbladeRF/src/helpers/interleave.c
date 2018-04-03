/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2017 Nuand LLC
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

#include "helpers/interleave.h"

size_t _interleave_calc_num_channels(bladerf_channel_layout layout)
{
    switch (layout) {
        case BLADERF_RX_X1:
        case BLADERF_TX_X1:
            return 1;
        case BLADERF_RX_X2:
        case BLADERF_TX_X2:
            return 2;
    }

    return 0;
}

size_t _interleave_calc_bytes_per_sample(bladerf_format format)
{
    switch (format) {
        case BLADERF_FORMAT_SC16_Q11:
        case BLADERF_FORMAT_SC16_Q11_META:
            return 4;
    }

    return 0;
}

size_t _interleave_calc_metadata_bytes(bladerf_format format)
{
    switch (format) {
        case BLADERF_FORMAT_SC16_Q11_META:
            return 0x10;
        case BLADERF_FORMAT_SC16_Q11:
            return 0;
    }

    return 0;
}

int _interleave_interleave_buf(bladerf_channel_layout layout,
                               bladerf_format format,
                               unsigned int buffer_size,
                               void *samples)
{
    void *buf;
    uint8_t *srcptr, *dstptr;
    size_t num_channels = _interleave_calc_num_channels(layout);
    size_t samp_size, meta_size, samps_per_ch;
    size_t srcidx, dstidx, samp, ch;

    // Easy:
    if (num_channels < 2) {
        return 0;
    }

    // Placeholder for an actually efficient algorithm
    samp_size    = _interleave_calc_bytes_per_sample(format);
    meta_size    = _interleave_calc_metadata_bytes(format);
    samps_per_ch = buffer_size / num_channels;
    buf          = malloc(samp_size * buffer_size);
    srcptr       = samples;
    dstptr       = buf;

    if (NULL == buf) {
        return BLADERF_ERR_MEM;
    }

    // Copy metadata if applicable
    if (meta_size > 0) {
        memcpy(dstptr, srcptr, meta_size);
        srcptr += meta_size;
        dstptr += meta_size;
        samps_per_ch -= (meta_size / samp_size / num_channels);
    }

    // Iterate...
    for (ch = 0; ch < num_channels; ++ch) {
        srcidx = samps_per_ch * ch;
        for (samp = 0; samp < samps_per_ch; ++samp) {
            dstidx = (samp * num_channels) + ch;
            memcpy(dstptr + (dstidx * samp_size),
                   srcptr + ((srcidx + samp) * samp_size),
                   samp_size);
        }
    }

    // Copy back...
    memcpy(samples, buf, buffer_size * samp_size);

    // Done
    free(buf);

    return 0;
}

int _interleave_deinterleave_buf(bladerf_channel_layout layout,
                                 bladerf_format format,
                                 unsigned int buffer_size,
                                 void *samples)
{
    void *buf;
    uint8_t *srcptr, *dstptr;
    size_t num_channels = _interleave_calc_num_channels(layout);
    size_t samp_size, meta_size, samps_per_ch;
    size_t srcidx, dstidx, samp, ch;

    // Easy:
    if (num_channels < 2) {
        return 0;
    }

    // Placeholder for an actually efficient algorithm
    samp_size    = _interleave_calc_bytes_per_sample(format);
    meta_size    = _interleave_calc_metadata_bytes(format);
    samps_per_ch = buffer_size / num_channels;
    buf          = malloc(samp_size * buffer_size);
    srcptr       = samples;
    dstptr       = buf;

    if (NULL == buf) {
        return BLADERF_ERR_MEM;
    }

    // Copy metadata if applicable
    if (meta_size > 0) {
        memcpy(dstptr, srcptr, meta_size);
        srcptr += meta_size;
        dstptr += meta_size;
        samps_per_ch -= (meta_size / samp_size / num_channels);
    }

    // Iterate...
    for (samp = 0; samp < samps_per_ch; ++samp) {
        srcidx = num_channels * samp;
        for (ch = 0; ch < num_channels; ++ch) {
            dstidx = (samps_per_ch * ch) + samp;
            memcpy(dstptr + (dstidx * samp_size),
                   srcptr + ((srcidx + ch) * samp_size), samp_size);
        }
    }

    // Copy back...
    memcpy(samples, buf, buffer_size * samp_size);

    // Done
    free(buf);

    return 0;
}
