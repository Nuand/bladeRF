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
#include "test_ctrl.h"

DECLARE_TEST_CASE(bandwidth);


static int set_and_check(struct bladerf *dev,
                         bladerf_module m,
                         bladerf_bandwidth bandwidth)
{
    bladerf_bandwidth actual, readback;
    int status;

    status = bladerf_set_bandwidth(dev, m, bandwidth, &actual);
    if (status != 0) {
        PR_ERROR("Failed to set bandwidth: %s\n", bladerf_strerror(status));
        return status;
    }

    status = bladerf_get_bandwidth(dev, m, &readback);
    if (status != 0) {
        PR_ERROR("Failed to read back bandwidth: %s\n",
                 bladerf_strerror(status));
        return status;
    }

    if (readback != actual) {
        PR_ERROR("Unexpected bandwidth. requested=%u, actual=%u, readback=%u\n",
                 bandwidth, actual, readback);
        return -1;
    }

    return 0;
}

static size_t sweep_bandwidths(struct bladerf *dev,
                               bladerf_module m,
                               bladerf_bandwidth min,
                               bladerf_bandwidth max)
{
    size_t const inc = 250000;

    bladerf_bandwidth b;
    size_t failures = 0;
    int status;

    for (b = min; b < max; b += inc) {
        status = set_and_check(dev, m, b);
        if (status != 0) {
            failures++;
        }
    }

    return failures;
}

static size_t random_bandwidths(struct bladerf *dev,
                                bladerf_module m,
                                struct app_params *p,
                                bladerf_bandwidth min,
                                bladerf_bandwidth max)
{
    size_t const iterations = 1500;

    bladerf_bandwidth bw;
    size_t i;
    size_t failures = 0;
    int status;

    for (i = 0; i < iterations; i++) {
        bladerf_bandwidth const mod = max - min + 1;

        randval_update(&p->randval_state);

        bw = min + (p->randval_state % mod);
        assert(bw <= max);

        status = set_and_check(dev, m, bw);
        if (status != 0) {
            failures++;
        }
    }

    return failures;
}

unsigned int test_bandwidth(struct bladerf *dev,
                            struct app_params *p,
                            bool quiet)
{
    size_t failures = 0;
    int status;

    ITERATE_DIRECTIONS({
        ITERATE_CHANNELS(
            DIRECTION, 1, ({
                struct bladerf_range const *range;
                bladerf_bandwidth min, max;

                PRINT("%s: Testing %s...\n", __FUNCTION__,
                      direction2str(DIRECTION));

                status = bladerf_get_bandwidth_range(dev, CHANNEL, &range);
                if (status < 0) {
                    PR_ERROR("Failed to get %s bandwidth range: %s\n",
                             direction2str(DIRECTION),
                             bladerf_strerror(status));
                    return status;
                };

                min = (range->min * range->scale);
                max = (range->max * range->scale);

                PRINT("%s: %s range: %u to %u\n", __FUNCTION__,
                      direction2str(DIRECTION), min, max);

                PRINT("%s: Sweeping %s bandwidths...\n", __FUNCTION__,
                      direction2str(DIRECTION));
                failures += sweep_bandwidths(dev, CHANNEL, min, max);

                PRINT("%s: Applying random %s bandwidths...\n", __FUNCTION__,
                      direction2str(DIRECTION));
                failures += random_bandwidths(dev, CHANNEL, p, min, max);
            }));  // ITERATE_CHANNELS
    });           // ITERATE_DIRECTIONS

    return failures;
}
