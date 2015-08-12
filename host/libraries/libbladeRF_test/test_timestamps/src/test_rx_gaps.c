/*
 * This test simply receives any available samples and checks that there are no
 * gaps/jumps in the expected timestamp
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

#define RANDOM_GAP_SIZE 0

struct test_case {
    uint64_t gap;
    unsigned int iterations;
};

static const struct test_case tests[] = {
    { 1,    2000000 },
    { 2,    2000000 },
    { 128,  75000 },
    { 256,  50000 },
    { 512,  50000 },
    { 1023, 10000 },
    { 1024, 10000 },
    { 1025, 10000 },
    { 2048, 5000 },
    { 3172, 5000 },
    { 4096, 2500 },
    { 8192, 2500 },
    { 16 * 1024, 1000 },
    { 32 * 1024, 1000 },
    { 64 * 1024, 1000 },
    { RANDOM_GAP_SIZE, 500 },
};

static inline unsigned int get_gap(struct app_params *p,
                                   const struct test_case *t,
                                   unsigned int buf_len)
{
    uint64_t gap;

    if (t->gap == 0) {
        const uint64_t tmp = randval_update(&p->prng_state) % buf_len;
        if (tmp == 0) {
            gap = buf_len;
        } else {
            gap = tmp;
        }
    } else {
        gap = t->gap;
    }

    assert(gap <= UINT_MAX);
    assert(gap <= buf_len);

    return (unsigned int) gap;
}

static inline uint64_t delta(uint64_t a, uint64_t b)
{
    return (a > b) ? a - b : b - a;
}

enum check_result {
    NO_ERRORS,
    DETECTED_OVERRUN,   /* A legitimate overrun occurred and was detected */
    FAILURE_DETECTED,   /* Overrun condition occurred but weren't flagged,
                         * or vice versa */
};

static inline enum check_result check_data(int16_t *samples,
                                           struct bladerf_metadata *meta,
                                           uint64_t timestamp,
                                           uint32_t counter,
                                           unsigned int n_read,
                                           bool *supress_overrun_msg)
{
    const uint64_t ts_delta =
        timestamp == UINT64_MAX ? 0 : delta(meta->timestamp, timestamp);

    const bool read_trunc = (meta->actual_count != n_read);
    const bool data_discont = !counter_data_is_valid(samples,
                                                     meta->actual_count,
                                                     counter);

    enum check_result result;
    bool had_error;

    had_error = (ts_delta != 0 || read_trunc || data_discont);

    if (!had_error) {
        const bool overrun = (meta->status & BLADERF_META_STATUS_OVERRUN) != 0;
        if (overrun && timestamp != UINT64_MAX) {
            result = FAILURE_DETECTED;
            fprintf(stderr, "Error: Overrun flagged without any symptoms!\n");
        } else {
            result = NO_ERRORS;
        }
    } else {
        if (meta->status & BLADERF_META_STATUS_OVERRUN) {
            result = DETECTED_OVERRUN;

            if (! (*supress_overrun_msg) ) {
                *supress_overrun_msg = true;
                fprintf(stderr, "Metadata indicates an overrun occurred, with "
                        "%u samples returned.\n", meta->actual_count);
                fprintf(stderr, "  Timestamp delta: %"PRIu64"\n", ts_delta);
                fprintf(stderr, "Suppressing additional messages for correctly "
                        "detected overruns.\n");
            }
        } else {
            result = FAILURE_DETECTED;
            fprintf(stderr, "Error: Metadata did NOT report an overrun for:\n");
            fprintf(stderr, "  Timestamp delta: %"PRIu64"\n", ts_delta);
            fprintf(stderr, "  Actual Read count: %u\n", meta->actual_count);
            fprintf(stderr, "  Data discontinuity: %s\n",
                                data_discont ? "Yes" : "No");
        }

        /* Within the samples we received, there should *never* be a data
         * discontinuity, since correctly detected overruns will return
         * the contiguous chunks of samples */
        if (data_discont) {
            result = FAILURE_DETECTED;
            fprintf(stderr,
                    "Error: Unexpected data discontinuity encountered @ "
                    "t=0x%"PRIx64"\n", meta->timestamp);
        }

    }

    return result;
}

static int run(struct bladerf *dev, struct app_params *p,
               const struct test_case *t)
{
    int status, status_out;
    struct bladerf_metadata meta;
    int16_t *samples;
    uint64_t timestamp;
    unsigned int gap;
    uint32_t counter;
    uint64_t tscount_diff;
    unsigned int i;
    bool suppress_overrun_msg = false;
    unsigned int overruns = 0;
    bool prev_iter_overrun = false;
    const unsigned int buf_len = (t->gap == RANDOM_GAP_SIZE) ?
                                    (128 * 1024) : t->gap;

    samples = calloc(buf_len, 2 * sizeof(int16_t));
    if (samples == NULL) {
        perror("calloc");
        status = -1;
        goto out;
    }

    /* Clear out metadata and request that we just received any available
     * samples, with the timestamp filled in for us */
    memset(&meta, 0, sizeof(meta));
    meta.flags = BLADERF_META_FLAG_RX_NOW;

    status = enable_counter_mode(dev, true);
    if (status != 0) {
        goto out;
    }

    status = perform_sync_init(dev, BLADERF_MODULE_RX, 0, p);
    if (status != 0) {
        goto out;
    }

    /* Initial read to get a starting timestamp, and counter value */
    gap = get_gap(p, t, buf_len);
    status = bladerf_sync_rx(dev, samples, gap, &meta, p->timeout_ms);
    if (status != 0) {
        fprintf(stderr, "Intial RX failed: %s\n", bladerf_strerror(status));
        goto out;
    }

    counter = extract_counter_val((uint8_t*) samples);
    timestamp = meta.timestamp;

    assert(timestamp >= (uint64_t) counter);
    tscount_diff = timestamp - (uint64_t) counter;

    if (t->gap != 0) {
        printf("\nTest Case: Read size=%"PRIu64" samples, %u iterations\n",
                t->gap, t->iterations);
    } else {
        printf("\nTest Case: Random read size, %u iterations\n", t->iterations);
    }

    printf("--------------------------------------------------------\n");

    assert((timestamp - tscount_diff) <= UINT32_MAX);
    status = check_data(samples, &meta, UINT64_MAX,
                        (uint32_t) (timestamp - tscount_diff),
                        meta.actual_count, &suppress_overrun_msg);

    if (status == DETECTED_OVERRUN) {
        overruns++;
        status = 0;
    }

    printf("Timestamp-counter diff: %"PRIu64"\n", tscount_diff);
    printf("Initial timestamp:      0x%016"PRIx64"\n", meta.timestamp);
    printf("Intital counter value:  0x%08"PRIx32"\n", counter);
    printf("Initial status:         0x%08"PRIu32"\n", meta.status);

    for (i = 0; i < t->iterations && status == 0; i++) {

        timestamp = meta.timestamp + gap;
        gap = get_gap(p, t, buf_len);

        status = bladerf_sync_rx(dev, samples, gap, &meta, p->timeout_ms);
        if (status != 0) {
            fprintf(stderr, "RX %u failed: %s\n", i, bladerf_strerror(status));
            goto out;
        }

        /* If an overrun occured on the previous iteration, we don't know what
         * the timestamp will actually be on this iteration. */
        if (prev_iter_overrun) {
            timestamp = meta.timestamp;
        }

        status = check_data(samples, &meta, timestamp,
                            (uint32_t) (timestamp - tscount_diff),
                            gap, &suppress_overrun_msg);

        if (status == DETECTED_OVERRUN) {
            overruns++;
            status = 0;

            prev_iter_overrun = true;
        }
    }

    if (status != 0) {
        printf("Test failed due to errors.\n");
    } else if (overruns != 0) {
        printf("Test failed due to %u overruns.\n", overruns);
        status = -1;
    } else {
        printf("Test passed.\n");
    }

out:
    free(samples);

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

int test_fn_rx_gaps(struct bladerf *dev, struct app_params *p)
{
    int status = 0;
    size_t i;
    unsigned int failures = 0;

    for (i = 0; i < ARRAY_SIZE(tests); i++) {
        status = run(dev, p, &tests[i]);
        if (status != 0) {
            failures++;
        }
    }

    printf("\n%u test cases failed.\n", failures);
    return status;
}
