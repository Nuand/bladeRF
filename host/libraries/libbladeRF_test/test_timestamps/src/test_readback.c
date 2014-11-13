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

/* This test is intended to simply check if timestamp readback is operational */
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <libbladeRF.h>
#include "test_timestamps.h"

struct readings {
    uint64_t rx, rx_delta;
    uint64_t tx, tx_delta;
    bool rx_neg, tx_neg;
};

void output_results(struct readings *r, unsigned int iterations,
                    const char *filename, unsigned int samplerate_tx,
                    unsigned int samplerate_rx)
{
    unsigned int i;
    FILE *f;
    double mean_rb;   /* Estimated time between readbacks */
    unsigned int rx_errors = 0, tx_errors = 0;

    mean_rb = 0;

    for (i = 1; i < iterations; i++) {
        if (r[i].rx >= r[i - 1].rx) {
            r[i].rx_delta = r[i].rx - r[i - 1].rx;
        } else {
            r[i].rx_neg = true;
            r[i].rx_delta = r[i - 1].rx - r[i].rx;
            rx_errors++;
        }

        if (r[i].tx >= r[i - 1].tx) {
            r[i].tx_delta = r[i].tx - r[i - 1].tx;
        } else {
            r[i].tx_neg = true;
            r[i].tx_delta = r[i - 1].tx - r[i].tx;
            tx_errors++;
        }

        mean_rb += r[i].tx_delta;
    }

    mean_rb /= (2 * iterations);        /* We do 2 readbacks per cycle */
    mean_rb = mean_rb / samplerate_tx;  /* We measure using the TX counter */


    printf("Done.\n");
    printf("Mean intra-readback time: %f s.\n", mean_rb);

    if (rx_errors) {
        printf("%u RX samples were not monotonic.\n", rx_errors);
    } else {
        printf("No RX errors.\n");
    }

    if (tx_errors) {
        printf("%u TX samples were not monotonic.\n", tx_errors);
    } else {
        printf("No TX errors.\n");
    }

    printf("Writing results to %s.\n", filename);

    f = fopen(filename, "w");
    if (f == NULL) {
        fprintf(stderr, "Failed to open %s: %s\n", filename, strerror(errno));
        return;
    }

    fprintf(f, "Mean intra-readback time: %f s.\n", mean_rb);
    fprintf(f, "RX samplerate: %u\n", samplerate_rx);
    fprintf(f, "TX samplerate: %u\n\n", samplerate_tx);

    for (i = 0; i < iterations; i++) {
        fprintf(f, "RX: %-10"PRIu64" (%s %-8"PRIu64")     ", r[i].rx,
                r[i].rx_neg ? "-" : "+", r[i].rx_delta);
        fprintf(f, "TX: %-10"PRIu64" (%s %-8"PRIu64")\n", r[i].tx,
                r[i].tx_neg ? "-" : "+", r[i].tx_delta);
    }

    fclose(f);
}

int test_fn_readback(struct bladerf *dev, struct app_params *p)
{
    int status, ret;
    bladerf_loopback lb_backup;
    unsigned int samplerate_rx, samplerate_tx, samplerate_tx_backup;
    const unsigned int iterations = 10000;
    unsigned int i;
    struct readings *readings;

    status = bladerf_get_loopback(dev, &lb_backup);
    if (status != 0) {
        fprintf(stderr, "Failed to get current loopback mode.\n");
        return status;
    }

    /* Generally, it's recommended that both modules be configured for the
     * same samplerate when using timestamps. However, it is possible to do, if
     * the appropriate scaling factor is applied.
     *
     * Here we configure the modules for separate sample rates just to see them
     * changing at different rates */

    status = bladerf_get_sample_rate(dev, BLADERF_MODULE_RX, &samplerate_rx);
    if (status != 0) {
        fprintf(stderr, "Failed to get current RX sample rate: %s\n",
                bladerf_strerror(status));
        return status;
    }

    status = bladerf_get_sample_rate(dev, BLADERF_MODULE_TX,
                                     &samplerate_tx_backup);
    if (status != 0) {
        fprintf(stderr, "Failed to get current TX sample rate: %s\n",
                bladerf_strerror(status));
        return status;
    }

    samplerate_tx = 2 * samplerate_rx;
    if (samplerate_tx > BLADERF_SAMPLERATE_REC_MAX) {
        samplerate_tx = samplerate_rx / 2;
    }

    readings = calloc(iterations, sizeof(readings[0]));
    if (readings == NULL) {
        perror("malloc");
        return BLADERF_ERR_MEM;
    }

    status = bladerf_set_sample_rate(dev, BLADERF_MODULE_TX,
                                     samplerate_tx, NULL);

    if (status != 0) {
        fprintf(stderr, "Failed to set TX sample rate: %s\n",
                bladerf_strerror(status));
    }

    status = bladerf_set_loopback(dev, BLADERF_LB_BB_TXVGA1_RXVGA2);
    if (status != 0) {
        fprintf(stderr, "Failed to set loopback mode.\n");
        goto out;
    }

    /* Setup the metadata format to enable the timestamp bit.
     * There's no need to actually call the rx/tx functions and start the
     * underlying worker threads. */
    status = bladerf_sync_config(dev, BLADERF_MODULE_RX,
                                 BLADERF_FORMAT_SC16_Q11_META, p->num_buffers,
                                 p->buf_size, p->num_xfers, p->timeout_ms);
    if (status != 0) {
        fprintf(stderr, "Failed to configure RX sync i/f: %s\n",
                bladerf_strerror(status));
        goto out;
    }

    status = bladerf_sync_config(dev, BLADERF_MODULE_TX,
                                 BLADERF_FORMAT_SC16_Q11_META, p->num_buffers,
                                 p->buf_size, p->num_xfers, p->timeout_ms);
    if (status != 0) {
        fprintf(stderr, "Failed to configure TX sync i/f: %s\n",
                bladerf_strerror(status));
        goto out;
    }

    printf("Running for %u iterations (~10-20 seconds)...\n", iterations);

    for (i = 0; i < iterations; i++) {
        status = bladerf_get_timestamp(dev, BLADERF_MODULE_RX, &readings[i].rx);
        if (status != 0) {
            fprintf(stderr, "Failed to fetch RX timestamp: %s\n",
                    bladerf_strerror(status));
            goto out;
        }

        status = bladerf_get_timestamp(dev, BLADERF_MODULE_TX, &readings[i].tx);
        if (status != 0) {
            fprintf(stderr, "Failed to fetch TX timestamp: %s\n",
                    bladerf_strerror(status));
            goto out;
        }
    }

out:
    ret = status;

    status = bladerf_enable_module(dev, BLADERF_MODULE_RX, false);
    if (status != 0) {
        fprintf(stderr, "Failed to disable RX module: %s\n",
                bladerf_strerror(status));
    }

    status = bladerf_enable_module(dev, BLADERF_MODULE_TX, false);
    if (status != 0) {
        fprintf(stderr, "Failed to disable TX module: %s\n",
                bladerf_strerror(status));
    }

    status = bladerf_set_sample_rate(dev, BLADERF_MODULE_TX,
                                     samplerate_tx_backup, NULL);

    if (status != 0) {
        fprintf(stderr, "Failed to restore TX sample rate: %s\n",
                bladerf_strerror(status));
    }

    status = bladerf_set_loopback(dev, lb_backup);
    if (status != 0) {
        fprintf(stderr, "Failed to restore loopback settings:%s \n",
                bladerf_strerror(status));
    }

    if (ret == 0) {
        output_results(readings, iterations, "readback_results.txt",
                       samplerate_tx, samplerate_rx);
    }

    free(readings);
    return ret;
}
