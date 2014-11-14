/*
 * Example of TX synchronous interface usage with metadata
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
#include <inttypes.h>
#include <libbladeRF.h>
#include "example_common.h"

static void get_samples(int16_t *samples, unsigned int num_samples)
{
    static bool data_populated = false;
    unsigned int i;

    if (data_populated) {
        return;
    }

    for (i = 0; i < 2 * num_samples; i += 2) {
        samples[i]      = 2000;
        samples[i + 1]  = 2000;
    }
}

/** [example_snippet] */
int sync_tx_meta_example(struct bladerf *dev, bool use_tx_now,
                         unsigned int samplerate)
{
    int status, ret;
    struct bladerf_metadata meta;
    unsigned int i, j;
    int16_t zeros[] = { 0, 0, 0, 0 };

    /* "User" buffer that we read samples into and do work on, and its
     * associated size, in units of samples. Recall that one sample = two
     * int16_t values. */
    int16_t *samples;
    const unsigned int samples_len = 60;

    int16_t *to_tx;
    unsigned int num_to_tx;

    /* These items configure the underlying asynch stream used by the the sync
     * interface. The "buffer" here refers to those used internally by worker
     * threads, not the `samples` buffer above. */
    const unsigned int num_buffers = 16;
    const unsigned int buffer_size = 1024;
    const unsigned int num_transfers = 8;
    const unsigned int timeout_ms  = 10000;

    memset(&meta, 0, sizeof(meta));

    samples = calloc(samples_len,  2 * sizeof(int16_t));
    if (samples == NULL) {
        perror("malloc");
        return BLADERF_ERR_MEM;
    }

    /* Configure the device's TX module for use with the sync interface.
     * SC16 Q11 samples *with* metadata are used. */
    status = bladerf_sync_config(dev,
                                 BLADERF_MODULE_TX,
                                 BLADERF_FORMAT_SC16_Q11_META,
                                 num_buffers,
                                 buffer_size,
                                 num_transfers,
                                 timeout_ms);

    if (status != 0) {
        fprintf(stderr, "Failed to configure RX sync interface: %s\n",
                bladerf_strerror(status));
        goto out;
    }

    /* We must always enable the TX module *after* calling
     * bladerf_sync_config(), and *before* attempting to TX samples via
     * bladerf_sync_tx(). */
    status = bladerf_enable_module(dev, BLADERF_MODULE_TX, true);
    if (status != 0) {
        fprintf(stderr, "Failed to enable RX module: %s\n",
                bladerf_strerror(status));
        goto out;
    }

    if (use_tx_now) {
        usleep(250);
    } else {
        /* Retrieve the current timestamp */
        status = bladerf_get_timestamp(dev, BLADERF_MODULE_TX, &meta.timestamp);
        if (status != 0) {
            fprintf(stderr, "Failed to get current TX timestamp: %s\n",
                    bladerf_strerror(status));
            goto out;
        } else {
            printf("\nCurrent TX timestamp: 0x%016"PRIx64"\n", meta.timestamp);
        }

        /* Schedule first burst 250 ms into the future */
        meta.timestamp += samplerate / 4;
    }

    for (i = 0; i < 5; i++) {

        /* The burst is broken up into multiple calls here simply to show that
         * it is possible. The meta.timestamp value only needs to be provided
         * to the call that starts the burst. The meta.timestamp field is not
         * read when using the TX_NOW flag.
         *
         * Note that with a sufficiently large buffer, one could send the entire
         * burst in a single function call, setting meta.flags to:
         *  BLADERF_META_FLAG_TX_BURST_START | BLADERF_META_FLAG_TX_BURST_END
         */
        for (j = 0; j < 4; j++) {
            switch (j) {
                case 0:
                    get_samples(samples, samples_len);
                    meta.flags = BLADERF_META_FLAG_TX_BURST_START;

                    if (use_tx_now) {
                        meta.flags |= BLADERF_META_FLAG_TX_NOW;
                    }

                    to_tx = samples;
                    num_to_tx = samples_len;
                    break;

                case 1:
                case 2:
                    get_samples(samples, samples_len);
                    meta.flags = 0;
                    to_tx = samples;
                    num_to_tx = samples_len;
                    break;

                case 3:
                    /* Ensure the burst ends with 0-valued samples, as required
                     * by the API */
                    meta.flags = BLADERF_META_FLAG_TX_BURST_END;
                    to_tx = zeros;
                    num_to_tx = sizeof(zeros) / sizeof(zeros[0]) / 2;
                    break;
            }

            status = bladerf_sync_tx(dev, to_tx, num_to_tx,
                                     &meta, 2 * timeout_ms);

            if (status != 0) {
                fprintf(stderr, "Failed to TX sample chunk %u/4: %s\n", j + 1,
                        bladerf_strerror(status));
                goto out;
            }
        }

        /* Schedule next burst to be 1 ms after the end of this burst */
        if (use_tx_now) {
            usleep(1000);
        } else {
            meta.timestamp += (3 * samples_len) + 2 + samplerate / 1000;

        }
    }

    /* Wait a couple seconds to ensure samples have been transmitted */
    printf("Waiting for bursts to finish...\n");
    usleep(2000000);

out:
    ret = status;


    /* Disable TX module, shutting down our underlying TX stream */
    status = bladerf_enable_module(dev, BLADERF_MODULE_TX, false);
    if (status != 0) {
        fprintf(stderr, "Failed to disable TX module: %s\n",
                bladerf_strerror(status));
    }

    free(samples);
    return ret;

}
/** [example_snippet] */

static void usage(const char *argv0) {
    printf("Usage: %s [device specifier]\n\n", argv0);
}


int main(int argc, char *argv[])
{
    int status = -1;
    struct bladerf *dev = NULL;
    const char *devstr = NULL;
    int option;
    bool use_tx_now = false;

    if (argc == 2) {
        if (!strcasecmp("-h", argv[1]) || !strcasecmp("--help", argv[1])) {
            usage(argv[0]);
            return 0;
        } else {
            devstr = argv[1];
        }
    } else if (argc > 1) {
        usage(argv[0]);
        return -1;
    }

    dev = example_init(devstr);
    if (dev) {

        do {
            printf("Enter (s) for (s)cheduled transmissions, "
                    "or (n) to use the (n)ow flag.\n");
            printf("> ");
            option = getchar();

            if (option == 'n') {
                use_tx_now = true;
            }

        } while (option != 's' && option != 'n');

        printf("Running...\n");
        status = sync_tx_meta_example(dev, use_tx_now, EXAMPLE_SAMPLERATE);
        printf("\nClosing the device...\n");
        bladerf_close(dev);
    }

    return status;
}
