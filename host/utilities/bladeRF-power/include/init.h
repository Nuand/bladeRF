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
#ifndef INIT_H
#define INIT_H

#include "libbladeRF.h"

struct test_params {
    bladerf_channel channel;
    bladerf_frequency frequency;
    bladerf_frequency frequency_actual;
    bladerf_frequency freq_min;
    bladerf_frequency freq_max;
    bladerf_gain gain;
    bladerf_gain gain_min;
    bladerf_gain gain_max;
    bladerf_gain gain_actual;
    bladerf_gain_mode gain_mode;
    bladerf_sample_rate samp_rate;
    bladerf_sample_rate bandwidth;
    bladerf_direction direction;
    double rx_power;
    bool gain_cal_enabled;
    char *gain_cal_file;
    bool show_messages;
    char message_buffer[4096];  // Adjust size as needed
};

/**
 * @brief Initialize the test parameters
 *
 * @param test The test parameters to initialize
 */
void init_params(struct test_params *test);

/**
 * @brief Initialize the device
 *
 * @param dev The device to initialize
 * @param dir The direction to initialize
 * @param test The test parameters to use
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int dev_init(struct bladerf *dev, bladerf_direction dir, struct test_params *test);

#endif
