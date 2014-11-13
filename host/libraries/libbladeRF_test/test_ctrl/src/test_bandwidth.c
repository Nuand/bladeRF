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

static int set_and_check(struct bladerf *dev, bladerf_module m,
                         unsigned int bandwidth)
{
    int status;
    unsigned int actual, readback;

    status = bladerf_set_bandwidth(dev, m, bandwidth, &actual);
    if (status != 0) {
        PR_ERROR("Failed to set bandwidth: %s\n",
                 bladerf_strerror(status));
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

static unsigned int sweep_bandwidths(struct bladerf *dev, bladerf_module m)
{
    int status;
    unsigned int b;
    const unsigned int inc = 250000;
    unsigned int failures = 0;

    for (b = BLADERF_BANDWIDTH_MIN; b < BLADERF_BANDWIDTH_MAX; b += inc) {
        status = set_and_check(dev, m, b);
        if (status != 0) {
            failures++;
        }
    }

    return failures;
}

static unsigned int random_bandwidths(struct bladerf *dev, bladerf_module m,
                                      struct app_params *p)
{
    int status;
    unsigned int bw, i;
    unsigned int failures = 0;
    const unsigned int iterations = 1500;

    for (i = 0; i < iterations; i++) {
        const unsigned int mod =
            BLADERF_BANDWIDTH_MAX - BLADERF_BANDWIDTH_MIN + 1;

        randval_update(&p->randval_state);

        bw = BLADERF_BANDWIDTH_MIN + (p->randval_state % mod);
        assert(bw <= BLADERF_BANDWIDTH_MAX);

        status = set_and_check(dev, m, bw);
        if (status != 0) {
            failures++;
        }
    }

    return failures;
}

unsigned int test_bandwidth(struct bladerf *dev,
                            struct app_params *p, bool quiet)
{
    unsigned int failures = 0;

    PRINT("%s: Sweeping RX bandwidths...\n", __FUNCTION__);
    failures += sweep_bandwidths(dev, BLADERF_MODULE_RX);

    PRINT("%s: Applying random RX bandwidths...\n", __FUNCTION__);
    failures += random_bandwidths(dev, BLADERF_MODULE_RX, p);

    PRINT("%s: Sweeping TX bandwidths...\n", __FUNCTION__);
    failures += sweep_bandwidths(dev, BLADERF_MODULE_TX);

    PRINT("%s: Applying random TX bandwidths...\n", __FUNCTION__);
    failures += random_bandwidths(dev, BLADERF_MODULE_TX, p);

    return failures;
}
