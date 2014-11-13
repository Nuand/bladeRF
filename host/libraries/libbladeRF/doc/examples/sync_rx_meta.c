/*
 * Example of RX synchronous interface usage with metadata
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

/** [example_snippet] */
int sync_rx_meta_example(struct bladerf *dev, unsigned int samplerate)
{
    int status, ret;
    struct bladerf_metadata meta;
    unsigned int i;

    /* "User" buffer that we read samples into and do work on, and its
     * associated size, in units of samples. Recall that one sample = two
     * int16_t values. */
    int16_t *samples;
    const unsigned int samples_len = 4096;

    /* These items configure the underlying asynch stream used by the the sync
     * interface. The "buffer" here refers to those used internally by worker
     * threads, not the `samples` buffer above. */
    const unsigned int num_buffers = 16;
    const unsigned int buffer_size = 16384;
    const unsigned int num_transfers = 8;
    const unsigned int timeout_ms  = 5000;

    memset(&meta, 0, sizeof(meta));

    samples = malloc(samples_len *  2 * sizeof(int16_t));
    if (samples == NULL) {
        perror("malloc");
        return BLADERF_ERR_MEM;
    }

    /* Configure the device's RX module for use with the sync interface.
     * SC16 Q11 samples *with* metadata are used. */
    status = bladerf_sync_config(dev,
                                 BLADERF_MODULE_RX,
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

    /* We must always enable the RX module *after* calling
     * bladerf_sync_config(), and *before* attempting to RX samples via
     * bladerf_sync_rx(). */
    status = bladerf_enable_module(dev, BLADERF_MODULE_RX, true);
    if (status != 0) {
        fprintf(stderr, "Failed to enable RX module: %s\n",
                bladerf_strerror(status));
        goto out;
    }

    /* Retrieve the current timestamp */
    status = bladerf_get_timestamp(dev, BLADERF_MODULE_RX, &meta.timestamp);
    if (status != 0) {
        fprintf(stderr, "Failed to get current RX timestamp: %s\n",
                bladerf_strerror(status));
    } else {
        printf("Current RX timestamp: 0x%016"PRIx64"\n", meta.timestamp);
    }

    /* Schedule the first reception to be 2 seconds in the future */
    meta.timestamp += 2 * samplerate;

    /* Receive samples and do work on them */
    for (i = 0; i < 5 && status == 0; i++) {

        /* Perform a scheduled RX by having meta.timestamp set appropriately
         * and ensuring the BLADERF_META_FLAG_RX_NOW flag is cleared. */
        meta.flags &= ~BLADERF_META_FLAG_RX_NOW;

        printf("Calling bladerf_sync_rx() for read @ t=0x%016"PRIx64"...\n",
                meta.timestamp);
        fflush(stdout);

        status = bladerf_sync_rx(dev, samples, samples_len,
                                 &meta, 2 * timeout_ms);

        if (status != 0) {
            fprintf(stderr, "Scheduled RX failed: %s\n\n",
                    bladerf_strerror(status));
        } else if (meta.status & BLADERF_META_STATUS_OVERRUN) {
            fprintf(stderr, "Overrun detected in scheduled RX. "
                    "%u valid samples were read.\n\n", meta.actual_count);
        } else {
            printf("Got %u samples at t=0x%016"PRIx64"\n\n",
                   meta.actual_count, meta.timestamp);
        }

        /* Perform a read immediately, and have the bladerf_sync_rx function
         * provide the timestamp of the read samples */
        printf("Calling bladerf_sync_rx() for an immediate read...\n");

        meta.flags |= BLADERF_META_FLAG_RX_NOW;
        status = bladerf_sync_rx(dev, samples, samples_len,
                                 &meta, 2 * timeout_ms);

        if (status != 0) {
            fprintf(stderr, "Immediate RX failed: %s\n\n",
                    bladerf_strerror(status));
        } else if (meta.status & BLADERF_META_STATUS_OVERRUN) {
            fprintf(stderr, "Overrun detected in immediate RX. "
                    "%u valid samples were read.\n\n", meta.actual_count);
        } else {
            printf("Got %u samples at t=0x%016"PRIx64"\n\n",
                   meta.actual_count, meta.timestamp);
        }

        /* Schedule the next read 1.25 seconds into the future */
        meta.timestamp += samplerate + samplerate / 4;
    }


out:
    ret = status;

    /* Disable RX module, shutting down our underlying RX stream */
    status = bladerf_enable_module(dev, BLADERF_MODULE_RX, false);
    if (status != 0) {
        fprintf(stderr, "Failed to disable RX module: %s\n",
                bladerf_strerror(status));
    }

    /* Free up our resources */
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
        printf("Running...\n");
        status = sync_rx_meta_example(dev, EXAMPLE_SAMPLERATE);
        printf("Closing the device...\n");
        bladerf_close(dev);
    }

    return status;
}
