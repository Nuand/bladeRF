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

/* The DAC range is [-2048, 2047] */
#define SC16Q11_IQ_MIN  (-2048)
#define SC16Q11_IQ_MAX  (2047)

struct tx_callback_data
{
    struct rxtx_data *tx;
    unsigned int delay;         /* How much time the callback should delay
                                 * before returning samples */
    unsigned int repeats_left;  /* Number of repeats left */
    bool repeat_inf;            /* Repeat infinitely (until stopped) */
    bool done;                  /* We're done */
};

/* This callback reads SC16 Q11 data from a file (assumed to be little endian)
 * and ships this data off in the provided sample buffers */
static void *tx_callback(struct bladerf *dev,
                         struct bladerf_stream *stream,
                         struct bladerf_metadata *meta,
                         void *samples,
                         size_t num_samples,
                         void *user_data)
{
    struct tx_callback_data *cb_data = (struct tx_callback_data*)user_data;
    struct rxtx_data *tx = cb_data->tx;
    struct tx_params *tx_params = tx->params;
    unsigned int delay = cb_data->delay;

    unsigned char requests; /* Requests from main control thread */
    size_t read_status;     /* Status from read() calls */
    size_t n_read;          /* # of int16_t I and Q values already read */
    size_t to_read;         /* # of int16_t I and Q values to attempt to read */
    bool zero_pad;          /* We need to zero-pad the rest of the buffer */

    int16_t *samples_int16 = NULL;

    /* Stop stream on STOP or SHUTDOWN, but only clear STOP. This will keep
     * the SHUTDOWN request around so we can read it when determining
     * our state transition */
    requests = rxtx_get_requests(tx, RXTX_TASK_REQ_STOP);
    if (cb_data->done ||
        requests & (RXTX_TASK_REQ_STOP | RXTX_TASK_REQ_SHUTDOWN)) {
        return NULL;
    }

    /* If we have a delay scheduled...wait around
     * TODO replace this with a buffer filled with the appropriate number
     * of 0's based upon the sample rate */
    if (delay) {
        usleep(delay);
        cb_data->delay = 0;
    }

    pthread_mutex_lock(&tx->data_mgmt.lock);
    pthread_mutex_lock(&tx->file_mgmt.file_lock);

    /* Get the next buffer */
    samples_int16 = (int16_t*)tx->data_mgmt.buffers[tx->data_mgmt.next_idx];
    tx->data_mgmt.next_idx++;
    tx->data_mgmt.next_idx %= tx->data_mgmt.num_buffers;

    n_read = 0;
    zero_pad = false;

    /* Keep reading from the file until we have enough data, or have a
     * a condition in which we'll just zero pad the rest of the buffer */
    while (n_read < (2 * tx->data_mgmt.samples_per_buffer) && !zero_pad) {
        to_read = 2 * tx->data_mgmt.samples_per_buffer - n_read;
        read_status = fread(samples_int16 + n_read, sizeof(int16_t),
                            to_read, tx->file_mgmt.file);

        if (read_status != to_read && ferror(tx->file_mgmt.file)) {
            set_last_error(&tx->last_error, ETYPE_CLI, CMD_RET_FILEOP);
            samples_int16 = NULL;
            goto tx_callback_out;
        }

        n_read += read_status;

        /* Hit the end of the file */
        if (feof(tx->file_mgmt.file)) {

            /* We're going to repeat from the start of the file  */
            if (cb_data->repeat_inf || --cb_data->repeats_left > 0) {

                pthread_mutex_lock(&tx->param_lock);
                cb_data->delay = tx_params->repeat_delay;
                pthread_mutex_unlock(&tx->param_lock);

                if (fseek(tx->file_mgmt.file, 0, SEEK_SET) < 0) {
                    set_last_error(&tx->last_error, ETYPE_ERRNO, errno);
                    samples_int16 = NULL;
                    goto tx_callback_out;
                }

                /* We have to delay first, so we'll zero out and get some
                 * data in the next callback */
                if (cb_data->delay != 0) {
                    zero_pad = true;
                }
            } else {
                /* No repeats left. Finish off whatever we have and note
                 * that the next callback should start shutting things down */
                zero_pad = true;
                cb_data->done = true;
            }
        }
    }


tx_callback_out:
    pthread_mutex_unlock(&tx->file_mgmt.file_lock);

    if (zero_pad) {
        memset(samples_int16 + n_read, 0,
               tx->data_mgmt.samples_per_buffer - n_read);
    }

    pthread_mutex_unlock(&tx->data_mgmt.lock);

    return samples_int16;
}

/* Create a temp (binary) file from a CSV so we don't have to waste time
 * parsing it in between sending samples.
 *
 * Postconditions: TX cfg's file descriptor, filename, and format will be
 *                 changed. (On success they'll be set to the binary file,
 *                 and on failure the csv will be closed.)
 *
 * return 0 on success, CMD_RET_* on failure
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

    csv = expand_and_open(tx->file_mgmt.path, "r");
    if (!csv) {
        status = CMD_RET_FILEOP;
        goto tx_csv_to_sc16q11_out;
    }

    status = 0;
    bin_name = strdup(TMP_FILE_NAME);
    if (!bin_name) {
        status = CMD_RET_MEM;
        goto tx_csv_to_sc16q11_out;
    }

    bin = expand_and_open(bin_name, "wb+");
    if (!bin) {
        status = CMD_RET_FILEOP;
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
                cli_err(s, "tx", "Line %d: Encountered invalid I value", line);
                status = CMD_RET_INVPARAM;
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
                    cli_err(s, "tx", "Line %d: encountered invalid Q value", line);
                    status = CMD_RET_INVPARAM;
                    break;
                }

            } else {
                cli_err(s, "tx", "Error: Q value missing");
                status = CMD_RET_INVPARAM;
                break;
            }

            /* Check for extraneous tokens */
            token = strtok_r(NULL, delim, &saveptr);
            if (!token) {
                if (fwrite(tmp_iq, sizeof(tmp_iq[0]), 2, bin) != 2) {
                    status = CMD_RET_FILEOP;
                    break;
                }
            } else {
                cli_err(s, "tx", "Line (%d): Encountered extra token(s)", line);
                status = CMD_RET_INVPARAM;
                break;
            }
        }

        line++;
    }

    if (status == 0 && feof(csv)) {
        tx->file_mgmt.format = RXTX_FMT_BIN_SC16Q11;
        free(tx->file_mgmt.path);
        tx->file_mgmt.path = bin_name;

        if (n_clamped != 0) {
            printf("    Warning: %u values clamped within DAC SC16 Q11 range "
                   "of [%d, %d].\n", n_clamped, SC16Q11_IQ_MIN, SC16Q11_IQ_MAX);
        }
    }

tx_csv_to_sc16q11_out:
    if (status < 0) {
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
    unsigned char requests;
    enum rxtx_state task_state;
    struct cli_state *cli_state = (struct cli_state *) cli_state_arg;
    struct rxtx_data *tx = cli_state->tx;
    struct tx_params *tx_params = tx->params;
    struct tx_callback_data cb_data;

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

                /* Set up repeat and delay parameters for this run */
                cb_data.tx = tx;
                cb_data.delay = 0;
                cb_data.done = false;

                pthread_mutex_lock(&tx->param_lock);
                cb_data.repeats_left = tx_params->repeat;
                pthread_mutex_unlock(&tx->param_lock);

                if (cb_data.repeats_left == 0) {
                    cb_data.repeat_inf = true;
                } else {
                    cb_data.repeat_inf = false;
                }

                /* Bug catcher */
                pthread_mutex_lock(&tx->file_mgmt.file_meta_lock);
                assert(tx->file_mgmt.file != NULL);
                pthread_mutex_unlock(&tx->file_mgmt.file_meta_lock);

                /* Initialize the stream */
                status = bladerf_init_stream(&tx->data_mgmt.stream,
                                             cli_state->dev,
                                             tx_callback,
                                             &tx->data_mgmt.buffers,
                                             tx->data_mgmt.num_buffers,
                                             BLADERF_FORMAT_SC16_Q11,
                                             tx->data_mgmt.samples_per_buffer,
                                             tx->data_mgmt.num_transfers,
                                             &cb_data);

                if (status < 0) {
                    err_type = ETYPE_BLADERF;
                }

                if (status == 0) {
                    rxtx_set_state(tx, RXTX_STATE_RUNNING);
                } else {
                    bladerf_deinit_stream(tx->data_mgmt.stream);
                    tx->data_mgmt.stream = NULL;
                    set_last_error(&tx->last_error, err_type, status);
                    rxtx_set_state(tx, RXTX_STATE_IDLE);
                }
            }
            break;

            case RXTX_STATE_RUNNING:
                rxtx_task_exec_running(tx, cli_state);
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
    pthread_mutex_lock(&s->tx->file_mgmt.file_meta_lock);

    if (s->tx->file_mgmt.format == RXTX_FMT_CSV_SC16Q11) {
        status = tx_csv_to_sc16q11(s);

        if (status == 0) {
            printf("    Converted CSV to SC16 Q11 file and "
                    "switched to converted file.\n\n");
        }
    }

    if (status == 0) {
        pthread_mutex_lock(&s->tx->file_mgmt.file_lock);

        assert(s->tx->file_mgmt.format == RXTX_FMT_BIN_SC16Q11);
        s->tx->file_mgmt.file = expand_and_open(s->tx->file_mgmt.path, "rb");
        if (!s->tx->file_mgmt.file) {
            set_last_error(&s->tx->last_error, ETYPE_ERRNO, errno);
            status = CMD_RET_FILEOP;
        } else {
            status = 0;
        }

        pthread_mutex_unlock(&s->tx->file_mgmt.file_lock);
    }

    pthread_mutex_unlock(&s->tx->file_mgmt.file_meta_lock);

    if (status != 0) {
        return status;
    }


    /* Set our stream timeout */
    status = bladerf_set_stream_timeout(s->dev, BLADERF_MODULE_TX,
                                        s->tx->data_mgmt.timeout_ms);

    if (status != 0) {
        s->last_lib_error = status;
        set_last_error(&s->rx->last_error, ETYPE_BLADERF, s->last_lib_error);
        return CMD_RET_LIBBLADERF;
    }

    /* Request thread to start running */
    rxtx_submit_request(s->tx, RXTX_TASK_REQ_START);
    status = rxtx_wait_for_state(s->tx, RXTX_STATE_RUNNING, 3000);

    /* This should never occur. If it does, there's likely a defect
     * present in the tx task */
    if (status != 0) {
        cli_err(s, "tx", "TX did not start up in the alloted time\n");
        status = CMD_RET_UNKNOWN;
    }

    return status;
}

static void tx_print_config(struct rxtx_data *tx)
{
    unsigned int repetitions, repeat_delay;
    struct tx_params *tx_params = tx->params;

    pthread_mutex_lock(&tx->param_lock);
    repetitions = tx_params->repeat;
    repeat_delay = tx_params->repeat_delay;
    pthread_mutex_unlock(&tx->param_lock);

    printf("\n");
    rxtx_print_state(tx, "    State: ", "\n");
    rxtx_print_error(tx, "    Last error: ", "\n");
    rxtx_print_file(tx, "    File: ", "\n");
    rxtx_print_file_format(tx, "    File format: ", "\n");

    if (repetitions) {
        printf("    Repetitions: %u\n", repetitions);
    } else {
        printf("    Repetitions: infinite\n");
    }

    if (repeat_delay) {
        printf("    Repetition delay: %u us\n", repeat_delay);
    } else {
        printf("    Repetition delay: none\n");
    }

    rxtx_print_stream_info(tx, "    ", "\n");

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
                    pthread_mutex_lock(&s->tx->param_lock);
                    tx_params->repeat = tmp;
                    pthread_mutex_unlock(&s->tx->param_lock);
                } else {
                    cli_err(s, argv[0], RXTX_ERRMSG_VALUE(argv[1], val));
                    return CMD_RET_INVPARAM;
                }

            } else if (!strcasecmp("delay", argv[i])) {
                /* Configure the number of useconds between each repetition  */

                unsigned int tmp;
                bool ok;

                tmp = str2uint(val, 0, UINT_MAX, &ok);

                if (ok) {
                    pthread_mutex_lock(&s->tx->param_lock);
                    tx_params->repeat_delay = tmp;
                    pthread_mutex_unlock(&s->tx->param_lock);
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
        cli_err(s, argv[0], "Invalid command: \"%s\"", argv[1]);
        status = CMD_RET_INVPARAM;
    }

    return status;
}
