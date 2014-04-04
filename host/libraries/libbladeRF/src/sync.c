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
        case BLADERF_MODULE_TX:
        case BLADERF_MODULE_RX:
            sync_deinit(dev->sync[module]);
            sync = dev->sync[module] =
                (struct bladerf_sync *) calloc(1, sizeof(struct bladerf_sync));

            if (dev->sync[module] == NULL) {
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
    sync->state = SYNC_STATE_CHECK_WORKER;

    sync->buf_mgmt.num_buffers = num_buffers;

    sync->stream_config.module = module;
    sync->stream_config.format = format;
    sync->stream_config.samples_per_buffer = buffer_size;
    sync->stream_config.num_xfers = num_transfers;
    sync->stream_config.timeout_ms = stream_timeout;
    sync->stream_config.bytes_per_sample = bytes_per_sample;


    pthread_mutex_init(&sync->buf_mgmt.lock, NULL);
    pthread_cond_init(&sync->buf_mgmt.buf_ready, NULL);

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
        sync_deinit(dev->sync[module]);
        dev->sync[module] = NULL;
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
                                   &sync->buf_mgmt.buf_ready);
                break;

            case BLADERF_MODULE_TX:
                bladerf_submit_stream_buffer(sync->worker->stream,
                                             BLADERF_STREAM_SHUTDOWN, 0);

                sync_worker_deinit(sync->worker,
                                   &sync->buf_mgmt.lock,
                                   &sync->buf_mgmt.buf_ready);
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

static int wait_for_buffer(struct buffer_mgmt *b, unsigned int timeout_ms,
                           const char *dbg_name, unsigned int dbg_idx)
{
    int status;
    struct timespec timeout;

    if (timeout_ms == 0) {
        log_verbose("%s: Infinite wait for [%d] to fill.\n", dbg_name, dbg_idx);
        status = pthread_cond_wait(&b->buf_ready, &b->lock);
    } else {
        log_verbose("%s: Timed wait for [%d] to fill.\n", dbg_name, dbg_idx);
        status = populate_abs_timeout(&timeout, timeout_ms);
        if (status == 0) {
            status = pthread_cond_timedwait(&b->buf_ready, &b->lock, &timeout);
        }
    }

    if (status == ETIMEDOUT) {
        status = BLADERF_ERR_TIMEOUT;
    } else if (status != 0) {
        status = BLADERF_ERR_UNEXPECTED;
    }

    return status;
}

#ifndef SYNC_WORKER_START_TIMEOUT_MS
#   define SYNC_WORKER_START_TIMEOUT_MS 250
#endif

int sync_rx(struct bladerf *dev, void *samples, unsigned num_samples,
             struct bladerf_metadata *metadata, unsigned int timeout_ms)
{
    struct bladerf_sync *s = dev->sync[BLADERF_MODULE_RX];
    struct buffer_mgmt *b = &s->buf_mgmt;

    int status = 0;
    unsigned int samples_returned = 0;
    uint8_t *samples_dest = (uint8_t*)samples;
    uint8_t *buf_src;
    unsigned int samples_to_copy;
    const unsigned int samples_per_buffer = s->stream_config.samples_per_buffer;

    if (s == NULL || samples == NULL) {
        return BLADERF_ERR_INVAL;
    }

    while (samples_returned < num_samples && status == 0) {

        switch (s->state) {
            case SYNC_STATE_CHECK_WORKER: {
                sync_worker_state worker_state = sync_worker_get_state(s->worker);

                if (worker_state == SYNC_WORKER_STATE_IDLE) {
                    log_debug("%s: Worker is idle. Going to reset buf mgmt.\n",
                              __FUNCTION__);
                    s->state = SYNC_STATE_RESET_BUF_MGMT;
                } else if (worker_state == SYNC_WORKER_STATE_RUNNING) {
                    s->state = SYNC_STATE_WAIT_FOR_BUFFER;
                } else {
                    status = BLADERF_ERR_UNEXPECTED;
                    log_debug("%s: Unexpected worker state=%d\n",
                              __FUNCTION__, worker_state);
                }

                break;
            }

            case SYNC_STATE_RESET_BUF_MGMT:
                pthread_mutex_lock(&b->lock);
                /* When the RX stream starts up, it will submit the first T
                 * transfers, so the consumer index must be reset to 0 */
                b->cons_i = 0;
                pthread_mutex_unlock(&b->lock);
                log_debug("%s: Reset buf_mgmt consumer index\n", __FUNCTION__);
                s->state = SYNC_STATE_START_WORKER;
                break;


            case SYNC_STATE_START_WORKER:
                sync_worker_submit_request(s->worker, SYNC_WORKER_START);

                status = sync_worker_wait_for_state(
                                                s->worker,
                                                SYNC_WORKER_STATE_RUNNING,
                                                SYNC_WORKER_START_TIMEOUT_MS);

                if (status == 0) {
                    s->state = SYNC_STATE_WAIT_FOR_BUFFER;
                    log_debug("%s: Worker is now running.\n", __FUNCTION__);
                } else {
                    log_debug("%s: Failed to start worker, (%d)\n",
                              __FUNCTION__, status);
                }
                break;

            case SYNC_STATE_WAIT_FOR_BUFFER:
                pthread_mutex_lock(&b->lock);

                /* Check the buffer state, as the worker may have produced one
                 * since we last queried the status */
                if (b->status[b->cons_i] == SYNC_BUFFER_FULL) {
                    s->state = SYNC_STATE_BUFFER_READY;
                } else {
                    status = wait_for_buffer(b, timeout_ms,
                                             __FUNCTION__, b->cons_i);

                    if (status == BLADERF_ERR_TIMEOUT) {
                        log_debug("%s: Timed out waiting for buffer %u\n",
                                  __FUNCTION__, b->cons_i);

                        s->state = SYNC_STATE_CHECK_WORKER;
                    }
                }

                pthread_mutex_unlock(&b->lock);
                break;

            case SYNC_STATE_BUFFER_READY:
                pthread_mutex_lock(&b->lock);
                b->status[b->cons_i] = SYNC_BUFFER_PARTIAL;
                b->partial_off = 0;
                pthread_mutex_unlock(&b->lock);

                s->state= SYNC_STATE_USING_BUFFER;
                break;

            case SYNC_STATE_USING_BUFFER:
                pthread_mutex_lock(&b->lock);

                buf_src = (uint8_t*)b->buffers[b->cons_i];

                samples_to_copy = uint_min(num_samples - samples_returned,
                                           samples_per_buffer - b->partial_off);

                memcpy(samples_dest + samples2bytes(s, samples_returned),
                       buf_src + samples2bytes(s, b->partial_off),
                       samples2bytes(s, samples_to_copy));

                b->partial_off += samples_to_copy;
                samples_returned += samples_to_copy;

                log_verbose("%s: Provided %u samples to caller\n",
                            __FUNCTION__, samples_to_copy);

                /* We've finished consuming this buffer and can start looking
                 * for available samples in the next buffer */
                if (b->partial_off >= samples_per_buffer) {

                    /* Check for symptom of out-of-bounds accesses */
                    assert(b->partial_off == samples_per_buffer);

                    log_verbose("%s: Marking buf[%u] empty.\n",
                                __FUNCTION__, b->cons_i);

                    b->status[b->cons_i] = SYNC_BUFFER_EMPTY;
                    b->cons_i = (b->cons_i + 1) % b->num_buffers;

                    s->state = SYNC_STATE_WAIT_FOR_BUFFER;
                }

                pthread_mutex_unlock(&b->lock);
                break;
        }
    }

    return status;
}

int sync_tx(struct bladerf *dev, void *samples, unsigned int num_samples,
             struct bladerf_metadata *metadata, unsigned int timeout_ms)
{
    struct bladerf_sync *s = dev->sync[BLADERF_MODULE_TX];
    struct buffer_mgmt *b = &s->buf_mgmt;

    int status = 0;
    unsigned int samples_written = 0;
    unsigned int samples_to_copy;
    const unsigned int samples_per_buffer = s->stream_config.samples_per_buffer;
    uint8_t *samples_src = (uint8_t*)samples;
    uint8_t *buf_dest;

    if (s == NULL || samples == NULL) {
        return BLADERF_ERR_INVAL;
    }

    while (status == 0 && samples_written < num_samples) {

        switch (s->state) {
            case SYNC_STATE_CHECK_WORKER: {
                sync_worker_state worker_state = sync_worker_get_state(s->worker);

                if (worker_state == SYNC_WORKER_STATE_IDLE) {
                    /* No need to reset any buffer managment for TX since
                     * the TX stream does not submit an initial set of buffers.
                     * Therefore the RESET_BUF_MGMT state is skipped here. */
                    s->state = SYNC_STATE_START_WORKER;
                }
                break;
            }

            case SYNC_STATE_RESET_BUF_MGMT:
                assert(!"Bug");
                break;

            case SYNC_STATE_START_WORKER:
                sync_worker_submit_request(s->worker, SYNC_WORKER_START);

                status = sync_worker_wait_for_state(
                        s->worker,
                        SYNC_WORKER_STATE_RUNNING,
                        SYNC_WORKER_START_TIMEOUT_MS);

                if (status == 0) {
                    s->state = SYNC_STATE_WAIT_FOR_BUFFER;
                    log_debug("%s: Worker is now running.\n", __FUNCTION__);
                }
                break;

            case SYNC_STATE_WAIT_FOR_BUFFER:
                pthread_mutex_lock(&b->lock);

                /* Check the buffer state, as the worker may have consumed one
                 * since we last queried the status */
                if (b->status[b->prod_i] == SYNC_BUFFER_EMPTY) {
                    s->state = SYNC_STATE_BUFFER_READY;
                } else {
                    status = wait_for_buffer(b, timeout_ms,
                                             __FUNCTION__, b->prod_i);
                }

                pthread_mutex_unlock(&b->lock);
                break;

            case SYNC_STATE_BUFFER_READY:
                pthread_mutex_lock(&b->lock);
                b->status[b->prod_i] = SYNC_BUFFER_PARTIAL;
                b->partial_off = 0;
                pthread_mutex_unlock(&b->lock);

                s->state= SYNC_STATE_USING_BUFFER;
                break;


            case SYNC_STATE_USING_BUFFER:
                pthread_mutex_lock(&b->lock);

                buf_dest = (uint8_t*)b->buffers[b->prod_i];
                samples_to_copy = uint_min(num_samples - samples_written,
                                           samples_per_buffer - b->partial_off);

                memcpy(buf_dest + samples2bytes(s, b->partial_off),
                        samples_src + samples2bytes(s, samples_written),
                        samples2bytes(s, samples_to_copy));

                b->partial_off += samples_to_copy;
                samples_written += samples_to_copy;

                log_verbose("%s: Buffered %u samples from caller\n",
                            __FUNCTION__, samples_to_copy);

                if (b->partial_off >= samples_per_buffer) {
                    /* Check for symptom of out-of-bounds accesses */
                    assert(b->partial_off == samples_per_buffer);

                    log_verbose("%s: Marking buf[%u] full\n",
                                __FUNCTION__, b->prod_i);

                    b->status[b->prod_i] = SYNC_BUFFER_IN_FLIGHT;
                    pthread_mutex_unlock(&b->lock);

                    /* This call may block and it results in a per-stream lock
                     * being held, so the buffer lock must be dropped.
                     *
                     * A callback may occur in the meantime, but this will
                     * not touch the status for this this buffer, or the
                     * producer index.
                     */
                    status = bladerf_submit_stream_buffer(
                                                s->worker->stream,
                                                buf_dest,
                                                s->stream_config.timeout_ms);

                    pthread_mutex_lock(&b->lock);

                    if (status == 0) {
                        b->prod_i = (b->prod_i + 1) % b->num_buffers;

                        /* Go handle the next buffer, if we have one available.
                         * Otherwise, check up on the worker's state and restart
                         * it if needed. */
                        if (b->status[b->prod_i] == SYNC_BUFFER_EMPTY) {
                            s->state = SYNC_STATE_BUFFER_READY;
                        } else {
                            s->state = SYNC_STATE_CHECK_WORKER;
                        }

                    }
                }

                pthread_mutex_unlock(&b->lock);
                break;
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

