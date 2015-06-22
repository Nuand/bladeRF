/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014-2015 Nuand LLC
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

#include <stdlib.h>
#include <stdint.h>
#include <libbladeRF.h>
#include "test_common.h"

double calc_avg_duration(const struct timespec *start,
                         const struct timespec *end,
                         double iterations)
{
    double start_d = start->tv_sec + start->tv_nsec * 1e-9;
    double end_d   = end->tv_sec   + end->tv_nsec * 1e-9;

    return (end_d - start_d) / iterations;
}

unsigned int get_rand_freq(uint64_t *prng_state, bool xb200_enabled)
{
    uint64_t tmp;
    unsigned int min_freq;

    if (xb200_enabled) {
        min_freq = BLADERF_FREQUENCY_MIN_XB200;
    } else {
        min_freq = BLADERF_FREQUENCY_MIN;
    }

    tmp = randval_update(prng_state);
    tmp = min_freq + tmp % (BLADERF_FREQUENCY_MAX - BLADERF_FREQUENCY_MIN);
    return (unsigned int) tmp;
}

int wait_for_timestamp(struct bladerf *dev, bladerf_module module,
                       uint64_t timestamp, unsigned int timeout_ms)
{
    int status;
    uint64_t curr_ts = 0;
    unsigned int slept_ms = 0;
    bool done;

    //printf("Waiting for 0x%"PRIx64"\r\n", timestamp);

    do {
        status = bladerf_get_timestamp(dev, module, &curr_ts);
        done = (status != 0) || curr_ts >= timestamp;

        //printf("Now = 0x%"PRIx64", done = %d\r\n", curr_ts , done);

        if (!done) {
            if (slept_ms > timeout_ms) {
                done = true;
                status = BLADERF_ERR_TIMEOUT;
            } else {
                usleep(10000);
                slept_ms += 10;
            }
        }
    } while (!done);

    return status;
}
