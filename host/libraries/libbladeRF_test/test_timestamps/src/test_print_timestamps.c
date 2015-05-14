/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2015 Nuand LLC
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

/* This test just prints RX and TX timestamps, and is intended to simply
 * verify that there aren't blatantly obvious FPGA issues such as inadequate
 * register widths. */

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <libbladeRF.h>
#include "test_timestamps.h"

int test_fn_print(struct bladerf *dev, struct app_params *p)
{
    int status;
    uint64_t ts_rx, ts_tx, diff;
    bool neg;
    unsigned int i;
    const unsigned int iterations = 60 * 60 * 2; /* Run for ~2 hours */

    status = bladerf_sync_config(dev, BLADERF_MODULE_RX,
                                 BLADERF_FORMAT_SC16_Q11_META, p->num_buffers,
                                 p->buf_size, p->num_xfers, p->timeout_ms);
    if (status != 0) {
        fprintf(stderr, "Failed to configure RX module: %s\n",
                bladerf_strerror(status));
        goto out;
    }

    /* Setup the metadata format to enable the timestamp bit.
     *
     * There's no need to actually call the rx/tx functions and start the
     * underlying worker threads. */
    status = bladerf_sync_config(dev, BLADERF_MODULE_TX,
                                 BLADERF_FORMAT_SC16_Q11_META, p->num_buffers,
                                 p->buf_size, p->num_xfers, p->timeout_ms);
    if (status != 0) {
        fprintf(stderr, "Failed to configure TX module: %s\n",
                bladerf_strerror(status));
        goto out;
    }

    for (i = 0; i < iterations; i++) {
        status = bladerf_get_timestamp(dev, BLADERF_MODULE_TX, &ts_tx);
        if (status != 0) {
            fprintf(stderr, "Failed to get TX timestamp: %s\n",
                    bladerf_strerror(status));
            goto out;
        }

        status = bladerf_get_timestamp(dev, BLADERF_MODULE_RX, &ts_rx);
        if (status != 0) {
            fprintf(stderr, "Failed to get TX timestamp: %s\n",
                    bladerf_strerror(status));
            goto out;
        }

        if (ts_rx > ts_tx) {
            neg = false;
            diff = ts_rx - ts_tx;
        } else {
            neg = true;
            diff = ts_tx - ts_rx;
        }

        printf("RX: 0x%016"PRIx64"\n", ts_rx);
        printf("TX: 0x%016"PRIx64" (%c%"PRIu64")\n\n",
               ts_tx, neg ? '-' : '+', diff);

        fflush(stdout);

        usleep(1000000);
    }

out:
    bladerf_enable_module(dev, BLADERF_MODULE_RX, false);
    bladerf_enable_module(dev, BLADERF_MODULE_TX, false);
    return status;
}
