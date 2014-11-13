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

DECLARE_TEST_CASE(enable_module);

static int enable_module(struct bladerf *dev, bladerf_module m, bool enable)
{
    int status;

    status = bladerf_enable_module(dev, m, enable);
    if (status != 0) {
        PR_ERROR("Failed to %s module: %s\n",
                 enable ? "enable" : "disable",
                 bladerf_strerror(status));

        /* Try to avoid leaving a module on */
        bladerf_enable_module(dev, m, false);

        return status;
    }

    return status;
}

unsigned int test_enable_module(struct bladerf *dev,
                                struct app_params *p, bool quiet)
{
    int status;
    unsigned int failures = 0;

    PRINT("%s: Enabling and disabling modules...\n",__FUNCTION__);

    status = enable_module(dev, BLADERF_MODULE_RX, false);
    if (status != 0) {
        failures++;
    }

    status = enable_module(dev, BLADERF_MODULE_RX, true);
    if (status != 0) {
        failures++;
    }

    status = enable_module(dev, BLADERF_MODULE_RX, false);
    if (status != 0) {
        failures++;
    }

    status = enable_module(dev, BLADERF_MODULE_TX, false);
    if (status != 0) {
        failures++;
    }

    status = enable_module(dev, BLADERF_MODULE_TX, true);
    if (status != 0) {
        failures++;
    }

    status = enable_module(dev, BLADERF_MODULE_TX, false);
    if (status != 0) {
        failures++;
    }

    return failures;
}
