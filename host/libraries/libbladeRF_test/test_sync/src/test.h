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
#include <stdio.h>
#include <libbladeRF.h>

/* Device config defaults */
#define DEFAULT_SAMPLERATE      1000000
#define DEFAULT_FREQUENCY       1000000000

/* Test defaults */
#define DEFAULT_TX_REPETITIONS  1
#define DEFAULT_RX_COUNT        10000000
#define DEFAULT_BLOCK_SIZE      2048

/* Stream defaults */
#define DEFAULT_STREAM_XFERS    16
#define DEFAULT_STREAM_BUFFERS  32
#define DEFAULT_STREAM_SAMPLES  8192
#define DEFAULT_STREAM_TIMEOUT  1000

#define SYNC_TIMEOUT_MS         500

struct test_params {
    const char *device_str;
    unsigned int samplerate;
    unsigned int frequency;
    bladerf_loopback loopback;

    FILE *in_file;
    FILE *out_file;
    unsigned int tx_repetitions;
    uint64_t rx_count;
    unsigned int block_size;

    /* Stream config */
    unsigned int num_xfers;
    unsigned int stream_buffer_count;
    unsigned int stream_buffer_size;    /* Units of samples */
    unsigned int timeout_ms;
};

void test_init_params(struct test_params *p);

int test_run(struct test_params *p);
