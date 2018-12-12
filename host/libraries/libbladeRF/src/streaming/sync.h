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

#ifndef STREAMING_SYNC_H_
#define STREAMING_SYNC_H_

#include <limits.h>
#include <pthread.h>

#include <libbladeRF.h>

#include "thread.h"

/* These parameters are only written during sync_init */
struct stream_config {
    bladerf_format format;
    bladerf_channel_layout layout;

    unsigned int samples_per_buffer;
    unsigned int num_xfers;
    unsigned int timeout_ms;

    size_t bytes_per_sample;
};

typedef enum {
    SYNC_BUFFER_EMPTY = 0, /**< Buffer contains no data */
    SYNC_BUFFER_PARTIAL,   /**< sync_rx/tx is currently emptying/filling */
    SYNC_BUFFER_FULL,      /**< Buffer is full of data */
    SYNC_BUFFER_IN_FLIGHT, /**< Currently being transferred */
} sync_buffer_status;

typedef enum {
    SYNC_META_STATE_HEADER,  /**< Extract the metadata header */
    SYNC_META_STATE_SAMPLES, /**< Process samples */
} sync_meta_state;

typedef enum {
    /** Invalid selection */
    SYNC_TX_SUBMITTER_INVALID = -1,

    /** sync_tx() is repsonsible for submitting buffers for async transfer */
    SYNC_TX_SUBMITTER_FN,

    /** The TX worker callbacks should be returning buffers for submission  */
    SYNC_TX_SUBMITTER_CALLBACK
} sync_tx_submitter;

#define BUFFER_MGMT_INVALID_INDEX (UINT_MAX)

struct buffer_mgmt {
    sync_buffer_status *status;

    void **buffers;
    unsigned int num_buffers;

    unsigned int prod_i;      /**< Producer index - next buffer to fill */
    unsigned int cons_i;      /**< Consumer index - next buffer to empty */
    unsigned int partial_off; /**< Current index into partial buffer */

    /* In the event of a SW RX overrun, this count is used to determine
     * how many more transfers should be considered invalid and require
     * resubmission */
    unsigned int resubmit_count;

    /* Applicable to TX only. Denotes which context is responsible for
     * submitting full buffers to the underlying async system */
    sync_tx_submitter submitter;


    MUTEX lock;
    pthread_cond_t buf_ready; /**< Buffer produced by RX callback, or
                               *   buffer emptied by TX callback */
};

/* State of API-side sync interface */
typedef enum {
    SYNC_STATE_CHECK_WORKER,
    SYNC_STATE_RESET_BUF_MGMT,
    SYNC_STATE_START_WORKER,
    SYNC_STATE_WAIT_FOR_BUFFER,
    SYNC_STATE_BUFFER_READY,
    SYNC_STATE_USING_BUFFER,
    SYNC_STATE_USING_BUFFER_META
} sync_state;

struct sync_meta {
    sync_meta_state state; /* State of metadata processing */

    uint8_t *curr_msg;            /* Points to current message in the buffer */
    size_t curr_msg_off;          /* Offset into current message (samples),
                                   * ignoring the 4-samples worth of metadata */
    size_t msg_size;              /* Size of data message */
    unsigned int msg_per_buf;     /* Number of data messages per buffer */
    unsigned int msg_num;         /* Which message within the buffer are we in?
                                   * Range is: 0 to msg_per_buf   */
    unsigned int samples_per_msg; /* Number of samples within a message */

    union {
        /* Used only for RX */
        struct {
            uint64_t
                msg_timestamp;  /* Timestamp contained in the current message */
            uint32_t msg_flags; /* Flags for the current message */
        };

        /* Used only for TX */
        struct {
            bool in_burst;
            bool now;
        };
    };

    uint64_t curr_timestamp; /* Timestamp at the sample we've
                              * consumed up to */
};

struct bladerf_sync {
    MUTEX lock;
    struct bladerf *dev;
    bool initialized;
    sync_state state;
    struct buffer_mgmt buf_mgmt;
    struct stream_config stream_config;
    struct sync_worker *worker;
    struct sync_meta meta;
};

/**
 * Create and initialize as synchronous interface handle for the specified
 * device and direction. If the synchronous handle is already initialized, this
 * call will first deinitialize it.
 *
 * The associated stream will be started at the first RX or TX call
 *
 * @return 0 or BLADERF_ERR_* value on failure
 */
int sync_init(struct bladerf_sync *sync,
              struct bladerf *dev,
              bladerf_channel_layout layout,
              bladerf_format format,
              unsigned int num_buffers,
              size_t buffer_size,
              size_t msg_size,
              unsigned int num_transfers,
              unsigned int stream_timeout);

/**
 * Deinitialize the sync handle. This tears down and deallocates the underlying
 * asynchronous stream.
 *
 * @param[inout]    sync    Handle to deinitialize.
 */
void sync_deinit(struct bladerf_sync *sync);

int sync_rx(struct bladerf_sync *sync,
            void *samples,
            unsigned int num_samples,
            struct bladerf_metadata *metadata,
            unsigned int timeout_ms);

int sync_tx(struct bladerf_sync *sync,
            void const *samples,
            unsigned int num_samples,
            struct bladerf_metadata *metadata,
            unsigned int timeout_ms);

unsigned int sync_buf2idx(struct buffer_mgmt *b, void *addr);

void *sync_idx2buf(struct buffer_mgmt *b, unsigned int idx);

#endif
