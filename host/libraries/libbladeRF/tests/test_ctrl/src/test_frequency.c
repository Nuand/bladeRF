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

/* Due to some known rounding issues, the readback may be +/- 1 Hz. We'll not
 * fail out on this for now... */
static inline bool freq_match(unsigned int a, unsigned int b)
{
    if (a == b) {
        return true;
    } else if (b > 0 && (a == (b - 1))) {
        return true;
    } else if (b < BLADERF_FREQUENCY_MAX && (a == (b + 1)) ) {
        return true;
    } else {
        return false;
    }
}


static int set_and_check(struct bladerf *dev, bladerf_module m,
                         unsigned int freq)
{
    int status;
    unsigned int readback;

    status = bladerf_set_frequency(dev, m, freq);
    if (status != 0) {
        fprintf(stderr, ERR_PFX "Failed to set frequency: %u Hz: %s\n", freq,
                bladerf_strerror(status));
        return status;
    }

    status = bladerf_get_frequency(dev, m, &readback);
    if (status != 0) {
        fprintf(stderr, ERR_PFX "Failed to get frequency: %s\n",
                bladerf_strerror(status));
        return status;
    }

    if (!freq_match(freq, readback)) {
        fprintf(stderr, "ERR_PFX Frequency (%u) != Readback value (%u)\n",
                freq, readback);

        return -1;
    }

    return status;
}

static unsigned int freq_sweep(struct bladerf *dev, bladerf_module m,
                               unsigned int min)
{
    int status;
    unsigned int freq, n;
    const unsigned int inc = 2500000;
    unsigned int failures = 0;

    for (freq = min, n = 0; freq <= BLADERF_FREQUENCY_MAX; freq += inc, n++) {
        status = set_and_check(dev, m, freq);
        if (status != 0) {
            failures++;
        } else if (n % 50 == 0) {
            printf("\r  Currently tuned to %-10u Hz...", freq);
            fflush(stdout);
        }
    }

    printf("\n");
    fflush(stdout);
    return failures;
}

static int random_tuning(struct bladerf *dev, bladerf_module m,
                         unsigned int min)
{
    int status = 0;
    unsigned int i, n;
    const unsigned int num_iterations = 2500;
    unsigned int freq;
    unsigned int failures = 0;
    const unsigned int scaling = (BLADERF_FREQUENCY_MAX / RAND_MAX) == 0 ?
                                 1 : (BLADERF_FREQUENCY_MAX / RAND_MAX);

    for (i = n = 0; i < num_iterations; i++, n++) {
        freq = min + ((scaling * rand()) % BLADERF_FREQUENCY_MAX);

        if (freq < min) {
            freq  = BLADERF_FREQUENCY_MIN;
        } else if (freq > BLADERF_FREQUENCY_MAX) {
            freq = BLADERF_FREQUENCY_MAX;
        }

        status = set_and_check(dev, m, freq);
        if (status != 0) {
            failures++;
        } else if (n % 50 == 0) {
            printf("\r  Currently tuned to %-10u Hz...", freq);
            fflush(stdout);
        }
    }

    printf("\n");
    fflush(stdout);
    return failures;
}

unsigned int test_frequency(struct bladerf *dev, struct app_params *p)
{
    unsigned int failures = 0;
    const unsigned int min = p->use_xb200 ?
                                BLADERF_FREQUENCY_MIN_XB200 :
                                BLADERF_FREQUENCY_MIN;

    printf("%s: Performing RX frequency sweep...\n", __FUNCTION__);
    failures += freq_sweep(dev, BLADERF_MODULE_RX, min);

    printf("%s: Performing random RX tuning...\n", __FUNCTION__);
    failures += random_tuning(dev, BLADERF_MODULE_RX, min);

    printf("%s: Performing TX frequency sweep...\n", __FUNCTION__);
    failures += freq_sweep(dev, BLADERF_MODULE_TX, min);

    printf("%s: Performing random TX tuning...\n", __FUNCTION__);
    failures += random_tuning(dev, BLADERF_MODULE_TX, min);

    return failures;
}
