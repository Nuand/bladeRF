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

/* This test TX's some On-Off bursts and verifies the burst length and gaps
 * via loopback to the RX module */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <inttypes.h>
#include <pthread.h>
#include <libbladeRF.h>
#include "test_timestamps.h"
#include "loopback.h"
#include "minmax.h"

static void * tx_task(void *args)
{
    int status;
    int16_t *samples;
    unsigned int i;
    struct bladerf_metadata meta;
    uint64_t samples_left;
    struct loopback_burst_test *t = (struct loopback_burst_test *) args;
    bool stop = false;
    int16_t zeros[] = { 0, 0, 0, 0 };

    memset(&meta, 0, sizeof(meta));

    samples = alloc_loopback_samples(t->params->buf_size);
    if (samples == NULL) {
        return NULL;
    }

    status = bladerf_get_timestamp(t->dev, BLADERF_MODULE_TX, &meta.timestamp);
    if (status != 0) {
        fprintf(stderr, "Failed to get current timestamp: %s\n",
                bladerf_strerror(status));
    }

    meta.timestamp += 400000;

    for (i = 0; i < t->num_bursts && !stop; i++) {
        meta.flags = BLADERF_META_FLAG_TX_BURST_START;
        samples_left = t->bursts[i].duration;

        assert(samples_left <= UINT_MAX);

        if (i != 0) {
            meta.timestamp += (t->bursts[i-1].duration + t->bursts[i].gap);
        }

        while (samples_left != 0 && status == 0) {
            unsigned int to_send = uint_min(t->params->buf_size,
                                            (unsigned int) samples_left);

            status = bladerf_sync_tx(t->dev, samples, to_send, &meta,
                                     t->params->timeout_ms);

            if (status != 0) {
                fprintf(stderr, "Failed to TX @ burst %-4u with %"PRIu64
                        " samples left: %s\n",
                        i + 1, samples_left, bladerf_strerror(status));

                /* Stop the RX worker */
                pthread_mutex_lock(&t->lock);
                t->stop = true;
                pthread_mutex_unlock(&t->lock);
            }

            meta.flags &= ~BLADERF_META_FLAG_TX_BURST_START;
            samples_left -= to_send;
        }

        /* Flush TX samples by ensuring we have 2 zero samples at the end
         * of our burst (as required by libbladeRF) */
        if (status == 0) {
            meta.flags = BLADERF_META_FLAG_TX_BURST_END;
            status = bladerf_sync_tx(t->dev, zeros, 2, &meta,
                                     t->params->timeout_ms);

            if (status != 0) {
                fprintf(stderr, "Failed to flush TX: %s\n",
                        bladerf_strerror(status));

                /* Stop the RX worker */
                pthread_mutex_lock(&t->lock);
                t->stop = true;
                pthread_mutex_unlock(&t->lock);
            }
        }

        pthread_mutex_lock(&t->lock);
        stop = t->stop;
        pthread_mutex_unlock(&t->lock);
    }

    /* Wait for samples to finish */
    printf("TX: Waiting for samples to finish.\n");
    fflush(stdout);
    status = wait_for_timestamp(t->dev, BLADERF_MODULE_TX,
                                meta.timestamp + t->bursts[i - 1].duration,
                                3000);

    if (status != 0) {
        fprintf(stderr, "Failed to wait for TX to complete: %s\n",
                bladerf_strerror(status));
    }

    free(samples);

    printf("TX: Exiting task.\n");
    fflush(stdout);
    return NULL;
}

static inline int fill_bursts(struct loopback_burst_test *t)
{
    uint64_t i;
    const uint64_t min_duration = 16;
    const uint64_t max_duration = 4 * t->params->buf_size;
    const uint64_t max_gap = 3 * max_duration;
    const uint64_t min_gap = t->params->buf_size;
    uint64_t prng_val, tmp;
    const char filename[] = "bursts.txt";
    FILE *f;

    f = fopen(filename, "w");
    if (f == NULL) {
        perror("fopen");
        return -1;
    }

    randval_init(&t->params->prng_state, t->params->prng_seed);

    for (i = 0; i < t->num_bursts; i++) {
        prng_val = t->params->prng_state;
        randval_update(&t->params->prng_state);

        tmp = t->params->prng_state % (max_duration - min_duration + 1);

        t->bursts[i].duration = tmp + min_duration;

        randval_update(&t->params->prng_state);

        if (i != 0) {
            tmp = t->params->prng_state % (max_gap - min_gap + 1);
            t->bursts[i].gap = tmp + min_gap;
        } else {
            t->bursts[i].gap = 0;
        }


        fprintf(f, "Burst #%-4"PRIu64
                   " gap=%-8"PRIu64
                   " duration=%-8"PRIu64
                   " prng=0x%016"PRIx64"\n",
               i + 1, t->bursts[i].gap, t->bursts[i].duration, prng_val);
    }

    fprintf(f, "\n");
    printf("Burst descriptions written to %s.\n", filename);

    fclose(f);
    return 0;
}


int test_fn_loopback_onoff(struct bladerf *dev, struct app_params *p)
{
    int status = 0;
    struct loopback_burst_test test;
    pthread_t tx_thread;
    bool tx_started = false;

    pthread_t rx_thread;
    bool rx_started = false;
    bool rx_ready = false;

    test.dev = dev;
    test.params = p;
    test.num_bursts = 1000;
    test.stop = false;
    test.rx_ready = false;

    pthread_mutex_init(&test.lock, NULL);

    test.bursts = (struct loopback_burst *) malloc(test.num_bursts * sizeof(test.bursts[0]));
    if (test.bursts == NULL) {
        perror("malloc");
        return -1;
    } else {
        fill_bursts(&test);
    }

    status = setup_device_loopback(&test);
    if (status != 0) {
        goto out;
    }

    printf("Starting bursts...\n");

    status = pthread_create(&rx_thread, NULL, loopback_burst_rx_task, &test);
    if (status != 0) {
        fprintf(stderr, "Failed to start RX thread: %s\n", strerror(status));
        goto out;
    } else {
        rx_started = true;
    }

    while (!rx_ready) {
        usleep(10000);
        pthread_mutex_lock(&test.lock);
        rx_ready = test.rx_ready;
        pthread_mutex_unlock(&test.lock);
    }

    status = pthread_create(&tx_thread, NULL, tx_task, &test);
    if (status != 0) {
        fprintf(stderr, "Failed to start TX thread: %s\n", strerror(status));
        goto out;
    } else {
        tx_started = true;
    }

out:
    if (tx_started) {
        pthread_join(tx_thread, NULL);
    }

    if (rx_started) {
        pthread_join(rx_thread, NULL);
    }

    free(test.bursts);

    bladerf_enable_module(dev, BLADERF_MODULE_RX, false);
    bladerf_enable_module(dev, BLADERF_MODULE_TX, false);
    bladerf_set_loopback(dev, BLADERF_LB_NONE);

    return status;
}
