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

static failure_count sweep_bandwidths(struct bladerf *dev,
                                      bladerf_module m,
                                      bladerf_bandwidth min,
                                      bladerf_bandwidth max)
{
    bladerf_bandwidth const inc = 250000;

    bladerf_bandwidth b;
    failure_count failures = 0;
    int status;

    for (b = min; b < max; b += inc) {
        status = set_and_check(dev, m, b);
        if (status != 0) {
            failures++;
        }
    }

    return failures;
}

static failure_count random_bandwidths(struct bladerf *dev,
                                       bladerf_module m,
                                       struct app_params *p,
                                       bladerf_bandwidth min,
                                       bladerf_bandwidth max)
{
    size_t const iterations = 1500;

    bladerf_bandwidth bw;
    size_t i;
    failure_count failures = 0;
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

failure_count test_bandwidth(struct bladerf *dev,
                             struct app_params *p,
                             bool quiet)
{
    failure_count failures = 0;
    int status;

    bladerf_direction dir;

    FOR_EACH_DIRECTION(dir)
    {
        size_t i;
        bladerf_channel ch;

        FOR_EACH_CHANNEL(dir, 1, i, ch)
        {
            struct bladerf_range const *range;
            bladerf_bandwidth min, max;

            PRINT("%s: Testing %s...\n", __FUNCTION__, direction2str(dir));

            status = bladerf_get_bandwidth_range(dev, ch, &range);
            if (status < 0) {
                PR_ERROR("Failed to get %s bandwidth range: %s\n",
                         direction2str(dir), bladerf_strerror(status));
                return status;
            };

            min = (bladerf_bandwidth)(range->min * range->scale);
            max = (bladerf_bandwidth)(range->max * range->scale);

            PRINT("%s: %s range: %u to %u\n", __FUNCTION__, direction2str(dir),
                  min, max);

            PRINT("%s: Sweeping %s bandwidths...\n", __FUNCTION__,
                  direction2str(dir));
            failures += sweep_bandwidths(dev, ch, min, max);

            PRINT("%s: Applying random %s bandwidths...\n", __FUNCTION__,
                  direction2str(dir));
            failures += random_bandwidths(dev, ch, p, min, max);
        }
    }

    return failures;
}
