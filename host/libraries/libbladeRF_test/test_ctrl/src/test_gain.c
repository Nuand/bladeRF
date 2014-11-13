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

DECLARE_TEST_CASE(gain);

struct gain {
    const char *name;
    int min;
    int max;
    int inc;
    int (*set)(struct bladerf *dev, int gain);
    int (*get)(struct bladerf *dev, int *gain);
};

static int set_lna_gain(struct bladerf *dev, int gain)
{
    bladerf_lna_gain lna_gain = (bladerf_lna_gain) gain;
    return bladerf_set_lna_gain(dev, lna_gain);
}

static int get_lna_gain(struct bladerf *dev, int *gain)
{
    int status;
    bladerf_lna_gain lna_gain = BLADERF_LNA_GAIN_UNKNOWN;

    status = bladerf_get_lna_gain(dev, &lna_gain);
    if (status == 0) {
        *gain = (int) lna_gain;
    }

    return status;
}

const struct gain gains[] = {
    {
        "TXVGA1",
        BLADERF_TXVGA1_GAIN_MIN,
        BLADERF_TXVGA1_GAIN_MAX,
        1,
        bladerf_set_txvga1,
        bladerf_get_txvga1
    },

    {
        "TXVGA2",
        BLADERF_TXVGA2_GAIN_MIN,
        BLADERF_TXVGA2_GAIN_MAX,
        1,
        bladerf_set_txvga2,
        bladerf_get_txvga2
    },

    {
        "LNA",
        BLADERF_LNA_GAIN_BYPASS,
        BLADERF_LNA_GAIN_MAX,
        1,
        set_lna_gain,
        get_lna_gain,
    },

    {
        "RXVGA1",
        BLADERF_RXVGA1_GAIN_MIN,
        BLADERF_RXVGA1_GAIN_MAX,
        1,
        bladerf_set_rxvga1,
        bladerf_get_rxvga1,
    },

    {
        "RXVGA2",
        BLADERF_RXVGA2_GAIN_MIN,
        BLADERF_RXVGA2_GAIN_MAX,
        3,
        bladerf_set_rxvga2,
        bladerf_get_rxvga2,
    },

    /* TODO exercise overall bladerf_set_gain() function */
};

static inline int set_and_check(struct bladerf *dev, const struct gain *g,
                                int gain)
{
    int status;
    int readback;

    status = g->set(dev, gain);
    if (status != 0) {
        PR_ERROR("Failed to set %s gain: %s\n",
                 g->name, bladerf_strerror(status));
        return status;
    }

    status = g->get(dev, &readback);
    if (status != 0) {
        PR_ERROR("Failed to read back %s gain: %s\n",
                 g->name, bladerf_strerror(status));
        return status;
    }

    if (gain != readback) {
        PR_ERROR("Erroneous %s gain readback=%d, expected=%d\n",
                 g->name, readback, gain);
        return -1;
    }

    return 0;
}

static unsigned int gain_sweep(struct bladerf *dev, bool quiet)
{
    int status;
    int gain;
    unsigned int failures = 0;
    size_t i;

    for (i = 0; i < ARRAY_SIZE(gains); i++) {
        PRINT("    %s\n", gains[i].name);
        fflush(stdout);
        for (gain = gains[i].min; gain <= gains[i].max; gain += gains[i].inc) {
            status = set_and_check(dev, &gains[i], gain);
            if (status != 0) {
                failures++;
            }
        }

    }

    return failures;
}

static int random_gains(struct bladerf *dev, struct app_params *p, bool quiet)
{
    int status, gain;
    unsigned int i, j;
    const unsigned int iterations = 250;
    unsigned int failures = 0;

    for (i = 0; i < ARRAY_SIZE(gains); i++) {
        const int n_incs = (gains[i].max - gains[i].min) / gains[i].inc;
        PRINT("    %s\n", gains[i].name);

        for (j = 0; j < iterations; j++) {
            randval_update(&p->randval_state);
            gain = gains[i].min + (p->randval_state % n_incs) * gains[i].inc;

            if (gain > gains[i].max) {
                gain = gains[i].max;
            } else if (gain < gains[i].min) {
                gain = gains[i].min;
            }

            status = set_and_check(dev, &gains[i], gain);
            if (status != 0) {
                failures++;
            }
        }
    }

    return failures;
}

unsigned int test_gain(struct bladerf *dev, struct app_params *p, bool quiet)
{
    unsigned int failures = 0;

    PRINT("%s: Performing gain sweep...\n", __FUNCTION__);
    failures += gain_sweep(dev, quiet);

    PRINT("%s: Applying random gains...\n", __FUNCTION__);
    failures += random_gains(dev, p, quiet);

    return failures;
}
