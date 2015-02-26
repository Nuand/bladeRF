/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014-2015 Nuand LLC
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

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include "loopback.h"
#include "test_timestamps.h"

/* You'll want to reduce the number of bursts if debugging with this. This has
 * potential to cause RX overruns. */
#define LOOPBACK_RX_TO_FILE 0

int setup_device_loopback(struct loopback_burst_test *t)
{
    int status;
    struct bladerf *dev = t->dev;

    status = bladerf_set_loopback(dev, BLADERF_LB_BB_TXVGA1_RXVGA2);
    if (status != 0) {
        fprintf(stderr, "Failed to set loopback mode: %s\n",
                bladerf_strerror(status));
        return status;
    }

    status = bladerf_set_lna_gain(dev, BLADERF_LNA_GAIN_MAX);
    if (status != 0) {
        fprintf(stderr, "Failed to set LNA gain value: %s\n",
                bladerf_strerror(status));
        return status;
    }

    status = bladerf_set_rxvga1(dev, 30);
    if (status != 0) {
        fprintf(stderr, "Failed to set RXVGA1 value: %s\n",
                bladerf_strerror(status));
        return status;
    }

    status = bladerf_set_rxvga2(dev, 10);
    if (status != 0) {
        fprintf(stderr, "Failed to set RXVGA2 value: %s\n",
                bladerf_strerror(status));
        return status;
    }

    status = bladerf_set_txvga1(dev, -10);
    if (status != 0) {
        fprintf(stderr, "Failed to set TXVGA1 value: %s\n",
                bladerf_strerror(status));
        return status;
    }

    status = bladerf_set_txvga2(dev, BLADERF_TXVGA2_GAIN_MIN);
    if (status != 0) {
        fprintf(stderr, "Failed to set TXVGA2 value: %s\n",
                bladerf_strerror(status));
        return status;
    }


    status = bladerf_sync_config(t->dev, BLADERF_MODULE_RX,
                                 BLADERF_FORMAT_SC16_Q11_META,
                                 t->params->num_buffers,
                                 t->params->buf_size,
                                 t->params->num_xfers,
                                 t->params->timeout_ms);
    if (status != 0) {
        fprintf(stderr, "Failed to configure RX stream: %s\n",
                bladerf_strerror(status));
        return status;
    }

    status = bladerf_enable_module(t->dev, BLADERF_MODULE_RX, true);
    if (status != 0) {
        fprintf(stderr, "Failed to enable RX module: %s\n",
                bladerf_strerror(status));
        return status;

    }

    status = bladerf_sync_config(t->dev, BLADERF_MODULE_TX,
                                 BLADERF_FORMAT_SC16_Q11_META,
                                 t->params->num_buffers,
                                 t->params->buf_size,
                                 t->params->num_xfers,
                                 t->params->timeout_ms);
    if (status != 0) {
        fprintf(stderr, "Failed to configure TX stream: %s\n",
                bladerf_strerror(status));
        return status;
    }

    status = bladerf_enable_module(t->dev, BLADERF_MODULE_TX, true);
    if (status != 0) {
        fprintf(stderr, "Failed to enable RX module: %s\n",
                bladerf_strerror(status));
        return status;

    }

    return status;
}

int16_t * alloc_loopback_samples(size_t n_samples)
{
    int16_t *samples;
    size_t i;

    samples = (int16_t*) malloc(2 * sizeof(samples[0]) * n_samples);
    if (samples == NULL) {
        perror("malloc");
        return NULL;
    }

    for (i = 0; i < (2 * n_samples); i += 2) {
        samples[i] = samples[i + 1] = LOOPBACK_TX_MAGNITUDE;
    }


    return samples;
}

typedef enum {
    GET_SAMPLES,
    FLUSH_INITIAL_SAMPLES,
    WAIT_FOR_BURST_START,
    WAIT_FOR_BURST_END,
} loopback_burst_rx_state;

void *loopback_burst_rx_task (void *args)
{
    struct loopback_burst_test *t = (struct loopback_burst_test *) args;
    int16_t *samples;
    int status;
    unsigned int burst_num;
    struct bladerf_metadata meta;
    unsigned int idx;
    loopback_burst_rx_state curr_state, next_state;
    uint64_t burst_start, burst_end, burst_end_prev;
    bool stop;
    unsigned int transient_delay = 0;

#if LOOPBACK_RX_TO_FILE
    FILE *debug = fopen("debug.bin", "wb");
    if (!debug) {
        perror("fopen");
    }
#endif

    samples = (int16_t*) malloc(2 * sizeof(samples[0]) * t->params->buf_size);
    if (samples == NULL) {
        perror("malloc");
        return NULL;
    }

    idx = 0;
    status = 0;
    burst_num = 0;

    memset(&meta, 0, sizeof(meta));
    meta.flags |= BLADERF_META_FLAG_RX_NOW;

    curr_state = GET_SAMPLES;
    next_state = FLUSH_INITIAL_SAMPLES;
    burst_start = burst_end = burst_end_prev = 0;
    stop = false;

    while (status == 0 && burst_num < t->num_bursts && !stop) {
        switch (curr_state) {
            case GET_SAMPLES:
                status = bladerf_sync_rx(t->dev, samples, t->params->buf_size,
                                         &meta, t->params->timeout_ms);

                if (status != 0) {
                    fprintf(stderr, "RX failed in burst %-4u: %s\n",
                            burst_num, bladerf_strerror(status));
                } else if (meta.status & BLADERF_META_STATUS_OVERRUN) {
                    fprintf(stderr, "Error: RX overrun detected.\n");
                    pthread_mutex_lock(&t->lock);
                    t->stop = true;
                    t->rx_ready = true;
                    pthread_mutex_unlock(&t->lock);
                } else {
                    /*
                    printf("Read %-8u samples @ 0x%08"PRIx64" (%-8"PRIu64")\n",
                           t->params->buf_size, meta.timestamp, meta.timestamp);
                    */

#if LOOPBACK_RX_TO_FILE
                    fwrite(samples, 2 * sizeof(samples[0]), t->params->buf_size, debug);
#endif
                }

                idx = 0;
                curr_state = next_state;
                break;

            case FLUSH_INITIAL_SAMPLES:
            {
                bool had_transient_spike = false;
                curr_state = GET_SAMPLES;
                next_state = FLUSH_INITIAL_SAMPLES;

                for (; idx < (2 * t->params->buf_size); idx += 2) {
                    const uint32_t sig_pow =
                        samples[idx] * samples[idx] +
                        samples[idx + 1] * samples[idx + 1];


                    /* Keep flushing samples if we encounter any transient "ON"
                     * samples prior to the TX task being started. */
                    if (sig_pow >= LOOPBACK_RX_POWER_THRESH) {
                        had_transient_spike = true;
                        fprintf(stderr, "Flushed an initial buffer due to a "
                                "transient spike.\n");
                        break;
                    }
                }

                if (had_transient_spike) {
                    /* Reset transient delay counter */
                    transient_delay = 0;
                } else {
                    transient_delay++;

                    if (transient_delay == 10) {
                        /* After 10 buffers of no transients, we've most likely
                         * rid ourselves of any junk in the RX FIFOs and are
                         * ready to start the test */
                        next_state = WAIT_FOR_BURST_START;
                        pthread_mutex_lock(&t->lock);
                        t->rx_ready = true;
                        pthread_mutex_unlock(&t->lock);
                    }
                }
                break;
            }

            case WAIT_FOR_BURST_START:
                for (; idx < (2 * t->params->buf_size); idx += 2) {
                    const uint32_t sig_pow =
                        samples[idx] * samples[idx] +
                        samples[idx + 1] * samples[idx + 1];

                    if (sig_pow >= LOOPBACK_RX_POWER_THRESH) {
                        burst_start = meta.timestamp + (idx / 2);
                        burst_end_prev = burst_end;
                        burst_end = 0;

                        curr_state = WAIT_FOR_BURST_END;
                        assert(burst_start > burst_end_prev);

                        if (burst_num != 0) {
                            const uint64_t gap = burst_start - burst_end_prev;
                            uint64_t delta;

                            if (gap > t->bursts[burst_num].gap) {
                                delta = gap - t->bursts[burst_num].gap;
                            } else {
                                delta = t->bursts[burst_num].gap - gap;
                            }

                            if (delta > 1) {
                                status = BLADERF_ERR_UNEXPECTED;
                                fprintf(stderr, "Burst #%-4u Failed. "
                                        " Gap varied by %"PRIu64 " samples."
                                        " Expected=%-8"PRIu64
                                        " rx'd=%-8"PRIu64"\n",
                                        burst_num + 1, delta,
                                        t->bursts[burst_num].gap, gap);
                            }
                        }
                        break;
                    }
                }

                /* Need to fetch more samples */
                if (idx >= (2 * t->params->buf_size)) {
                    next_state = curr_state;
                    curr_state = GET_SAMPLES;
                }

                break;

            case WAIT_FOR_BURST_END:
                for (; idx < (2 * t->params->buf_size); idx += 2) {
                    const uint32_t sig_pow =
                        samples[idx] * samples[idx] +
                        samples[idx + 1] * samples[idx + 1];

                    if (sig_pow < LOOPBACK_RX_POWER_THRESH) {
                        uint64_t duration, delta;

                        burst_end = meta.timestamp + (idx / 2);
                        assert(burst_end > burst_start);
                        duration = burst_end - burst_start;

                        if (duration > t->bursts[burst_num].duration) {
                            delta  = duration - t->bursts[burst_num].duration;
                        } else {
                            delta  = t->bursts[burst_num].duration - duration;
                        }

                        if (delta > 1) {
                            status = BLADERF_ERR_UNEXPECTED;
                            fprintf(stderr, "Burst #%-4u Failed. "
                                    "Duration varied by %"PRIu64" samples. "
                                    "Expected=%-8"PRIu64"rx'd=%-8"PRIu64"\n",
                                    burst_num + 1, delta,
                                    t->bursts[burst_num].duration, duration);

                        } else {
                            const uint64_t gap =
                                (burst_num == 0) ? 0 : t->bursts[burst_num].gap;

                            printf("Burst #%-4u Passed. gap=%-8"PRIu64
                                   "duration=%-8"PRIu64"\n",
                                   burst_num + 1, gap,
                                   t->bursts[burst_num].duration);

                            curr_state = WAIT_FOR_BURST_START;
                            burst_num++;
                        }

                        break;
                    }
                }

                /* Need to fetch more samples */
                if (idx >= (2 * t->params->buf_size)) {
                    next_state = curr_state;
                    curr_state = GET_SAMPLES;
                }

                break;
        }

        pthread_mutex_lock(&t->lock);
        stop = t->stop;
        pthread_mutex_unlock(&t->lock);
    }

    if (status < 0) {
        fprintf(stderr, "RX: Shutting down due to error.\n");
    }

    free(samples);

#if LOOPBACK_RX_TO_FILE
    fclose(debug);
#endif

    /* Ensure the TX side is signalled to stop, if it isn't already */
    pthread_mutex_lock(&t->lock);
    t->stop = true;
    pthread_mutex_unlock(&t->lock);

    return NULL;
}
