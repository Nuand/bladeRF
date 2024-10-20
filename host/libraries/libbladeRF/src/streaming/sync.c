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

#include <string.h>
#include <errno.h>
#include <inttypes.h>

/* Only switch on the verbose debug prints in this file when we *really* want
 * them. Otherwise, compile them out to avoid excessive log level checks
 * in our data path */
#include "log.h"
#ifndef ENABLE_LIBBLADERF_SYNC_LOG_VERBOSE
#undef log_verbose
#define log_verbose(...)
#endif
#include "minmax.h"
#include "rel_assert.h"

#include "async.h"
#include "sync.h"
#include "sync_worker.h"
#include "metadata.h"

#include "board/board.h"
#include "helpers/timeout.h"
#include "helpers/have_cap.h"

#ifdef ENABLE_LIBBLADERF_SYNC_LOG_VERBOSE
static inline void dump_buf_states(struct bladerf_sync *s)
{
    static char *out      = NULL;
    struct buffer_mgmt *b = &s->buf_mgmt;
    char *statestr        = "UNKNOWN";

    if (out == NULL) {
        out = malloc((b->num_buffers + 1) * sizeof(char));
    }

    if (out == NULL) {
        log_verbose("%s: malloc failed\n");
        return;
    }

    out[b->num_buffers] = '\0';

    for (size_t i = 0; i < b->num_buffers; ++i) {
        switch (b->status[i]) {
            case SYNC_BUFFER_EMPTY:
                out[i] = '_';
                break;
            case SYNC_BUFFER_IN_FLIGHT:
                out[i] = '-';
                break;
            case SYNC_BUFFER_FULL:
                out[i] = '*';
                break;
            case SYNC_BUFFER_PARTIAL:
                out[i] = 'o';
                break;
        }
    }

    switch (s->state) {
        case SYNC_STATE_BUFFER_READY:
            statestr = "BUFFER_READY";
            break;
        case SYNC_STATE_CHECK_WORKER:
            statestr = "CHECK_WORKER";
            break;
        case SYNC_STATE_RESET_BUF_MGMT:
            statestr = "RESET_BUF_MGMT";
            break;
        case SYNC_STATE_START_WORKER:
            statestr = "START_WORKER";
            break;
        case SYNC_STATE_USING_BUFFER:
            statestr = "USING_BUFFER";
            break;
        case SYNC_STATE_USING_BUFFER_META:
            statestr = "USING_BUFFER_META";
            break;
        case SYNC_STATE_USING_PACKET_META:
            statestr = "USING_PACKET_META";
            break;
        case SYNC_STATE_WAIT_FOR_BUFFER:
            statestr = "WAIT_FOR_BUFFER";
            break;
    }

    log_verbose("%s: %s (%s)\n", __FUNCTION__, out, statestr);
}
#else
#define dump_buf_states(...)
#endif  // ENABLE_LIBBLADERF_SYNC_LOG_VERBOSE

static inline size_t samples2bytes(struct bladerf_sync *s, size_t n) {
    return s->stream_config.bytes_per_sample * n;
}

static inline unsigned int msg_per_buf(size_t msg_size, size_t buf_size,
                                       size_t bytes_per_sample)
{
    size_t n = buf_size / (msg_size / bytes_per_sample);
    assert(n <= UINT_MAX);
    return (unsigned int) n;
}

static inline unsigned int samples_per_msg(size_t msg_size,
                                           size_t bytes_per_sample)
{
    size_t n = (msg_size - METADATA_HEADER_SIZE) / bytes_per_sample;
    assert(n <= UINT_MAX);
    return (unsigned int) n;
}

int sync_init(struct bladerf_sync *sync,
              struct bladerf *dev,
              bladerf_channel_layout layout,
              bladerf_format format,
              unsigned int num_buffers,
              size_t buffer_size,
              size_t msg_size,
              unsigned int num_transfers,
              unsigned int stream_timeout)

{
    int status = 0;
    size_t i, bytes_per_sample;

    if (num_transfers >= num_buffers) {
        return BLADERF_ERR_INVAL;
    }

    if (format == BLADERF_FORMAT_PACKET_META) {
        if (!have_cap_dev(dev, BLADERF_CAP_FW_SHORT_PACKET)) {
            log_error("Firmware does not support short packets. "
                    "Upgrade to at least firmware version 2.4.0.\n");
            return BLADERF_ERR_UNSUPPORTED;
        }

        if (!have_cap_dev(dev, BLADERF_CAP_FPGA_PACKET_META)) {
            log_error("FPGA does not support packet meta format. "
                    "Upgrade to at least FPGA version 0.12.0.\n");
            return BLADERF_ERR_UNSUPPORTED;
        }
    }

    if (format == BLADERF_FORMAT_SC8_Q7 || format == BLADERF_FORMAT_SC8_Q7_META) {
        if (!have_cap_dev(dev, BLADERF_CAP_FPGA_8BIT_SAMPLES)) {
            log_error("FPGA does not support 8bit mode. "
                      "Upgrade to at least FPGA version 0.15.0.\n");
            return BLADERF_ERR_UNSUPPORTED;
        }
    }

    switch (format) {
        case BLADERF_FORMAT_SC8_Q7:
        case BLADERF_FORMAT_SC8_Q7_META:
            bytes_per_sample = 2;
            break;

        case BLADERF_FORMAT_SC16_Q11:
        case BLADERF_FORMAT_SC16_Q11_META:
        case BLADERF_FORMAT_PACKET_META:
            bytes_per_sample = 4;
            break;

        default:
            log_debug("Invalid format value: %d\n", format);
            return BLADERF_ERR_INVAL;
    }

    /* bladeRF GPIF DMA requirement */
    if ((bytes_per_sample * buffer_size) % 4096 != 0) {
        assert(!"Invalid buffer size");
        return BLADERF_ERR_INVAL;
    }

    /* Deinitialize sync handle if it's initialized */
    sync_deinit(sync);

    MUTEX_INIT(&sync->lock);

    switch (layout & BLADERF_DIRECTION_MASK) {
        case BLADERF_TX:
            sync->buf_mgmt.submitter = SYNC_TX_SUBMITTER_FN;
            break;
        case BLADERF_RX:
            sync->buf_mgmt.submitter = SYNC_TX_SUBMITTER_INVALID;
            break;
    }

    sync->dev = dev;
    sync->state = SYNC_STATE_CHECK_WORKER;

    sync->buf_mgmt.num_buffers = num_buffers;
    sync->buf_mgmt.resubmit_count = 0;

    sync->stream_config.layout = layout;
    sync->stream_config.format = format;
    sync->stream_config.samples_per_buffer = (unsigned int)buffer_size;
    sync->stream_config.num_xfers = num_transfers;
    sync->stream_config.timeout_ms = stream_timeout;
    sync->stream_config.bytes_per_sample = bytes_per_sample;

    sync->meta.state = SYNC_META_STATE_HEADER;
    sync->meta.msg_size = msg_size;
    sync->meta.msg_per_buf = msg_per_buf(msg_size, buffer_size, bytes_per_sample);
    sync->meta.samples_per_msg = samples_per_msg(msg_size, bytes_per_sample);
    sync->meta.samples_per_ts = (layout == BLADERF_RX_X2 || layout == BLADERF_TX_X2) ? 2:1;

    log_verbose("%s: Buffer size (in bytes): %u\n",
                __FUNCTION__, buffer_size * bytes_per_sample);

    log_verbose("%s: Buffer size (in samples): %u\n",
                __FUNCTION__, buffer_size);

    log_verbose("%s: Msg per buffer: %u\n",
                __FUNCTION__, sync->meta.msg_per_buf);

    log_verbose("%s: Samples per msg: %u\n",
                __FUNCTION__, sync->meta.samples_per_msg);

    MUTEX_INIT(&sync->buf_mgmt.lock);
    pthread_cond_init(&sync->buf_mgmt.buf_ready, NULL);

    sync->buf_mgmt.status = (sync_buffer_status*) malloc(num_buffers * sizeof(sync_buffer_status));
    if (sync->buf_mgmt.status == NULL) {
        status = BLADERF_ERR_MEM;
        goto error;
    }

    sync->buf_mgmt.actual_lengths = (size_t *) malloc(num_buffers * sizeof(size_t));
    if (sync->buf_mgmt.actual_lengths == NULL) {
        status = BLADERF_ERR_MEM;
        goto error;
    }

    switch (layout & BLADERF_DIRECTION_MASK) {
        case BLADERF_RX:
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

            sync->meta.msg_timestamp = 0;
            sync->meta.msg_flags = 0;

            break;

        case BLADERF_TX:
            sync->buf_mgmt.prod_i = 0;
            sync->buf_mgmt.cons_i = BUFFER_MGMT_INVALID_INDEX;
            sync->buf_mgmt.partial_off = 0;

            for (i = 0; i < num_buffers; i++) {
                sync->buf_mgmt.status[i] = SYNC_BUFFER_EMPTY;
            }

            sync->meta.msg_timestamp = 0;
            sync->meta.in_burst = false;
            sync->meta.now = false;
            break;
    }

    status = sync_worker_init(sync);
    if (status < 0) {
        goto error;
    }

    sync->initialized = true;

    return 0;

error:
    sync_deinit(sync);
    return status;
}

void sync_deinit(struct bladerf_sync *sync)
{
    if (sync->initialized) {
        if ((sync->stream_config.layout & BLADERF_DIRECTION_MASK) == BLADERF_TX) {
            async_submit_stream_buffer(sync->worker->stream,
                                       BLADERF_STREAM_SHUTDOWN, NULL, 0, false);
        }

        sync_worker_deinit(sync->worker, &sync->buf_mgmt.lock,
                           &sync->buf_mgmt.buf_ready);

        if (sync->buf_mgmt.actual_lengths) {
            free(sync->buf_mgmt.actual_lengths);
        }
        /* De-allocate our buffer management resources */
        if (sync->buf_mgmt.status) {
            MUTEX_DESTROY(&sync->buf_mgmt.lock);
            free(sync->buf_mgmt.status);
        }

        MUTEX_DESTROY(&sync->lock);

        sync->initialized = false;
    }
}

static int wait_for_buffer(struct buffer_mgmt *b,
                           unsigned int timeout_ms,
                           const char *dbg_name,
                           unsigned int dbg_idx)
{
    int status;
    struct timespec timeout;

    if (timeout_ms == 0) {
        log_verbose("%s: Infinite wait for buffer[%d] (status: %d).\n",
                    dbg_name, dbg_idx, b->status[dbg_idx]);
        status = pthread_cond_wait(&b->buf_ready, &b->lock);
    } else {
        log_verbose("%s: Timed wait for buffer[%d] (status: %d).\n", dbg_name,
                    dbg_idx, b->status[dbg_idx]);
        status = populate_abs_timeout(&timeout, timeout_ms);
        if (status == 0) {
            status = pthread_cond_timedwait(&b->buf_ready, &b->lock, &timeout);
        }
    }

    if (status == ETIMEDOUT) {
        log_error("%s: Timed out waiting for buf_ready after %d ms\n",
                  __FUNCTION__, timeout_ms);
        status = BLADERF_ERR_TIMEOUT;
    } else if (status != 0) {
        status = BLADERF_ERR_UNEXPECTED;
    }

    return status;
}

#ifndef SYNC_WORKER_START_TIMEOUT_MS
#   define SYNC_WORKER_START_TIMEOUT_MS 250
#endif

/* Returns # of timestamps (or time steps) left in a message */
static inline unsigned int ts_remaining(struct bladerf_sync *s)
{
    size_t ret = s->meta.samples_per_msg / s->meta.samples_per_ts - s->meta.curr_msg_off;
    assert(ret <= UINT_MAX);

    return (unsigned int) ret;
}

/* Returns # of samples left in a message (SC16Q11 mode only) */
static inline unsigned int left_in_msg(struct bladerf_sync *s)
{
    size_t ret = s->meta.samples_per_msg - s->meta.curr_msg_off;
    assert(ret <= UINT_MAX);

    return (unsigned int) ret;
}

static inline void advance_rx_buffer(struct buffer_mgmt *b)
{
    log_verbose("%s: Marking buf[%u] empty.\n", __FUNCTION__, b->cons_i);

    b->status[b->cons_i] = SYNC_BUFFER_EMPTY;
    b->cons_i = (b->cons_i + 1) % b->num_buffers;
}

static inline unsigned int timestamp_to_msg(struct bladerf_sync *s, uint64_t t)
{
    uint64_t m =  t / s->meta.samples_per_msg;
    assert(m <= UINT_MAX);
    return (unsigned int) m;
}

int sync_rx(struct bladerf_sync *s, void *samples, unsigned num_samples,
            struct bladerf_metadata *user_meta, unsigned int timeout_ms)
{
    struct buffer_mgmt *b;

    int status = 0;
    bool exit_early = false;
    bool copied_data = false;
    unsigned int samples_returned = 0;
    uint8_t *samples_dest = (uint8_t*)samples;
    uint8_t *buf_src = NULL;
    unsigned int samples_to_copy = 0;
    unsigned int samples_per_buffer = 0;
    uint64_t target_timestamp = UINT64_MAX;
    unsigned int pkt_len_dwords = 0;

    if (s == NULL || samples == NULL) {
        log_debug("NULL pointer passed to %s\n", __FUNCTION__);
        return BLADERF_ERR_INVAL;
    } else if (!s->initialized) {
        return BLADERF_ERR_INVAL;
    }

    if (num_samples % s->meta.samples_per_ts != 0) {
        log_debug("%s: %u samples %% %u channels != 0\n",
                  __FUNCTION__, num_samples, s->meta.samples_per_ts);
        return BLADERF_ERR_INVAL;
    }

    MUTEX_LOCK(&s->lock);

    if (s->stream_config.format == BLADERF_FORMAT_SC16_Q11_META ||
          s->stream_config.format == BLADERF_FORMAT_SC8_Q7_META ||
          s->stream_config.format == BLADERF_FORMAT_PACKET_META) {
        if (user_meta == NULL) {
            log_debug("NULL metadata pointer passed to %s\n", __FUNCTION__);
            status = BLADERF_ERR_INVAL;
            goto out;
        } else {
            user_meta->status = 0;
            target_timestamp = user_meta->timestamp;
        }
    }

    b = &s->buf_mgmt;
    samples_per_buffer = s->stream_config.samples_per_buffer;

    log_verbose("%s: Requests %u samples.\n", __FUNCTION__, num_samples);

    while (!exit_early && samples_returned < num_samples && status == 0) {
        dump_buf_states(s);

        switch (s->state) {
            case SYNC_STATE_CHECK_WORKER: {
                int stream_error;
                sync_worker_state worker_state =
                    sync_worker_get_state(s->worker, &stream_error);

                /* Propagate stream error back to the caller.
                 * They can call this function again to restart the stream and
                 * try again.
                 */
                if (stream_error != 0) {
                    status = stream_error;
                } else {
                    if (worker_state == SYNC_WORKER_STATE_IDLE) {
                        log_debug("%s: Worker is idle. Going to reset buf "
                                  "mgmt.\n", __FUNCTION__);
                        s->state = SYNC_STATE_RESET_BUF_MGMT;
                    } else if (worker_state == SYNC_WORKER_STATE_RUNNING) {
                        s->state = SYNC_STATE_WAIT_FOR_BUFFER;
                    } else {
                        status = BLADERF_ERR_UNEXPECTED;
                        log_debug("%s: Unexpected worker state=%d\n",
                                __FUNCTION__, worker_state);
                    }
                }

                break;
            }

            case SYNC_STATE_RESET_BUF_MGMT:
                MUTEX_LOCK(&b->lock);
                /* When the RX stream starts up, it will submit the first T
                 * transfers, so the consumer index must be reset to 0 */
                b->cons_i = 0;
                MUTEX_UNLOCK(&b->lock);
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
                MUTEX_LOCK(&b->lock);

                /* Check the buffer state, as the worker may have produced one
                 * since we last queried the status */
                if (b->status[b->cons_i] == SYNC_BUFFER_FULL) {
                    s->state = SYNC_STATE_BUFFER_READY;
                    log_verbose("%s: buffer %u is ready to consume\n",
                                __FUNCTION__, b->cons_i);
                } else {
                    status = wait_for_buffer(b, timeout_ms,
                                             __FUNCTION__, b->cons_i);

                    if (status == 0) {
                        if (b->status[b->cons_i] != SYNC_BUFFER_FULL) {
                            s->state = SYNC_STATE_CHECK_WORKER;
                        } else {
                            s->state = SYNC_STATE_BUFFER_READY;
                            log_verbose("%s: buffer %u is ready to consume\n",
                                        __FUNCTION__, b->cons_i);
                        }
                    }
                }

                MUTEX_UNLOCK(&b->lock);
                break;

            case SYNC_STATE_BUFFER_READY:
                MUTEX_LOCK(&b->lock);
                b->status[b->cons_i] = SYNC_BUFFER_PARTIAL;
                b->partial_off = 0;

                switch (s->stream_config.format) {
                    case BLADERF_FORMAT_SC16_Q11:
                    case BLADERF_FORMAT_SC8_Q7:
                        s->state = SYNC_STATE_USING_BUFFER;
                        break;

                    case BLADERF_FORMAT_SC16_Q11_META:
                    case BLADERF_FORMAT_SC8_Q7_META:
                        s->state = SYNC_STATE_USING_BUFFER_META;
                        s->meta.curr_msg_off = 0;
                        s->meta.msg_num = 0;
                        break;

                    case BLADERF_FORMAT_PACKET_META:
                        s->state = SYNC_STATE_USING_PACKET_META;
                        break;

                    default:
                        assert(!"Invalid stream format");
                        status = BLADERF_ERR_UNEXPECTED;
                }

                MUTEX_UNLOCK(&b->lock);
                break;

            case SYNC_STATE_USING_BUFFER: /* SC16Q11 buffers w/o metadata */
                MUTEX_LOCK(&b->lock);

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

                    advance_rx_buffer(b);
                    s->state = SYNC_STATE_WAIT_FOR_BUFFER;
                }

                MUTEX_UNLOCK(&b->lock);
                break;


            case SYNC_STATE_USING_BUFFER_META: /* SC16Q11 buffers w/ metadata */
                MUTEX_LOCK(&b->lock);

                switch (s->meta.state) {
                    case SYNC_META_STATE_HEADER:

                        assert(s->meta.msg_num < s->meta.msg_per_buf);

                        buf_src = (uint8_t*)b->buffers[b->cons_i];

                        s->meta.curr_msg =
                            buf_src + s->meta.msg_size * s->meta.msg_num;

                        s->meta.msg_timestamp =
                            metadata_get_timestamp(s->meta.curr_msg);

                        s->meta.msg_flags =
                            metadata_get_flags(s->meta.curr_msg);

                        user_meta->status |= s->meta.msg_flags &
                           (BLADERF_META_FLAG_RX_HW_UNDERFLOW |
                              BLADERF_META_FLAG_RX_HW_MINIEXP1 |
                              BLADERF_META_FLAG_RX_HW_MINIEXP2);

                        s->meta.curr_msg_off = 0;

                        /* We've encountered a discontinuity and need to return
                         * what we have so far, setting the status flags */
                        if (copied_data &&
                            s->meta.msg_timestamp != s->meta.curr_timestamp) {

                            user_meta->status |= BLADERF_META_STATUS_OVERRUN;
                            exit_early = true;
                            log_debug("Sample discontinuity detected @ "
                                      "buffer %u, message %u: Expected t=%llu, "
                                      "got t=%llu\n",
                                      b->cons_i, s->meta.msg_num,
                                      (unsigned long long)s->meta.curr_timestamp,
                                      (unsigned long long)s->meta.msg_timestamp);

                        } else {
                            log_verbose("Got header for message %u: "
                                        "t_new=%u, t_old=%u\n",
                                        s->meta.msg_num,
                                        s->meta.msg_timestamp,
                                        s->meta.curr_timestamp);
                        }

                        s->meta.curr_timestamp = s->meta.msg_timestamp;
                        s->meta.state = SYNC_META_STATE_SAMPLES;
                        break;

                    case SYNC_META_STATE_SAMPLES:
                        if (!copied_data &&
                            (user_meta->flags & BLADERF_META_FLAG_RX_NOW) == 0 &&
                            target_timestamp < s->meta.curr_timestamp) {

                            log_debug("Current timestamp is %llu, "
                                      "target=%llu (user=%llu)\n",
                                      (unsigned long long)s->meta.curr_timestamp,
                                      (unsigned long long)target_timestamp,
                                      (unsigned long long)user_meta->timestamp);

                            status = BLADERF_ERR_TIME_PAST;
                        } else if ((user_meta->flags & BLADERF_META_FLAG_RX_NOW) ||
                                   target_timestamp == s->meta.curr_timestamp) {

                            /* Copy the request amount up to the end of a
                             * this message in the current buffer */
                            samples_to_copy =
                                uint_min(num_samples - samples_returned,
                                         left_in_msg(s));

                            memcpy(samples_dest + samples2bytes(s, samples_returned),
                                   s->meta.curr_msg +
                                        METADATA_HEADER_SIZE +
                                        samples2bytes(s, s->meta.curr_msg_off),
                                   samples2bytes(s, samples_to_copy));

                            samples_returned += samples_to_copy;
                            s->meta.curr_msg_off += samples_to_copy;

                            if (!copied_data &&
                                (user_meta->flags & BLADERF_META_FLAG_RX_NOW)) {

                                /* Provide the user with the timestamp at the
                                 * first returned sample when the
                                 * NOW flag has been provided */
                                user_meta->timestamp = s->meta.curr_timestamp;
                                log_verbose("Updated user meta timestamp with: "
                                            "%llu\n", (unsigned long long)
                                            user_meta->timestamp);
                            }

                            copied_data = true;

                            s->meta.curr_timestamp += samples_to_copy / s->meta.samples_per_ts;

                            /* We've begun copying samples, so our target will
                             * just keep tracking the current timestamp. */
                            target_timestamp = s->meta.curr_timestamp;

                            log_verbose("After copying samples, t=%llu\n",
                                        (unsigned long long)s->meta.curr_timestamp);

                            if (left_in_msg(s) == 0) {
                                assert(s->meta.curr_msg_off == s->meta.samples_per_msg);

                                s->meta.state = SYNC_META_STATE_HEADER;
                                s->meta.msg_num++;

                                if (s->meta.msg_num >= s->meta.msg_per_buf) {
                                    assert(s->meta.msg_num == s->meta.msg_per_buf);
                                    advance_rx_buffer(b);
                                    s->meta.msg_num = 0;
                                    s->state = SYNC_STATE_WAIT_FOR_BUFFER;
                                }
                            }

                        } else {
                            const uint64_t time_delta = target_timestamp - s->meta.curr_timestamp;
                            uint64_t samples_left = time_delta * s->meta.samples_per_ts;
                            uint64_t left_in_buffer =
                                (uint64_t) s->meta.samples_per_msg *
                                    (s->meta.msg_per_buf - s->meta.msg_num);

                            /* Account for current position in buffer */
                            left_in_buffer -= s->meta.curr_msg_off;

                            if (samples_left >= left_in_buffer) {
                                /* Discard the remainder of this buffer */
                                advance_rx_buffer(b);
                                s->state = SYNC_STATE_WAIT_FOR_BUFFER;
                                s->meta.state = SYNC_META_STATE_HEADER;

                                log_verbose("%s: Discarding rest of buffer.\n",
                                            __FUNCTION__);

                            } else if (time_delta <= ts_remaining(s)) {
                                /* Fast forward within the current message */
                                assert(time_delta <= SIZE_MAX);

                                s->meta.curr_msg_off += (size_t)samples_left;
                                s->meta.curr_timestamp += time_delta;

                                log_verbose("%s: Seeking within message (t=%llu)\n",
                                            __FUNCTION__,
                                            s->meta.curr_timestamp);
                            } else {
                                s->meta.state = SYNC_META_STATE_HEADER;
                                s->meta.msg_num += timestamp_to_msg(s, samples_left);

                                log_verbose("%s: Seeking to message %u.\n",
                                            __FUNCTION__, s->meta.msg_num);
                            }
                        }
                        break;

                    default:
                        assert(!"Invalid state");
                        status = BLADERF_ERR_UNEXPECTED;
                }

                MUTEX_UNLOCK(&b->lock);
                break;

            case SYNC_STATE_USING_PACKET_META: /* Packet buffers w/ metadata */
                MUTEX_LOCK(&b->lock);

                buf_src = (uint8_t*)b->buffers[b->cons_i];

                pkt_len_dwords = metadata_get_packet_len(buf_src);

                if (pkt_len_dwords > 0) {
                   samples_returned += num_samples;
                   user_meta->actual_count = pkt_len_dwords;
                   memcpy(samples_dest, buf_src + METADATA_HEADER_SIZE, samples2bytes(s, pkt_len_dwords));
                }

                advance_rx_buffer(b);
                s->state = SYNC_STATE_WAIT_FOR_BUFFER;
                MUTEX_UNLOCK(&b->lock);
                break;


        }
    }

    if (user_meta && s->stream_config.format != BLADERF_FORMAT_PACKET_META) {
        user_meta->actual_count = samples_returned;
    }

out:
    MUTEX_UNLOCK(&s->lock);

    return status;
}

/* Assumes buffer lock is held */
static int advance_tx_buffer(struct bladerf_sync *s, struct buffer_mgmt *b)
{
    int status = 0;
    const unsigned int idx = b->prod_i;

    if (b->submitter == SYNC_TX_SUBMITTER_FN) {
        /* Mark buffer in flight because we're going to send it out.
         * This ensures that if the callback fires before this function
         * completes, its state will be correct. */
        b->status[idx] = SYNC_BUFFER_IN_FLIGHT;

        /* This call may block and it results in a per-stream lock being held,
         * so the buffer lock must be dropped.
         *
         * A callback may occur in the meantime, but this will not touch the
         * status for this this buffer, or the producer index.
         */
        MUTEX_UNLOCK(&b->lock);
        size_t len;
        if (s->stream_config.format == BLADERF_FORMAT_PACKET_META) {
           len = b->actual_lengths[idx];
        } else {
           len = async_stream_buf_bytes(s->worker->stream);
        }
        status = async_submit_stream_buffer(s->worker->stream,
                                            b->buffers[idx],
                                            &len,
                                            s->stream_config.timeout_ms,
                                            true);
        MUTEX_LOCK(&b->lock);

        if (status == 0) {
            log_verbose("%s: buf[%u] submitted.\n",
                        __FUNCTION__, idx);

        } else if (status == BLADERF_ERR_WOULD_BLOCK) {
            log_verbose("%s: Deferring buf[%u] submission to worker callback.\n",
                        __FUNCTION__, idx);

            /* Mark this buffer as being full of data, but not in flight */
            b->status[idx] = SYNC_BUFFER_FULL;

            /* Assign callback the duty of submitting deferred buffers,
             * and use buffer_mgmt.cons_i to denote which it should submit
             * (i.e., consume). */
            b->submitter = SYNC_TX_SUBMITTER_CALLBACK;
            b->cons_i = idx;

            /* This is expected and we are handling it. Don't propagate this
             * status back up */
            status = 0;
        } else {
            /* Unmark this as being in flight */
            b->status[idx] = SYNC_BUFFER_FULL;

            log_debug("%s: Failed to submit buf[%u].\n", __FUNCTION__, idx);
            return status;
       }
    } else {
        /* We are not submitting this buffer; this is deffered to the worker
         * call back. Just update its state to being full of samples. */
        b->status[idx] = SYNC_BUFFER_FULL;
    }

    /* Advance "producer" insertion index. */
    b->prod_i = (idx + 1) % b->num_buffers;

    /* Determine our next state based upon the state of the next buffer we
     * want to use. */
    if (b->status[b->prod_i] == SYNC_BUFFER_EMPTY) {
        /* Buffer is empty and ready for use */
        s->state = SYNC_STATE_BUFFER_READY;
    } else {
        /* We'll have to wait on this buffer to become ready. First, we'll
         * verify that the worker is running. */
        s->state = SYNC_STATE_CHECK_WORKER;
    }

    return status;
}

static inline bool timestamp_in_past(struct bladerf_metadata *user_meta,
                                     struct bladerf_sync *s)
{
    const bool in_past = user_meta->timestamp < s->meta.curr_timestamp;

    if (in_past) {
        log_debug("Provided timestamp=%"PRIu64" is in past: current=%"PRIu64"\n",
                  user_meta->timestamp, s->meta.curr_timestamp);
    }

    return in_past;
}

struct tx_options {
    bool flush;
    bool zero_pad;
};

static inline int handle_tx_parameters(struct bladerf_metadata *user_meta,
                                       struct bladerf_sync *s,
                                       struct tx_options *options)
{
    if (s->stream_config.format == BLADERF_FORMAT_SC16_Q11_META) {
        if (user_meta == NULL) {
            log_debug("NULL metadata pointer passed to %s\n", __FUNCTION__);
            return BLADERF_ERR_INVAL;
        }

        if (user_meta->flags & BLADERF_META_FLAG_TX_BURST_START) {
            bool now = user_meta->flags & BLADERF_META_FLAG_TX_NOW;

            if (s->meta.in_burst) {
                log_debug("%s: BURST_START provided while already in a burst.\n",
                          __FUNCTION__);
                return BLADERF_ERR_INVAL;
            } else if (!now && timestamp_in_past(user_meta, s)) {
                return BLADERF_ERR_TIME_PAST;
            }

            s->meta.in_burst = true;
            if (now) {
                s->meta.now = true;
                log_verbose("%s: Starting burst \"now\"\n", __FUNCTION__);
            } else {
                s->meta.curr_timestamp = user_meta->timestamp;
                log_verbose("%s: Starting burst @ %llu\n", __FUNCTION__,
                            (unsigned long long)s->meta.curr_timestamp);
            }

            if (user_meta->flags & BLADERF_META_FLAG_TX_UPDATE_TIMESTAMP) {
                log_debug("UPDATE_TIMESTAMP ignored; BURST_START flag was used.\n");
            }

        } else if (user_meta->flags & BLADERF_META_FLAG_TX_NOW) {
            log_debug("%s: TX_NOW was specified without BURST_START.\n",
                    __FUNCTION__);
            return BLADERF_ERR_INVAL;
        } else if (user_meta->flags & BLADERF_META_FLAG_TX_UPDATE_TIMESTAMP) {
            if (timestamp_in_past(user_meta, s)) {
                return BLADERF_ERR_TIME_PAST;
            } else {
                options->zero_pad = true;
            }
        }

        if (user_meta->flags & BLADERF_META_FLAG_TX_BURST_END) {
            if (s->meta.in_burst) {
                options->flush = true;
            } else {
                log_debug("%s: BURST_END provided while not in a burst.\n",
                          __FUNCTION__);
                return BLADERF_ERR_INVAL;
            }
        }

        user_meta->status = 0;
    }

    return 0;
}

int sync_tx(struct bladerf_sync *s,
            void const *samples,
            unsigned int num_samples,
            struct bladerf_metadata *user_meta,
            unsigned int timeout_ms)
{
    struct buffer_mgmt *b = NULL;

    int status                      = 0;
    unsigned int samples_written    = 0;
    unsigned int samples_to_copy    = 0;
    unsigned int samples_per_buffer = 0;
    uint8_t const *samples_src      = (uint8_t const *)samples;
    uint8_t *buf_dest               = NULL;
    struct tx_options op            = {
        FIELD_INIT(.flush, false), FIELD_INIT(.zero_pad, false),
    };

    log_verbose("%s: called for %u samples.\n", __FUNCTION__, num_samples);

    if (s == NULL || samples == NULL || !s->initialized) {
        return BLADERF_ERR_INVAL;
    }

    MUTEX_LOCK(&s->lock);

    status = handle_tx_parameters(user_meta, s, &op);
    if (status != 0) {
        goto out;
    }

    b                  = &s->buf_mgmt;
    samples_per_buffer = s->stream_config.samples_per_buffer;

    while (status == 0 && ((samples_written < num_samples) || op.flush)) {
        switch (s->state) {
            case SYNC_STATE_CHECK_WORKER: {
                int stream_error;
                sync_worker_state worker_state =
                    sync_worker_get_state(s->worker, &stream_error);

                if (stream_error != 0) {
                    status = stream_error;
                } else {
                    if (worker_state == SYNC_WORKER_STATE_IDLE) {
                        /* No need to reset any buffer management for TX since
                         * the TX stream does not submit an initial set of
                         * buffers.  Therefore the RESET_BUF_MGMT state is
                         * skipped here. */
                        s->state = SYNC_STATE_START_WORKER;
                    } else {
                        /* Worker is running - continue onto checking for and
                         * potentially waiting for an available buffer */
                        s->state = SYNC_STATE_WAIT_FOR_BUFFER;
                    }
                }
                break;
            }

            case SYNC_STATE_RESET_BUF_MGMT:
                assert(!"Bug");
                break;

            case SYNC_STATE_START_WORKER:
                sync_worker_submit_request(s->worker, SYNC_WORKER_START);

                status = sync_worker_wait_for_state(
                    s->worker, SYNC_WORKER_STATE_RUNNING,
                    SYNC_WORKER_START_TIMEOUT_MS);

                if (status == 0) {
                    s->state = SYNC_STATE_WAIT_FOR_BUFFER;
                    log_debug("%s: Worker is now running.\n", __FUNCTION__);
                }
                break;

            case SYNC_STATE_WAIT_FOR_BUFFER:
                MUTEX_LOCK(&b->lock);

                /* Check the buffer state, as the worker may have consumed one
                 * since we last queried the status */
                if (b->status[b->prod_i] == SYNC_BUFFER_EMPTY) {
                    s->state = SYNC_STATE_BUFFER_READY;
                } else {
                    status =
                        wait_for_buffer(b, timeout_ms, __FUNCTION__, b->prod_i);
                }

                MUTEX_UNLOCK(&b->lock);
                break;

            case SYNC_STATE_BUFFER_READY:
                MUTEX_LOCK(&b->lock);
                b->status[b->prod_i] = SYNC_BUFFER_PARTIAL;
                b->partial_off       = 0;

                switch (s->stream_config.format) {
                    case BLADERF_FORMAT_SC16_Q11:
                    case BLADERF_FORMAT_SC8_Q7:
                        s->state = SYNC_STATE_USING_BUFFER;
                        break;

                    case BLADERF_FORMAT_SC16_Q11_META:
                    case BLADERF_FORMAT_SC8_Q7_META:
                        s->state             = SYNC_STATE_USING_BUFFER_META;
                        s->meta.curr_msg_off = 0;
                        s->meta.msg_num      = 0;
                        break;

                    case BLADERF_FORMAT_PACKET_META:
                        s->state             = SYNC_STATE_USING_PACKET_META;
                        s->meta.curr_msg_off = 0;
                        s->meta.msg_num      = 0;
                        break;

                    default:
                        assert(!"Invalid stream format");
                        status = BLADERF_ERR_UNEXPECTED;
                }

                MUTEX_UNLOCK(&b->lock);
                break;


            case SYNC_STATE_USING_BUFFER:
                MUTEX_LOCK(&b->lock);

                buf_dest        = (uint8_t *)b->buffers[b->prod_i];
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

                    /* Submit buffer and advance to the next one */
                    status = advance_tx_buffer(s, b);
                }

                MUTEX_UNLOCK(&b->lock);
                break;

            case SYNC_STATE_USING_BUFFER_META: /* SC16Q11 buffers w/ metadata */
                MUTEX_LOCK(&b->lock);

                switch (s->meta.state) {
                    case SYNC_META_STATE_HEADER:
                        buf_dest = (uint8_t *)b->buffers[b->prod_i];

                        s->meta.curr_msg =
                            buf_dest + s->meta.msg_size * s->meta.msg_num;

                        log_verbose("%s: Set curr_msg to: %p (buf @ %p)\n",
                                    __FUNCTION__, s->meta.curr_msg, buf_dest);

                        s->meta.curr_msg_off = 0;

                        if (s->meta.now) {
                            metadata_set(s->meta.curr_msg, 0, 0);
                        } else {
                            metadata_set(s->meta.curr_msg,
                                         s->meta.curr_timestamp, 0);
                        }

                        s->meta.state = SYNC_META_STATE_SAMPLES;

                        log_verbose("%s: Filled in header (t=%llu)\n",
                                    __FUNCTION__,
                                    (unsigned long long)s->meta.curr_timestamp);
                        break;

                    case SYNC_META_STATE_SAMPLES:
                        if (op.zero_pad) {
                            const uint64_t delta =
                                user_meta->timestamp - s->meta.curr_timestamp;

                            size_t to_zero;

                            log_verbose("%s: User requested zero padding to "
                                        "t=%" PRIu64 " (%" PRIu64 " + %" PRIu64
                                        ")\n",
                                        __FUNCTION__, user_meta->timestamp,
                                        s->meta.curr_timestamp, delta);

                            if (delta < left_in_msg(s)) {
                                to_zero = (size_t)delta;

                                log_verbose("%s: Padded subset of msg "
                                            "(%" PRIu64 " samples)\n",
                                            __FUNCTION__, (uint64_t)to_zero);
                            } else {
                                to_zero = left_in_msg(s);

                                log_verbose("%s: Padded remainder of msg "
                                            "(%" PRIu64 " samples)\n",
                                            __FUNCTION__, (uint64_t)to_zero);
                            }

                            memset(s->meta.curr_msg + METADATA_HEADER_SIZE +
                                       samples2bytes(s, s->meta.curr_msg_off),
                                   0, samples2bytes(s, to_zero));

                            s->meta.curr_msg_off += to_zero;

                            /* If we're going to supply the FPGA with a
                             * discontinuity, it is required that the last three
                             * samples provided be zero in order to hold the
                             * DAC @ (0 + 0j).
                             *
                             * See "Figure 9: TX data interface" in the LMS6002D
                             * data sheet for the register stages that create
                             * this requirement.
                             *
                             * If we're ending a burst with < 3 zeros samples at
                             * the end of the message, we'll need to continue
                             * onto the next message. At this next message,
                             * we'll either encounter the requested timestamp or
                             * zero-fill the message to fulfil this "three zero
                             * sample" requirement, and set the timestamp
                             * appropriately at the following message.
                             */
                            if (to_zero < 3 && left_in_msg(s) == 0) {
                                s->meta.curr_timestamp += to_zero;
                                log_verbose("Ended msg with < 3 zero samples. "
                                            "Padding into next message.\n");
                            } else {
                                s->meta.curr_timestamp = user_meta->timestamp;
                                op.zero_pad            = false;
                            }
                        }

                        samples_to_copy = uint_min(
                            num_samples - samples_written, left_in_msg(s));

                        if (samples_to_copy != 0) {
                            /* We have user data to copy into the current
                             * message within the buffer */
                            memcpy(s->meta.curr_msg + METADATA_HEADER_SIZE +
                                       samples2bytes(s, s->meta.curr_msg_off),
                                   samples_src +
                                       samples2bytes(s, samples_written),
                                   samples2bytes(s, samples_to_copy));

                            s->meta.curr_msg_off += samples_to_copy;
                            if (s->stream_config.layout == BLADERF_TX_X2)
                               s->meta.curr_timestamp += samples_to_copy / 2;
                            else
                               s->meta.curr_timestamp += samples_to_copy;

                            samples_written += samples_to_copy;

                            log_verbose("%s: Copied %u samples. "
                                        "Current message offset is now: %u\n",
                                        __FUNCTION__, samples_to_copy,
                                        s->meta.curr_msg_off);
                        }

                        if (left_in_msg(s) != 0 && op.flush) {
                            /* We're ending this buffer early and need to
                             * flush the remaining samples by setting all
                             * samples in the messages to (0 + 0j) */
                            const unsigned int to_zero = left_in_msg(s);

                            const size_t off =
                                METADATA_HEADER_SIZE +
                                samples2bytes(s, s->meta.curr_msg_off);

                            /* If we're here, we should have already copied
                             * all requested data to the buffer */
                            assert(num_samples == samples_written);

                            memset(s->meta.curr_msg + off, 0,
                                   samples2bytes(s, to_zero));

                            log_verbose(
                                "%s: Flushed %u samples @ %u (0x%08x)\n",
                                __FUNCTION__, to_zero, s->meta.curr_msg_off,
                                off);

                            s->meta.curr_msg_off += to_zero;
                            s->meta.curr_timestamp += to_zero;
                        }

                        if (left_in_msg(s) == 0) {
                            s->meta.msg_num++;
                            s->meta.state = SYNC_META_STATE_HEADER;

                            log_verbose("%s: Advancing to next message (%u)\n",
                                        __FUNCTION__, s->meta.msg_num);
                        }

                        if (s->meta.msg_num >= s->meta.msg_per_buf) {
                            assert(s->meta.msg_num == s->meta.msg_per_buf);

                            /* Submit buffer of samples for transmission */
                            status = advance_tx_buffer(s, b);

                            s->meta.msg_num = 0;
                            s->state        = SYNC_STATE_WAIT_FOR_BUFFER;

                            /* We want to clear the flush flag if we've written
                             * all of our data, but keep it set if we have more
                             * data and need wrap around to another buffer */
                            op.flush =
                                op.flush && (samples_written != num_samples);
                        }

                        break;

                    default:
                        assert(!"Invalid state");
                        status = BLADERF_ERR_UNEXPECTED;
                }

                MUTEX_UNLOCK(&b->lock);
                break;

            case SYNC_STATE_USING_PACKET_META: /* Packet buffers w/ metadata */
                MUTEX_LOCK(&b->lock);

                buf_dest = (uint8_t *)b->buffers[b->prod_i];

                memcpy(buf_dest + METADATA_HEADER_SIZE, samples_src, num_samples*4);

                b->actual_lengths[b->prod_i] = samples2bytes(s, num_samples) + METADATA_HEADER_SIZE;

                metadata_set_packet(buf_dest, 0, 0, num_samples, 0, 0);

                samples_written = num_samples;

                status = advance_tx_buffer(s, b);

                s->meta.msg_num = 0;
                s->state        = SYNC_STATE_WAIT_FOR_BUFFER;

                MUTEX_UNLOCK(&b->lock);
                break;

        }
    }

    if (status == 0 &&
        s->stream_config.format == BLADERF_FORMAT_SC16_Q11_META &&
        (user_meta->flags & BLADERF_META_FLAG_TX_BURST_END)) {
        s->meta.in_burst = false;
        s->meta.now      = false;
    }

out:
    MUTEX_UNLOCK(&s->lock);

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
