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
#include "host_config.h"
#include "test_ctrl.h"
#include <inttypes.h>

DECLARE_TEST_CASE(samplerate);


static int set_and_check(struct bladerf *dev,
                         bladerf_channel ch,
                         bladerf_sample_rate rate)
{
    bladerf_sample_rate actual, readback;
    int status;

    status = bladerf_set_sample_rate(dev, ch, rate, &actual);
    if (status != 0) {
        PR_ERROR("Failed to set sample rate: %s\n", bladerf_strerror(status));
        return status;
    }

    status = bladerf_get_sample_rate(dev, ch, &readback);
    if (status != 0) {
        PR_ERROR("Failed to read back sample rate: %s\n",
                 bladerf_strerror(status));
        return status;
    }

    return 0;
}

static int set_and_check_rational(struct bladerf *dev,
                                  bladerf_channel ch,
                                  struct bladerf_rational_rate *rate)
{
    struct bladerf_rational_rate actual, readback;
    int status;

    status = bladerf_set_rational_sample_rate(dev, ch, rate, &actual);
    if (status != 0) {
        PR_ERROR("Failed to set rational sample rate: %s\n",
                 bladerf_strerror(status));
        return status;
    }

    status = bladerf_get_rational_sample_rate(dev, ch, &readback);
    if (status != 0) {
        PR_ERROR("Failed to read back rational sample rate: %s\n",
                 bladerf_strerror(status));
        return status;
    }

    if (actual.integer != readback.integer || actual.num != readback.num ||
        actual.den != readback.den) {
        PR_ERROR("Readback mismatch:\n"
                 " actual:   int=%" PRIu64 " num=%" PRIu64 " den=%" PRIu64 "\n"
                 "  readback: int=%" PRIu64 " num=%" PRIu64 " den=%" PRIu64
                 "\n",
                 actual.integer, actual.num, actual.den, readback.integer,
                 readback.num, readback.den);

        return status;
    }

    return 0;
}

static int sweep_samplerate(struct bladerf *dev,
                            bladerf_channel ch,
                            bool quiet,
                            bladerf_sample_rate min,
                            bladerf_sample_rate max)
{
    size_t const inc = 10000;

    bladerf_sample_rate rate;
    size_t n;
    size_t failures = 0;
    int status;

    for (rate = min, n = 0; rate <= max; rate += inc, n++) {
        status = set_and_check(dev, ch, rate);
        if (status != 0) {
            failures++;
        } else if (n % 50 == 0) {
            PRINT("\r  Sample rate currently set to %-10u Hz...", rate);
            fflush(stdout);
        }
    }

    PRINT("\n");
    fflush(stdout);
    return failures;
}

static int random_samplerates(struct bladerf *dev,
                              struct app_params *p,
                              bladerf_channel ch,
                              bool quiet,
                              bladerf_sample_rate min,
                              bladerf_sample_rate max)
{
    size_t const interations = 2500;

    bladerf_sample_rate rate;
    size_t i, n;
    size_t failures = 0;
    int status;

    for (i = n = 0; i < interations; i++, n++) {
        size_t const mod = max - min + 1;

        randval_update(&p->randval_state);

        rate = min + (p->randval_state % mod);
        assert(rate <= max);

        status = set_and_check(dev, ch, rate);
        if (status != 0) {
            failures++;
        } else if (n % 50 == 0) {
            PRINT("\r  Sample rate currently set to %-10u Hz...", rate);
            fflush(stdout);
        }
    }

    PRINT("\n");
    fflush(stdout);
    return failures;
}

static int random_rational_samplerates(struct bladerf *dev,
                                       struct app_params *p,
                                       bladerf_channel ch,
                                       bool quiet,
                                       bladerf_sample_rate min,
                                       bladerf_sample_rate max)
{
    size_t const iterations = 2500;

    struct bladerf_rational_rate rate;
    size_t i, n;
    unsigned failures = 0;
    int status;

    for (i = n = 0; i < iterations; i++, n++) {
        size_t const mod = max - min;

        randval_update(&p->randval_state);
        rate.integer = min + (p->randval_state % mod);

        if (rate.integer < min) {
            rate.integer = min;
        } else if (rate.integer > max) {
            rate.integer = max;
        }

        if (rate.integer != max) {
            randval_update(&p->randval_state);
            rate.num = (p->randval_state % max);

            randval_update(&p->randval_state);
            rate.den = (p->randval_state % max);

            if (0 == rate.den) {
                rate.den = 1;
            }

            while ((rate.num / rate.den) > max) {
                rate.num /= 2;
            }

            while ((1 + rate.integer + (rate.num / rate.den)) > max) {
                rate.num /= 2;
            }

        } else {
            rate.num = 0;
            rate.den = 1;
        }

        status = set_and_check_rational(dev, ch, &rate);
        if (status != 0) {
            failures++;
        } else if (n % 50 == 0) {
            PRINT("\r  Sample rate currently set to %-10" PRIu64 " %-10" PRIu64
                  "/%-10" PRIu64 " Hz...",
                  rate.integer, rate.num, rate.den);
            fflush(stdout);
        }
    }

    PRINT("\n");
    fflush(stdout);
    return failures;
}

unsigned int test_samplerate(struct bladerf *dev,
                             struct app_params *p,
                             bool quiet)
{
    size_t failures = 0;
    int status;

    ITERATE_DIRECTIONS({
        ITERATE_CHANNELS(
            DIRECTION, 1, ({
                struct bladerf_range const *range;
                bladerf_sample_rate min, max;

                PRINT("%s: Testing %s...\n", __FUNCTION__,
                      direction2str(DIRECTION));

                status = bladerf_get_sample_rate_range(dev, CHANNEL, &range);
                if (status < 0) {
                    PR_ERROR("Failed to get %s sample rate range: %s\n",
                             direction2str(DIRECTION),
                             bladerf_strerror(status));
                    return status;
                };

                min = (range->min * range->scale);
                max = (range->max * range->scale);

                PRINT("%s: %s range: %u to %u\n", __FUNCTION__,
                      direction2str(DIRECTION), min, max);

                PRINT("%s: Sweeping %s sample rates...\n", __FUNCTION__,
                      direction2str(DIRECTION));
                failures += sweep_samplerate(dev, CHANNEL, quiet, min, max);

                PRINT("%s: Applying random %s sample rates...\n", __FUNCTION__,
                      direction2str(DIRECTION));
                failures +=
                    random_samplerates(dev, p, CHANNEL, quiet, min, max);

                PRINT("%s: Applying random %s rational sample rates...\n",
                      __FUNCTION__, direction2str(DIRECTION));
                failures += random_rational_samplerates(dev, p, CHANNEL, quiet,
                                                        min, max);
            }));  // ITERATE_CHANNELS
    });           // ITERATE_DIRECTIONS

    /* Restore the device back to a sane default sample rate, as not to
     * interfere with later tests */
    failures += set_and_check(dev, BLADERF_CHANNEL_RX(0), DEFAULT_SAMPLERATE);
    failures += set_and_check(dev, BLADERF_CHANNEL_TX(0), DEFAULT_SAMPLERATE);

    return failures;
}
