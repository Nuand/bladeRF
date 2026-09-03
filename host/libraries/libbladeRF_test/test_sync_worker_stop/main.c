/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2026 Nuand LLC
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
#include <stdlib.h>

#include "streaming/async.h"
#include "streaming/sync_worker.h"

int main(void)
{
    static struct bladerf_stream stream;
    static struct sync_worker worker;
    int failures = 0;

    MUTEX_INIT(&stream.lock);
    MUTEX_INIT(&worker.request_lock);
    COND_INIT(&worker.requests_pending);

    stream.layout = BLADERF_TX_X1;
    stream.state  = STREAM_RUNNING;
    worker.stream = &stream;

    sync_worker_submit_request(&worker, SYNC_WORKER_STOP);

    if ((worker.requests & SYNC_WORKER_STOP) == 0) {
        fputs("STOP request was not recorded\n", stderr);
        failures++;
    }

    if (stream.state != STREAM_SHUTTING_DOWN) {
        fprintf(stderr, "TX stream state is %d, expected %d\n", stream.state,
                STREAM_SHUTTING_DOWN);
        failures++;
    }

    stream.layout   = BLADERF_RX_X1;
    stream.state    = STREAM_RUNNING;
    worker.requests = 0;

    sync_worker_submit_request(&worker, SYNC_WORKER_STOP);

    if (stream.state != STREAM_SHUTTING_DOWN) {
        fprintf(stderr, "RX stream state is %d, expected %d\n", stream.state,
                STREAM_SHUTTING_DOWN);
        failures++;
    }

    MUTEX_DESTROY(&worker.request_lock);
    MUTEX_DESTROY(&stream.lock);

    if (failures != 0) {
        return EXIT_FAILURE;
    }

    puts("PASS: STOP requests start RX and TX stream shutdown");
    return EXIT_SUCCESS;
}
