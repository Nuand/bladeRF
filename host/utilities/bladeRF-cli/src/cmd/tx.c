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

#include "rel_assert.h"
#include "conversions.h"
#include "host_config.h"
#include "rxtx_impl.h"
#include "minmax.h"

/* The DAC range is [-2048, 2047] */
#define SC16Q11_IQ_MIN  (-2048)
#define SC16Q11_IQ_MAX  (2047)

static int tx_task_exec_running(struct rxtx_data *tx, struct cli_state *s)
{
    int status = 0;
    unsigned int samples_per_buffer;
    int16_t *tx_buffer;
    struct tx_params *tx_params = tx->params;
    unsigned int repeats_remaining;
    unsigned int delay_us;
    unsigned int delay_samples;
    unsigned int delay_samples_remaining;
    bool repeat_infinite;
    unsigned int timeout_ms;
    unsigned int sample_rate;

    enum state { INIT, READ_FILE, DELAY, PAD_TRAILING, DONE };
    enum state state = INIT;

    /* Fetch the parameters required for the TX operation */
    MUTEX_LOCK(&tx->param_lock);
    repeats_remaining = tx_params->repeat;
    delay_us = tx_params->repeat_delay;
    MUTEX_UNLOCK(&tx->param_lock);

    repeat_infinite = (repeats_remaining == 0);

    MUTEX_LOCK(&tx->data_mgmt.lock);
    samples_per_buffer = (unsigned int)tx->data_mgmt.samples_per_buffer;
    timeout_ms = tx->data_mgmt.timeout_ms;
    MUTEX_UNLOCK(&tx->data_mgmt.lock);

    status = bladerf_get_sample_rate(s->dev, tx->module, &sample_rate);
    if (status != 0) {
        set_last_error(&tx->last_error, ETYPE_BLADERF, status);
        return CLI_RET_LIBBLADERF;
    }

    /* Compute delay time as a sample count */
    delay_samples = (unsigned int)((uint64_t)sample_rate * delay_us / 1000000);
    delay_samples_remaining = delay_samples;

    /* Allocate a buffer to hold each block of samples to transmit */
    tx_buffer = (int16_t*) malloc(samples_per_buffer * 2 * sizeof(int16_t));
    if (tx_buffer == NULL) {
        status = CLI_RET_MEM;
        set_last_error(&tx->last_error, ETYPE_ERRNO,
                       errno == 0 ? ENOMEM : errno);
    }

    /* Keep writing samples while there is more data to send and no failures
     * have occurred */
    while (state != DONE && status == 0) {

        unsigned char requests;
        unsigned int buffer_samples_remaining = samples_per_buffer;
        int16_t *tx_buffer_current = tx_buffer;

        /* Stop stream on STOP or SHUTDOWN, but only clear STOP. This will keep
         * the SHUTDOWN request around so we can read it when determining
         * our state transition */
        requests = rxtx_get_requests(tx, RXTX_TASK_REQ_STOP);
        if (requests & (RXTX_TASK_REQ_STOP | RXTX_TASK_REQ_SHUTDOWN)) {
            break;
        }

        /* Keep adding to the buffer until it is full or a failure occurs */
        while (buffer_samples_remaining > 0 && status == 0 && state != DONE) {
            size_t samples_populated = 0;

            switch (state) {
                case INIT:
                case READ_FILE:

                    MUTEX_LOCK(&tx->file_mgmt.file_lock);

                    /* Read from the input file */
                    samples_populated = fread(tx_buffer_current,
                                              2 * sizeof(int16_t),
                                              buffer_samples_remaining,
                                              tx->file_mgmt.file);

                    assert(samples_populated <= UINT_MAX);

                    /* If the end of the file was reached, determine whether
                     * to delay, re-read from the file, or pad the rest of the
                     * buffer and finish */
                    if (feof(tx->file_mgmt.file)) {
                        repeats_remaining--;

                        if ((repeats_remaining > 0) || repeat_infinite) {
                            if (delay_samples != 0) {
                                delay_samples_remaining = delay_samples;
                                state = DELAY;
                            }
                        }
                        else {
                            state = PAD_TRAILING;
                        }

                        /* Clear the EOF condition and rewind the file */
                        clearerr(tx->file_mgmt.file);
                        rewind(tx->file_mgmt.file);
                    }

                    /* Check for errors */
                    else if (ferror(tx->file_mgmt.file)) {
                        status = errno;
                        set_last_error(&tx->last_error, ETYPE_ERRNO, status);
                    }

                    MUTEX_UNLOCK(&tx->file_mgmt.file_lock);
                    break;

                case DELAY:
                    /* Insert as many zeros as are necessary to realize the
                     * specified repeat delay */
                    samples_populated = uint_min(buffer_samples_remaining,
                                                 delay_samples_remaining);

                    memset(tx_buffer_current, 0,
                           samples_populated * 2 * sizeof(uint16_t));

                    delay_samples_remaining -= (unsigned int)samples_populated;

                    if (delay_samples_remaining == 0) {
                        state = READ_FILE;
                    }
                    break;

                case PAD_TRAILING:
                    /* Populate the remainder of the buffer with zeros */
                    memset(tx_buffer_current, 0,
                            buffer_samples_remaining * 2 * sizeof(uint16_t));

                    state = DONE;
                    break;

                case DONE:
                default:
                    break;
            }

            /* Advance the buffer pointer.
             * Remember, two int16_t's make up 1 sample in the SC16Q11 format */
            buffer_samples_remaining -= (unsigned int)samples_populated;
            tx_buffer_current += (2 * samples_populated);
        }

        /* If there were no errors, transmit the data buffer */
        if (status == 0) {
            bladerf_sync_tx(s->dev, tx_buffer, samples_per_buffer, NULL,
                            timeout_ms);
        }
    }

    /* Flush zero samples through the device to ensure samples reach the RFFE
     * before we exit and then disable the TX module.
     *
     * This is a bit excessive, but sufficient for the time being. */
    if (status == 0) {
        const unsigned int num_buffers = tx->data_mgmt.num_buffers;
        unsigned int i;

        memset(tx_buffer, 0, samples_per_buffer * 2 * sizeof(int16_t));
        for (i = 0; i < (num_buffers + 1) && status == 0; i++) {
            status = bladerf_sync_tx(s->dev, tx_buffer, samples_per_buffer,
                                     NULL, timeout_ms);
        }
    }

    free(tx_buffer);
    return status;
}

/* Create a temp (binary) file from a CSV so we don't have to waste time
 * parsing it in between sending samples.
 *
 * Postconditions: TX cfg's file descriptor, filename, and format will be
 *                 changed. (On success they'll be set to the binary file,
 *                 and on failure the csv will be closed.)
 *
 * return 0 on success, CLI_RET_* on failure
 */
static int tx_csv_to_sc16q11(struct cli_state *s)
{
    struct rxtx_data *tx = s->tx;
    const char delim[] = " \r\n\t,.:";
    char buf[81] = { 0 };
    char *token, *saveptr;
    int tmp_int;
    int16_t tmp_iq[2];
    bool ok;
    int status;
    FILE *bin = NULL;
    FILE *csv = NULL;
    char *bin_name = NULL;
    int line = 1;
    unsigned int n_clamped = 0;

    assert(tx->file_mgmt.path != NULL);

    status = expand_and_open(tx->file_mgmt.path, "r", &csv);
    if (status != 0) {
        goto tx_csv_to_sc16q11_out;
    }

    bin_name = strdup(TMP_FILE_NAME);
    if (!bin_name) {
        status = CLI_RET_MEM;
        goto tx_csv_to_sc16q11_out;
    }

    status = expand_and_open(bin_name, "wb+", &bin);
    if (status != 0) {
        goto tx_csv_to_sc16q11_out;
    }

    while (fgets(buf, sizeof(buf), csv))
    {
        /* I */
        token = strtok_r(buf, delim, &saveptr);

        if (token) {
            tmp_int = str2int(token, INT16_MIN, INT16_MAX, &ok);

            if (tmp_int < SC16Q11_IQ_MIN) {
                tmp_int = SC16Q11_IQ_MIN;
                n_clamped++;
            } else if (tmp_int > SC16Q11_IQ_MAX) {
                tmp_int = SC16Q11_IQ_MAX;
                n_clamped++;
            }

            if (ok) {
                tmp_iq[0] = tmp_int;
            } else {
                cli_err(s, "tx",
                        "Line %d: Encountered invalid I value.\n", line);
                status = CLI_RET_INVPARAM;
                break;
            }

            /* Q */
            token = strtok_r(NULL, delim, &saveptr);

            if (token) {
                tmp_int = str2int(token, INT16_MIN, INT16_MAX, &ok);

                if (tmp_int < SC16Q11_IQ_MIN) {
                    tmp_int = SC16Q11_IQ_MIN;
                    n_clamped++;
                } else if (tmp_int > SC16Q11_IQ_MAX) {
                    tmp_int = SC16Q11_IQ_MAX;
                    n_clamped++;
                }

                if (ok) {
                    tmp_iq[1] = tmp_int;
                } else {
                    cli_err(s, "tx",
                            "Line %d: encountered invalid Q value.\n", line);
                    status = CLI_RET_INVPARAM;
                    break;
                }

            } else {
                cli_err(s, "tx", "Error: Q value missing.\n");
                status = CLI_RET_INVPARAM;
                break;
            }

            /* Check for extraneous tokens */
            token = strtok_r(NULL, delim, &saveptr);
            if (!token) {
                if (fwrite(tmp_iq, sizeof(tmp_iq[0]), 2, bin) != 2) {
                    status = CLI_RET_FILEOP;
                    break;
                }
            } else {
                cli_err(s, "tx",
                        "Line (%d): Encountered extra token(s).\n", line);
                status = CLI_RET_INVPARAM;
                break;
            }
        }

        line++;
    }

    if (status == 0) {
        if (feof(csv)) {
            tx->file_mgmt.format = RXTX_FMT_BIN_SC16Q11;
            free(tx->file_mgmt.path);
            tx->file_mgmt.path = bin_name;

            if (n_clamped != 0) {
                printf("  Warning: %u values clamped within DAC SC16 Q11 "
                       "range of [%d, %d].\n",
                       n_clamped, SC16Q11_IQ_MIN, SC16Q11_IQ_MAX);
            }
        } else {
            status = CLI_RET_FILEOP;
        }
    }

tx_csv_to_sc16q11_out:
    if (status != 0) {
        free(bin_name);
    }

    if (csv) {
        fclose(csv);
    }

    if (bin) {
        fclose(bin);
    }

    return status;
}

void *tx_task(void *cli_state_arg)
{
    int status = 0;
    int disable_status;
    unsigned char requests;
    enum rxtx_state task_state;
    struct cli_state *cli_state = (struct cli_state *) cli_state_arg;
    struct rxtx_data *tx = cli_state->tx;
    MUTEX *dev_lock = &cli_state->dev_lock;

    /* We expect to be in the IDLE state when this is kicked off. We could
     * also get into the shutdown state if the program exits before we
     * finish up initialization */
    task_state = rxtx_get_state(tx);
    assert(task_state == RXTX_STATE_INIT);

    set_last_error(&tx->last_error, ETYPE_BLADERF, 0);
    requests = 0;

    while (task_state != RXTX_STATE_SHUTDOWN) {
        task_state = rxtx_get_state(tx);
        switch (task_state) {
            case RXTX_STATE_INIT:
                rxtx_set_state(tx, RXTX_STATE_IDLE);
                break;

            case RXTX_STATE_IDLE:
                rxtx_task_exec_idle(tx, &requests);
                break;

            case RXTX_STATE_START:
            {
                enum error_type err_type = ETYPE_BUG;

                /* Clear out the last error */
                set_last_error(&tx->last_error, ETYPE_ERRNO, 0);

                /* Bug catcher */
                MUTEX_LOCK(&tx->file_mgmt.file_meta_lock);
                assert(tx->file_mgmt.file != NULL);
                MUTEX_UNLOCK(&tx->file_mgmt.file_meta_lock);

                /* Initialize the TX synchronous data configuration */
                status = bladerf_sync_config(cli_state->dev,
                                             BLADERF_MODULE_TX,
                                             BLADERF_FORMAT_SC16_Q11,
                                             tx->data_mgmt.num_buffers,
                                             tx->data_mgmt.samples_per_buffer,
                                             tx->data_mgmt.num_transfers,
                                             tx->data_mgmt.timeout_ms);

                if (status < 0) {
                    err_type = ETYPE_BLADERF;
                }

                if (status == 0) {
                    rxtx_set_state(tx, RXTX_STATE_RUNNING);
                } else {
                    set_last_error(&tx->last_error, err_type, status);
                    rxtx_set_state(tx, RXTX_STATE_IDLE);
                }
            }
            break;

            case RXTX_STATE_RUNNING:
                MUTEX_LOCK(dev_lock);
                status = bladerf_enable_module(cli_state->dev,
                                               tx->module, true);
                MUTEX_UNLOCK(dev_lock);

                if (status < 0) {
                    set_last_error(&tx->last_error, ETYPE_BLADERF, status);
                } else {
                    status = tx_task_exec_running(tx, cli_state);

                    if (status < 0) {
                        set_last_error(&tx->last_error, ETYPE_BLADERF, status);
                    }

                    MUTEX_LOCK(dev_lock);
                    disable_status = bladerf_enable_module(cli_state->dev,
                                                           tx->module, false);
                    MUTEX_UNLOCK(dev_lock);

                    if (status == 0 && disable_status < 0) {
                        set_last_error(
                                &tx->last_error,
                                ETYPE_BLADERF,
                                disable_status);
                    }
                }
                rxtx_set_state(tx, RXTX_STATE_STOP);
                break;

            case RXTX_STATE_STOP:
                rxtx_task_exec_stop(tx, &requests);
                break;

            case RXTX_STATE_SHUTDOWN:
                break;

            default:
                /* Bug */
                assert(0);
                rxtx_set_state(tx, RXTX_STATE_IDLE);
        }
    }

    return NULL;
}

static int tx_cmd_start(struct cli_state *s)
{
    int status = 0;

    /* Check that we're able to start up in our current state */
    status = rxtx_cmd_start_check(s, s->tx, "tx");
    if (status != 0) {
        return status;
    }

    /* Perform file conversion (if needed) and open input file */
    MUTEX_LOCK(&s->tx->file_mgmt.file_meta_lock);

    if (s->tx->file_mgmt.format == RXTX_FMT_CSV_SC16Q11) {
        status = tx_csv_to_sc16q11(s);

        if (status == 0) {
            printf("  Converted CSV to SC16 Q11 file and "
                    "switched to converted file.\n\n");
        }
    }

    if (status == 0) {
        MUTEX_LOCK(&s->tx->file_mgmt.file_lock);

        assert(s->tx->file_mgmt.format == RXTX_FMT_BIN_SC16Q11);
        status = expand_and_open(s->tx->file_mgmt.path, "rb",
                                 &s->tx->file_mgmt.file);
        MUTEX_UNLOCK(&s->tx->file_mgmt.file_lock);
    }

    MUTEX_UNLOCK(&s->tx->file_mgmt.file_meta_lock);

    if (status != 0) {
        return status;
    }

    /* Request thread to start running */
    rxtx_submit_request(s->tx, RXTX_TASK_REQ_START);
    status = rxtx_wait_for_state(s->tx, RXTX_STATE_RUNNING, 3000);

    /* This should never occur. If it does, there's likely a defect
     * present in the tx task */
    if (status != 0) {
        cli_err(s, "tx", "TX did not start up in the alloted time\n");
        status = CLI_RET_UNKNOWN;
    }

    return status;
}

static void tx_print_config(struct rxtx_data *tx)
{
    unsigned int repetitions, repeat_delay;
    struct tx_params *tx_params = tx->params;

    MUTEX_LOCK(&tx->param_lock);
    repetitions = tx_params->repeat;
    repeat_delay = tx_params->repeat_delay;
    MUTEX_UNLOCK(&tx->param_lock);

    printf("\n");
    rxtx_print_state(tx, "  State: ", "\n");
    rxtx_print_error(tx, "  Last error: ", "\n");
    rxtx_print_file(tx, "  File: ", "\n");
    rxtx_print_file_format(tx, "  File format: ", "\n");

    if (repetitions) {
        printf("  Repetitions: %u\n", repetitions);
    } else {
        printf("  Repetitions: infinite\n");
    }

    if (repeat_delay) {
        printf("  Repetition delay: %u us\n", repeat_delay);
    } else {
        printf("  Repetition delay: none\n");
    }

    rxtx_print_stream_info(tx, "  ", "\n");

    printf("\n");
}

static int tx_config(struct cli_state *s, int argc, char **argv)
{
    int i;
    char *val;
    int status;
    struct tx_params *tx_params = s->tx->params;

    assert(argc >= 2);

    if (argc == 2) {
        tx_print_config(s->tx);
        return 0;
    }

    for (i = 2; i < argc; i++) {

        status = rxtx_handle_config_param(s, s->tx, argv[0], argv[i], &val);

        if (status < 0) {
            return status;
        } else if (status == 0) {
            if (!strcasecmp("repeat", argv[i])) {
                /* Configure the number of transmission repetitions to use */
                unsigned int tmp;
                bool ok;

                tmp = str2uint(val, 0, UINT_MAX, &ok);
                if (ok) {
                    MUTEX_LOCK(&s->tx->param_lock);
                    tx_params->repeat = tmp;
                    MUTEX_UNLOCK(&s->tx->param_lock);
                } else {
                    cli_err(s, argv[0], RXTX_ERRMSG_VALUE(argv[1], val));
                    return CLI_RET_INVPARAM;
                }

            } else if (!strcasecmp("delay", argv[i])) {
                /* Configure the number of useconds between each repetition  */

                unsigned int tmp;
                bool ok;

                tmp = str2uint(val, 0, UINT_MAX, &ok);

                if (ok) {
                    MUTEX_LOCK(&s->tx->param_lock);
                    tx_params->repeat_delay = tmp;
                    MUTEX_UNLOCK(&s->tx->param_lock);
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

int cmd_tx(struct cli_state *s, int argc, char **argv)
{
    int status;

    assert(argc > 0);

    if (argc == 1) {
        tx_print_config(s->tx);
        status = 0;
    } else if (!strcasecmp(argv[1], RXTX_CMD_START)) {
        status = tx_cmd_start(s);
    } else if (!strcasecmp(argv[1], RXTX_CMD_STOP)) {
        status = rxtx_cmd_stop(s, s->tx);
    } else if (!strcasecmp(argv[1], RXTX_CMD_CONFIG)) {
        status = tx_config(s, argc, argv);
    } else if (!strcasecmp(argv[1], RXTX_CMD_WAIT)) {
        status = rxtx_handle_wait(s, s->tx, argc, argv);
    } else {
        cli_err(s, argv[0], "Invalid command: \"%s\"\n", argv[1]);
        status = CLI_RET_INVPARAM;
    }

    return status;
}
