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


static int enable_module(struct bladerf *dev, bladerf_channel ch, bool enable)
{
    int status;

    status = bladerf_enable_module(dev, ch, enable);
    if (status != 0) {
        PR_ERROR("Failed to %s module: %s\n", enable ? "enable" : "disable",
                 bladerf_strerror(status));

        /* Try to avoid leaving a module on */
        bladerf_enable_module(dev, ch, false);

        return status;
    }

    return status;
}

failure_count test_enable_module(struct bladerf *dev,
                                 struct app_params *p,
                                 bool quiet)
{
    size_t const rx_ch = bladerf_get_channel_count(dev, BLADERF_RX);
    size_t const tx_ch = bladerf_get_channel_count(dev, BLADERF_TX);

    bladerf_direction dir;
    int pass;
    size_t idx;
    failure_count failures = 0;
    int status;

    PRINT("%s: Enabling and disabling channels (%zu RX, %zu TX)...\n",
          __FUNCTION__, rx_ch, tx_ch);

    // do off, then on, then off
    for (pass = 0; pass <= 2; ++pass) {
        bool enable = (1 == (pass % 2));

        // first RX, then TX
        for (dir = BLADERF_RX; dir <= BLADERF_TX; ++dir) {
            size_t ch_count = (BLADERF_RX == dir) ? rx_ch : tx_ch;

            // iterate through the channels...
            for (idx = 0; idx < ch_count; ++idx) {
                bladerf_channel ch =
                    (BLADERF_RX == dir ? BLADERF_CHANNEL_RX(idx)
                                       : BLADERF_CHANNEL_TX(idx));

                status = enable_module(dev, ch, enable);
                if (status != 0) {
                    failures++;
                }
            }
        }
    }

    return failures;
}
