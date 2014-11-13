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
 * returns 0 on success, CLI_RET_* on failure (and calls set_last_error()) */
static int rx_write_bin_sc16q11(struct rxtx_data *rx,
                                int16_t *samples, size_t n_samples)
{
    size_t status;

    MUTEX_LOCK(&rx->file_mgmt.file_lock);
    status = fwrite(samples, sizeof(int16_t),
                    2 * n_samples,  /* I and Q are each an int16_t */
                    rx->file_mgmt.file);
    MUTEX_UNLOCK(&rx->file_mgmt.file_lock);

    if (status != (2 * n_samples)) {
        set_last_error(&rx->last_error, ETYPE_CLI, CLI_RET_FILEOP);
        return CLI_RET_FILEOP;
    } else {
        return 0;
    }
}

/* returns 0 on success, CLI_RET_* on failure (and calls set_last_error()) */
static int rx_write_csv_sc16q11(struct rxtx_data *rx,
                                int16_t *samples, size_t n_samples)
{
    size_t i;
    int status = 0;
    const size_t to_write = n_samples * 2;  /* int16_t for I, another for Q */
    char line[32] = { 0 };

    MUTEX_LOCK(&rx->file_mgmt.file_lock);

    for (i = 0; i < to_write; i += 2) {
        snprintf(line, sizeof(line), "%d, %d" EOL, samples[i], samples[i + 1]);

        if (fputs(line, rx->file_mgmt.file) < 0) {
            set_last_error(&rx->last_error, ETYPE_ERRNO, errno);
            status = CLI_RET_FILEOP;
            break;
        }
    }

    MUTEX_UNLOCK(&rx->file_mgmt.file_lock);
    return status;
}

static int rx_task_exec_running(struct rxtx_data *rx, struct cli_state *s)
{
    int status = 0;
    int samples_per_buffer;
    void *samples;
    size_t num_samples;
    size_t samples_read = 0;
    int (*write_samples)(struct rxtx_data *rx, int16_t *samples, size_t n);
    unsigned int timeout_ms;

    /* Read the parameters that will be used for the sync transfers */
    MUTEX_LOCK(&rx->data_mgmt.lock);
    timeout_ms = rx->data_mgmt.timeout_ms;
    samples_per_buffer = rx->data_mgmt.samples_per_buffer;
    MUTEX_UNLOCK(&rx->data_mgmt.lock);

    MUTEX_LOCK(&rx->param_lock);
    num_samples = ((struct rx_params*)rx->params)->n_samples;
    write_samples = ((struct rx_params*)rx->params)->write_samples;
    MUTEX_UNLOCK(&rx->param_lock);

    /* Allocate a buffer for the block of samples */
    samples = malloc(samples_per_buffer * sizeof(uint16_t) * 2);
    if (samples == NULL) {
        status = errno;
        set_last_error(&rx->last_error, ETYPE_ERRNO, status);
    }

    /*
     * Keep reading samples until a failure or until all requested samples
     * have been read
     */
    while (status == 0 && (num_samples == 0 || samples_read < num_samples)) {
        /*
         * Stop stream on STOP or SHUTDOWN, but only clear STOP. This will keep
         * the SHUTDOWN request around so we can read it when determining our
         * state transition
         */
        unsigned char requests = rxtx_get_requests(rx, RXTX_TASK_REQ_STOP);
        if (requests & (RXTX_TASK_REQ_STOP | RXTX_TASK_REQ_SHUTDOWN)) {
            break;
        }

        /* Read the samples into the sample buffer */
        status = bladerf_sync_rx(s->dev, samples, samples_per_buffer, NULL,
                                 timeout_ms);

        if (status != 0) {
            set_last_error(&rx->last_error, ETYPE_BLADERF, status);
        } else {
            size_t to_write = min_sz(samples_per_buffer,
                                     (num_samples - samples_read));

            /* Write the samples to the output file */
            sc16q11_sample_fixup(samples, to_write);
            status = write_samples(rx, samples, to_write);

            if (status != 0) {
                set_last_error(&rx->last_error, ETYPE_CLI, status);
            }
        }

        samples_read += samples_per_buffer;
    }

    /* Free the sample buffer */
    if (samples != NULL) {
        free(samples);
    }

    return status;
}

void *rx_task(void *cli_state_arg)
{
    int status = 0;
    int disable_status;
    unsigned char requests;
    enum rxtx_state task_state;
    struct cli_state *cli_state = (struct cli_state *) cli_state_arg;
    struct rxtx_data *rx = cli_state->rx;
    struct rx_params *rx_params = rx->params;
    MUTEX *dev_lock = &cli_state->dev_lock;

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
                status = 0;

                /* Choose the callback appropriate for the desired file type */
                MUTEX_LOCK(&rx->file_mgmt.file_meta_lock);

                switch (rx->file_mgmt.format) {
                    case RXTX_FMT_CSV_SC16Q11:
                        rx_params->write_samples = rx_write_csv_sc16q11;
                        break;

                    case RXTX_FMT_BIN_SC16Q11:
                        rx_params->write_samples = rx_write_bin_sc16q11;
                        break;

                    default:
                        status = CLI_RET_INVPARAM;
                        set_last_error(&rx->last_error, ETYPE_CLI, status);
                        rxtx_set_state(rx, RXTX_STATE_IDLE);
                }

                if (status == 0) {
                    assert(rx->file_mgmt.path);
                }

                MUTEX_UNLOCK(&rx->file_mgmt.file_meta_lock);

                /* Set up the reception stream and buffer information */
                if (status == 0) {
                    MUTEX_LOCK(&rx->data_mgmt.lock);

                    status = bladerf_sync_config(cli_state->dev,
                                                 BLADERF_MODULE_RX,
                                                 BLADERF_FORMAT_SC16_Q11,
                                                 rx->data_mgmt.num_buffers,
                                                 rx->data_mgmt.samples_per_buffer,
                                                 rx->data_mgmt.num_transfers,
                                                 rx->data_mgmt.timeout_ms);

                    if (status < 0) {
                        err_type = ETYPE_BLADERF;
                    }

                    MUTEX_UNLOCK(&rx->data_mgmt.lock);
                }

                if (status == 0) {
                    rxtx_set_state(rx, RXTX_STATE_RUNNING);
                } else {
                    set_last_error(&rx->last_error, err_type, status);
                    rxtx_set_state(rx, RXTX_STATE_IDLE);
                }
            }
            break;

            case RXTX_STATE_RUNNING:

                MUTEX_LOCK(dev_lock);
                status = bladerf_enable_module(cli_state->dev,
                                               rx->module, true);
                MUTEX_UNLOCK(dev_lock);

                if (status < 0) {
                    set_last_error(&rx->last_error, ETYPE_BLADERF, status);
                } else {
                    status = rx_task_exec_running(rx, cli_state);

                    MUTEX_LOCK(dev_lock);
                    disable_status = bladerf_enable_module(cli_state->dev,
                                                           rx->module, false);
                    MUTEX_UNLOCK(dev_lock);

                    if (status == 0 && disable_status < 0) {
                        set_last_error(&rx->last_error, ETYPE_BLADERF, status);
                    }
                }

                rxtx_set_state(rx, RXTX_STATE_STOP);
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
    MUTEX_LOCK(&s->rx->file_mgmt.file_lock);
    if(s->rx->file_mgmt.format == RXTX_FMT_CSV_SC16Q11) {
        status = expand_and_open(s->rx->file_mgmt.path, "w",
                                 &s->rx->file_mgmt.file);

    } else {
        /* RXTX_FMT_BIN_SC16Q11, open file in binary mode */
        status = expand_and_open(s->rx->file_mgmt.path, "wb",
                                 &s->rx->file_mgmt.file);
    }
    MUTEX_UNLOCK(&s->rx->file_mgmt.file_lock);

    if (status != 0) {
        return status;
    }

    /* Request thread to start running */
    rxtx_submit_request(s->rx, RXTX_TASK_REQ_START);
    status = rxtx_wait_for_state(s->rx, RXTX_STATE_RUNNING, 3000);

    /* This should never occur. If it does, there's likely a defect
     * present in the rx task */
    if (status != 0) {
        cli_err(s, "rx", "RX did not start up in the allotted time.\n");
        status = CLI_RET_UNKNOWN;
    }

    return status;
}

static void rx_print_config(struct rxtx_data *rx)
{
    size_t n_samples;
    struct rx_params *rx_params = rx->params;

    MUTEX_LOCK(&rx->param_lock);
    n_samples = rx_params->n_samples;
    MUTEX_UNLOCK(&rx->param_lock);

    rxtx_print_state(rx, "\n  State: ", "\n");
    rxtx_print_error(rx, "  Last error: ", "\n");
    rxtx_print_file(rx, "  File: ", "\n");
    rxtx_print_file_format(rx, "  File format: ", "\n");

    if (n_samples) {
        printf("  # Samples: %" PRIu64  "\n", (uint64_t)n_samples);
    } else {
        printf("  # Samples: infinite\n");
    }
    rxtx_print_stream_info(rx, "  ", "\n");

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
                    MUTEX_LOCK(&s->rx->param_lock);
                    rx_params->n_samples = n;
                    MUTEX_UNLOCK(&s->rx->param_lock);
                } else {
                    cli_err(s, argv[0], RXTX_ERRMSG_VALUE(argv[1], val));
                    return CLI_RET_INVPARAM;
                }

            } else {
                cli_err(s, argv[0],
                        "Unrecognized config parameter: %s\n", argv[i]);
                return CLI_RET_INVPARAM;
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
        cli_err(s, argv[0], "Invalid command: \"%s\"\n", argv[1]);
        ret = CLI_RET_INVPARAM;
    }

    return ret;
}
