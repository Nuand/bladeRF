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

DECLARE_TEST_CASE(loopback);

static int set_and_check(struct bladerf *dev, bladerf_loopback l)
{
    int status, status_disable_lb;
    bladerf_loopback readback;

    status = bladerf_set_loopback(dev, l);
    if (status != 0) {
        PR_ERROR("Failed to set loopback: %s\n",
                 bladerf_strerror(status));

        /* Try to ensure we don't leave the device in loopback */
        status_disable_lb = bladerf_set_loopback(dev, BLADERF_LB_NONE);
        if (status_disable_lb) {
            PR_ERROR("Failed to restore loopback to 'none': %s\n",
                     bladerf_strerror(status_disable_lb));
        }
        return status;
    }

    status = bladerf_get_loopback(dev, &readback);
    if (status != 0) {
        PR_ERROR("Failed to get loopback setting: %s\n",
                 bladerf_strerror(status));
        return status;
    }

    if (l != readback) {
        PR_ERROR("Unexpected loopback readback=%d, expected=%d\n",
                 readback, l);
        return -1;
    }

    return 0;
}

unsigned int test_loopback(struct bladerf *dev,
                           struct app_params *p, bool quiet)
{
    int status;
    size_t i;
    unsigned int failures = 0;
    const bladerf_loopback modes[] = {
        BLADERF_LB_NONE,
        BLADERF_LB_FIRMWARE,
        BLADERF_LB_BB_TXLPF_RXVGA2,
        BLADERF_LB_BB_TXVGA1_RXVGA2,
        BLADERF_LB_BB_TXLPF_RXLPF,
        BLADERF_LB_BB_TXVGA1_RXLPF,
        BLADERF_LB_RF_LNA1,
        BLADERF_LB_RF_LNA2,
        BLADERF_LB_RF_LNA3,
        BLADERF_LB_NONE,    /* Ensure we don't leave the device in loopback */
    };

    PRINT("%s: Setting and checking loopback modes...\n", __FUNCTION__);

    for (i = 0; i < ARRAY_SIZE(modes); i++) {
        status = set_and_check(dev, modes[i]);
        if (status != 0) {
            failures++;
        }
    }

    return failures;
}

