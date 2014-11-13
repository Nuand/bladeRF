/*
 * This test excercises functionatity to wait for specific timestamps
 *
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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include <inttypes.h>
#include <libbladeRF.h>
#include "test_timestamps.h"
#include "rel_assert.h"

struct test_case {
    uint64_t gap;
    unsigned int read_size;
    unsigned int iterations;
};

static const struct test_case tests[] = {
    /* Gap          Read Size       Iterations */
    { 1,               1,               100000 },
    { 17,               1,              50000 },
    { 17,               2,              50000 },
    { 17,               13,             50000 },
    { 17,               16,             50000 },
    { 1 * 1024,         1,              2000 },
    { 1 * 1024,         2,              2000 },
    { 1 * 1024,         13,             2000 },
    { 1 * 1024,         16,             2000 },
    { 1 * 1024,         64,             2000 },
    { 1 * 1024,         64,             2000 },
    { 1 * 1024,         1024,           2000 },
    { 2 * 1024,         1,              2000 },
    { 2 * 1024,         2,              2000 },
    { 2 * 1024,         13,             2000 },
    { 2 * 1024,         16,             2000 },
    { 2 * 1024,         64,             2000 },
    { 2 * 1024,         64,             2000 },
    { 2 * 1024,         1024,           2000 },
    { 2 * 1024,         2 * 1024,       2000 },
    { 1024 * 1024,      64,             50 },
    { 1024 * 1024,      1023,           50 },
    { 1024 * 1024,      1024,           50 },
    { 1024 * 1024,      4095,           50 },
    { 1024 * 1024,      4096,           50 },
    { 1024 * 1024,      4097,           50 },
};

static int run(struct bladerf *dev, struct app_params *p,
               int16_t *samples, const struct test_case *t)
{
    int status, status_out;
    struct bladerf_metadata meta;
    unsigned int i;
    uint32_t counter;
    uint64_t tscount_diff;
    bool pass = true;
    unsigned int discont = 0;

    memset(&meta, 0, sizeof(meta));

    status = enable_counter_mode(dev, true);
    if (status != 0) {
        goto out;
    }

    status = perform_sync_init(dev, BLADERF_MODULE_RX, 0, p);
    if (status != 0) {
        goto out;
    }

    /* Perform an initial read to get the difference between the timestamp
     * and the FPGA counter */
    meta.flags = BLADERF_META_FLAG_RX_NOW;
    status = bladerf_sync_rx(dev, samples, t->read_size, &meta, p->timeout_ms);
    if (status != 0) {
        fprintf(stderr, "Intial RX failed: %s\n", bladerf_strerror(status));
        goto out;
    }
    meta.flags = 0;

    counter = extract_counter_val((uint8_t*) samples);
    assert(meta.timestamp >= (uint64_t) counter);
    tscount_diff = meta.timestamp - (uint64_t) counter;

    printf("\nTest case: Gap=%"PRIu64" samples, Read size=%u, %u iterations\n",
           t->gap, t->read_size, t->iterations);
    printf("--------------------------------------------------------\n");
    printf("  Intial timestamp:             0x%"PRIx64"\n", meta.timestamp);
    printf("  Timestamp-counter difference: %"PRIu64"\n", tscount_diff);

    /* Start 250ms in the future */
    meta.timestamp += p->samplerate / 4;

    for (i = 0; i < t->iterations && status == 0 && pass; i++) {
        assert(t->gap >= t->read_size);

        meta.timestamp += t->gap;

        status = bladerf_sync_rx(dev, samples, t->read_size,
                                 &meta, p->timeout_ms);
        if (status != 0) {
            fprintf(stderr, "RX #%u @ 0x%"PRIx64" failed: %s\n",
                    i, meta.timestamp, bladerf_strerror(status));
            pass = false;
        } else if (meta.status & BLADERF_META_STATUS_OVERRUN) {
            discont++;
            fprintf(stderr, "RX overrun in read @ t=0x%"PRIu64
                    ". Got %u samples.\n", meta.timestamp, meta.actual_count);
        }

        assert((meta.timestamp - tscount_diff) <= UINT32_MAX);
        if (!counter_data_is_valid(samples, meta.actual_count,
                                   (uint32_t) (meta.timestamp - tscount_diff))) {
            pass = false;
        } else {
            counter += (unsigned int) t->gap;
        }
    }

    if (!pass) {
        printf("Test failed.\n");
    } else if (discont != 0) {
        printf("Test failed due to discontinuities.\n");
    } else {
        printf("Test passed.\n");
    }

out:
    status_out = bladerf_enable_module(dev, BLADERF_MODULE_RX, false);
    if (status_out != 0) {
        fprintf(stderr, "Failed to disable RX module: %s\n",
                bladerf_strerror(status));
    }
    status = first_error(status, status_out);

    status_out = enable_counter_mode(dev, false);
    status = first_error(status, status_out);

    return status;
}

int test_fn_rx_scheduled(struct bladerf *dev, struct app_params *p)
{
    int status = 0;
    int16_t *samples;
    size_t i;

    samples = malloc(p->buf_size * 2 * sizeof(int16_t));
    if (samples == NULL) {
        perror("malloc");
        return BLADERF_ERR_MEM;
    }

    for (i = 0; i < ARRAY_SIZE(tests) && status == 0; i++) {
        status = run(dev, p, samples, &tests[i]);
    }

    free(samples);
    return status;
}
