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
 * Convert SC16Q11 samples to bytes
 */
static inline size_t sc16q11_to_bytes(size_t n_samples)
{
    const size_t sample_size = 2 * sizeof(int16_t);
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

/* Covert samples to bytes based upon the provided format */
static inline size_t samples_to_bytes(bladerf_format format, size_t n)
{
    switch (format) {
        case BLADERF_FORMAT_SC16_Q11:
        case BLADERF_FORMAT_SC16_Q11_META:
            return sc16q11_to_bytes(n);

        default:
            assert(!"Invalid format");
            return 0;
    }
}

/* Convert bytes to samples based upon the provided format */
static inline size_t bytes_to_samples(bladerf_format format, size_t n)
{
    switch (format) {
        case BLADERF_FORMAT_SC16_Q11:
        case BLADERF_FORMAT_SC16_Q11_META:
            return bytes_to_sc16q11(n);

        default:
            assert(!"Invalid format");
            return 0;
    }
}

#endif
