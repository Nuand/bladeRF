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

#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include "log.h"

#include "backend/usb/usb.h"

#include "async.h"
#include "board/board.h"
#include "helpers/timeout.h"

int async_init_stream(struct bladerf_stream **stream,
                      struct bladerf *dev,
                      bladerf_stream_cb callback,
                      void ***buffers,
                      size_t num_buffers,
                      bladerf_format format,
                      size_t samples_per_buffer,
                      size_t num_transfers,
                      void *user_data)
{
    struct bladerf_stream *lstream;
    size_t buffer_size_bytes;
    size_t i;
    int status = 0;

    if (num_transfers > num_buffers) {
        log_debug("num_transfers must be <= num_buffers\n");
        return BLADERF_ERR_INVAL;
    }

    if (samples_per_buffer < 1024 || samples_per_buffer % 1024 != 0) {
        log_debug("samples_per_buffer must be multiples of 1024\n");
        return BLADERF_ERR_INVAL;
    }

    /* Create a stream and populate it with the appropriate information */
    lstream = malloc(sizeof(struct bladerf_stream));

    if (!lstream) {
        return BLADERF_ERR_MEM;
    }

    MUTEX_INIT(&lstream->lock);

    if (pthread_cond_init(&lstream->can_submit_buffer, NULL) != 0) {
        free(lstream);
        return BLADERF_ERR_UNEXPECTED;
    }

    if (pthread_cond_init(&lstream->stream_started, NULL) != 0) {
        free(lstream);
        return BLADERF_ERR_UNEXPECTED;
    }

    lstream->dev = dev;
    lstream->error_code = 0;
    lstream->state = STREAM_IDLE;
    lstream->samples_per_buffer = samples_per_buffer;
    lstream->num_buffers = num_buffers;
    lstream->format = format;
    lstream->transfer_timeout = BULK_TIMEOUT_MS;
    lstream->cb = callback;
    lstream->user_data = user_data;
    lstream->buffers = NULL;

    switch(format) {
        case BLADERF_FORMAT_SC16_Q11:
        case BLADERF_FORMAT_SC16_Q11_META:
            buffer_size_bytes = sc16q11_to_bytes(samples_per_buffer);
            break;

        default:
            status = BLADERF_ERR_INVAL;
            break;
    }

    if (!status) {
        lstream->buffers = calloc(num_buffers, sizeof(lstream->buffers[0]));
        if (lstream->buffers) {
            for (i = 0; i < num_buffers && !status; i++) {
                lstream->buffers[i] = calloc(1, buffer_size_bytes);
                if (!lstream->buffers[i]) {
                    status = BLADERF_ERR_MEM;
                }
            }
        } else {
            status = BLADERF_ERR_MEM;
        }
    }

    /* Clean up everything we've allocated if we hit any errors */
    if (status) {

        if (lstream->buffers) {
            for (i = 0; i < num_buffers; i++) {
                free(lstream->buffers[i]);
            }

            free(lstream->buffers);
        }

        free(lstream);
    } else {
        /* Perform any backend-specific stream initialization */
        status = dev->backend->init_stream(lstream, num_transfers);

        if (status < 0) {
            async_deinit_stream(lstream);
            *stream = NULL;
        } else {
            /* Update the caller's pointers */
            *stream = lstream;

            if (buffers) {
                *buffers = lstream->buffers;
            }
        }
    }

    return status;
}

int async_set_transfer_timeout(struct bladerf_stream *stream,
                               unsigned int transfer_timeout_ms)
{
    MUTEX_LOCK(&stream->lock);
    stream->transfer_timeout = transfer_timeout_ms;
    MUTEX_UNLOCK(&stream->lock);

    return 0;
}

int async_get_transfer_timeout(struct bladerf_stream *stream,
                               unsigned int *transfer_timeout_ms)
{
    MUTEX_LOCK(&stream->lock);
    *transfer_timeout_ms = stream->transfer_timeout;
    MUTEX_UNLOCK(&stream->lock);

    return 0;
}

int async_run_stream(struct bladerf_stream *stream, bladerf_channel_layout layout)
{
    int status;
    struct bladerf *dev = stream->dev;

    MUTEX_LOCK(&stream->lock);
    stream->layout = layout;
    stream->state = STREAM_RUNNING;
    pthread_cond_signal(&stream->stream_started);
    MUTEX_UNLOCK(&stream->lock);

    status = dev->backend->stream(stream, layout);

    /* Backend return value takes precedence over stream error status */
    return status == 0 ? stream->error_code : status;
}

int async_submit_stream_buffer(struct bladerf_stream *stream,
                               void *buffer, unsigned int timeout_ms,
                               bool nonblock)
{
    int status = 0;
    struct timespec timeout_abs;

    MUTEX_LOCK(&stream->lock);

    if (buffer != BLADERF_STREAM_SHUTDOWN) {
        if (stream->state != STREAM_RUNNING && timeout_ms != 0) {
            status = populate_abs_timeout(&timeout_abs, timeout_ms);
            if (status != 0) {
                log_debug("Failed to populate timeout value\n");
                goto error;
            }
        }

        while (stream->state != STREAM_RUNNING) {
            log_debug("Buffer submitted while stream's not running. "
                    "Waiting for stream to start.\n");

            if (timeout_ms == 0) {
                status = pthread_cond_wait(&stream->stream_started,
                                           &stream->lock);
            } else {
                status = pthread_cond_timedwait(&stream->stream_started,
                        &stream->lock, &timeout_abs);
            }

            if (status == ETIMEDOUT) {
                status = BLADERF_ERR_TIMEOUT;
                log_debug("%s: %u ms timeout expired",
                          __FUNCTION__, timeout_ms);
                goto error;
            } else if (status != 0) {
                status = BLADERF_ERR_UNEXPECTED;
                goto error;
            }
        }
    }

    status = stream->dev->backend->submit_stream_buffer(stream, buffer,
                                                   timeout_ms, nonblock);

error:
    MUTEX_UNLOCK(&stream->lock);
    return status;
}

void async_deinit_stream(struct bladerf_stream *stream)
{
    size_t i;

    if (!stream) {
        log_debug("%s called with NULL stream\n", __FUNCTION__);
        return;
    }

    while(stream->state != STREAM_DONE && stream->state != STREAM_IDLE) {
        log_verbose( "Stream not done...\n" );
        usleep(1000000);
    }

    /* Free up the backend data */
    stream->dev->backend->deinit_stream(stream);

    /* Free up the buffers */
    for (i = 0; i < stream->num_buffers; i++) {
        free(stream->buffers[i]);
    }

    /* Free up the pointer to the buffers */
    free(stream->buffers);

    /* Free up the stream itself */
    free(stream);
}

