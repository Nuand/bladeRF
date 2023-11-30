/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2023 Nuand LLC
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
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <inttypes.h>
#include <libbladeRF.h>
#include "test_timestamps.h"
#include "minmax.h"

#ifdef _WIN32
#include "gettimeofday.h"
#else
#include <sys/time.h>
#endif

/* This test requires external verification via a spectrum analyzer.
 * It simply transmits ON/OFF bursts, and is more intended to ensure the API
 * functions aren't bombing out than it is to exercise signal integrity/timing.
 */

#define MAGNITUDE 2000

struct test_case {
    unsigned int buf_len;
    unsigned int burst_len;     /* Length of a burst, in samples */
    unsigned int iterations;
};


static void init_app_params(struct app_params *p) {
    memset(p, 0, sizeof(p[0]));
    p->samplerate = 1e6;
    p->prng_seed = 1;

    p->num_buffers = 16;
    p->num_xfers = 8;
    p->buf_size = 64 * 1024;
    p->timeout_ms = 10000;
}

int main(int argc, char *argv[]) {
    struct app_params p;
    struct bladerf *dev = NULL;
    int status, status_out, status_wait;
    unsigned int samples_left;
    size_t i;
    struct bladerf_metadata meta;
    int16_t *samples, *buf;
    struct timeval start, end;
    double time_passed;

    struct test_case test;
    test.buf_len = 2048;
    test.burst_len = 100e3;
    unsigned int num_zero_samples = 1e3;
    test.iterations = 10;

    init_app_params(&p);
    p.buf_size = test.burst_len + num_zero_samples;

    samples = calloc(2 * sizeof(int16_t), test.burst_len + num_zero_samples);
    if (samples == NULL) {
        perror("malloc");
        return BLADERF_ERR_MEM;
    }

    status = bladerf_open(&dev, p.device_str);
    if (status != 0) {
        fprintf(stderr, "Failed to open device: %s\n",
                bladerf_strerror(status));
        goto out;
    }

    status = bladerf_set_sample_rate(dev, BLADERF_MODULE_TX, p.samplerate, NULL);
    if (status != 0) {
        fprintf(stderr, "Failed to set TX sample rate: %s\n",
                bladerf_strerror(status));
        goto out;
    }

    /* Leave the last two samples zero */
    for (i = 0; i < (2 * test.burst_len); i += 2) {
        samples[i] = samples[i + 1] = MAGNITUDE;
    }

    memset(&meta, 0, sizeof(meta));

    status = perform_sync_init(dev, BLADERF_MODULE_TX, test.buf_len, &p);
    if (status != 0) {
        goto out;
    }

    status = bladerf_set_frequency(dev, BLADERF_MODULE_TX, 900e6);
    if (status != 0) {
        fprintf(stderr, "Failed to set TX frequency: %s\n", bladerf_strerror(status));
        goto out;
    }

    status = bladerf_get_timestamp(dev, BLADERF_MODULE_TX, &meta.timestamp);
    if (status != 0) {
        fprintf(stderr, "Failed to get timestamp: %s\n",
                bladerf_strerror(status));
        goto out;
    } else {
        printf("Initial timestamp: 0x%016"PRIx64"\n", meta.timestamp);
    }

    meta.timestamp += 200000;


    gettimeofday(&start, NULL);
    for (i = 0; i < test.iterations && status == 0; i++) {
        meta.flags = BLADERF_META_FLAG_TX_BURST_START;
        samples_left = test.burst_len + num_zero_samples;
        buf = samples;


        printf("Sending burst @ %"PRIu64"\n", meta.timestamp);

        while (samples_left && status == 0) {
            unsigned int to_send = uint_min(p.buf_size, samples_left);

            if (to_send == samples_left) {
                meta.flags |= BLADERF_META_FLAG_TX_BURST_END;
            } else {
                meta.flags &= ~BLADERF_META_FLAG_TX_BURST_END;
            }

            status = bladerf_sync_tx(dev, buf, to_send, &meta, 10000); //p->timeout_ms);
            if (status != 0) {
                fprintf(stderr, "TX failed @ iteration (%u) %s\n",
                        (unsigned int )i, bladerf_strerror(status));
            }

            meta.flags &= ~BLADERF_META_FLAG_TX_BURST_START;
            samples_left -= to_send;
            buf += 2 * to_send;
        }

        meta.timestamp += p.samplerate;
    }

    printf("Waiting for samples to finish...\n");
    fflush(stdout);

    /* Wait for samples to be transmitted before shutting down the TX module */
    status_wait = wait_for_timestamp(dev, BLADERF_MODULE_TX, meta.timestamp, 3000);
    if (status_wait != 0) {
        status = first_error(status, status_wait);
        fprintf(stderr, "Failed to wait for TX to finish: %s\n",
                bladerf_strerror(status_wait));
    }
    gettimeofday(&end, NULL);
    time_passed = (end.tv_sec - start.tv_sec) + (end.tv_usec - end.tv_usec) / 1000000.0;
    printf("TX finished in %.4f seconds\n", time_passed);
out:
    status_out = bladerf_enable_module(dev, BLADERF_MODULE_TX, false);
    if (status_out != 0) {
        fprintf(stderr, "Failed to disable TX module: %s\n",
                bladerf_strerror(status));
    } else {
        printf("Done waiting.\n");
        fflush(stdout);
    }

    status = first_error(status, status_out);

    free(samples);
    return status;
}