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
#include "async.h"
#include "log.h"

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

    if (pthread_mutex_init(&lstream->lock, NULL) != 0) {
        free(lstream);
        return BLADERF_ERR_UNEXPECTED;
    }

    if (pthread_cond_init(&lstream->can_submit_buffer, NULL) != 0) {
        free(lstream);
        return BLADERF_ERR_UNEXPECTED;
    }

    lstream->dev = dev;
    lstream->error_code = 0;
    lstream->state = STREAM_IDLE;
    lstream->samples_per_buffer = samples_per_buffer;
    lstream->num_buffers = num_buffers;
    lstream->format = format;
    lstream->cb = callback;
    lstream->user_data = user_data;
    lstream->buffers = NULL;

    switch(format) {
        case BLADERF_FORMAT_SC16_Q11:
            buffer_size_bytes = c16_samples_to_bytes(samples_per_buffer);
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
        status = dev->fn->init_stream(lstream, num_transfers);

        if (status < 0) {
            bladerf_deinit_stream(lstream);
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

/* No device control calls may be made in this function and the associated
 * backend stream implementations, as the stream and control functionality are
 * will generally be executed from separate thread contexts. */
int async_run_stream(struct bladerf_stream *stream, bladerf_module module)
{
    int status;
    struct bladerf *dev = stream->dev;

    stream->module = module;
    stream->state = STREAM_RUNNING;
    status = dev->fn->stream(stream, module);

    /* Backend return value takes precedence over stream error status */
    return status == 0 ? stream->error_code : status;
}

int async_submit_stream_buffer(struct bladerf_stream *stream,
                               void *buffer,
                               unsigned int timeout_ms)
{
    int status;

    pthread_mutex_lock(&stream->lock);
    status = stream->dev->fn->submit_stream_buffer(stream, buffer, timeout_ms);
    pthread_mutex_unlock(&stream->lock);

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
    stream->dev->fn->deinit_stream(stream);

    /* Free up the buffers */
    for (i = 0; i < stream->num_buffers; i++) {
        free(stream->buffers[i]);
    }

    /* Free up the pointer to the buffers */
    free(stream->buffers);

    /* Free up the stream itself */
    free(stream);
}

