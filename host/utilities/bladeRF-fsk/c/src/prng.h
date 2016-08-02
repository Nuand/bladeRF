/**
 * @file
 * @brief   XORshift Pseudorandom number generator
 *
 * https://en.wikipedia.org/wiki/Xorshift#xorshift.2A
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

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include "host_config.h"

/**
 * Update the PRNG value
 *
 * @param   state_in    Input PRNG state. Must be non-zero.
 *
 * @return next PRNG value in the sequence
 */
static inline uint64_t prng_update(uint64_t state_in)
{
    uint64_t state = state_in;
    assert(state != 0);

    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;
    state *= 0x2545f4914f6cdd1d;

    return state;
}

/**
 * Allocate and fill a byte array with PRNG values. The caller is responsible
 * for freeing the returned buffer.
 *
 * @param[inout]  state    PRNG state or seed. Updated with new state.
 * @param[in]     len      Desired buffer length, in bytes. Recommended
 *                         to be a multiple of 8.
 *
 * @return Heap-allocated buffer filled with PRNG output on success,
 *         NULL on failure.
 */
uint8_t * prng_fill(uint64_t *state, size_t len);
