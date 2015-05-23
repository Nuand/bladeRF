/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2015 Nuand LLC
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libbladeRF.h>
#include "conversions.h"

#define BUF_LEN    4096
#define TIMEOUT_MS 2500

#define ITERATIONS 10000

/* Select frequencies  such that we ensure we cross the low/high band limit */
#define F1 ((unsigned int) 1.499e9)
#define F2 ((unsigned int) 1.501e9)

int run_test(struct bladerf *dev)
{
    int status;
    struct bladerf_quick_tune f1, f2;
    int16_t *samples = NULL;
    unsigned int i;

    samples = malloc(2 * BUF_LEN * sizeof(samples[0]));
    if (samples == NULL) {
        perror("malloc");
        return BLADERF_ERR_MEM;
    }

    /* Just send a carrier tone */
    for (i = 0; i < (2 * BUF_LEN); i += 2) {
        samples[i] = samples[i+1] = 1448;;
    }

    status = bladerf_sync_config(dev, BLADERF_MODULE_TX,
                                 BLADERF_FORMAT_SC16_Q11,
                                 16, 4096, 8, TIMEOUT_MS);
    if (status != 0) {
        fprintf(stderr, "Failed to initialize sync i/f: %s\n",
                bladerf_strerror(status));
        goto out;
    }

    /* Set up our quick tune set */

    status = bladerf_set_frequency(dev, BLADERF_MODULE_TX, F1);
    if (status != 0) {
        fprintf(stderr, "Failed to set frequency to %u: %s\n",
                F1, bladerf_strerror(status));
        goto out;
    }

    status = bladerf_get_quick_tune(dev, BLADERF_MODULE_TX, &f1);
    if (status != 0) {
        fprintf(stderr, "Failed to get f1 quick tune: %s\n",
                bladerf_strerror(status));
        goto out;
    }

    status = bladerf_set_frequency(dev, BLADERF_MODULE_TX, F2);
    if (status != 0) {
        fprintf(stderr, "Failed to set frequency to %u: %s\n",
                F1, bladerf_strerror(status));
        goto out;
    }

    status = bladerf_get_quick_tune(dev, BLADERF_MODULE_TX, &f2);
    if (status != 0) {
        fprintf(stderr, "Failed to get f2 quick tune: %s\n",
                bladerf_strerror(status));
        goto out;
    }

    /* Enable and run! */

    status = bladerf_enable_module(dev, BLADERF_MODULE_TX, true);
    if (status != 0) {
        fprintf(stderr, "Failed to enable module: %s\n",
                bladerf_strerror(status));
        goto out;
    }


    for (i = 0; i < ITERATIONS; i++) {
        status = bladerf_schedule_retune(dev, BLADERF_MODULE_TX,
                                         BLADERF_RETUNE_NOW, 0, &f1);

        if (status != 0) {
            fprintf(stderr, "Failed to perform quick tune to f1: %s\n",
                    bladerf_strerror(status));
            goto out;
        }

        status = bladerf_sync_tx(dev, samples, BUF_LEN, NULL, 3500);
        if (status != 0) {
            fprintf(stderr, "Failed to TX data: %s\n",
                    bladerf_strerror(status));
            goto out;
        }

        status = bladerf_schedule_retune(dev, BLADERF_MODULE_TX,
                                         BLADERF_RETUNE_NOW, 0, &f2);

        if (status != 0) {
            fprintf(stderr, "Failed to perform quick tune to f2: %s\n",
                    bladerf_strerror(status));
            goto out;
        }

        status = bladerf_sync_tx(dev, samples, BUF_LEN, NULL, 3500);
        if (status != 0) {
            fprintf(stderr, "Failed to TX data: %s\n",
                    bladerf_strerror(status));
            goto out;
        }
    }

out:
    free(samples);
    bladerf_enable_module(dev, BLADERF_MODULE_TX, false);
    return status;
}

int main(int argc, char *argv[])
{
    int status;
    struct bladerf *dev = NULL;
    const char *devstr = NULL;

    if (argc > 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
        fprintf(stderr, "TX a tone across 4 frequencies.\n");
        fprintf(stderr, "Usage: %s [device string]\n", argv[0]);
        return 1;
    } else {
        devstr = argv[1];
    }

    status = bladerf_open(&dev, devstr);
    if (status != 0) {
        fprintf(stderr, "Unable to open device: %s\n",
                bladerf_strerror(status));
        return status;
    }

    status = run_test(dev);

    bladerf_close(dev);
    return status;
}
