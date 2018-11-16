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
#include <inttypes.h>

DECLARE_TEST_CASE(frequency);


/* Due to some known rounding issues, the readback may be off. We'll not fail
 * out on this for now... */
static inline bool freq_match(bladerf_frequency a, bladerf_frequency b)
{
    bladerf_frequency const allowed_slack = (a >= 3000000000) ? 5 : 3;

    return (a >= (b - allowed_slack) && a <= (b + allowed_slack));
}

static int set_and_check(struct bladerf *dev,
                         bladerf_module m,
                         bladerf_frequency freq,
                         bladerf_frequency prev_freq)
{
    bladerf_frequency readback;
    int status;

    status = bladerf_set_frequency(dev, m, freq);
    if (status != 0) {
        PR_ERROR("Failed to set frequency: %" BLADERF_PRIuFREQ
                 " Hz (Prev: %" BLADERF_PRIuFREQ " Hz): %s\n",
                 freq, prev_freq, bladerf_strerror(status));
        return status;
    }

    status = bladerf_get_frequency(dev, m, &readback);
    if (status != 0) {
        PR_ERROR("Failed to get frequency: %s\n", bladerf_strerror(status));
        return status;
    }

    if (!freq_match(freq, readback)) {
        PR_ERROR("Frequency (%" BLADERF_PRIuFREQ
                 ") != Readback value (%" BLADERF_PRIuFREQ ")\n",
                 freq, readback);

        return -1;
    }

    return status;
}

static int freq_sweep(struct bladerf *dev,
                      bladerf_module m,
                      bladerf_frequency min,
                      bladerf_frequency max,
                      bool quiet)
{
    size_t const repetitions = 3;
    size_t const inc         = 1000000;

    bladerf_frequency freq      = 0;
    bladerf_frequency prev_freq = 0;
    size_t n, r;
    size_t failures = 0;
    int status;

    for (r = 0; r < repetitions; r++) {
        for (freq = min, n = 0; freq <= max; freq += inc, n++) {
            status = set_and_check(dev, m, freq, prev_freq);
            if (status != 0) {
                failures++;
            } else if (n % 50 == 0) {
                PRINT("\r  Currently tuned to %-10" BLADERF_PRIuFREQ " Hz...",
                      freq);
                fflush(stdout);
            }

            prev_freq = freq;
        }
    }

    PRINT("\n");
    fflush(stdout);
    return failures;
}

static int random_tuning(struct bladerf *dev,
                         struct app_params *p,
                         bladerf_module m,
                         bladerf_frequency min,
                         bladerf_frequency max,
                         bool quiet)
{
    size_t const num_iterations = 10000;

    bladerf_frequency freq      = 0;
    bladerf_frequency prev_freq = 0;
    size_t i, n;
    size_t failures = 0;
    int status;

    for (i = n = 0; i < num_iterations; i++, n++) {
        randval_update(&p->randval_state);
        freq = min + (p->randval_state % max);

        if (freq < min) {
            freq = min;
        } else if (freq > max) {
            freq = max;
        }

        status = set_and_check(dev, m, freq, prev_freq);
        if (status != 0) {
            failures++;
        } else if (n % 50 == 0) {
            PRINT("\r  Currently tuned to %-10" BLADERF_PRIuFREQ " Hz...",
                  freq);
            fflush(stdout);
        }

        prev_freq = freq;
    }

    PRINT("\n");
    fflush(stdout);
    return failures;
}

unsigned int test_frequency(struct bladerf *dev,
                            struct app_params *p,
                            bool quiet)
{
    size_t failures = 0;
    int status;

    ITERATE_DIRECTIONS({
        ITERATE_CHANNELS(
            DIRECTION, 1, ({
                struct bladerf_range const *range;
                bladerf_frequency min, max;

                PRINT("%s: Testing %s...\n", __FUNCTION__,
                      direction2str(DIRECTION));

                status = bladerf_get_frequency_range(dev, CHANNEL, &range);
                if (status < 0) {
                    PR_ERROR("Failed to get %s frequency range: %s\n",
                             direction2str(DIRECTION),
                             bladerf_strerror(status));
                    return status;
                };

                min = (range->min * range->scale);
                max = (range->max * range->scale);

                PRINT("%s: %s range: %" BLADERF_PRIuFREQ
                      " to %" BLADERF_PRIuFREQ "\n",
                      __FUNCTION__, direction2str(DIRECTION), min, max);

                PRINT("%s: Performing %s frequency sweep...\n", __FUNCTION__,
                      direction2str(DIRECTION));
                failures += freq_sweep(dev, CHANNEL, min, max, quiet);

                PRINT("%s: Performing random %s tuning...\n", __FUNCTION__,
                      direction2str(DIRECTION));
                failures += random_tuning(dev, p, CHANNEL, min, max, quiet);
            }));  // ITERATE_CHANNELS
    });           // ITERATE_DIRECTIONS

    return failures;
}
