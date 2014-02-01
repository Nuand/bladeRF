/*
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2013 Nuand LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>

#include "rel_assert.h"
#include "host_config.h"
#include "rxtx_impl.h"
#include "minmax.h"

#if BLADERF_OS_WINDOWS
#   define EOL "\r\n"
#else
#   define EOL "\n"
#endif

struct rx_callback_data {
    int (*write_samples)(struct rxtx_data *rx, int16_t *samples, size_t n);
    struct rxtx_data *rx;
    size_t samples_left;
    bool inf;
};

/**
 * Peform adjustments on received samples before writing them out:
 *  (1) Mask off FPGA markers
 *  (2) Convert little-endian samples to host endianness, if needed.
 *
 *  @param  buff    Sample buffer
 *  @param  n       Number of samples
 */
static inline void sc16q11_sample_fixup(int16_t *buf, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++) {
        /* I - Mask off the marker and sign extend */
//        *buf &= (*buf) & 0x0fff;
//        if (*buf & 0x800) {
//            *buf |= 0xf000;
//        }

        *buf = LE16_TO_HOST(*buf);
        buf++;

        /* Q - Mask off the marker and sign extend */
//        *buf = HOST_TO_LE16(*buf) & 0x0fff;
//        if (*buf & 0x800) {
//            *buf |= 0xf000;
//        }

        *buf = LE16_TO_HOST(*buf);
        buf++;
    }
}

/*
 * @pre data_mgmt lock is held
 *
 * returns 0 on success, CMD_RET_* on failure (and calls set_last_error()) */
static int rx_write_bin_sc16q11(struct rxtx_data *rx,
                                int16_t *samples, size_t n_samples)
{
    size_t status;

    pthread_mutex_lock(&rx->file_mgmt.file_lock);
    status = fwrite(samples, sizeof(int16_t),
                    2 * n_samples,  /* I and Q are each an int16_t */
                    rx->file_mgmt.file);
    pthread_mutex_unlock(&rx->file_mgmt.file_lock);

    if (status != (2 * n_samples)) {
        set_last_error(&rx->last_error, ETYPE_CLI, CMD_RET_FILEOP);
        return CMD_RET_FILEOP;
    } else {
        return 0;
    }
}

/* returns 0 on success, CMD_RET_* on failure (and calls set_last_error()) */
static int rx_write_csv_sc16q11(struct rxtx_data *rx,
                                int16_t *samples, size_t n_samples)
{
    size_t i;
    int status = 0;
    const size_t to_write = n_samples * 2;  /* int16_t for I, another for Q */
    char line[32] = { 0 };

    pthread_mutex_lock(&rx->file_mgmt.file_lock);

    for (i = 0; i < to_write; i += 2) {
        snprintf(line, sizeof(line), "%d, %d" EOL, samples[i], samples[i + 1]);

        if (fputs(line, rx->file_mgmt.file) < 0) {
            set_last_error(&rx->last_error, ETYPE_ERRNO, errno);
            status = CMD_RET_FILEOP;
            break;
        }
    }

    pthread_mutex_unlock(&rx->file_mgmt.file_lock);
    return status;
}

static void *rx_callback(struct bladerf *dev,
                         struct bladerf_stream *stream,
                         struct bladerf_metadata *meta,
                         void *samples,
                         size_t num_samples,
                         void *user_data)
{
    struct rx_callback_data *cb_data = (struct rx_callback_data *)user_data;
    struct rxtx_data *rx = cb_data->rx;
    unsigned char requests;
    void *ret;
    size_t n_to_write;

    /* Stop stream on STOP or SHUTDOWN, but only clear STOP. This will keep
     * the SHUTDOWN request around so we can read it when determining
     * our state transition */
    requests = rxtx_get_requests(rx, RXTX_TASK_REQ_STOP);
    if (requests & (RXTX_TASK_REQ_STOP | RXTX_TASK_REQ_SHUTDOWN)) {
        return NULL;
    }

    pthread_mutex_lock(&rx->data_mgmt.lock);

    if (!cb_data->inf) {
        n_to_write = min_sz(cb_data->samples_left, num_samples);
        cb_data->samples_left -= n_to_write;
    } else {
        n_to_write = num_samples;
    }

    /* We assume we have IQ pairs */
    assert((n_to_write & 1) == 0);

    if (n_to_write > 0) {
        sc16q11_sample_fixup(samples, n_to_write);

        /* Write the samples out */
        cb_data->write_samples(rx, (int16_t*)samples, n_to_write);

        /* Fetch the next buffer */
        rx->data_mgmt.next_idx++;
        rx->data_mgmt.next_idx %= rx->data_mgmt.num_buffers;
        ret = rx->data_mgmt.buffers[rx->data_mgmt.next_idx];
    } else {
        /* We've already written the request number of samples */
        ret = NULL;
    }

    pthread_mutex_unlock(&rx->data_mgmt.lock);
    return ret;
}

void *rx_task(void *cli_state_arg)
{
    int status = 0;
    unsigned char requests;
    enum rxtx_state task_state;
    struct cli_state *cli_state = (struct cli_state *) cli_state_arg;
    struct rxtx_data *rx = cli_state->rx;
    struct rx_params *rx_params = rx->params;
    struct rx_callback_data cb_data;

    task_state = rxtx_get_state(rx);
    assert(task_state == RXTX_STATE_INIT);

    set_last_error(&rx->last_error, ETYPE_BLADERF, 0);
    requests = 0;

    while (task_state != RXTX_STATE_SHUTDOWN) {
        switch (task_state) {
            case RXTX_STATE_INIT:
                rxtx_set_state(rx, RXTX_STATE_IDLE);
                break;

            case RXTX_STATE_IDLE:
                rxtx_task_exec_idle(rx, &requests);
                break;

            case RXTX_STATE_START:
            {
                /* This should be set to an appropriate value upon
                 * encountering an error condition */
                enum error_type err_type = ETYPE_BUG;

                /* Clear the last error */
                set_last_error(&rx->last_error, ETYPE_ERRNO, 0);

                /* Set up count of samples to receive */
                pthread_mutex_lock(&rx->param_lock);
                cb_data.samples_left = rx_params->n_samples;
                pthread_mutex_unlock(&rx->param_lock);

                cb_data.rx = rx;
                cb_data.inf = cb_data.samples_left == 0;

                /* Choose the callback appropriate for the desired file type */
                pthread_mutex_lock(&rx->file_mgmt.file_meta_lock);

                switch (rx->file_mgmt.format) {
                    case RXTX_FMT_CSV_SC16Q11:
                        cb_data.write_samples = rx_write_csv_sc16q11;
                        break;

                    case RXTX_FMT_BIN_SC16Q11:
                        cb_data.write_samples = rx_write_bin_sc16q11;
                        break;

                    default:
                        status = CMD_RET_INVPARAM;
                        set_last_error(&rx->last_error, ETYPE_CLI, status);
                        rxtx_set_state(rx, RXTX_STATE_IDLE);
                }

                /* Open the specified file */
                if (status == 0) {
                    assert(rx->file_mgmt.path);
                }

                pthread_mutex_unlock(&rx->file_mgmt.file_meta_lock);

                /* Set up the reception stream and buffer information */
                if (status == 0) {
                    pthread_mutex_lock(&rx->data_mgmt.lock);

                    rx->data_mgmt.next_idx = 0;

                    status = bladerf_init_stream(&rx->data_mgmt.stream,
                                cli_state->dev,
                                rx_callback,
                                &rx->data_mgmt.buffers,
                                rx->data_mgmt.num_buffers,
                                BLADERF_FORMAT_SC16_Q11,
                                rx->data_mgmt.samples_per_buffer,
                                rx->data_mgmt.num_transfers,
                                &cb_data);

                    if (status < 0) {
                        err_type = ETYPE_BLADERF;
                    }

                    pthread_mutex_unlock(&rx->data_mgmt.lock);
                }

                if (status == 0) {
                    rxtx_set_state(rx, RXTX_STATE_RUNNING);
                } else {
                    bladerf_deinit_stream(rx->data_mgmt.stream);
                    rx->data_mgmt.stream = NULL;
                    set_last_error(&rx->last_error, err_type, status);
                    rxtx_set_state(rx, RXTX_STATE_IDLE);
                }
            }
            break;

            case RXTX_STATE_RUNNING:
                rxtx_task_exec_running(rx, cli_state);
                break;

            case RXTX_STATE_STOP:
                rxtx_task_exec_stop(rx, &requests);
                break;

            case RXTX_STATE_SHUTDOWN:
                break;

            default:
                /* Bug - can only get here with a corrupted value */
                assert(0);
        }

        task_state = rxtx_get_state(rx);
    }

    return NULL;
}

static int rx_cmd_start(struct cli_state *s)
{
    int status;

    /* Check that we can start up in our current state */
    status = rxtx_cmd_start_check(s, s->rx, "rx");
    if (status != 0) {
        return status;
    }

    /* Set up output file */
    pthread_mutex_lock(&s->rx->file_mgmt.file_lock);
    if(s->rx->file_mgmt.format == RXTX_FMT_CSV_SC16Q11) {
        s->rx->file_mgmt.file = expand_and_open(s->rx->file_mgmt.path, "w");
    } else {
        /* RXTX_FMT_BIN_SC16Q11, open file in binary mode */
        s->rx->file_mgmt.file = expand_and_open(s->rx->file_mgmt.path, "wb");
    }
    if (!s->rx->file_mgmt.file) {
        set_last_error(&s->rx->last_error, ETYPE_ERRNO, errno);
        status = CMD_RET_FILEOP;
    } else {
        status = 0;
    }
    pthread_mutex_unlock(&s->rx->file_mgmt.file_lock);

    if (status != 0) {
        return status;
    }

    /* Set our stream timeout */
    pthread_mutex_lock(&s->rx->data_mgmt.lock);
    status = bladerf_set_stream_timeout(s->dev, BLADERF_MODULE_RX,
                                        s->rx->data_mgmt.timeout_ms);
    pthread_mutex_unlock(&s->rx->data_mgmt.lock);

    if (status != 0) {
        s->last_lib_error = status;
        set_last_error(&s->rx->last_error, ETYPE_BLADERF, s->last_lib_error);
        return CMD_RET_LIBBLADERF;
    }

    /* Request thread to start running */
    rxtx_submit_request(s->rx, RXTX_TASK_REQ_START);
    status = rxtx_wait_for_state(s->rx, RXTX_STATE_RUNNING, 3000);

    /* This should never occur. If it does, there's likely a defect
     * present in the rx task */
    if (status != 0) {
        cli_err(s, "rx", "RX did not start up in the alloted time\n");
        status = CMD_RET_UNKNOWN;
    }

    return status;
}

static void rx_print_config(struct rxtx_data *rx)
{
    size_t n_samples;
    struct rx_params *rx_params = rx->params;

    pthread_mutex_lock(&rx->param_lock);
    n_samples = rx_params->n_samples;
    pthread_mutex_unlock(&rx->param_lock);

    rxtx_print_state(rx, "    State: ", "\n");
    rxtx_print_error(rx, "    Last error: ", "\n");
    rxtx_print_file(rx, "    File: ", "\n");
    rxtx_print_file_format(rx, "    File format: ", "\n");

    if (n_samples) {
        printf("    # Samples: %" PRIu64  "\n", (uint64_t)n_samples);
    } else {
        printf("    # Samples: infinite\n");
    }
    rxtx_print_stream_info(rx, "    ", "\n");

    printf("\n");
}

static int rx_cmd_config(struct cli_state *s, int argc, char **argv)
{
    int i;
    char *val;
    int status;
    struct rx_params *rx_params = s->rx->params;

    assert(argc >= 2);

    if (argc == 2) {
        rx_print_config(s->rx);
        return 0;
    }

    for (i = 2; i < argc; i++) {
        status = rxtx_handle_config_param(s, s->rx, argv[0], argv[i], &val);

        if (status < 0) {
            return status;
        } else if (status == 0) {
            if (!strcasecmp("n", argv[i])) {
                /* Configure number of samples to receive */
                unsigned int n;
                bool ok;

                n = str2uint_suffix(val, 0, UINT_MAX, rxtx_kmg_suffixes,
                                    (int)rxtx_kmg_suffixes_len, &ok);

                if (ok) {
                    pthread_mutex_lock(&s->rx->param_lock);
                    rx_params->n_samples = n;
                    pthread_mutex_unlock(&s->rx->param_lock);
                } else {
                    cli_err(s, argv[0], RXTX_ERRMSG_VALUE(argv[1], val));
                    return CMD_RET_INVPARAM;
                }

            } else {
                cli_err(s, argv[0], "Unrecognized config parameter: %s", argv[i]);
                return CMD_RET_INVPARAM;
            }
        }
    }

    return 0;
}

int cmd_rx(struct cli_state *s, int argc, char **argv)
{
    int ret;

    assert(argc > 0);

    if (argc == 1) {
        rx_print_config(s->rx);
        ret = 0;
    } else if (!strcasecmp(argv[1], RXTX_CMD_START)) {
        ret = rx_cmd_start(s);
    } else if (!strcasecmp(argv[1], RXTX_CMD_STOP)) {
        ret = rxtx_cmd_stop(s, s->rx);
    } else if (!strcasecmp(argv[1], RXTX_CMD_CONFIG)) {
        ret = rx_cmd_config(s, argc, argv);
    } else if (!strcasecmp(argv[1], RXTX_CMD_WAIT)) {
        ret = rxtx_handle_wait(s, s->rx, argc, argv);
    } else {
        cli_err(s, argv[0], "Invalid command: \"%s\"", argv[1]);
        ret = CMD_RET_INVPARAM;
    }

    return ret;
}
