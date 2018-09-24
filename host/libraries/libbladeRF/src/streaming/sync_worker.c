/*
 * Copyright (C) 2014-2015 Nuand LLC
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* Only switch on the verbose debug prints in this file when we *really* want
 * them. Otherwise, compile them out to avoid excessive log level checks
 * in our data path */
#include "log.h"
#ifndef ENABLE_LIBBLADERF_SYNC_LOG_VERBOSE
#undef log_verbose
#define log_verbose(...)
#endif
#include "rel_assert.h"
#include "conversions.h"
#include "minmax.h"

#include "async.h"
#include "sync.h"
#include "sync_worker.h"

#include "board/board.h"
#include "backend/usb/usb.h"

#define worker2str(s) (direction2str(s->stream_config.layout & BLADERF_DIRECTION_MASK))

void *sync_worker_task(void *arg);

static void *rx_callback(struct bladerf *dev,
                         struct bladerf_stream *stream,
                         struct bladerf_metadata *meta,
                         void *samples,
                         size_t num_samples,
                         void *user_data)
{
    unsigned int requests;      /* Pending requests */
    unsigned int next_idx;
    unsigned int samples_idx;
    void *next_buf = NULL;      /* Next buffer to submit for reception */

    struct bladerf_sync *s = (struct bladerf_sync *)user_data;
    struct sync_worker  *w = s->worker;
    struct buffer_mgmt  *b = &s->buf_mgmt;

    /* Check if the caller has requested us to shut down. We'll keep the
     * SHUTDOWN bit set through our transition into the IDLE state so we
     * can act on it there. */
    MUTEX_LOCK(&w->request_lock);
    requests = w->requests;
    MUTEX_UNLOCK(&w->request_lock);

    if (requests & SYNC_WORKER_STOP) {
        log_verbose("%s worker: Got STOP request upon entering callback. "
                    "Ending stream.\n", worker2str(s));
        return NULL;
    }

    MUTEX_LOCK(&b->lock);

    /* Get the index of the buffer that was just filled */
    samples_idx = sync_buf2idx(b, samples);

    if (b->resubmit_count == 0) {
        if (b->status[b->prod_i] == SYNC_BUFFER_EMPTY) {

            /* This buffer is now ready for the consumer */
            b->status[samples_idx] = SYNC_BUFFER_FULL;
            pthread_cond_signal(&b->buf_ready);

            /* Update the state of the buffer being submitted next */
            next_idx = b->prod_i;
            b->status[next_idx] = SYNC_BUFFER_IN_FLIGHT;
            next_buf = b->buffers[next_idx];

            /* Advance to the next buffer for the next callback */
            b->prod_i = (next_idx + 1) % b->num_buffers;

            log_verbose("%s worker: buf[%u] = full, buf[%u] = in_flight\n",
                        worker2str(s), samples_idx, next_idx);

        } else {
            /* TODO propagate back the RX Overrun to the sync_rx() caller */
            log_debug("RX overrun @ buffer %u\r\n", samples_idx);

            next_buf = samples;
            b->resubmit_count = s->stream_config.num_xfers - 1;
        }
    } else {
        /* We're still recovering from an overrun at this point. Just
         * turn around and resubmit this buffer */
        next_buf = samples;
        b->resubmit_count--;
        log_verbose("Resubmitting buffer %u (%u resubmissions left)\r\n",
                    samples_idx, b->resubmit_count);
    }


    MUTEX_UNLOCK(&b->lock);
    return next_buf;
}

static void *tx_callback(struct bladerf *dev,
                         struct bladerf_stream *stream,
                         struct bladerf_metadata *meta,
                         void *samples,
                         size_t num_samples,
                         void *user_data)
{
    unsigned int requests;      /* Pending requests */
    unsigned int completed_idx; /* Index of completed buffer */

    struct bladerf_sync *s = (struct bladerf_sync *)user_data;
    struct sync_worker  *w = s->worker;
    struct buffer_mgmt  *b = &s->buf_mgmt;

    void *ret = BLADERF_STREAM_NO_DATA;

    /* Check if the caller has requested us to shut down. We'll keep the
     * SHUTDOWN bit set through our transition into the IDLE state so we
     * can act on it there. */
    MUTEX_LOCK(&w->request_lock);
    requests = w->requests;
    MUTEX_UNLOCK(&w->request_lock);

    if (requests & SYNC_WORKER_STOP) {
        log_verbose("%s worker: Got STOP request upon entering callback. "
                    "Ending stream.\r\n", worker2str(s));
        return NULL;
    }

    /* The initial set of callbacks will do not provide us with any
     * completed sample buffers */
    if (samples != NULL) {
        MUTEX_LOCK(&b->lock);

        /* Mark the completed buffer as being empty */
        completed_idx = sync_buf2idx(b, samples);
        assert(b->status[completed_idx] == SYNC_BUFFER_IN_FLIGHT);
        b->status[completed_idx] = SYNC_BUFFER_EMPTY;
        pthread_cond_signal(&b->buf_ready);

        /* If the callback is assigned to be the submitter, there are
         * buffers pending submission */
        if (b->submitter == SYNC_TX_SUBMITTER_CALLBACK) {
            assert(b->cons_i != BUFFER_MGMT_INVALID_INDEX);
            if (b->status[b->cons_i] == SYNC_BUFFER_FULL) {
                /* This buffer is ready to ship out ("consume") */
                log_verbose("%s: Submitting deferred buf[%u]\n",
                            __FUNCTION__, b->cons_i);

                ret = b->buffers[b->cons_i];
                b->status[b->cons_i] = SYNC_BUFFER_IN_FLIGHT;
                b->cons_i = (b->cons_i + 1) % b->num_buffers;
            } else {
                log_verbose("%s: No deferred buffer available. "
                            "Assigning submitter=FN\n", __FUNCTION__);

                b->submitter = SYNC_TX_SUBMITTER_FN;
                b->cons_i = BUFFER_MGMT_INVALID_INDEX;
            }
        }

        MUTEX_UNLOCK(&b->lock);

        log_verbose("%s worker: Buffer %u emptied.\r\n",
                    worker2str(s), completed_idx);
    }

    return ret;
}

int sync_worker_init(struct bladerf_sync *s)
{
    int status = 0;
    s->worker  = (struct sync_worker *)calloc(1, sizeof(*s->worker));

    if (s->worker == NULL) {
        status = BLADERF_ERR_MEM;
        goto worker_init_out;
    }

    s->worker->state    = SYNC_WORKER_STATE_STARTUP;
    s->worker->err_code = 0;

    s->worker->cb =
        (s->stream_config.layout & BLADERF_DIRECTION_MASK) == BLADERF_RX
            ? rx_callback
            : tx_callback;

    status = async_init_stream(
        &s->worker->stream, s->dev, s->worker->cb, &s->buf_mgmt.buffers,
        s->buf_mgmt.num_buffers, s->stream_config.format,
        s->stream_config.samples_per_buffer, s->stream_config.num_xfers, s);

    if (status != 0) {
        log_debug("%s worker: Failed to init stream: %s\n", worker2str(s),
                  bladerf_strerror(status));
        goto worker_init_out;
    }

    status = async_set_transfer_timeout(
        s->worker->stream,
        uint_max(s->stream_config.timeout_ms, BULK_TIMEOUT_MS));
    if (status != 0) {
        log_debug("%s worker: Failed to set transfer timeout: %s\n",
                  worker2str(s), bladerf_strerror(status));
        goto worker_init_out;
    }

    MUTEX_INIT(&s->worker->state_lock);
    MUTEX_INIT(&s->worker->request_lock);

    status = pthread_cond_init(&s->worker->state_changed, NULL);
    if (status != 0) {
        log_debug("%s worker: pthread_cond_init(state_changed) failed: %d\n",
                  worker2str(s), status);
        status = BLADERF_ERR_UNEXPECTED;
        goto worker_init_out;
    }

    status = pthread_cond_init(&s->worker->requests_pending, NULL);
    if (status != 0) {
        log_debug("%s worker: pthread_cond_init(requests_pending) failed: %d\n",
                  worker2str(s), status);
        status = BLADERF_ERR_UNEXPECTED;
        goto worker_init_out;
    }

    status = pthread_create(&s->worker->thread, NULL, sync_worker_task, s);
    if (status != 0) {
        log_debug("%s worker: pthread_create failed: %d\n", worker2str(s),
                  status);
        status = BLADERF_ERR_UNEXPECTED;
        goto worker_init_out;
    }

    /* Wait until the worker thread has initialized and is ready to go */
    status =
        sync_worker_wait_for_state(s->worker, SYNC_WORKER_STATE_IDLE, 1000);
    if (status != 0) {
        log_debug("%s worker: sync_worker_wait_for_state failed: %d\n",
                  worker2str(s), status);
        status = BLADERF_ERR_TIMEOUT;
        goto worker_init_out;
    }

worker_init_out:
    if (status != 0) {
        free(s->worker);
        s->worker = NULL;
    }

    return status;
}

void sync_worker_deinit(struct sync_worker *w,
                        pthread_mutex_t *lock, pthread_cond_t *cond)
{
    int status;

    if (w == NULL) {
        log_debug("%s called with NULL ptr\n", __FUNCTION__);
        return;
    }

    log_verbose("%s: Requesting worker %p to stop...\n", __FUNCTION__, w);

    sync_worker_submit_request(w, SYNC_WORKER_STOP);

    if (lock != NULL && cond != NULL) {
        MUTEX_LOCK(lock);
        pthread_cond_signal(cond);
        MUTEX_UNLOCK(lock);
    }

    status = sync_worker_wait_for_state(w, SYNC_WORKER_STATE_STOPPED, 3000);

    if (status != 0) {
        log_warning("Timed out while stopping worker. Canceling thread.\n");
        pthread_cancel(w->thread);
    }

    pthread_join(w->thread, NULL);
    log_verbose("%s: Worker joined.\n", __FUNCTION__);

    async_deinit_stream(w->stream);

    free(w);
}

void sync_worker_submit_request(struct sync_worker *w, unsigned int request)
{
    MUTEX_LOCK(&w->request_lock);
    w->requests |= request;
    pthread_cond_signal(&w->requests_pending);
    MUTEX_UNLOCK(&w->request_lock);
}

int sync_worker_wait_for_state(struct sync_worker *w, sync_worker_state state,
                               unsigned int timeout_ms)
{
    int status = 0;
    struct timespec timeout_abs;
    const int nsec_per_sec = 1000 * 1000 * 1000;

    if (timeout_ms != 0) {
        const unsigned int timeout_sec = timeout_ms / 1000;

        status = clock_gettime(CLOCK_REALTIME, &timeout_abs);
        if (status != 0) {
            return BLADERF_ERR_UNEXPECTED;
        }

        timeout_abs.tv_sec += timeout_sec;
        timeout_abs.tv_nsec += (timeout_ms % 1000) * 1000 * 1000;

        if (timeout_abs.tv_nsec >= nsec_per_sec) {
            timeout_abs.tv_sec += timeout_abs.tv_nsec / nsec_per_sec;
            timeout_abs.tv_nsec %= nsec_per_sec;
        }

        MUTEX_LOCK(&w->state_lock);
        status = 0;
        while (w->state != state && status == 0) {
            status = pthread_cond_timedwait(&w->state_changed,
                                            &w->state_lock,
                                            &timeout_abs);
        }
        MUTEX_UNLOCK(&w->state_lock);

    } else {
        MUTEX_LOCK(&w->state_lock);
        while (w->state != state) {
            log_verbose(": Waiting for state change, current = %d\n", w->state);
            status = pthread_cond_wait(&w->state_changed,
                                       &w->state_lock);
        }
        MUTEX_UNLOCK(&w->state_lock);
    }

    if (status != 0) {
        log_debug("%s: Wait on state change failed: %s\n",
                   __FUNCTION__, strerror(status));

        if (status == ETIMEDOUT) {
            status = BLADERF_ERR_TIMEOUT;
        } else {
            status = BLADERF_ERR_UNEXPECTED;
        }
    }

    return status;
}

sync_worker_state sync_worker_get_state(struct sync_worker *w,
                                        int *err_code)
{
    sync_worker_state ret;

    MUTEX_LOCK(&w->state_lock);
    ret = w->state;
    if (err_code) {
        *err_code = w->err_code;
        w->err_code = 0;
    }
    MUTEX_UNLOCK(&w->state_lock);

    return ret;
}

static void set_state(struct sync_worker *w, sync_worker_state state)
{
    MUTEX_LOCK(&w->state_lock);
    w->state = state;
    pthread_cond_signal(&w->state_changed);
    MUTEX_UNLOCK(&w->state_lock);
}


static sync_worker_state exec_idle_state(struct bladerf_sync *s)
{
    sync_worker_state next_state = SYNC_WORKER_STATE_IDLE;
    unsigned int requests;
    unsigned int i;

    MUTEX_LOCK(&s->worker->request_lock);

    while (s->worker->requests == 0) {
        log_verbose("%s worker: Waiting for pending requests\n", worker2str(s));

        pthread_cond_wait(&s->worker->requests_pending,
                          &s->worker->request_lock);
    }

    requests = s->worker->requests;
    s->worker->requests = 0;
    MUTEX_UNLOCK(&s->worker->request_lock);

    if (requests & SYNC_WORKER_STOP) {
        log_verbose("%s worker: Got request to stop\n", worker2str(s));

        next_state = SYNC_WORKER_STATE_SHUTTING_DOWN;

    } else if (requests & SYNC_WORKER_START) {
        log_verbose("%s worker: Got request to start\n", worker2str(s));
        MUTEX_LOCK(&s->buf_mgmt.lock);

        if ((s->stream_config.layout & BLADERF_DIRECTION_MASK) == BLADERF_TX) {
            /* If we've previously timed out on a stream, we'll likely have some
            * stale buffers marked "in-flight" that have since been cancelled. */
            for (i = 0; i < s->buf_mgmt.num_buffers; i++) {
                if (s->buf_mgmt.status[i] == SYNC_BUFFER_IN_FLIGHT) {
                    s->buf_mgmt.status[i] = SYNC_BUFFER_EMPTY;
                }
            }

            pthread_cond_signal(&s->buf_mgmt.buf_ready);
        } else {
            s->buf_mgmt.prod_i = s->stream_config.num_xfers;

            for (i = 0; i < s->buf_mgmt.num_buffers; i++) {
                if (i < s->stream_config.num_xfers) {
                    s->buf_mgmt.status[i] = SYNC_BUFFER_IN_FLIGHT;
                } else if (s->buf_mgmt.status[i] == SYNC_BUFFER_IN_FLIGHT) {
                    s->buf_mgmt.status[i] = SYNC_BUFFER_EMPTY;
                }
            }
        }

        MUTEX_UNLOCK(&s->buf_mgmt.lock);

        next_state = SYNC_WORKER_STATE_RUNNING;
    } else {
        log_warning("Invalid request value encountered: 0x%08X\n",
                    s->worker->requests);
    }

    return next_state;
}

static void exec_running_state(struct bladerf_sync *s)
{
    int status;

    status = async_run_stream(s->worker->stream, s->stream_config.layout);

    log_verbose("%s worker: stream ended with: %s\n",
                worker2str(s), bladerf_strerror(status));

    /* Save off the result of running the stream so we can report what
     * happened to the API caller */
    MUTEX_LOCK(&s->worker->state_lock);
    s->worker->err_code = status;
    MUTEX_UNLOCK(&s->worker->state_lock);

    /* Wake the API-side if an error occurred, so that it can propagate
     * the stream error code back to the API caller */
    if (status != 0) {
        MUTEX_LOCK(&s->buf_mgmt.lock);
        pthread_cond_signal(&s->buf_mgmt.buf_ready);
        MUTEX_UNLOCK(&s->buf_mgmt.lock);
    }
}

void *sync_worker_task(void *arg)
{
    sync_worker_state state = SYNC_WORKER_STATE_IDLE;
    struct bladerf_sync *s = (struct bladerf_sync *)arg;

    log_verbose("%s worker: task started\n", worker2str(s));
    set_state(s->worker, state);
    log_verbose("%s worker: task state set\n", worker2str(s));

    while (state != SYNC_WORKER_STATE_STOPPED) {

        switch (state) {
            case SYNC_WORKER_STATE_STARTUP:
                assert(!"Worker in unexpected state, shutting down. (STARTUP)");
                set_state(s->worker, SYNC_WORKER_STATE_SHUTTING_DOWN);
                break;

            case SYNC_WORKER_STATE_IDLE:
                state = exec_idle_state(s);
                set_state(s->worker, state);
                break;

            case SYNC_WORKER_STATE_RUNNING:
                exec_running_state(s);
                state = SYNC_WORKER_STATE_IDLE;
                set_state(s->worker, state);
                break;

            case SYNC_WORKER_STATE_SHUTTING_DOWN:
                log_verbose("%s worker: Shutting down...\n", worker2str(s));

                state = SYNC_WORKER_STATE_STOPPED;
                set_state(s->worker, state);
                break;

            case SYNC_WORKER_STATE_STOPPED:
                assert(!"Worker in unexpected state: STOPPED");
                break;

            default:
                assert(!"Worker in unexpected state, shutting down. (UNKNOWN)");
                set_state(s->worker, SYNC_WORKER_STATE_SHUTTING_DOWN);
                break;
        }
    }

    return NULL;
}
