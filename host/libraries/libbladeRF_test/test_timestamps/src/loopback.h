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
#ifndef TEST_TIMESTAMPS_LOOPBACK_H_
#define TEST_TIMESTAMPS_LOOPBACK_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <libbladeRF.h>

#define LOOPBACK_TX_MAGNITUDE    2000
#define LOOPBACK_RX_POWER_THRESH (256 * 256)

struct loopback_burst_test {
    struct bladerf *dev;
    struct app_params *params;
    struct loopback_burst *bursts;
    unsigned int num_bursts;

    pthread_mutex_t lock;
    bool stop;
    bool rx_ready;
};

struct loopback_burst {
    uint64_t duration;
    uint64_t gap;
};

/* Returns 0 on success, non-zero on failure */
int setup_device_loopback(struct loopback_burst_test *t);

/* Alloc and fill sample buffer for TX.
 * Returns heap-allocated sample buffer on success, NULL on failure */
int16_t * alloc_loopback_samples(size_t n_samples);

/* Returns NULL, args should be the loopback_burst_test structure */
void *loopback_burst_rx_task(void *args);

#endif
