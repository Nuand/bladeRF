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
    bladerf_frequency const band_split = 3000000000UL;
    bladerf_frequency const allowed_slack = (a >= band_split) ? 5 : 3;

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

static failure_count freq_sweep(struct bladerf *dev,
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
    failure_count failures = 0;
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

static failure_count random_tuning(struct bladerf *dev,
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
    failure_count failures = 0;
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

failure_count test_frequency(struct bladerf *dev,
                             struct app_params *p,
                             bool quiet)
{
    failure_count failures = 0;
    int status;

    bladerf_direction dir;

    FOR_EACH_DIRECTION(dir)
    {
        size_t i;
        bladerf_channel ch;

        FOR_EACH_CHANNEL(dir, 1, i, ch)
        {
            struct bladerf_range const *range;
            bladerf_frequency min, max;

            PRINT("%s: Testing %s...\n", __FUNCTION__, direction2str(dir));

            status = bladerf_get_frequency_range(dev, ch, &range);
            if (status < 0) {
                PR_ERROR("Failed to get %s frequency range: %s\n",
                         direction2str(dir), bladerf_strerror(status));
                return status;
            };

            min = (bladerf_frequency)(range->min * range->scale);
            max = (bladerf_frequency)(range->max * range->scale);

            PRINT("%s: %s range: %" BLADERF_PRIuFREQ " to %" BLADERF_PRIuFREQ
                  "\n",
                  __FUNCTION__, direction2str(dir), min, max);

            PRINT("%s: Performing %s frequency sweep...\n", __FUNCTION__,
                  direction2str(dir));
            failures += freq_sweep(dev, ch, min, max, quiet);

            PRINT("%s: Performing random %s tuning...\n", __FUNCTION__,
                  direction2str(dir));
            failures += random_tuning(dev, p, ch, min, max, quiet);
        }
    }

    return failures;
}
