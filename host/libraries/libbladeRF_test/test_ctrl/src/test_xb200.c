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

DECLARE_TEST_CASE(xb200);

static int set_and_check_paths(struct bladerf *dev,
                               bladerf_channel ch,
                               bladerf_xb200_path p)
{
    int status;
    bladerf_xb200_path readback;

    status = bladerf_xb200_set_path(dev, ch, p);
    if (status != 0) {
        PR_ERROR("Failed to set XB-200 path: %s\n", bladerf_strerror(status));
        return status;
    }

    status = bladerf_xb200_get_path(dev, ch, &readback);
    if (status != 0) {
        PR_ERROR("Failed to set XB-200 path: %s\n", bladerf_strerror(status));
        return status;
    }

    if (p != readback) {
        PR_ERROR("Path mismatch -- path=%d, readback=%d\n", p, readback);
        return -1;
    }

    return 0;
}

static int set_and_check_filterbank(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bladerf_xb200_filter f)
{
    int status;
    bladerf_xb200_filter readback;

    status = bladerf_xb200_set_filterbank(dev, ch, f);
    if (status != 0) {
        PR_ERROR("Failed to set XB-200 filter bank: %s\n",
                 bladerf_strerror(status));
        return status;
    }

    status = bladerf_xb200_get_filterbank(dev, ch, &readback);
    if (status != 0) {
        PR_ERROR("Failed to read back filter bank: %s\n",
                 bladerf_strerror(status));
        return status;
    }

    if (f != readback) {
        PR_ERROR("Mismatch detected -- fiterbank=%d, readback=%d\n", f,
                 readback);
        return -1;
    }

    return 0;
}

static failure_count test_paths(struct bladerf *dev)
{
    int status;
    failure_count failures = 0;

    status =
        set_and_check_paths(dev, BLADERF_CHANNEL_RX(0), BLADERF_XB200_BYPASS);
    if (status != 0) {
        failures++;
    }

    status = set_and_check_paths(dev, BLADERF_CHANNEL_RX(0), BLADERF_XB200_MIX);
    if (status != 0) {
        failures++;
    }

    status =
        set_and_check_paths(dev, BLADERF_CHANNEL_TX(0), BLADERF_XB200_BYPASS);
    if (status != 0) {
        failures++;
    }

    status = set_and_check_paths(dev, BLADERF_CHANNEL_TX(0), BLADERF_XB200_MIX);
    if (status != 0) {
        failures++;
    }

    return failures;
}

static failure_count test_filterbanks(struct bladerf *dev)
{
    int status;
    failure_count failures = 0;

    status = set_and_check_filterbank(dev, BLADERF_CHANNEL_RX(0),
                                      BLADERF_XB200_50M);  //
    if (status != 0) {
        failures++;
    }

    status = set_and_check_filterbank(dev, BLADERF_CHANNEL_RX(0),
                                      BLADERF_XB200_144M);
    if (status != 0) {
        failures++;
    }

    status = set_and_check_filterbank(dev, BLADERF_CHANNEL_RX(0),
                                      BLADERF_XB200_222M);
    if (status != 0) {
        failures++;
    }

    status = set_and_check_filterbank(dev, BLADERF_CHANNEL_RX(0),
                                      BLADERF_XB200_CUSTOM);
    if (status != 0) {
        failures++;
    }

    status = set_and_check_filterbank(dev, BLADERF_CHANNEL_TX(0),
                                      BLADERF_XB200_50M);  //
    if (status != 0) {
        failures++;
    }

    status = set_and_check_filterbank(dev, BLADERF_CHANNEL_TX(0),
                                      BLADERF_XB200_144M);
    if (status != 0) {
        failures++;
    }

    status = set_and_check_filterbank(dev, BLADERF_CHANNEL_TX(0),
                                      BLADERF_XB200_222M);
    if (status != 0) {
        failures++;
    }

    status = set_and_check_filterbank(dev, BLADERF_CHANNEL_TX(0),
                                      BLADERF_XB200_CUSTOM);
    if (status != 0) {
        failures++;
    }

    return failures;
}

failure_count test_xb200(struct bladerf *dev, struct app_params *p, bool quiet)
{
    failure_count failures = 0;

    if (!p->use_xb200) {
        return 0;
    }

    PRINT("%s: Setting and reading back filter bank settings...\n",
          __FUNCTION__);
    failures += test_filterbanks(dev);

    PRINT("%s: Setting and reading back path settings...\n", __FUNCTION__);
    failures += test_paths(dev);

    return failures;
}
