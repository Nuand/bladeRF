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
#include <string.h>

DECLARE_TEST_CASE(lpf_mode);

static int set_and_check(struct bladerf *dev,
                         bladerf_channel ch,
                         bladerf_lpf_mode mode)
{
    bladerf_lpf_mode readback;
    int status;

    status = bladerf_set_lpf_mode(dev, ch, mode);
    if (status != 0) {
        PR_ERROR("Failed to set LPF mode: %s\n", bladerf_strerror(status));
        return status;
    }

    status = bladerf_get_lpf_mode(dev, ch, &readback);
    if (status != 0) {
        PR_ERROR("Failed to get LPF mode: %s\n", bladerf_strerror(status));

        /* Last ditch effor to restore normal configuration */
        bladerf_set_lpf_mode(dev, ch, BLADERF_LPF_NORMAL);
        return status;
    }

    if (readback != mode) {
        PR_ERROR("Readback failure -- value=%d, expected=%d\n", readback, mode);
        return -1;
    }

    return 0;
}

static failure_count test_module(struct bladerf *dev, bladerf_channel ch)
{
    int status;
    failure_count failures = 0;

    status = set_and_check(dev, ch, BLADERF_LPF_NORMAL);
    if (status != 0) {
        failures++;
    }

    status = set_and_check(dev, ch, BLADERF_LPF_BYPASSED);
    if (status != 0) {
        failures++;
    }

    status = set_and_check(dev, ch, BLADERF_LPF_DISABLED);
    if (status != 0) {
        failures++;
    }

    status = set_and_check(dev, ch, BLADERF_LPF_NORMAL);
    if (status != 0) {
        failures++;
    }

    return failures;
}

failure_count test_lpf_mode(struct bladerf *dev,
                            struct app_params *p,
                            bool quiet)
{
    failure_count failures = 0;

    if (0 != strcmp(bladerf_get_board_name(dev), "bladerf1")) {
        PRINT("%s: board is not a bladerf1, skipping\n", __FUNCTION__);
        return 0;
    }

    PRINT("%s: Setting and reading back RX LPF modes...\n", __FUNCTION__);
    failures += test_module(dev, BLADERF_CHANNEL_RX(0));

    PRINT("%s: Setting and reading back TX LPF modes...\n", __FUNCTION__);
    failures += test_module(dev, BLADERF_CHANNEL_TX(0));

    return failures;
}
