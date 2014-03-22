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
#if !defined(BLADERF_SYNC_WORKER_H_) && defined(ENABLE_LIBBLADERF_SYNC)
#define BLADERF_SYNC_WORKER_H_

#include "host_config.h"
#include <pthread.h>
#include <libbladeRF.h>
#include "sync.h"

#if BLADERF_OS_WINDOWS || BLADERF_OS_OSX
#include "clock_gettime.h"
#else
#include <time.h>
#endif

/* Worker lifetime:
 *
 * STARTUP --+--> IDLE --> RUNNING --+--> SHUTTING_DOWN --> STOPPED
 *           ^----------------------/
 */

/* Request flags */
#define SYNC_WORKER_START    (1 << 0)
#define SYNC_WORKER_STOP     (1 << 1)

typedef enum {
    SYNC_WORKER_STATE_STARTUP,
    SYNC_WORKER_STATE_IDLE,
    SYNC_WORKER_STATE_RUNNING,
    SYNC_WORKER_STATE_SHUTTING_DOWN,
    SYNC_WORKER_STATE_STOPPED
} sync_worker_state;

struct sync_worker {
    pthread_t thread;

    struct bladerf_stream *stream;
    bladerf_stream_cb cb;

    sync_worker_state state;
    pthread_mutex_t state_lock;
    pthread_cond_t state_changed;   /* Worker thread uses this to inform a
                                     * waiting main thread about a state
                                     * change */

    /* The requests lock should always be acquired AFTER
     * the sync->buf_mgmt.lock
     */
    unsigned int requests;
    pthread_cond_t requests_pending;
    pthread_mutex_t request_lock;
};

/**
 * Create a launch a worker thread. It will enter the IDLE state upon
 * executing.
 *
 * @param[in]   s   Sync handle containing worker to initialize
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int sync_worker_init(struct bladerf_sync *s);

/**
 * Shutdown and deinitialize
 *
 * @param[in]   w       Worker to deinitialize
 * @param[in]   lock    Acquired to signal `cond` if non-NULL
 * @param[in]   cond    If non-NULL, this is signaled after requesting the
 *                      worker to shut down, waking a potentially blocked
 *                      workers.
 */
void sync_worker_deinit(struct sync_worker *w,
                        pthread_mutex_t *lock, pthread_cond_t *cond);

/**
 * Wait for state change with optional timeout
 *
 * @param   w               Worker to wait for
 * @param   state           State to wait for
 * @param   timeout_ms      Timeout in ms. 0 implies "wait forever"
 *
 * @return  0 on success,
 *          BLADERF_ERR_TIMEOUT on timeout,
 *          BLADERF_ERR_UNKNOWN on other errors
 */
int sync_worker_wait_for_state(struct sync_worker *w,
                               sync_worker_state state,
                               unsigned int timeout_ms);

/**
 * Get the worker's current state.
 *
 * @param   w       Worker to query
 *
 * @return Worker's current state
 */
sync_worker_state sync_worker_get_state(struct sync_worker *w);

/**
 * Submit a request to the worker task
 *
 * @param   w               Worker to send request to
 * @param   request         Bitmask of requests to submit
 */
void sync_worker_submit_request(struct sync_worker *w, unsigned int request);

#endif
