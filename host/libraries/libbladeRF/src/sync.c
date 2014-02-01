/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */
#ifndef ENABLE_LIBBLADERF_SYNC
#error "Build configuration bug: this file should not be included in the build."
#endif

#include <errno.h>

/* Only switch on the verbose debug prints in this file when we *really* want
 * them. Otherwise, compile them out to avoid excessive log level checks
 * in our data path */
#include "log.h"
#ifndef ENABLE_LIBBLADERF_SYNC_LOG_VERBOSE
#undef log_verbose
#define log_verbose(...)
#endif

#include "bladerf_priv.h"
#include "sync.h"
#include "sync_worker.h"
#include "minmax.h"

static inline size_t samples2bytes(struct bladerf_sync *s, unsigned int n) {
    return s->stream_config.bytes_per_sample * n;
}


int sync_init(struct bladerf *dev,
              bladerf_module module,
              bladerf_format format,
              unsigned int num_buffers,
              unsigned int buffer_size,
              unsigned int num_transfers,
              unsigned int stream_timeout)

{
    struct bladerf_sync *sync;
    int status = 0;
    size_t i, bytes_per_sample;

    if (num_transfers >= num_buffers) {
        return BLADERF_ERR_INVAL;
    }

    switch (format) {
        case BLADERF_FORMAT_SC16_Q11:
            bytes_per_sample = 4;
            break;

        default:
            return BLADERF_ERR_INVAL;
    }

    /* bladeRF GPIF DMA requirement */
    if ((bytes_per_sample * buffer_size) % 4096 != 0) {
        return BLADERF_ERR_INVAL;
    }

    /* Deallocate any existing sync handle for this module */
    switch (module) {
        case BLADERF_MODULE_RX:
            sync_deinit(dev->sync_rx);
            sync = dev->sync_rx = (struct bladerf_sync *) calloc(1, sizeof(struct bladerf_sync));
            if (dev->sync_rx == NULL) {
                status = BLADERF_ERR_MEM;
            }
            break;

        case BLADERF_MODULE_TX:
            sync_deinit(dev->sync_tx);
            sync = dev->sync_tx = (struct bladerf_sync *) calloc(1, sizeof(struct bladerf_sync));
            if (dev->sync_tx == NULL) {
                status = BLADERF_ERR_MEM;
            }
            break;

        default:
            log_debug("Invalid bladerf_module value encountered: %d", module);
            status = BLADERF_ERR_INVAL;
    }

    if (status != 0) {
        return status;
    }

    sync->dev = dev;

    sync->stream_config.module = module;
    sync->stream_config.format = format;
    sync->stream_config.samples_per_buffer = buffer_size;
    sync->stream_config.num_xfers = num_transfers;
    sync->stream_config.timeout_ms = stream_timeout;
    sync->stream_config.bytes_per_sample = bytes_per_sample;

    sync->buf_mgmt.num_buffers = num_buffers;

    pthread_mutex_init(&sync->buf_mgmt.lock, NULL);
    pthread_cond_init(&sync->buf_mgmt.buf_consumed, NULL);
    pthread_cond_init(&sync->buf_mgmt.buf_produced, NULL);

    sync->buf_mgmt.status = (sync_buffer_status*) malloc(num_buffers * sizeof(sync_buffer_status));
    if (sync->buf_mgmt.status == NULL) {
        status = BLADERF_ERR_MEM;
    } else {
        switch (module) {
            case BLADERF_MODULE_RX:
                /* When starting up an RX stream, the first 'num_transfers'
                 * transfers will be submitted to the USB layer to grab data */
                sync->buf_mgmt.prod_i = num_transfers;
                sync->buf_mgmt.cons_i = 0;
                sync->buf_mgmt.partial_off = 0;

                for (i = 0; i < num_buffers; i++) {
                    if (i < num_transfers) {
                        sync->buf_mgmt.status[i] = SYNC_BUFFER_IN_FLIGHT;
                    } else {
                        sync->buf_mgmt.status[i] = SYNC_BUFFER_EMPTY;
                    }
                }
                break;

            case BLADERF_MODULE_TX:
                sync->buf_mgmt.prod_i = 0;
                sync->buf_mgmt.cons_i = 0;
                sync->buf_mgmt.partial_off = 0;

                for (i = 0; i < num_buffers; i++) {
                    sync->buf_mgmt.status[i] = SYNC_BUFFER_EMPTY;
                }

                break;
        }

        status = sync_worker_init(sync);
    }

    if (status != 0) {
        switch (module) {
            case BLADERF_MODULE_RX:
                sync_deinit(dev->sync_rx);
                dev->sync_rx = NULL;
                break;

            case BLADERF_MODULE_TX:
                sync_deinit(dev->sync_tx);
                dev->sync_tx = NULL;
                break;
        }
    }

    return status;
}

void sync_deinit(struct bladerf_sync *sync)
{
    if (sync != NULL) {

        switch (sync->stream_config.module) {
            case BLADERF_MODULE_RX:
                sync_worker_deinit(sync->worker,
                                   &sync->buf_mgmt.lock,
                                   &sync->buf_mgmt.buf_consumed);
                break;

            case BLADERF_MODULE_TX:
                sync_worker_deinit(sync->worker,
                                   &sync->buf_mgmt.lock,
                                   &sync->buf_mgmt.buf_produced);
                break;

            default:
                assert(!"Invalid module encountered");
                break;
        }

         /* De-allocate our buffer management resources */
        free(sync->buf_mgmt.status);
        free(sync);
    }
}

static int check_worker(struct bladerf_sync *s)
{
    sync_worker_state state;
    int status = 0;
    const int max_retries = 8;
    int retry = 0;

    do {
        state = sync_worker_get_state(s->worker);

        if (state != SYNC_WORKER_STATE_RUNNING) {

            /* We need to start up the worker */
            log_debug("%s: Worker not running (state=%d)... %s\n",
                        __FUNCTION__, state,
                        retry == 0 ? "starting it." : "retrying.");

            sync_worker_submit_request(s->worker, SYNC_WORKER_START);
            status = sync_worker_wait_for_state(s->worker,
                                                SYNC_WORKER_STATE_RUNNING, 250);
            if (status == 0) {
                state = SYNC_WORKER_STATE_RUNNING;
                log_debug("%s: Worker is now running.\n", __FUNCTION__);
            }
        }
    } while ((state != SYNC_WORKER_STATE_RUNNING) && (++retry <= max_retries));

    /* By this point, our worker should be running */
    if (status != 0 || state != SYNC_WORKER_STATE_RUNNING) {
        if (status == 0) {
            status = BLADERF_ERR_UNEXPECTED;
        }

        log_debug("%s: %s worker failed to start (state=%d): %s\n",
                  __FUNCTION__, module2str(s->stream_config.module),
                  state, bladerf_strerror(status));
    }

    return status;
}

int sync_rx(struct bladerf *dev, void *samples, unsigned num_samples,
             struct bladerf_metadata *metadata, unsigned int timeout_ms)
{
    int status;
    struct bladerf_sync *s = dev->sync_rx;
    struct buffer_mgmt *b = &s->buf_mgmt;
    unsigned int samples_returned = 0;
    struct timespec timeout_abs;

    if (s == NULL) {
        return BLADERF_ERR_INVAL;
    }

    /* Check if our worker thread is running, and start it if it's not */
    status = check_worker(s);

    while(status == 0 && samples_returned < num_samples) {
        pthread_mutex_lock(&b->lock);

        /* If we have a fresh buffer, mark that we're consuming it */
        if (b->status[b->cons_i] == SYNC_BUFFER_FULL) {
            b->status[b->cons_i] = SYNC_BUFFER_PARTIAL;
            b->partial_off = 0;
        }

        if (b->status[b->cons_i] == SYNC_BUFFER_PARTIAL) {
            /* We currently have a buffer to remove samples from */
            uint8_t *samples_dest = (uint8_t*)samples;
            uint8_t *buf_src = (uint8_t*)b->buffers[b->cons_i];
            const unsigned int samples_to_copy =
                uint_min(num_samples - samples_returned,
                         s->stream_config.samples_per_buffer - b->partial_off);

            memcpy(samples_dest + samples2bytes(s, samples_returned),
                   buf_src + samples2bytes(s, b->partial_off),
                   samples2bytes(s, samples_to_copy));

            b->partial_off += samples_to_copy;
            samples_returned += samples_to_copy;

            log_verbose("%s: Provided %u samples to caller\n",
                        __FUNCTION__, samples_to_copy);

            /* We've finished consuming this buffer and can start looking
             * for available samples in the next buffer */
            if (b->partial_off >= s->stream_config.samples_per_buffer) {
                log_verbose("%s: Marking buf[%u] empty.\n",
                            __FUNCTION__, b->cons_i);

                b->status[b->cons_i] = SYNC_BUFFER_EMPTY;
                b->cons_i = (b->cons_i + 1) % b->num_buffers;
                pthread_cond_signal(&b->buf_consumed);
            }

        } else {
            /* Need to wait for a buffer to become available */
            if (timeout_ms == 0) {
                status = pthread_cond_wait(&b->buf_produced, &b->lock);
                log_verbose("%s: Infinite wait for [%d] to fill.\n",
                           __FUNCTION__, b->cons_i);
            } else {
                log_verbose("%s: Timed wait for [%d] to fill.\n",
                           __FUNCTION__, b->cons_i);
               status = populate_abs_timeout(&timeout_abs, timeout_ms);
               if (status == 0) {
                   status = pthread_cond_timedwait(&b->buf_produced, &b->lock,
                                                   &timeout_abs);
               }
            }
        }
        pthread_mutex_unlock(&b->lock);

        if (status == -ETIMEDOUT) {
            status = BLADERF_ERR_TIMEOUT;
        } else if (status != 0) {
            status = BLADERF_ERR_UNEXPECTED;
        } else {
            status = check_worker(s);
        }
    }

    return status;
}

int sync_tx(struct bladerf *dev, void *samples, unsigned int num_samples,
             struct bladerf_metadata *metadata, unsigned int timeout_ms)
{
    struct bladerf_sync *s = dev->sync_tx;
    struct buffer_mgmt *b = &s->buf_mgmt;
    struct timespec timeout_abs;
    unsigned int samples_written = 0;
    int status = 0;

    if (s == NULL) {
        return BLADERF_ERR_INVAL;
    }

    /* Check that our work thread is running, and start it if it's not */
    status = check_worker(s);
    while (status == 0 && samples_written < num_samples) {
        pthread_mutex_lock(&b->lock);

        /* If we've got an empty buffer, mark that we're producing it */
        if (b->status[b->prod_i] == SYNC_BUFFER_EMPTY) {
            b->status[b->prod_i] = SYNC_BUFFER_PARTIAL;
            b->partial_off = 0;
        }

        if (b->status[b->prod_i] == SYNC_BUFFER_PARTIAL) {
            /* We're currently filling a buffer */
            uint8_t *buf_dest = (uint8_t*)b->buffers[b->prod_i];
            uint8_t *samples_src = (uint8_t*)samples;

            const unsigned int samples_to_copy =
                uint_min(num_samples - samples_written,
                         s->stream_config.samples_per_buffer - b->partial_off);

            memcpy(buf_dest + samples2bytes(s, b->partial_off),
                   samples_src + samples2bytes(s, samples_written),
                   samples2bytes(s, samples_to_copy));

            b->partial_off += samples_to_copy;
            samples_written += samples_to_copy;

            log_verbose("%s: Buffered %u samples from caller\n",
                        __FUNCTION__, samples_to_copy);

            if (b->partial_off >= s->stream_config.samples_per_buffer) {
                log_verbose("%s: Marking buf[%u] full\n",
                            __FUNCTION__, b->prod_i);

                b->status[b->prod_i] = SYNC_BUFFER_FULL;
                b->prod_i = (b->prod_i + 1) % b->num_buffers;
                pthread_cond_signal(&b->buf_produced);
            }

        } else {
            /* We need to wait for a buffer to be emptied by the worker */
            if (timeout_ms == 0) {
                log_verbose("%s: Infinite wait for [%d] to empty.\n",
                           __FUNCTION__, b->prod_i);
                status = pthread_cond_wait(&b->buf_consumed, &b->lock);
            } else {
                status = populate_abs_timeout(&timeout_abs, timeout_ms);
                if (status == 0) {
                    log_verbose("%s: Timed wait for [%d] to empty.\n",
                                __FUNCTION__, b->prod_i);
                    status = pthread_cond_timedwait(&b->buf_consumed, &b->lock,
                                                    &timeout_abs);
                }
            }
        }

        pthread_mutex_unlock(&b->lock);

        if (status == -ETIMEDOUT) {
            status = BLADERF_ERR_TIMEOUT;
        } else if (status != 0) {
            status = BLADERF_ERR_UNEXPECTED;
        } else {
            status = check_worker(s);
        }
    }

    return status;
}

unsigned int sync_buf2idx(struct buffer_mgmt *b, void *addr)
{
    unsigned int i;

    for (i = 0; i < b->num_buffers; i++) {
        if (b->buffers[i] == addr) {
            return i;
        }
    }

    assert(!"Bug: Buffer not found.");

    /* Assertions are intended to always remain on. If someone turned them
     * off, do the best we can...complain loudly and clobber a buffer */
    log_critical("Bug: Buffer not found.");
    return 0;
}

