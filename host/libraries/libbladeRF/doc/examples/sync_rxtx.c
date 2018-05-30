/*
 * Example usage of synchronous interface for RX & TX, without metadata
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

#include <libbladeRF.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "example_common.h"

/* Just retransmit the received samples */
bool do_work(int16_t *rx,
             unsigned int rx_len,
             bool *have_tx_data,
             int16_t *tx,
             unsigned int tx_len)
{
    static unsigned int call_no = 0;

    assert(tx_len == rx_len);
    memcpy(tx, rx, rx_len * 2 * sizeof(int16_t));
    *have_tx_data = true;

    return (++call_no >= 5000);
}

/** [example_snippet] */
/** [sync_init] */
static int init_sync(struct bladerf *dev)
{
    int status;

    /* These items configure the underlying asynch stream used by the sync
     * interface. The "buffer" here refers to those used internally by worker
     * threads, not the user's sample buffers.
     *
     * It is important to remember that TX buffers will not be submitted to
     * the hardware until `buffer_size` samples are provided via the
     * bladerf_sync_tx call.  Similarly, samples will not be available to
     * RX via bladerf_sync_rx() until a block of `buffer_size` samples has been
     * received.
     */
    const unsigned int num_buffers   = 16;
    const unsigned int buffer_size   = 8192; /* Must be a multiple of 1024 */
    const unsigned int num_transfers = 8;
    const unsigned int timeout_ms    = 3500;

    /* Configure both the device's x1 RX and TX channels for use with the
     * synchronous
     * interface. SC16 Q11 samples *without* metadata are used. */

    status = bladerf_sync_config(dev, BLADERF_RX_X1, BLADERF_FORMAT_SC16_Q11,
                                 num_buffers, buffer_size, num_transfers,
                                 timeout_ms);
    if (status != 0) {
        fprintf(stderr, "Failed to configure RX sync interface: %s\n",
                bladerf_strerror(status));
        return status;
    }

    status = bladerf_sync_config(dev, BLADERF_TX_X1, BLADERF_FORMAT_SC16_Q11,
                                 num_buffers, buffer_size, num_transfers,
                                 timeout_ms);
    if (status != 0) {
        fprintf(stderr, "Failed to configure TX sync interface: %s\n",
                bladerf_strerror(status));
    }

    return status;
}

/** [sync_init] */

int sync_rx_example(struct bladerf *dev)
{
    int status, ret;
    bool done         = false;
    bool have_tx_data = false;

    /** [user_buffers] */

    /* "User" samples buffers and their associated sizes, in units of samples.
     * Recall that one sample = two int16_t values. */
    int16_t *rx_samples            = NULL;
    int16_t *tx_samples            = NULL;
    const unsigned int samples_len = 10000; /* May be any (reasonable) size */

    /* Allocate a buffer to store received samples in */
    rx_samples = malloc(samples_len * 2 * 1 * sizeof(int16_t));
    if (rx_samples == NULL) {
        perror("malloc");
        return BLADERF_ERR_MEM;
    }

    /* Allocate a buffer to prepare transmit data in */
    tx_samples = malloc(samples_len * 2 * 1 * sizeof(int16_t));
    if (tx_samples == NULL) {
        perror("malloc");
        free(rx_samples);
        return BLADERF_ERR_MEM;
    }

    /** [user_buffers] */

    /* Initialize synch interface on RX and TX */
    status = init_sync(dev);
    if (status != 0) {
        goto out;
    }

    /** [enable_modules] */

    status = bladerf_enable_module(dev, BLADERF_RX, true);
    if (status != 0) {
        fprintf(stderr, "Failed to enable RX: %s\n", bladerf_strerror(status));
        goto out;
    }

    status = bladerf_enable_module(dev, BLADERF_TX, true);
    if (status != 0) {
        fprintf(stderr, "Failed to enable TX: %s\n", bladerf_strerror(status));
        goto out;
    }

    /** [enable_modules] */
    /** [rxtx_loop] */

    while (status == 0 && !done) {
        /* Receive samples */
        status = bladerf_sync_rx(dev, rx_samples, samples_len, NULL, 5000);
        if (status == 0) {
            /* Process these samples, and potentially produce a response
             * to transmit */
            done = do_work(rx_samples, samples_len, &have_tx_data, tx_samples,
                           samples_len);

            if (!done && have_tx_data) {
                /* Transmit a response */
                status =
                    bladerf_sync_tx(dev, tx_samples, samples_len, NULL, 5000);

                if (status != 0) {
                    fprintf(stderr, "Failed to TX samples: %s\n",
                            bladerf_strerror(status));
                }
            }
        } else {
            fprintf(stderr, "Failed to RX samples: %s\n",
                    bladerf_strerror(status));
        }
    }

    if (status == 0) {
        /* Wait a few seconds for any remaining TX samples to finish
         * reaching the RF front-end */
        usleep(2000000);
    }

/** [rxtx_loop] */

out:
    ret = status;

    /** [disable_modules] */

    /* Disable RX, shutting down our underlying RX stream */
    status = bladerf_enable_module(dev, BLADERF_RX, false);
    if (status != 0) {
        fprintf(stderr, "Failed to disable RX: %s\n", bladerf_strerror(status));
    }

    /* Disable TX, shutting down our underlying TX stream */
    status = bladerf_enable_module(dev, BLADERF_TX, false);
    if (status != 0) {
        fprintf(stderr, "Failed to disable TX: %s\n", bladerf_strerror(status));
    }


    /* Free up our resources */
    free(rx_samples);
    free(tx_samples);

    /** [disable_modules] */

    return ret;
}
/** [example_snippet] */

static void usage(const char *argv0)
{
    printf("Usage: %s [device specifier]\n\n", argv0);
}


int main(int argc, char *argv[])
{
    int status          = -1;
    struct bladerf *dev = NULL;
    const char *devstr  = NULL;

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
        status = sync_rx_example(dev);
        printf("Closing the device...\n");
        bladerf_close(dev);
    }

    return status;
}
