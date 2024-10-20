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
#ifndef HELPERS_H_
#define HELPERS_H_

#include "libbladeRF.h"
#include "init.h"
#include "conversions.h"

#define DIRECTION_UNSET 3
#define NUM_FREQ_SUFFIXES 6

extern const struct numeric_suffix freq_suffixes[NUM_FREQ_SUFFIXES];

/**
 * @brief  Ask the user for TX or RX operation.
 *
 * @return bladerf_direction
 */
bladerf_direction ask_direction();

int start_streaming(struct bladerf *dev, struct test_params *test);

#endif // HELPERS_H_
