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
#include "bladerf_priv.h"
#include "sync.h"
#include "sync_worker.h"
#include "conversions.h"

void *sync_worker_task(void *arg);

static void *rx_callback(struct bladerf *dev,
                         struct bladerf_stream *stream,
                         struct bladerf_metadata *meta,
                         void *samples,
                         size_t num_samples,
                         void *user_data)
{
    int status = 0;
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
    pthread_mutex_lock(&w->request_lock);
    requests = w->requests;
    pthread_mutex_unlock(&w->request_lock);

    if (requests & SYNC_WORKER_STOP) {
        log_verbose("%s worker: Got STOP request upon entering callback. "
                    "Ending stream.\n", MODULE_STR(s));
        return NULL;
    }

    pthread_mutex_lock(&b->lock);

    /* Mark this buffer as full */
    samples_idx = sync_buf2idx(b, samples);
    assert(b->status[samples_idx] == SYNC_BUFFER_IN_FLIGHT);
    b->status[samples_idx] = SYNC_BUFFER_FULL;

    /* Update to our next buffer */
    next_idx = b->prod_i;
    next_buf = b->buffers[next_idx];

    /* Wait for a buffer to free up, if needed */
    if (s->stream_config.timeout_ms == 0) {
        while (status == 0 &&
                b->status[next_idx] != SYNC_BUFFER_EMPTY &&
                (requests & SYNC_WORKER_STOP) == 0) {

            log_verbose("%s worker: Infinite wait for buf[%u] to empty\n",
                    MODULE_STR(s), next_idx);

            status = pthread_cond_wait(&b->buf_consumed, &b->lock);

            /* Fetch requests that we might have gotten while sleeping */
            pthread_mutex_lock(&w->request_lock);
            requests = w->requests;
            pthread_mutex_unlock(&w->request_lock);
        }
    } else {
        struct timespec timeout_abs;
        status = populate_abs_timeout(&timeout_abs,
                s->stream_config.timeout_ms);

        while (status == 0 &&
               b->status[next_idx] != SYNC_BUFFER_EMPTY &&
               (requests & SYNC_WORKER_STOP) == 0) {

            log_verbose("%s worker: Timed wait for buf[%u] to empty\n",
                    MODULE_STR(s), next_idx);

            status = pthread_cond_timedwait(&b->buf_consumed, &b->lock,
                    &timeout_abs);

            pthread_mutex_lock(&w->request_lock);
            requests = w->requests;
            pthread_mutex_unlock(&w->request_lock);
        }
    }

    if (requests & SYNC_WORKER_STOP) {
        next_buf = NULL;
        log_verbose("%s worker: Got STOP request while waiting in callback. "
                    "Ending stream.\n", MODULE_STR(s));
    } else if (status == 0) {
        b->status[next_idx] = SYNC_BUFFER_IN_FLIGHT;
        b->prod_i = (next_idx + 1) % b->num_buffers;

        pthread_cond_signal(&b->buf_produced);
    }

    pthread_mutex_unlock(&b->lock);

    if (status == 0) {
        log_verbose("%s worker: buf[%u] = full, buf[%u] = in_flight\n",
                    MODULE_STR(s), samples_idx, next_idx);
    } else {
        next_buf = NULL;
        if (status == ETIMEDOUT) {
            log_debug("%s worker: Timed out. Shutting down.\n",
                      MODULE_STR(s), status);
        } else {
            log_debug("%s worker: Unexpected error (%d), shutting down.\n",
                      MODULE_STR(s), status);
        }

        /* The API caller may be blocked waiting on us to produce a buffer,
         * so we need to wake them and let them figure out what to do */
        pthread_mutex_lock(&b->lock);
        pthread_cond_signal(&b->buf_produced);
        pthread_mutex_unlock(&b->lock);
    }

    return next_buf;
}

static void *tx_callback(struct bladerf *dev,
                         struct bladerf_stream *stream,
                         struct bladerf_metadata *meta,
                         void *samples,
                         size_t num_samples,
                         void *user_data)
{
    int status = 0;
    unsigned int requests;      /* Pending requests */
    unsigned int completed_idx; /* Index of completed buffer */
    unsigned int next_idx;      /* Index of next buffer to submit */
    void *next_buf = NULL;      /* Next buffer to submit for transmission */

    struct bladerf_sync *s = (struct bladerf_sync *)user_data;
    struct sync_worker  *w = s->worker;
    struct buffer_mgmt  *b = &s->buf_mgmt;

    /* Check if the caller has requested us to shut down. We'll keep the
     * SHUTDOWN bit set through our transition into the IDLE state so we
     * can act on it there. */
    pthread_mutex_lock(&w->request_lock);
    requests = w->requests;
    pthread_mutex_unlock(&w->request_lock);

    if (requests & SYNC_WORKER_STOP) {
        log_verbose("%s worker: Got STOP request upon entering callback. "
                    "Ending stream.\n", MODULE_STR(s));
        return NULL;
    }

    pthread_mutex_lock(&b->lock);

    /* Mark the last transfer as being completed. Note that the first
     * callbacks we get have samples=NULL */
    if (samples != NULL) {
        completed_idx = sync_buf2idx(b, samples);
        assert(b->status[completed_idx] == SYNC_BUFFER_IN_FLIGHT);
        b->status[completed_idx] = SYNC_BUFFER_EMPTY;
        pthread_cond_signal(&b->buf_consumed);
    } else {
        completed_idx = UINT_MAX;   /* Just for debug purposes */
    }

    /* Wait for a full buffer to become available for consumption */
    if (s->stream_config.timeout_ms == 0) {
        while (status == 0 &&
               b->status[b->cons_i] != SYNC_BUFFER_FULL &&
               (requests & SYNC_WORKER_STOP) == 0) {

            log_verbose("%s worker: Infinite wait for buf[%u] to fill\n",
                    MODULE_STR(s), b->cons_i);

            status = pthread_cond_wait(&b->buf_produced, &b->lock);

            /* Fetch requests that we might have gotten while sleeping */
            pthread_mutex_lock(&w->request_lock);
            requests = w->requests;
            pthread_mutex_unlock(&w->request_lock);

            log_verbose("%s worker: Woke, requests are now: 0x%08x\n",
                        MODULE_STR(s), requests);
        }
    } else {
        struct timespec timeout_abs;
        status = populate_abs_timeout(&timeout_abs,
                s->stream_config.timeout_ms);

        while (status == 0 &&
               b->status[b->cons_i] != SYNC_BUFFER_FULL &&
               (requests & SYNC_WORKER_STOP) == 0) {

            log_verbose("%s worker: Timed wait for buf[%u] to fill\n",
                    MODULE_STR(s), b->cons_i);

            status = pthread_cond_timedwait(&b->buf_produced, &b->lock,
                    &timeout_abs);

            pthread_mutex_lock(&w->request_lock);
            requests = w->requests;
            pthread_mutex_unlock(&w->request_lock);
        }
    }

    /* Mark this buffer as being sent and update our reference to the
     * next buffer we'll use */
    if (status == 0) {
        next_idx = b->cons_i;
        b->status[next_idx] = SYNC_BUFFER_IN_FLIGHT;
        next_buf = b->buffers[next_idx];
        b->cons_i = (b->cons_i + 1) % b->num_buffers;
    }

    pthread_mutex_unlock(&b->lock);

    if (requests & SYNC_WORKER_STOP) {
        next_buf = NULL;
        log_verbose("%s worker: Got STOP request while waiting in callback. "
                    "Ending stream.\n", MODULE_STR(s));
    } else if (status == 0) {
        log_verbose("%s worker: buf[%u] = in_flight, buf[%u] = empty\n",
                    MODULE_STR(s), next_idx, completed_idx);
    } else {
        next_buf = NULL;

        if (status == ETIMEDOUT) {
            log_debug("%s worker: Timed out. Shutting down.\n",
                      MODULE_STR(s), status);
        } else {
            log_debug("%s worker: Unexpected error (%d), shutting down.\n",
                      MODULE_STR(s), status);
        }

        /* The API caller may be blocked waiting on us to consume a buffer,
         * so we need to wake them and let them figure out what to do */
        pthread_mutex_lock(&b->lock);
        pthread_cond_signal(&b->buf_consumed);
        pthread_mutex_unlock(&b->lock);
    }

    return next_buf;
}

int sync_worker_init(struct bladerf_sync *s)
{
    int status = 0;
    s->worker = (struct sync_worker*) calloc(1, sizeof(*s->worker));

    if (s->worker == NULL) {
        status = BLADERF_ERR_MEM;
        goto worker_init_out;
    }

    s->worker->state = SYNC_WORKER_STATE_STARTUP;
    s->worker->cb = s->stream_config.module == BLADERF_MODULE_RX ?
                        rx_callback : tx_callback;

    status = bladerf_init_stream(&s->worker->stream,
                                 s->dev,
                                 s->worker->cb,
                                 &s->buf_mgmt.buffers,
                                 s->buf_mgmt.num_buffers,
                                 s->stream_config.format,
                                 s->stream_config.samples_per_buffer,
                                 s->stream_config.num_xfers,
                                 s);

    if (status != 0) {
        log_debug("%s worker: Failed to init stream: %s\n", MODULE_STR(s),
                  bladerf_strerror(status));
        goto worker_init_out;
    }


    status = pthread_mutex_init(&s->worker->state_lock, NULL);
    if (status != 0) {
        status = BLADERF_ERR_UNEXPECTED;
        goto worker_init_out;
    }

    status = pthread_cond_init(&s->worker->state_changed, NULL);
    if (status != 0) {
        status = BLADERF_ERR_UNEXPECTED;
        goto worker_init_out;
    }

    status = pthread_mutex_init(&s->worker->request_lock, NULL);
    if (status != 0) {
        status = BLADERF_ERR_UNEXPECTED;
        goto worker_init_out;
    }

    status = pthread_cond_init(&s->worker->requests_pending, NULL);
    if (status != 0) {
        status = BLADERF_ERR_UNEXPECTED;
        goto worker_init_out;
    }

    status = pthread_create(&s->worker->thread, NULL, sync_worker_task, s);
    if (status != 0) {
        status = BLADERF_ERR_UNEXPECTED;
        goto worker_init_out;
    }

    /* Wait until the worker thread has initialized and is ready to go */
    status = sync_worker_wait_for_state(s->worker, SYNC_WORKER_STATE_IDLE, 1000);
    if (status != 0) {
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
        pthread_mutex_lock(lock);
        pthread_cond_signal(cond);
        pthread_mutex_unlock(lock);
    }

    status = sync_worker_wait_for_state(w, SYNC_WORKER_STATE_STOPPED, 3000);

    if (status != 0) {
        log_warning("Timed out while stopping worker. Canceling thread.\n");
        pthread_cancel(w->thread);
    }

    pthread_join(w->thread, NULL);
    log_verbose("%s: Worker joined.\n", __FUNCTION__);

    bladerf_deinit_stream(w->stream);

    free(w);
}

void sync_worker_submit_request(struct sync_worker *w, unsigned int request)
{
    pthread_mutex_lock(&w->request_lock);
    w->requests |= request;
    pthread_cond_signal(&w->requests_pending);
    pthread_mutex_unlock(&w->request_lock);
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

        pthread_mutex_lock(&w->state_lock);
        status = 0;
        while (w->state != state && status == 0) {
            status = pthread_cond_timedwait(&w->state_changed,
                                            &w->state_lock,
                                            &timeout_abs);
        }
        pthread_mutex_unlock(&w->state_lock);

    } else {
        pthread_mutex_lock(&w->state_lock);
        while (w->state != state) {
            log_verbose(": Waiting for state change, current = %d\n", w->state);
            status = pthread_cond_wait(&w->state_changed,
                                       &w->state_lock);
        }
        pthread_mutex_unlock(&w->state_lock);
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

sync_worker_state sync_worker_get_state(struct sync_worker *w)
{
    sync_worker_state ret;

    pthread_mutex_lock(&w->state_lock);
    ret = w->state;
    pthread_mutex_unlock(&w->state_lock);

    return ret;
}

static void set_state(struct sync_worker *w, sync_worker_state state)
{
    pthread_mutex_lock(&w->state_lock);
    w->state = state;
    pthread_cond_signal(&w->state_changed);
    pthread_mutex_unlock(&w->state_lock);
}


static sync_worker_state exec_idle_state(struct bladerf_sync *s)
{
    sync_worker_state next_state = SYNC_WORKER_STATE_IDLE;

    pthread_mutex_lock(&s->worker->request_lock);

    while (s->worker->requests == 0) {
        log_verbose("%s worker: Waiting for pending requests\n", MODULE_STR(s));

        pthread_cond_wait(&s->worker->requests_pending,
                          &s->worker->request_lock);
    }

    if (s->worker->requests & SYNC_WORKER_STOP) {
        log_verbose("%s worker: Got request to stop\n",
                module2str(s->stream_config.module));

        next_state = SYNC_WORKER_STATE_SHUTTING_DOWN;

    } else if (s->worker->requests & SYNC_WORKER_START) {
        log_verbose("%s worker: Got request to start\n",
                module2str(s->stream_config.module));

        next_state = SYNC_WORKER_STATE_RUNNING;
    } else {
        log_warning("Invalid request value encountered: 0x%08X\n",
                    s->worker->requests);
    }

    s->worker->requests = 0;
    pthread_mutex_unlock(&s->worker->request_lock);

    return next_state;
}

static void exec_running_state(struct bladerf_sync *s)
{
    int status;

    status = bladerf_stream(s->worker->stream, s->stream_config.module);

    log_verbose("%s worker: stream ended with: %s\n",
                MODULE_STR(s), bladerf_strerror(status));

    /* Suppress warning if log_verbose is disabled */
    (void)status;
}

void *sync_worker_task(void *arg)
{
    sync_worker_state state = SYNC_WORKER_STATE_IDLE;
    struct bladerf_sync *s = (struct bladerf_sync *)arg;

    log_verbose("%s worker: task started\n", MODULE_STR(s));
    set_state(s->worker, state);
    log_verbose("%s worker: task state set\n", MODULE_STR(s));

    while (state != SYNC_WORKER_STATE_STOPPED) {

        switch (state) {
            case SYNC_WORKER_STATE_STARTUP:
                assert(!"Worker in unexepected state, shutting down. (STARTUP)");
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
                log_verbose("%s worker: Shutting down...\n", MODULE_STR(s));

                state = SYNC_WORKER_STATE_STOPPED;
                set_state(s->worker, state);
                break;

            case SYNC_WORKER_STATE_STOPPED:
                assert(!"Worker in unexepected state: STOPPED");
                break;

            default:
                assert(!"Worker in unexepected state, shutting down. (UNKNOWN)");
                set_state(s->worker, SYNC_WORKER_STATE_SHUTTING_DOWN);
                break;
        }
    }

    return NULL;
}
