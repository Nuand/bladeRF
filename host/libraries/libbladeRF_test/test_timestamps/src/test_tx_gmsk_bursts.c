/*
 * This test sends some GMSK bursts. The output should be verified with a
 * spectrum analyze, preferably one able quantify phase and freq error.
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
#include <stdint.h>
#include <limits.h>
#include <inttypes.h>
#include <libbladeRF.h>
#include "test_timestamps.h"
#include "gmsk_burst.h"

struct settings {
    struct bladerf_rational_rate sample_rate;
    unsigned int frequency, bandwidth, timeout_ms;
    int txvga1, txvga2;
};

enum tx_mode {
    TX_MODE_SCHEDULED_BURST,
    TX_MODE_NOW_FLAG,
    TX_MODE_UPDATE_TIMESTAMP_FLAG,
};

static int backup_settings(struct bladerf *dev, struct app_params *p,
                           struct settings *s)
{
    int status;

    printf("\nBacking up device settings...\n");

    s->timeout_ms = p->timeout_ms;
    printf("  Stream timeout: %u ms\n", p->timeout_ms);

    status = bladerf_get_frequency(dev, BLADERF_MODULE_TX, &s->frequency);
    if (status != 0) {
        fprintf(stderr, "Failed to get frequency: %s\n",
                bladerf_strerror(status));
        return status;
    } else {
        printf("  Frequency: %u\n", s->frequency);
    }


    status = bladerf_get_rational_sample_rate(dev, BLADERF_MODULE_TX,
                                              &s->sample_rate);
    if (status != 0) {
        fprintf(stderr, "Failed to get sample rate: %s\n",
                bladerf_strerror(status));
        return status;
    } else {
        printf("  Samplerate %"PRIu64" %"PRIu64"/%"PRIu64"\n",
               s->sample_rate.integer, s->sample_rate.num, s->sample_rate.den);
    }

    status = bladerf_get_bandwidth(dev, BLADERF_MODULE_TX, &s->bandwidth);
    if (status != 0) {
        fprintf(stderr, "Failed to get bandwidth: %s\n",
                bladerf_strerror(status));
        return status;
    } else {
        printf("  Bandwidth: %u\n", s->bandwidth);
    }

    status = bladerf_get_txvga1(dev, &s->txvga1);
    if (status != 0) {
        fprintf(stderr, "Failed to get txvga1: %s\n", bladerf_strerror(status));
        return status;
    } else {
        printf("  TXVGA1: %d\n", s->txvga1);
    }

    status = bladerf_get_txvga2(dev, &s->txvga2);
    if (status != 0) {
        fprintf(stderr, "Failed to get txvga2: %s\n", bladerf_strerror(status));
        return status;
    } else {
        printf("  TXVGA2: %d\n", s->txvga2);
    }

    return status;
}

static int setup_device(struct bladerf *dev, struct app_params *p)
{
    int status;
    struct bladerf_rational_rate sample_rate = GMSK_SAMPLERATE_INITIALIZER;
    const unsigned int frequency = 1000000000;
    const unsigned int bandwidth = 1500000;
    const int txvga1 = -15;
    const int txvga2 = 0;

    printf("\nApplying device settings...\n");

    p->timeout_ms = 2500;
    printf("  Stream timeout: %u ms\n", p->timeout_ms);

    status = bladerf_set_frequency(dev, BLADERF_MODULE_TX, frequency);
    if (status != 0) {
        fprintf(stderr, "Failed to set frequency: %s\n",
                bladerf_strerror(status));
        return status;
    } else {
        printf("  Frequency: %u\n", frequency);
    }

    status = bladerf_set_rational_sample_rate(dev, BLADERF_MODULE_TX,
                                              &sample_rate, NULL);
    if (status != 0) {
        fprintf(stderr, "Failed to set sample rate: %s\n",
                bladerf_strerror(status));
        return status;
    } else {
        printf("  Samplerate %"PRIu64" %"PRIu64"/%"PRIu64"\n",
                sample_rate.integer, sample_rate.num, sample_rate.den);
    }

    status = bladerf_set_bandwidth(dev, BLADERF_MODULE_TX, bandwidth, NULL);
    if (status != 0) {
        fprintf(stderr, "Failed to set bandwidth: %s\n",
                bladerf_strerror(status));
        return status;
    } else {
        printf("  Bandwidth: %u\n", bandwidth);
    }



    status = bladerf_set_txvga1(dev, txvga1);
    if (status != 0) {
        fprintf(stderr, "Failed to set TXVGA1: %s\n", bladerf_strerror(status));
        return status;
    } else {
        printf("  TXVGA1: %d\n", txvga1);
    }

    status = bladerf_set_txvga2(dev, txvga2);
    if (status != 0) {
        fprintf(stderr, "Failed to set TXVGA2: %s\n", bladerf_strerror(status));
        return status;
    } else {
        printf("  TXVGA2: %d\n", txvga2);
    }

    return status;
}

static int restore_settings(struct bladerf *dev, struct app_params *p,
                            struct settings *s)
{
    int status, error = 0;

    printf("\nRestoring settings...\n");

    p->timeout_ms = s->timeout_ms;
    printf("  Stream timeout: %u\n", p->timeout_ms);

    status = bladerf_set_frequency(dev, BLADERF_MODULE_TX, s->frequency);
    if (status != 0) {
        fprintf(stderr, "Failed to set frequency: %s\n",
                bladerf_strerror(status));
        return status;
    } else {
        printf("  Frequency: %u\n", s->frequency);
    }

    status = bladerf_set_rational_sample_rate(dev, BLADERF_MODULE_TX,
                                              &s->sample_rate, NULL);
    if (status != 0) {
        error = first_error(status, error);
        fprintf(stderr, "Failed to set sample rate: %s\n",
                bladerf_strerror(status));
    } else {
        printf("  Samplerate %"PRIu64" %"PRIu64"/%"PRIu64"\n",
                s->sample_rate.integer, s->sample_rate.num, s->sample_rate.den);
    }

    status = bladerf_set_bandwidth(dev, BLADERF_MODULE_TX, s->bandwidth, NULL);
    if (status != 0) {
        error = first_error(status, error);
        fprintf(stderr, "Failed to set bandwidth: %s\n",
                bladerf_strerror(status));
    } else {
        printf("  Bandwidth: %u\n", s->bandwidth);
    }

    status = bladerf_set_txvga1(dev, s->txvga1);
    if (status != 0) {
        error = first_error(status, error);
        fprintf(stderr, "Failed to set txvga1: %s\n", bladerf_strerror(status));
    } else {
        printf("  TXVGA1: %d\n", s->txvga1);
    }

    status = bladerf_set_txvga2(dev, s->txvga2);
    if (status != 0) {
        error = first_error(status, error);
        fprintf(stderr, "Failed to set txvga2: %s\n", bladerf_strerror(status));
    } else {
        printf("  TXVGA2: %d\n", s->txvga2);
    }

    return error;
}

static int transmit_bursts(struct bladerf *dev, struct app_params *p,
                           enum tx_mode mode)
{
    int status, status_out;
    unsigned int i;
    struct bladerf_metadata meta;
    int16_t *samples;
    unsigned int to_tx;
    const unsigned int burst_len = (unsigned int) ARRAY_SIZE(gmsk_burst) / 2;
    const unsigned int buf_len = (burst_len / 1024) * 1024 + 1024;
    const unsigned int iterations = 10000;

    assert(burst_len < 5000);

    samples = calloc(5000, 2 * sizeof(int16_t));
    if (samples == NULL) {
        perror("calloc");
        return -1;
    }

    memcpy(samples, gmsk_burst, burst_len * 2 * sizeof(int16_t));

    printf("\nTransmitting scheduled bursts (%u iterations)...\n", iterations);

    memset(&meta, 0, sizeof(meta));

    meta.flags = BLADERF_META_FLAG_TX_BURST_START;

    if (mode != TX_MODE_UPDATE_TIMESTAMP_FLAG) {
        meta.flags |= BLADERF_META_FLAG_TX_BURST_END;
    }

    status = perform_sync_init(dev, BLADERF_MODULE_TX, buf_len, p);
    if (status != 0) {
        goto out;
    }

    if (mode == TX_MODE_NOW_FLAG) {
        meta.flags |= BLADERF_META_FLAG_TX_NOW;

        /* Use a value that would induce timeouts if the API were to actually
         * use this value in any way */
        meta.timestamp = 0xfedcba9896543210;

        to_tx = 5000;

    } else {
        status = bladerf_get_timestamp(dev, BLADERF_MODULE_TX, &meta.timestamp);
        if (status != 0) {
            fprintf(stderr, "Failed to get timestamp: %s\n",
                    bladerf_strerror(status));
            goto out;
        } else {
            printf("  Initial timestamp: 0x%016"PRIx64"\n", meta.timestamp);
        }

        /* Start 1M samples in */
        meta.timestamp += 1000000;

        to_tx = burst_len;
    }


    for (i = status = 0; i < iterations && status == 0; i++) {

        if (mode == TX_MODE_UPDATE_TIMESTAMP_FLAG) {
            if (i != 0) {
                /* Use the "Update timestamp" flag on everything after the
                 * first iteration */
                meta.flags = BLADERF_META_FLAG_TX_UPDATE_TIMESTAMP;
            }

            if (i == (iterations - 1)) {
                /* End the burst on the last iteration */
                meta.flags |= BLADERF_META_FLAG_TX_BURST_END;
            }

        }

        status = bladerf_sync_tx(dev, samples, to_tx, &meta, p->timeout_ms);
        if (status != 0) {
            fprintf(stderr, "TX failed at iteration %u: %s\n", i,
                    bladerf_strerror(status));
        }

        if (mode != TX_MODE_NOW_FLAG) {
            /* Distance between beginning of each burst is 5k samples */
            meta.timestamp += 5000 - burst_len;
        }
    }

    /* 2 seconds should be sufficient to ensure our samples have been
     * transmitted */
    usleep(2000000);

out:
    status_out = bladerf_enable_module(dev, BLADERF_MODULE_TX, false);
    status = first_error(status, status_out);
    free(samples);
    return status;
}

int test_fn_tx_gmsk_bursts(struct bladerf *dev, struct app_params *p)
{
    int status, status_restore;
    struct settings dev_settings;
    enum tx_mode mode;
    int c = 0;

    printf("Enter one of the following:\n"
           "  's' to use scheduled bursts\n"
           "  'n' to use the TX_NOW flag\n"
           "  'u' to use the UPDATE_TIMESTAMP flag\n\n");
    printf("> ");

    c = getchar();
    switch (c) {
        case 's':
        case 'S':
            mode = TX_MODE_SCHEDULED_BURST;
            break;

        case 'n':
        case 'N':
            mode = TX_MODE_NOW_FLAG;
            break;

        case 'u':
        case 'U':
            mode = TX_MODE_UPDATE_TIMESTAMP_FLAG;
            break;

        default:
            fprintf(stderr, "Invalid option: %c\n", (char) c);
            return -1;
    }

    status = backup_settings(dev, p, &dev_settings);
    if (status != 0) {
        return status;
    }

    status = setup_device(dev, p);
    if (status == 0) {
        status = transmit_bursts(dev, p, mode);
    }

    status_restore = restore_settings(dev, p, &dev_settings);
    if (status != 0) {
        status = first_error(status, status_restore);
    }

    return status;
}
