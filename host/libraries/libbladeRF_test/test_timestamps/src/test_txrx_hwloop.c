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
#include <getopt.h>
#include <pthread.h>
#include "conversions.h"
#include "test_timestamps.h"
#include "test_txrx_hwloop.h"
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

static void init_app_params(struct app_params *p, struct test_case *tc) {
    memset(tc, 0, sizeof(tc[0]));
    tc->just_tx = true;
    tc->compare = false;
    tc->frequency = 900e6;
    tc->dev_tx_str = malloc(100);
    tc->dev_rx_str = malloc(100);
    strcpy(tc->dev_tx_str, "*:serial=52f4b4c4e1164ce3a7b89d3e47c8c0e8");
    strcpy(tc->dev_rx_str, "*:serial=96707f3c6ffcba3fcea7f8ae85c04178");
    tc->iterations = 10;
    tc->fill = 50; //percent
    tc->init_ts_delay = 200000;
    tc->gain = 30;

    memset(p, 0, sizeof(p[0]));
    p->samplerate = 1e6;
    p->prng_seed = 1;
    p->num_buffers = 16;
    p->num_xfers = 8;
    p->buf_size = 2048;
    p->timeout_ms = 10000;

    // Interdependencies
    tc->period = p->samplerate;
    tc->burst_len = tc->period/2;
}

int main(int argc, char *argv[]) {
    struct app_params p;
    struct test_case test;
    struct bladerf *dev_tx = NULL;
    struct bladerf *dev_rx = NULL;
    int status, status_out, status_wait;
    unsigned int samples_left;
    size_t i;
    struct bladerf_metadata meta;
    int16_t *samples, *buf;
    struct timeval start, end;
    double time_passed;

    int opt_ind = 0;
    bool ok;
    char* optstr;
    int c;
    bladerf_log_level log_level;

    /** Threading */
    pthread_t thread_rx;
    pthread_t thread_loopcompare;
    thread_args args_loopbackcompare;
    thread_args args_rx;

    init_app_params(&p, &test);
    memset(&meta, 0, sizeof(meta));

    optstr = getopt_str(long_options);
    while ((c = getopt_long(argc, argv, optstr, long_options, &opt_ind)) >= 0) {
        switch (c) {
            case 't':
                strcpy(test.dev_tx_str, optarg);
                break;

            case 'r':
                strcpy(test.dev_rx_str, optarg);
                break;

            case 's':
                p.samplerate = str2uint(optarg, 0, UINT32_MAX, &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid sample rate: %s\n", optarg);
                    return -1;
                }
                break;

            case 'b':
                test.burst_len = str2uint(optarg, 0, UINT32_MAX, &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid burst length: %s\n", optarg);
                    return -1;
                }
                break;

            case 'p':
                test.period = str2uint(optarg, 0, UINT32_MAX, &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid period: %s\n", optarg);
                    return -1;
                }
                break;

            case 'f':
                test.fill = str2uint(optarg, 0, 100, &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid fill: %s\n", optarg);
                    return -1;
                }
                break;

            case 'l':
                test.just_tx = false;
                break;

            case 'c':
                test.compare = true;
                break;

            case 'i':
                test.iterations = str2uint(optarg, 1, UINT32_MAX, &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid number of iterations %s\n", optarg);
                    return -1;
                }
                break;

            case 'v':
                log_level = str2loglevel(optarg, &ok);
                if (!ok) {
                    fprintf(stderr, "[ERROR] Invalid log level provided: %s\n", optarg);
                    return -1;
                }

                bladerf_log_set_verbosity(log_level);
                break;

            case 'h':
                usage();
                return 1;

            default:
                return -1;
                break;
        }
    }

    /** Parameter conflict management */
    if (test.burst_len > test.period) {
        fprintf(stderr, "[ERROR] Burst length must be less than period\n");
        return -1;
    }

    printf("Fill:   %u%%\n", test.fill);
    printf("Burst:  %3.3fk samples || %2.3fs\n", test.burst_len/1e3, (float)test.burst_len/p.samplerate);
    printf("Period: %3.3fk samples || %2.3fs\n", test.period/1e3, (float)test.period/p.samplerate);
    printf("Gap:    %3.3fk samples || %2.3fms\n", (test.period - test.burst_len)/1e3, (test.period - test.burst_len)*1e3/p.samplerate);
    printf("Sample Rate: %3.3f MSPS\n", p.samplerate/1e6);
    printf("\n");

    samples = calloc(test.burst_len, 2 * sizeof(int16_t));
    if (samples == NULL) {
        perror("malloc");
        return BLADERF_ERR_MEM;
    }

    for (i = 0; i < (2 * test.burst_len * test.fill/100); i += 2) {
        samples[i] = samples[i + 1] = MAGNITUDE;
    }

    status = init_devices(&dev_tx, &dev_rx, &p, &test);
    if (status < 0) {
        fprintf(stderr, "Error initializing devices: %s\n", bladerf_strerror(status));
        goto out;
    }

    status = bladerf_get_timestamp(dev_tx, BLADERF_MODULE_TX, &meta.timestamp);
    if (status != 0) {
        fprintf(stderr, "Failed to get timestamp: %s\n",
                bladerf_strerror(status));
        goto out;
    } else {
        printf("\nInitial timestamp: 0x%016"PRIx64"\n", meta.timestamp);
    }

    /** Timestamp to start pulse */
    meta.timestamp += test.init_ts_delay;

    gettimeofday(&start, NULL);

    if (!test.just_tx) {
        args_rx.is_rx_device = true;
        args_rx.dev = dev_rx;
        args_rx.tc  = &test;
        pthread_create(&thread_rx, NULL, rx_task, &args_rx);
    }

    if (test.compare == true) {
        args_loopbackcompare.is_rx_device = false;
        args_loopbackcompare.dev = dev_tx;
        args_loopbackcompare.tc  = &test;
        pthread_create(&thread_loopcompare, NULL, rx_task, &args_loopbackcompare);
    }

    for (i = 0; i < test.iterations && status == 0; i++) {
        meta.flags = BLADERF_META_FLAG_TX_BURST_START;
        samples_left = test.burst_len;
        buf = samples;


        printf("Sending burst @ %"PRIu64"\n", meta.timestamp);

        while (samples_left && status == 0) {
            unsigned int to_send = uint_min(p.buf_size, samples_left);

            if (to_send == samples_left) {
                meta.flags |= BLADERF_META_FLAG_TX_BURST_END;
            } else {
                meta.flags &= ~BLADERF_META_FLAG_TX_BURST_END;
            }

            status = bladerf_sync_tx(dev_tx, buf, to_send, &meta, 10000); //p->timeout_ms);
            if (status != 0) {
                fprintf(stderr, "TX failed @ iteration (%u) %s\n",
                        (unsigned int )i, bladerf_strerror(status));
            }

            meta.flags &= ~BLADERF_META_FLAG_TX_BURST_START;
            samples_left -= to_send;
            buf += 2 * to_send;
        }

        meta.timestamp += test.period;
    }

    printf("Waiting for samples to finish...\n");
    fflush(stdout);

    /* Wait for samples to be transmitted before shutting down the TX module */
    status_wait = wait_for_timestamp(dev_tx, BLADERF_MODULE_TX, meta.timestamp, 3000);
    if (status_wait != 0) {
        status = first_error(status, status_wait);
        fprintf(stderr, "Failed to wait for TX to finish: %s\n",
                bladerf_strerror(status_wait));
    }
    gettimeofday(&end, NULL);
    time_passed = (end.tv_sec - start.tv_sec) + (end.tv_usec - end.tv_usec) / 1000000.0;
    printf("TX finished in %.4f seconds\n", time_passed);
out:
    if (!test.just_tx) {
        pthread_join(thread_rx, NULL);
        bladerf_close(dev_rx);
    }

    if (test.compare)
        pthread_join(thread_loopcompare, NULL);

    status_out = bladerf_enable_module(dev_tx, BLADERF_MODULE_TX, false);
    if (status_out != 0) {
        fprintf(stderr, "Failed to disable TX module: %s\n",
                bladerf_strerror(status));
    } else {
        printf("Done waiting.\n");
        fflush(stdout);
    }

    status = first_error(status, status_out);

    bladerf_close(dev_tx);
    free(test.dev_tx_str);
    free(test.dev_rx_str);
    free(samples);
    return status;
}
