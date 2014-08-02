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

DECLARE_TEST_CASE(correction);

const struct correction {
    const char *name;
    bladerf_correction type;
    int min;
    int max;
} corrections[] = {
    {
        "I DC offset",
        BLADERF_CORR_LMS_DCOFF_I,
        -2048,
        2048,
    },

    {
        "Q DC offset",
        BLADERF_CORR_LMS_DCOFF_Q,
        -2048,
        2048,
    },

    {
        "Phase",
        BLADERF_CORR_FPGA_PHASE,
        -4096,
        4096,
    },

    {
        "Gain",
        BLADERF_CORR_FPGA_GAIN,
        -4096,
        4096,
    },
};

static int set_and_check(struct bladerf *dev, const struct correction *c,
                         bladerf_module m, int16_t val)
{
    int status;
    int16_t readback, scaled_readback, scaled_val;

    status = bladerf_set_correction(dev, m, c->type, val);
    if (status != 0) {
        PR_ERROR("Failed to set %s correction: %s\n",
                 c->name, bladerf_strerror(status));

        return status;
    }

    status = bladerf_get_correction(dev, m, c->type, &readback);
    if (status != 0) {
        PR_ERROR("Failed to get %s correction: %s\n",
                 c->name, bladerf_strerror(status));

        return status;
    }

    /* These functions will scale and clamp. Mirror that here so we can check
     * the expected values... */
    switch (c->type) {
        case BLADERF_CORR_LMS_DCOFF_I:
        case BLADERF_CORR_LMS_DCOFF_Q:
            if (m == BLADERF_MODULE_RX) {
                if (val == -2048) {
                    val = -2047;
                }
                scaled_val = val / 64;
                scaled_readback = val / 64;
            } else {
                scaled_val = val / 128;
                scaled_readback = val / 128;
            }
            break;

        default:
            scaled_val = val;
            scaled_readback = readback;
            break;
    }

    if (scaled_val != scaled_readback) {
        PR_ERROR("Correction mismatch, val=%d, readback=%d\n",
                 val, readback);
        return -1;
    }

    return 0;
}

static int sweep_vals(struct bladerf *dev, bladerf_module m, bool quiet)
{
    int status;
    int val;
    size_t i;
    unsigned int failures = 0;

    for (i = 0; i < ARRAY_SIZE(corrections); i++) {
        PRINT("    %s\n", corrections[i].name);

        for (val = corrections[i].min; val <= corrections[i].max; val++) {
            status = set_and_check(dev, &corrections[i], m, val);
            if (status != 0) {
                failures++;
            }
        }
    }

    return failures;
}

static unsigned random_vals(struct bladerf *dev, struct app_params *p,
                            bladerf_module m, bool quiet)
{
    int status;
    int val, j;
    size_t i;
    const int iterations = 2500;
    unsigned int failures = 0;

    for (i = 0; i < ARRAY_SIZE(corrections); i++) {
        PRINT("    %s\n", corrections[i].name);

        for (j = 0; j < iterations; j++ ){
            randval_update(&p->randval_state);
            val = corrections[i].min + (p->randval_state % corrections[i].max);

            if (val < corrections[i].min) {
                val = corrections[i].min;
            } else if (val > corrections[i].max) {
                val = corrections[i].max;
            }

            status = set_and_check(dev, &corrections[i], m, val);
            if (status != 0) {
                failures++;
            }
        }
    }

    return failures;
}


unsigned int test_correction(struct bladerf *dev,
                             struct app_params *p, bool quiet)
{
    unsigned int failures = 0;

    PRINT("%s: Sweeping RX corrections...\n", __FUNCTION__);
    failures += sweep_vals(dev, BLADERF_MODULE_RX, quiet);

    PRINT("%s: Random RX corrections...\n", __FUNCTION__);
    failures += random_vals(dev, p, BLADERF_MODULE_RX, quiet);

    PRINT("%s: Sweeping TX corrections...\n", __FUNCTION__);
    failures += sweep_vals(dev, BLADERF_MODULE_TX, quiet);

    PRINT("%s: Random TX corrections...\n", __FUNCTION__);
    failures += random_vals(dev, p, BLADERF_MODULE_TX, quiet);

    return failures;
}
