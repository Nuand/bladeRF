/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014 Nuand LLC
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#ifndef BLADERF_ASYNC_H_
#define BLADERF_ASYNC_H_

#include <pthread.h>
#include "libbladeRF.h"
#include "bladerf_priv.h"

typedef enum {
    STREAM_IDLE,            /* Idle and initialized */
    STREAM_RUNNING,         /* Currently running */
    STREAM_SHUTTING_DOWN,   /* Currently tearing down.
                             * See bladerf_stream->error_code to determine
                             * whether or not the shutdown was a clean exit
                             * or due to an error. */
    STREAM_DONE             /* Done and deallocated */
} bladerf_stream_state;

struct bladerf_stream {

    /* These items are configured in async_init_stream() and should only be
     * read (NOT MODIFIED) during the execution of the stream */
    struct bladerf *dev;
    bladerf_module module;
    bladerf_format format;
    bladerf_stream_cb cb;
    void *user_data;
    size_t samples_per_buffer;
    size_t num_buffers;
    void **buffers;

    pthread_mutex_t lock;

    /* The following items must be accessed atomically */
    int error_code;
    bladerf_stream_state state;
    pthread_cond_t can_submit_buffer;
    void *backend_data;
};


int async_init_stream(struct bladerf_stream **stream,
                      struct bladerf *dev,
                      bladerf_stream_cb callback,
                      void ***buffers,
                      size_t num_buffers,
                      bladerf_format format,
                      size_t buffer_size,
                      size_t num_transfers,
                      void *user_data);

/* Backend code is responsible for acquiring stream->lock in thier callbacks */
int async_run_stream(struct bladerf_stream *stream, bladerf_module module);


/* This function WILL acquire stream->lock before calling backend code */
int async_submit_stream_buffer(struct bladerf_stream *stream,
                               void *buffer,
                               unsigned int timeout_ms);


void async_deinit_stream(struct bladerf_stream *stream);

#endif
