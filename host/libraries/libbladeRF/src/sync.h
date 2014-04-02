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

#if !defined(BLADERF_SYNC_H_) && defined(ENABLE_LIBBLADERF_SYNC)
#define BLADERF_SYNC_H_

#include <pthread.h>
#include <libbladeRF.h>

#define MODULE_STR(s) module2str(s->stream_config.module)

/* These parameters are only written during sync_init */
struct stream_config
{
    bladerf_format format;
    bladerf_module module;

    unsigned int samples_per_buffer;
    unsigned int num_xfers;
    unsigned int timeout_ms;

    size_t bytes_per_sample;
};

typedef enum {
    SYNC_BUFFER_EMPTY = 0,      /**< Buffer contains no data */
    SYNC_BUFFER_PARTIAL,        /**< sync_rx/tx is currently emptying/filling */
    SYNC_BUFFER_FULL,           /**< Buffer is full of data */
    SYNC_BUFFER_IN_FLIGHT,      /**< Currently being transferred */
} sync_buffer_status;

struct buffer_mgmt {
    sync_buffer_status *status;

    void **buffers;
    unsigned int num_buffers;

    unsigned int prod_i;        /**< Producer index - next buffer to fill */
    unsigned int cons_i;        /**< Consumer index - next buffer to empty */
    unsigned int partial_off;   /**< Current index into partial buffer */

    pthread_mutex_t lock;
    pthread_cond_t  buf_ready;  /** Buffer produced by RX callback, or
                                 *  buffer emptied by TX callback */
};

/* State of API-side sync interface */
typedef enum {
    SYNC_STATE_CHECK_WORKER,
    SYNC_STATE_RESET_BUF_MGMT,
    SYNC_STATE_START_WORKER,
    SYNC_STATE_WAIT_FOR_BUFFER,
    SYNC_STATE_BUFFER_READY,
    SYNC_STATE_USING_BUFFER
} sync_state;

struct bladerf_sync {
    struct bladerf *dev;
    sync_state state;
    struct buffer_mgmt buf_mgmt;
    struct stream_config stream_config;
    struct sync_worker *worker;
};

/**
 * Create and initialize as synchronous interface handle for the specified
 * device and module. If the synchronous handle is already initialized, this
 * call will first deinitialize it.
 *
 * The associated stream will be started at the first RX or TX call
 *
 * @return 0 or BLADERF_ERR_* value on failure
 */
int sync_init(struct bladerf *dev,
              bladerf_module module,
              bladerf_format format,
              unsigned int num_buffers,
              unsigned int buffer_size,
              unsigned int num_transfers,
              unsigned int stream_timeout);

/**
 * Deinitialize the sync handle. This tears down and deallocates the underlying
 * asynchronous stream.
 *
 * @param   sync    Handle to deinitialize.
 */
void sync_deinit(struct bladerf_sync *sync);


int sync_rx(struct bladerf *dev, void *samples, unsigned int num_samples,
             struct bladerf_metadata *metadata, unsigned int timeout_ms);

int sync_tx(struct bladerf *dev, void *samples, unsigned int num_samples,
             struct bladerf_metadata *metadata, unsigned int timeout_ms);

unsigned int sync_buf2idx(struct buffer_mgmt *b, void *addr);

void * sync_idx2buf(struct buffer_mgmt *b, unsigned int idx);

#endif
