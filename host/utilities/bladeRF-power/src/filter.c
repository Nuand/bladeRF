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
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#include "log.h"
#include "libbladeRF.h"

#define CHECK_NULL(...) do { \
    const void* _args[] = { __VA_ARGS__, NULL }; \
    for (size_t _i = 0; _args[_i] != NULL; ++_i) { \
        if (_args[_i] == NULL) { \
            log_error("%s:%d: Argument %zu is a NULL pointer\n", __FILE__, __LINE__, _i + 1); \
            return BLADERF_ERR_INVAL; \
        } \
    } \
} while (0)

typedef struct {
    const char *device_name;
    double *filter_taps;
    size_t tap_num;
} device_fir_filter_t;

#define PASS_THROUGH_TAPS_NUM 1
#define PASS_THROUGH_TAPS {1.00000000000000000}
static double pass_through_taps[PASS_THROUGH_TAPS_NUM] = PASS_THROUGH_TAPS;

#define BLADERF1_TAPS_NUM PASS_THROUGH_TAPS_NUM
static double bladerf1_filter_taps[BLADERF1_TAPS_NUM] = PASS_THROUGH_TAPS;

/*
FIR filter designed with
http://t-filter.appspot.com

sampling frequency: 2000 Hz

* 0 Hz - 50 Hz
  gain = 0.96
  desired ripple = 0.1 dB
  actual ripple = 0.02235661175114112 dB

* 590 Hz - 600 Hz
  gain = 0.7
  desired ripple = 0.5 dB
  actual ripple = 0.07461272495123161 dB

* 990 Hz - 1000 Hz
  gain = 0.56
  desired ripple = 0.1 dB
  actual ripple = 0.00253374647750082 dB
*/
#define BLADERF2_TAPS_NUM 3
static double bladerf2_filter_taps[BLADERF2_TAPS_NUM] = {
    0.10034970104726319,
    0.7605360692461476,
    0.10034970104726319
};

static device_fir_filter_t filters[] = {
    {"passthrough",     pass_through_taps,          PASS_THROUGH_TAPS_NUM},     // Default filter
    {"bladerf1",        bladerf1_filter_taps,       BLADERF1_TAPS_NUM},
    {"bladerf2",        bladerf2_filter_taps,       BLADERF2_TAPS_NUM},
};

static device_fir_filter_t* get_device_filter(struct bladerf *dev) {
    const char* board_name = bladerf_get_board_name(dev);
    for (size_t i = 0; i < sizeof(filters) / sizeof(filters[0]); i++) {
        if (strncmp(board_name, filters[i].device_name, strlen(filters[i].device_name)) == 0) {
            return &filters[i];
        }
    }

    log_debug("No FIR filter found for bladeRF device. Using passthrough.");
    return &filters[0]; // Default filter
}

int flatten_noise_figure(struct bladerf *dev,
                         const int16_t *samples,
                         size_t num_samples,
                         int16_t *convolved_samples)
{
    int status = 0;
    double acc_i, acc_q;
    CHECK_NULL(dev, samples, convolved_samples);

    device_fir_filter_t* filter = get_device_filter(dev);

    for (size_t i = 0; i < num_samples; i++) {
        acc_i = 0.0;
        acc_q = 0.0;

        // Convolve filter with input samples
        for (size_t j = 0; j < filter->tap_num; j++) {
            if (i >= j) {
                acc_i += filter->filter_taps[j] * (double)samples[2*(i-j)];
                acc_q += filter->filter_taps[j] * (double)samples[2*(i-j) + 1];
            }
        }

        // Clamping to the to int16_t
        convolved_samples[2*i] = (int16_t)(fmax(fmin(acc_i, INT16_MAX), INT16_MIN));
        convolved_samples[2*i + 1] = (int16_t)(fmax(fmin(acc_q, INT16_MAX), INT16_MIN));
    }

    return status;
}
