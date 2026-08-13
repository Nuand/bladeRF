/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014 Nuand LLC
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

#ifndef STREAMING_FORMAT_H_
#define STREAMING_FORMAT_H_

#include "rel_assert.h"

/*
 * Convert SC8Q8 samples to bytes
 */
static inline size_t sc8q7_to_bytes(size_t n_samples)
{
    const size_t sample_size = 2 * sizeof(int8_t);
    assert(n_samples <= (SIZE_MAX / sample_size));
    return n_samples * sample_size;
}

/*
 * Convert bytes to SC8Q8 samples
 */
static inline size_t bytes_to_sc8q7(size_t n_bytes)
{
    const size_t sample_size = 2 * sizeof(int8_t);
    assert((n_bytes % sample_size) == 0);
    return n_bytes / sample_size;
}

/*
 * Convert SC16Q11 samples to bytes
 */
static inline size_t sc16q11_to_bytes(size_t n_samples)
{
    const size_t sample_size = 2 * sizeof(int16_t);
    assert(n_samples <= (SIZE_MAX / sample_size));
    return n_samples * sample_size;
}

/*
 * Convert SC16Q11 packed (SC12Q11) samples to bytes
 * I and Q are packed into 3 bytes. I and Q are 12 bits each.
 * That makes a sample 24 bits = 3 bytes.
 */
static inline size_t sc16q11_packed_to_bytes(size_t n_samples)
{
    const size_t sample_size = 3*sizeof(int8_t);
    assert(n_samples <= (SIZE_MAX / sample_size));
    return n_samples * sample_size;
}

/*
 * Convert bytes to SC16Q11 samples
 */
static inline size_t bytes_to_sc16q11(size_t n_bytes)
{
    const size_t sample_size = 2 * sizeof(int16_t);
    assert((n_bytes % sample_size) == 0);
    return n_bytes / sample_size;
}

static inline int16_t sign_extend_q11(uint16_t value)
{
    if ((value & 0x0800) != 0) {
        value |= 0xf000;
    }

    return (int16_t)value;
}

static inline void sc16q11_pack_in_place(void *samples, size_t num_samples)
{
    int16_t const *unpacked = (int16_t const *)samples;
    uint8_t *packed         = (uint8_t *)samples;
    size_t i;

    assert(num_samples <= SIZE_MAX / 4);

    for (i = 0; i < num_samples; ++i) {
        uint16_t i_sample = (uint16_t)unpacked[2 * i] & 0x0fff;
        uint16_t q_sample = (uint16_t)unpacked[2 * i + 1] & 0x0fff;

        packed[3 * i]     = (uint8_t)i_sample;
        packed[3 * i + 1] = (uint8_t)((i_sample >> 8) | (q_sample << 4));
        packed[3 * i + 2] = (uint8_t)(q_sample >> 4);
    }
}

static inline void sc16q11_unpack_in_place(void *samples, size_t num_samples)
{
    uint8_t const *packed = (uint8_t const *)samples;
    int16_t *unpacked     = (int16_t *)samples;
    size_t i;

    assert(num_samples <= SIZE_MAX / 4);

    for (i = num_samples; i > 0; --i) {
        size_t const sample = i - 1;
        uint16_t i_sample =
            (uint16_t)packed[3 * sample] |
            ((uint16_t)(packed[3 * sample + 1] & 0x0f) << 8);
        uint16_t q_sample =
            ((uint16_t)packed[3 * sample + 1] >> 4) |
            ((uint16_t)packed[3 * sample + 2] << 4);

        unpacked[2 * sample]     = sign_extend_q11(i_sample);
        unpacked[2 * sample + 1] = sign_extend_q11(q_sample);
    }
}

/* Convert samples to wire bytes based upon the provided format */
static inline size_t samples_to_wire_bytes(bladerf_format format, size_t n)
{
    switch (format) {
        case BLADERF_FORMAT_SC8_Q7:
        case BLADERF_FORMAT_SC8_Q7_META:
            return sc8q7_to_bytes(n);

        case BLADERF_FORMAT_SC16_Q11_PACKED:
            return sc16q11_packed_to_bytes(n);

        case BLADERF_FORMAT_SC16_Q11:
        case BLADERF_FORMAT_SC16_Q11_META:
            return sc16q11_to_bytes(n);

        case BLADERF_FORMAT_PACKET_META:
            return n*4;

        default:
            assert(!"Invalid format");
            return 0;
    }
}

/* Convert logical samples to bytes based upon the provided format */
static inline size_t samples_to_bytes(bladerf_format format, size_t n)
{
    if (format == BLADERF_FORMAT_SC16_Q11_PACKED) {
        return sc16q11_to_bytes(n);
    }

    return samples_to_wire_bytes(format, n);
}

/* Convert bytes to samples based upon the provided format */
static inline size_t bytes_to_samples(bladerf_format format, size_t n)
{
    switch (format) {
        case BLADERF_FORMAT_SC8_Q7:
        case BLADERF_FORMAT_SC8_Q7_META:
            return bytes_to_sc8q7(n);

        case BLADERF_FORMAT_SC16_Q11_PACKED:
            return n / 3;

        case BLADERF_FORMAT_SC16_Q11:
        case BLADERF_FORMAT_SC16_Q11_META:
            return bytes_to_sc16q11(n);

        case BLADERF_FORMAT_PACKET_META:
            return (n+3)/4;

        default:
            assert(!"Invalid format");
            return 0;
    }
}

#endif
