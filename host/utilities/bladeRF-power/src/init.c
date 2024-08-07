/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2024 Nuand LLC
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
 *
 * This program is intended to verify that C programs build against
 * libbladeRF without any unintended dependencies.
 */
#include <stdio.h>
#include "init.h"
#include "helpers.h"
#include "libbladeRF.h"

#define CHECK(fn) do { \
    status = fn; \
    if (status != 0) { \
        fprintf(stderr, "[Error] %s:%d: %s - %s\n", __FILE__, __LINE__, #fn, bladerf_strerror(status)); \
        fprintf(stderr, "Exiting.\n"); \
        goto error; \
    } \
} while (0)

void init_params(struct test_params *test) {
    test->channel          = 0;
    test->frequency        = 1e9;
    test->frequency_actual = test->frequency;
    test->freq_min         = 65e6;
    test->freq_max         = 6e9;

    test->gain        = 0;
    test->gain_actual = 0;
    test->gain_min    = -10;
    test->gain_max    = 60;
    test->gain_mode   = BLADERF_GAIN_DEFAULT;

    test->samp_rate = 20e6;
    test->bandwidth = test->samp_rate;
    test->direction = DIRECTION_UNSET;

    test->rx_power = 0.0;
    test->gain_cal_enabled = false;
    test->gain_cal_file = NULL;
}

int dev_init(struct bladerf *dev, bladerf_direction dir, struct test_params *test) {
    int status = 0;
    const struct bladerf_range *freq_range;
    const struct bladerf_range *gain_range;
    bladerf_channel_layout layout = (dir == BLADERF_TX) ? BLADERF_TX_X1 : BLADERF_RX_X1;
    bladerf_format format = BLADERF_FORMAT_SC16_Q11;
    size_t num_buffers = 512;
    size_t buffer_size = 16*1024;
    size_t num_transfers = 32;
    size_t stream_timeout = 1000; //ms

    bladerf_channel ch = (dir == BLADERF_TX)
        ? BLADERF_CHANNEL_TX(test->channel)
        : BLADERF_CHANNEL_RX(test->channel);

    CHECK(bladerf_set_gain(dev, ch, test->gain));
    CHECK(bladerf_set_sample_rate(dev, ch, test->samp_rate, NULL));
    CHECK(bladerf_set_bandwidth(dev, ch, test->bandwidth, &test->bandwidth));
    CHECK(bladerf_set_frequency(dev, ch, test->frequency));
    CHECK(bladerf_sync_config(dev, layout, format, num_buffers, buffer_size,
                              num_transfers, stream_timeout));

    if (dir == BLADERF_RX) {
        CHECK(bladerf_set_gain_mode(dev, ch, test->gain_mode));
        CHECK(bladerf_get_gain_mode(dev, ch, &test->gain_mode));
    }

    CHECK(bladerf_get_frequency_range(dev, ch, &freq_range));
    test->freq_min = freq_range->min * freq_range->scale;
    test->freq_max = freq_range->max * freq_range->scale;

    CHECK(bladerf_get_gain_range(dev, ch, &gain_range));
    test->gain_min = gain_range->min * gain_range->scale;
    test->gain_max = gain_range->max * gain_range->scale;

    CHECK(bladerf_load_gain_calibration(dev, ch, test->gain_cal_file));

error:
    return status;
}
