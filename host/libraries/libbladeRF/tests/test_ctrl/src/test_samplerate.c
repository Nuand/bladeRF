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
#include <inttypes.h>
#include "host_config.h"
#include "test_ctrl.h"

DECLARE_TEST_CASE(samplerate);

static int set_and_check(struct bladerf *dev, bladerf_module m,
                         unsigned int rate)
{
    int status;
    unsigned int actual, readback;

    status = bladerf_set_sample_rate(dev, m, rate, &actual);
    if (status != 0) {
        PR_ERROR("Failed to set sample rate: %s\n",
                 bladerf_strerror(status));
        return status;
    }

    status = bladerf_get_sample_rate(dev, m, &readback);
    if (status != 0) {
        PR_ERROR("Failed to read back sample rate: %s\n",
                 bladerf_strerror(status));
        return status;
    }

    return 0;
}

static int set_and_check_rational(struct bladerf *dev, bladerf_module m,
                                  struct bladerf_rational_rate *rate)
{
    int status;
    struct bladerf_rational_rate actual, readback;

    status = bladerf_set_rational_sample_rate(dev, m, rate, &actual);
    if (status != 0) {
        PR_ERROR("Failed to set rational sample rate: %s\n",
                 bladerf_strerror(status));
        return status;
    }

    status = bladerf_get_rational_sample_rate(dev, m, &readback);
    if (status != 0) {
        PR_ERROR("Failed to read back rational sample rate: %s\n",
                 bladerf_strerror(status));
        return status;
    }

    if (actual.integer != readback.integer  ||
        actual.num != readback.num          ||
        actual.den != readback.den          ) {

        PR_ERROR("Readback mismatch:\n"
                 " actual:   int=%"PRIu64" num=%"PRIu64" den=%"PRIu64"\n"
                 "  readback: int=%"PRIu64" num=%"PRIu64" den=%"PRIu64"\n",
                 actual.integer, actual.num, actual.den,
                 readback.integer, readback.num, readback.den);

        return status;
    }

    return 0;
}

static int sweep_samplerate(struct bladerf *dev, bladerf_module m, bool quiet)
{
    int status;
    unsigned int rate;
    const unsigned int inc = 10000;
    unsigned int n;
    unsigned int failures = 0;

    for (rate = BLADERF_SAMPLERATE_MIN, n = 0;
         rate <= BLADERF_SAMPLERATE_REC_MAX;
         rate += inc, n++) {

        status = set_and_check(dev, m, rate);
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
                              bladerf_module m, bool quiet)
{
    int status;
    unsigned int i, n;
    const unsigned int interations = 2500;
    unsigned int rate;
    unsigned failures = 0;

    for (i = n = 0; i < interations; i++, n++) {
        const unsigned int mod =
            BLADERF_SAMPLERATE_REC_MAX - BLADERF_SAMPLERATE_MIN + 1;

        randval_update(&p->randval_state);

        rate = BLADERF_SAMPLERATE_MIN + (p->randval_state % mod);
        assert(rate <= BLADERF_SAMPLERATE_REC_MAX);

        status = set_and_check(dev, m, rate);
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
                                       bladerf_module m, bool quiet)
{
    int status;
    unsigned int i, n;
    const unsigned int iterations = 2500;
    struct bladerf_rational_rate rate;
    unsigned failures = 0;

    for (i = n = 0; i < iterations; i++, n++) {
        const uint64_t mod =
                BLADERF_SAMPLERATE_REC_MAX - BLADERF_SAMPLERATE_MIN;

        randval_update(&p->randval_state);
        rate.integer = BLADERF_SAMPLERATE_MIN + (p->randval_state % mod);

        if (rate.integer < BLADERF_SAMPLERATE_MIN) {
            rate.integer = BLADERF_SAMPLERATE_MIN;
        } else if (rate.integer > BLADERF_SAMPLERATE_REC_MAX) {
            rate.integer = BLADERF_SAMPLERATE_REC_MAX;
        }

        if (rate.integer != BLADERF_SAMPLERATE_REC_MAX) {
            randval_update(&p->randval_state);
            rate.num = (p->randval_state % BLADERF_SAMPLERATE_REC_MAX);

            randval_update(&p->randval_state);
            rate.den = (p->randval_state % BLADERF_SAMPLERATE_REC_MAX);

            if (rate.den == 0) {
                rate.den = 1;
            }

            while ( (rate.num / rate.den) > BLADERF_SAMPLERATE_REC_MAX) {
                rate.num /= 2;
            }

            while ( (1 + rate.integer + (rate.num / rate.den))
                        > BLADERF_SAMPLERATE_REC_MAX) {
                rate.num /= 2;
            }

        } else {
            rate.num = 0;
            rate.den = 1;
        }

        status = set_and_check_rational(dev, m, &rate);
        if (status != 0) {
            failures++;
        } else if (n % 50 == 0) {
            PRINT("\r  Sample rate currently set to "
                   "%-10"PRIu64" %-10"PRIu64"/%-10"PRIu64" Hz...",
                   rate.integer, rate.num, rate.den);
            fflush(stdout);
        }
    }

    PRINT("\n");
    fflush(stdout);
    return failures;
}

unsigned int test_samplerate(struct bladerf *dev,
                             struct app_params *p, bool quiet)
{
    unsigned int failures = 0;

    PRINT("%s: Sweeping RX sample rates...\n", __FUNCTION__);
    failures += sweep_samplerate(dev, BLADERF_MODULE_RX, quiet);

    PRINT("%s: Applying random RX sample rates...\n", __FUNCTION__);
    failures += random_samplerates(dev, p, BLADERF_MODULE_RX, quiet);

    PRINT("%s: Applying random RX rational sample rates...\n",
          __FUNCTION__);
    failures += random_rational_samplerates(dev, p, BLADERF_MODULE_RX, quiet);

    PRINT("%s: Sweeping TX sample rates...\n",
          __FUNCTION__);
    failures += sweep_samplerate(dev, BLADERF_MODULE_TX, quiet);

    PRINT("%s: Applying random TX sample rates...\n",
          __FUNCTION__);
    failures += random_samplerates(dev, p, BLADERF_MODULE_TX, quiet);

    PRINT("%s: Applying random TX rational sample rates...\n",
          __FUNCTION__);

    failures += random_rational_samplerates(dev, p, BLADERF_MODULE_TX, quiet);

    /* Restore the device back to a sane default sample rate, as not to
     * interfere with later tests */
    failures += set_and_check(dev, BLADERF_MODULE_RX, DEFAULT_SAMPLERATE);
    failures += set_and_check(dev, BLADERF_MODULE_TX, DEFAULT_SAMPLERATE);

    return failures;
}
