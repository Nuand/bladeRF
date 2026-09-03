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

#include <libusb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "streaming/async.h"

struct test_lusb_backend {
    libusb_device *device;
    libusb_device_handle *handle;
    libusb_context *context;
};

struct test_lusb_stream_data {
    size_t num_transfers;
    size_t num_available;
    size_t next_transfer;
    struct libusb_transfer **transfers;
    int *transfer_status;
    bool out_of_order_event;
    int done_flag;
};

static struct bladerf_stream *active_stream;
static unsigned int completed_wait_calls;
static unsigned int legacy_wait_calls;

struct bladerf_devinfo_list;

int test_lusb_stream(void *driver,
                     struct bladerf_stream *stream,
                     bladerf_channel_layout layout);

int bladerf_devinfo_list_add(struct bladerf_devinfo_list *list,
                             struct bladerf_devinfo *info)
{
    (void)list;
    (void)info;
    return 0;
}

void bladerf_init_devinfo(struct bladerf_devinfo *info)
{
    (void)info;
}

bool bladerf_devinfo_matches(const struct bladerf_devinfo *a,
                             const struct bladerf_devinfo *b)
{
    (void)a;
    (void)b;
    return false;
}

int __wrap_libusb_handle_events_timeout(libusb_context *context,
                                        struct timeval *timeout)
{
    (void)context;
    (void)timeout;
    legacy_wait_calls++;
    active_stream->state = STREAM_DONE;
    return 0;
}

int __wrap_libusb_handle_events_timeout_completed(libusb_context *context,
                                                  struct timeval *timeout,
                                                  int *completed)
{
    (void)context;
    (void)timeout;
    completed_wait_calls++;
    *completed           = 1;
    active_stream->state = STREAM_DONE;
    return 0;
}

int main(void)
{
    static struct test_lusb_stream_data stream_data;
    static struct test_lusb_backend backend;
    static struct bladerf_stream stream;
    int status;

    MUTEX_INIT(&stream.lock);
    stream.backend_data = &stream_data;
    stream.state        = STREAM_RUNNING;
    active_stream       = &stream;

    status = test_lusb_stream(&backend, &stream, BLADERF_TX_X1);

    MUTEX_DESTROY(&stream.lock);

    if (status != 0) {
        fprintf(stderr, "libusb stream returned %d\n", status);
        return EXIT_FAILURE;
    }

    if (completed_wait_calls != 1 || legacy_wait_calls != 0) {
        fprintf(stderr, "completed waits=%u legacy waits=%u, expected 1 and 0\n",
                completed_wait_calls, legacy_wait_calls);
        return EXIT_FAILURE;
    }

    puts("PASS: libusb stream uses the completed event-wait protocol");
    return EXIT_SUCCESS;
}
