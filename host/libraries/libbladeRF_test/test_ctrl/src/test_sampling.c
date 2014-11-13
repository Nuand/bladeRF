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

DECLARE_TEST_CASE(sampling);

static inline const char *sampling_str(bladerf_sampling s)
{
    switch (s) {
        case BLADERF_SAMPLING_INTERNAL:
            return "internal";
        case BLADERF_SAMPLING_EXTERNAL:
            return "external";
        default:
            return "unknown";
    }
}

static int set_and_check(struct bladerf *dev, bladerf_sampling s)
{
    int status;
    bladerf_sampling readback;

    status = bladerf_set_sampling(dev, s);
    if (status != 0) {
        PR_ERROR("Failed to set sampling=%s: %s\n",
                 sampling_str(s), bladerf_strerror(status));

        return status;
    }

    status = bladerf_get_sampling(dev, &readback);
    if (status != 0) {
        PR_ERROR("Failed to read back sampling type: %s\n",
                 bladerf_strerror(status));

        /* Last ditch attempt to restore internal sampling */
        bladerf_set_sampling(dev, BLADERF_SAMPLING_INTERNAL);
        return status;
    }

    if (s != readback) {
        PR_ERROR("Sampling mismatch -- readback=%s, expected=%s\n",
                 sampling_str(readback), sampling_str(s));
        return -1;
    }

    return 0;
}

unsigned int test_sampling(struct bladerf *dev,
                           struct app_params *p, bool quiet)
{
    int status;
    unsigned int failures = 0;

    PRINT("%s: Setting and reading back sampling modes...\n", __FUNCTION__);

    status = set_and_check(dev, BLADERF_SAMPLING_EXTERNAL);
    if (status != 0) {
        failures++;
    }

    status = set_and_check(dev, BLADERF_SAMPLING_INTERNAL);
    if (status != 0) {
        failures++;
    }

    status = set_and_check(dev, BLADERF_SAMPLING_EXTERNAL);
    if (status != 0) {
        failures++;
    }

    return failures;
}

