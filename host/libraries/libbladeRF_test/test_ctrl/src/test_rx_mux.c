/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2015 Nuand LLC
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

DECLARE_TEST_CASE(rx_mux);

static int set_and_check(struct bladerf *dev, bladerf_rx_mux mux)
{
    int status, status_restore;
    bladerf_rx_mux readback;

    status = bladerf_set_rx_mux(dev, mux);
    if (status != 0) {
        PR_ERROR("Failed to set rx mux: %s\n",
                 bladerf_strerror(status));

        /* Try to ensure we don't leave the device in an alternate MUX mode */
        status_restore = bladerf_set_rx_mux(dev, BLADERF_RX_MUX_BASEBAND_LMS);
        if (status_restore) {
            PR_ERROR("Failed to restore rx mux to 'BASEBAND_LMS': %s\n",
                     bladerf_strerror(status_restore));
        }
        return status;
    }

    status = bladerf_get_rx_mux(dev, &readback);
    if (status != 0) {
        PR_ERROR("Failed to get rx mux setting: %s\n",
                 bladerf_strerror(status));
        return status;
    }

    if (mux != readback) {
        PR_ERROR("Unexpected rx mux readback=%d, expected=%d\n",
                 readback, mux);
        return -1;
    }

    return 0;
}

unsigned int test_rx_mux(struct bladerf *dev,
                           struct app_params *p, bool quiet)
{
    int status;
    size_t i;
    unsigned int failures = 0;
    const bladerf_rx_mux muxes[] = {
        BLADERF_RX_MUX_BASEBAND_LMS,
        BLADERF_RX_MUX_12BIT_COUNTER,
        BLADERF_RX_MUX_32BIT_COUNTER,
        BLADERF_RX_MUX_DIGITAL_LOOPBACK,
        BLADERF_RX_MUX_BASEBAND_LMS /* Restore normal operation */
    };

    PRINT("%s: Setting and checking rx muxes...\n", __FUNCTION__);

    for (i = 0; i < ARRAY_SIZE(muxes); i++) {
        status = set_and_check(dev, muxes[i]);
        if (status != 0) {
            failures++;
        }
    }

    return failures;
}

