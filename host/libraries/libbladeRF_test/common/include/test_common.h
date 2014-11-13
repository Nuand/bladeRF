/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014 Nuand LLC
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
#ifndef TEST_COMMON_H_
#define TEST_COMMON_H_

#include <stdint.h>
#include "rel_assert.h"
#include "host_config.h"

/**
 * Initialize a seed for use with randval_update
 *
 * @param[out]   state      PRNG state
 * @param[in]    seed       PRNG seed value. Should not be 0.
 *
 */
static inline void randval_init(uint64_t *state, uint64_t seed) {
    if (seed == 0) {
        seed = UINT64_MAX;
    }

    *state = seed;
}

/**
 * Get the next PRNG value, using a simple xorshift implementation
 *
 * @param[inout] state      PRNG state
 *
 * @return  next PRNG value
 */
static inline uint64_t randval_update(uint64_t *state)
{
    assert(*state != 0);
    (*state) ^= (*state) >> 12;
    (*state) ^= (*state) << 25;
    (*state) ^= (*state) >> 27;
    (*state) *= 0x2545f4914f6cdd1d;
    return *state;
}

#endif
