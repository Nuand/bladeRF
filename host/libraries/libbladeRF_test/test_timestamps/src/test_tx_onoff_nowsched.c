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

/* This test excercises switching between TX_NOW and scheduled TX's */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <libbladeRF.h>
#include "test_timestamps.h"
#include "minmax.h"

struct test_data {
    int16_t *burst1;
    int16_t *burst2;
    unsigned int gap_us;
    unsigned int gap_samples;
    unsigned int num_iterations;
    unsigned int burst_len;
};

static int send_bursts(struct bladerf *dev, struct app_params *p,
                       struct test_data *t)
{
    int status = 0;
    unsigned int i;
    struct bladerf_metadata meta;
    uint64_t curr_time, next_time;

    memset(&meta, 0, sizeof(meta));

    usleep(250);

    for (i = 0; i < t->num_iterations; i++) {

        meta.timestamp = 0;
        meta.flags = BLADERF_META_FLAG_TX_BURST_START |
                     BLADERF_META_FLAG_TX_NOW |
                     BLADERF_META_FLAG_TX_BURST_END;

        status = bladerf_sync_tx(dev, t->burst1, t->burst_len, &meta,
                                 2 * p->timeout_ms);

        if (status != 0) {
            fprintf(stderr, "TX_NOW @ iteration %u failed: %s\n",
                    i, bladerf_strerror(status));
            return status;
        }

        status = bladerf_get_timestamp(dev, BLADERF_MODULE_TX, &meta.timestamp);
        if (status != 0) {
            fprintf(stderr, "Failed to get current timestamp: %s\n",
                    bladerf_strerror(status));
            return status;
        }

        meta.timestamp += t->gap_samples;
        meta.flags = BLADERF_META_FLAG_TX_BURST_START |
                     BLADERF_META_FLAG_TX_BURST_END;

        status = bladerf_sync_tx(dev, t->burst2, t->burst_len, &meta,
                                 2 * p->timeout_ms);

        if (status != 0) {
            fprintf(stderr, "Scheduled TX @ iteration %u failed: %s\n",
                    i, bladerf_strerror(status));
            return status;
        }

        curr_time = meta.timestamp;
        next_time = curr_time + t->gap_samples + t->burst_len;

        do {
            status = bladerf_get_timestamp(dev, BLADERF_MODULE_TX, &curr_time);
            if (status != 0) {
                fprintf(stderr, "Failed to get current timestamp: %s\n",
                        bladerf_strerror(status));
                return status;
            }
        } while (curr_time < next_time);
    }

    return status;
}



int test_fn_tx_onoff_nowsched(struct bladerf *dev, struct app_params *p)
{
    int status, enable_status;
    struct test_data test;
    unsigned int i;

    memset(&test, 0, sizeof(test));
    test.num_iterations = 100;
    test.burst_len = 200;

    test.gap_us = 2000;
    test.gap_samples =
        (unsigned int) ((uint64_t)p->samplerate * test.gap_us / 1000000);

    test.burst1 = calloc(test.burst_len * 2,  sizeof(int16_t));
    if (test.burst1 == NULL) {
        return BLADERF_ERR_MEM;
    }

    test.burst2 = calloc(test.burst_len * 2, sizeof(int16_t));
    if (test.burst2 == NULL) {
        free(test.burst1);
        return BLADERF_ERR_MEM;
    }

    /* Leave two samples at the end at zero values, as this is required at
     * the end of a burst */
    for (i = 0; i < (test.burst_len - 2) * 2; i += 2) {
        test.burst1[i]       = 500;
        test.burst1[i + 1]   = 500;

        test.burst2[i]       = 2000;
        test.burst2[i + 1]   = 2000;
    }

    status = perform_sync_init(dev, BLADERF_MODULE_TX, 1024, p);
    if (status == 0) {
        status = send_bursts(dev, p, &test);
    }

    enable_status = bladerf_enable_module(dev, BLADERF_MODULE_TX, false);
    if (enable_status != 0) {
        fprintf(stderr, "Failed to disable TX module: %s\n",
                bladerf_strerror(status));

        if (status == 0) {
            status = enable_status;
        }
    }

    free(test.burst1);
    free(test.burst2);

    return status;
}
