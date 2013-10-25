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
#include <string.h>
#include <limits.h>

#include "rxtx.h"
#include "rxtx_impl.h"
#include "cmd/cmd.h"
#include "rel_assert.h"
#include <inttypes.h>

/* A "seems good enough" arbitrary minimum */
#define RXTX_BUFFERS_MIN 4

/* Dictated by bladeRF's transfer sizes */
#define RXTX_SAMPLES_MIN 1024

const struct numeric_suffix rxtx_kmg_suffixes[] = {
    { FIELD_INIT(.suffix, "K"), FIELD_INIT(.multiplier, 1024) },
    { FIELD_INIT(.suffix, "k"), FIELD_INIT(.multiplier, 1024) },
    { FIELD_INIT(.suffix, "M"), FIELD_INIT(.multiplier, 1024 * 1024)},
    { FIELD_INIT(.suffix, "m"), FIELD_INIT(.multiplier, 1024 * 1024)},
    { FIELD_INIT(.suffix, "G"), FIELD_INIT(.multiplier, 1024 * 1024 * 1024)},
    { FIELD_INIT(.suffix, "g"), FIELD_INIT(.multiplier, 1024 * 1024 * 1024)}
};

const size_t rxtx_kmg_suffixes_len =
    sizeof(rxtx_kmg_suffixes) / sizeof(rxtx_kmg_suffixes[0]);

void rxtx_set_state(struct rxtx_data *rxtx, enum rxtx_state state)
{
    pthread_mutex_lock(&rxtx->task_mgmt.lock);
    rxtx->task_mgmt.state = state;
    pthread_mutex_unlock(&rxtx->task_mgmt.lock);
}

enum rxtx_state rxtx_get_state(struct rxtx_data *rxtx)
{
    enum rxtx_state ret;

    pthread_mutex_lock(&rxtx->task_mgmt.lock);
    ret = rxtx->task_mgmt.state;
    pthread_mutex_unlock(&rxtx->task_mgmt.lock);

    return ret;
}

int rxtx_set_file_path(struct rxtx_data *rxtx, const char *file_path)
{
    int status = 0;

    pthread_mutex_lock(&rxtx->file_mgmt.file_meta_lock);

    if (rxtx->file_mgmt.path) {
        free(rxtx->file_mgmt.path);
    }

    rxtx->file_mgmt.path = strdup(file_path);
    if (!rxtx->file_mgmt.path) {
        status = CMD_RET_MEM;
    }

    pthread_mutex_unlock(&rxtx->file_mgmt.file_meta_lock);

    return status;
}

void rxtx_set_file_format(struct rxtx_data *rxtx, enum rxtx_fmt format)
{
    pthread_mutex_lock(&rxtx->file_mgmt.file_meta_lock);
    rxtx->file_mgmt.format = format;
    pthread_mutex_unlock(&rxtx->file_mgmt.file_meta_lock);
}

void rxtx_submit_request(struct rxtx_data *rxtx, unsigned char req)
{
    pthread_mutex_lock(&rxtx->task_mgmt.lock);
    rxtx->task_mgmt.req |= req;
    pthread_cond_signal(&rxtx->task_mgmt.signal_req);
    pthread_mutex_unlock(&rxtx->task_mgmt.lock);
}

unsigned char rxtx_get_requests(struct rxtx_data *rxtx, unsigned char mask)
{
    unsigned char ret;

    pthread_mutex_lock(&rxtx->task_mgmt.lock);
    ret = rxtx->task_mgmt.req;
    rxtx->task_mgmt.req &= ~mask;
    pthread_mutex_unlock(&rxtx->task_mgmt.lock);

    return ret;
}

void rxtx_print_state(struct rxtx_data *rxtx,
                      const char *prefix, const char *suffix)
{
    enum rxtx_state state = rxtx_get_state(rxtx);

    switch (state) {
        case RXTX_STATE_IDLE:
            printf("%sIdle%s", prefix, suffix);
            break;

        case RXTX_STATE_SHUTDOWN:
            printf("%sShutting down%s", prefix, suffix);
            break;

        case RXTX_STATE_RUNNING:
            printf("%sRunning%s", prefix, suffix);
            break;

        default:
            printf("%sInvalid/Unknown (BUG)%s", prefix, suffix);
    }
}

void rxtx_print_file(struct rxtx_data *rxtx,
                     const char *prefix, const char *suffix)
{
    pthread_mutex_lock(&rxtx->file_mgmt.file_meta_lock);

    printf("%s%s%s", prefix,rxtx->file_mgmt.path != NULL ?
           rxtx->file_mgmt.path : "Not configured", suffix);

    pthread_mutex_unlock(&rxtx->file_mgmt.file_meta_lock);
}

void rxtx_print_file_format(struct rxtx_data *rxtx,
                            const char *prefix, const char *suffix)
{
    enum rxtx_fmt fmt;

    pthread_mutex_lock(&rxtx->file_mgmt.file_meta_lock);
    fmt = rxtx->file_mgmt.format;
    pthread_mutex_unlock(&rxtx->file_mgmt.file_meta_lock);

    switch (fmt) {
        case RXTX_FMT_CSV_SC16Q12:
            printf("%sSC16 Q12, CSV%s", prefix, suffix);
            break;
        case RXTX_FMT_BIN_SC16Q12:
            printf("%sSC16 Q12, Binary%s", prefix, suffix);
            break;
        default:
            printf("%sNot configured%s", prefix, suffix);
    }
}

void rxtx_print_error(struct rxtx_data *rxtx,
                      const char *prefix, const char *suffix)
{
    enum error_type type;
    int val;

    get_last_error(&rxtx->last_error, &type, &val);

    if (val != 0) {
        switch (type) {
            case ETYPE_ERRNO:
                printf("%s%s%s", prefix, strerror(val), suffix);
                break;

            case ETYPE_CLI:
                printf("%s%s%s", prefix, cmd_strerror(val, 0), suffix);
                break;

            case ETYPE_BLADERF:
                printf("%s%s%s", prefix, bladerf_strerror(val), suffix);
                break;

            default:
                printf("%sBUG: Unexpected status=%d%s", prefix, val, suffix);
        }
    } else {
        printf("%sNone%s", prefix, suffix);
    }
}

void rxtx_print_stream_info(struct rxtx_data *rxtx,
                            const char *prefix, const char *suffix)
{
    unsigned int bufs, samps, xfers;

    pthread_mutex_lock(&rxtx->data_mgmt.lock);
    bufs = (unsigned int)rxtx->data_mgmt.num_buffers;
    samps = (unsigned int)rxtx->data_mgmt.samples_per_buffer;
    xfers = (unsigned int)rxtx->data_mgmt.num_transfers;
    pthread_mutex_unlock(&rxtx->data_mgmt.lock);

    printf("%s# Buffers: %u%s", prefix, bufs, suffix);
    printf("%s# Samples per buffer: %u%s", prefix, samps, suffix);
    printf("%s# Transfers: %u%s", prefix, xfers, suffix);
}

enum rxtx_fmt rxtx_str2fmt(const char *str)
{
    enum rxtx_fmt ret = RXTX_FMT_INVALID;

    if (!strcasecmp("csv", str)) {
        ret = RXTX_FMT_CSV_SC16Q12;
    } else if (!strcasecmp("bin", str)) {
        ret = RXTX_FMT_BIN_SC16Q12;
    }

    return ret;
}

struct rxtx_data *rxtx_data_alloc(bladerf_module module)
{
    struct rxtx_data *ret;

    if (module != BLADERF_MODULE_RX && module != BLADERF_MODULE_TX) {
        return NULL;
    }

    ret = malloc(sizeof(*ret));

    if (!ret) {
        return NULL;
    }

    /* Allocate and initialize module-specific parameters */
    if (module == BLADERF_MODULE_RX) {
        struct rx_params *rx_params;

        rx_params = malloc(sizeof(*rx_params));
        if (!rx_params) {
            free(ret);
            return NULL;
        } else {
            rx_params->n_samples = 100000;
            ret->params = rx_params;
        }
    } else {
        struct tx_params *tx_params;

        tx_params = malloc(sizeof(*tx_params));
        if (!tx_params) {
            free(ret);
            return NULL;
        } else {
            tx_params->repeat = 1;
            tx_params->repeat_delay = 0;
            ret->params = tx_params;
        }
    }
    pthread_mutex_init(&ret->param_lock,NULL);

    /* Initialize data management items */
    ret->data_mgmt.stream = NULL;
    ret->data_mgmt.buffers = NULL;
    ret->data_mgmt.next_idx = 0;
    ret->data_mgmt.num_buffers = 32;
    ret->data_mgmt.samples_per_buffer = 32 * 1024;
    ret->data_mgmt.num_transfers = 16;
    pthread_mutex_init(&ret->data_mgmt.lock, NULL);

    /* Initialize file management items */
    ret->file_mgmt.file = NULL;
    ret->file_mgmt.path = NULL;
    ret->file_mgmt.format = RXTX_FMT_BIN_SC16Q12;
    pthread_mutex_init(&ret->file_mgmt.file_lock, NULL);
    pthread_mutex_init(&ret->file_mgmt.file_meta_lock, NULL);

    /* Initialize task management */
   ret->task_mgmt.state = RXTX_STATE_IDLE;
   ret->task_mgmt.req = 0;
   pthread_mutex_init(&ret->task_mgmt.lock, NULL);
   pthread_cond_init(&ret->task_mgmt.signal_req, NULL);

   cli_error_init(&ret->last_error);

   ret->module = module;

   return ret;
}

int rxtx_startup(struct cli_state *s, bladerf_module module)
{
    int status;

    if (module != BLADERF_MODULE_RX && module != BLADERF_MODULE_TX) {
        return CMD_RET_INVPARAM;
    }

    if (module == BLADERF_MODULE_RX) {
        rxtx_set_state(s->rx, RXTX_STATE_IDLE);
        status = pthread_create(&s->rx->task_mgmt.thread, NULL, rx_task, s);

        if (status < 0) {
            rxtx_set_state(s->rx, RXTX_STATE_FAIL);
        }

    } else {
        rxtx_set_state(s->tx, RXTX_STATE_IDLE);
        status = pthread_create(&s->tx->task_mgmt.thread, NULL, tx_task, s);

        if (status < 0) {
            rxtx_set_state(s->tx, RXTX_STATE_FAIL);
        }
    }

    if (status < 0) {
        status = CMD_RET_UNKNOWN;
    }

    return status;
}

bool rxtx_task_running(struct rxtx_data *rxtx)
{
    return rxtx_get_state(rxtx) == RXTX_STATE_RUNNING;
}

void rxtx_shutdown(struct rxtx_data *rxtx)
{
    if (rxtx_get_state(rxtx) != RXTX_STATE_FAIL) {
        rxtx_submit_request(rxtx, RXTX_TASK_REQ_SHUTDOWN);
        pthread_join(rxtx->task_mgmt.thread, NULL);
    }

    free(rxtx->file_mgmt.path);
}

void rxtx_data_free(struct rxtx_data *rxtx)
{
    if(rxtx) {
        free(rxtx->params);
        free(rxtx);
    }
}

int rxtx_handle_config_param(struct cli_state *s, struct rxtx_data *rxtx,
                             const char *argv0, char *param, char **val)
{
    int status = 0;
    unsigned int tmp;
    bool ok;

    if (!param) {
        return CMD_RET_MEM;
    }

    *val = strchr(param, '=');

    if (!*val || strlen(&(*val)[1]) < 1) {
        cli_err(s, argv0, "No value provided for parameter \"%s\"", param);
        status = CMD_RET_INVPARAM;
    }

    if (status == 0) {
        (*val)[0] = '\0';
        (*val) = (*val) + 1;

        if (!strcasecmp("file", param)) {
            status = rxtx_set_file_path(rxtx, *val);
            if (status == 0) {
                status = 1;
            }
        } else if (!strcasecmp("format", param)) {
            enum rxtx_fmt fmt;
            fmt = rxtx_str2fmt(*val);

            if (fmt == RXTX_FMT_INVALID) {
                cli_err(s, argv0, RXTX_ERRMSG_VALUE(param, *val));
                status = CMD_RET_INVPARAM;
            } else {
                rxtx_set_file_format(rxtx, fmt);
                status = 1;
            }
        } else if (!strcasecmp("buffers", param)) {
            tmp = str2uint_suffix(*val, RXTX_BUFFERS_MIN,
                                  UINT_MAX, rxtx_kmg_suffixes,
                                  (int)rxtx_kmg_suffixes_len, &ok);

            if (!ok) {
                cli_err(s, argv0, RXTX_ERRMSG_VALUE(param, *val));
                status = CMD_RET_INVPARAM;
            } else {
                pthread_mutex_lock(&rxtx->data_mgmt.lock);
                rxtx->data_mgmt.num_buffers = tmp;
                pthread_mutex_unlock(&rxtx->data_mgmt.lock);
                status = 1;
            }

        } else if (!strcasecmp("samples", param)) {
            tmp = str2uint_suffix(*val, RXTX_BUFFERS_MIN,
                                  UINT_MAX, rxtx_kmg_suffixes,
                                  (int)rxtx_kmg_suffixes_len, &ok);

            if (!ok) {
                cli_err(s, argv0, RXTX_ERRMSG_VALUE(param, *val));
                status = CMD_RET_INVPARAM;
            } else if (tmp % RXTX_SAMPLES_MIN != 0) {
                cli_err(s, argv0,
                        "The '%s' paramter must be a multiple of %u.",
                        param, RXTX_BUFFERS_MIN);
                status = CMD_RET_INVPARAM;
            } else {
                pthread_mutex_lock(&rxtx->data_mgmt.lock);
                rxtx->data_mgmt.samples_per_buffer= tmp;
                pthread_mutex_unlock(&rxtx->data_mgmt.lock);
                status = 1;
            }
        } else if (!strcasecmp("xfers", param)) {
            tmp = str2uint_suffix(*val, RXTX_BUFFERS_MIN - 1, UINT_MAX,
                                  rxtx_kmg_suffixes, (int)rxtx_kmg_suffixes_len,
                                  &ok);

            if (!ok) {
                cli_err(s, argv0, RXTX_ERRMSG_VALUE(param, *val));
                status = CMD_RET_INVPARAM;
            } else {
                pthread_mutex_lock(&rxtx->data_mgmt.lock);
                rxtx->data_mgmt.num_transfers= tmp;
                pthread_mutex_unlock(&rxtx->data_mgmt.lock);
                status = 1;
            }
        }

    }

    return status;
}

/* Require a 10% margin on the sample rate to ensure we can keep up, and
 * warn if this margin is violated
 */
static void check_samplerate(struct cli_state *s, struct rxtx_data *rxtx)
{
    int status;
    uint64_t samplerate_min;        /* Min required sample rate */
    unsigned int samplerate_dev;    /* Device's current sample rate */
    unsigned int n_xfers, samp_per_buf;

    pthread_mutex_lock(&rxtx->data_mgmt.lock);
    n_xfers = (unsigned int)rxtx->data_mgmt.num_transfers;
    samp_per_buf = (unsigned int)rxtx->data_mgmt.samples_per_buffer;
    pthread_mutex_unlock(&rxtx->data_mgmt.lock);

    samplerate_min = (uint64_t)n_xfers * samp_per_buf;
    samplerate_min += (samplerate_min + 9) / 10;

    status = bladerf_get_sample_rate(s->dev, rxtx->module, &samplerate_dev);
    if (status < 0) {
        cli_err(s, "Error", "Failed read device's current sample rate. "
                            "Unable to perform sanity check.");
    } else if (samplerate_dev < samplerate_min) {
        if (samplerate_min <= 40000000) {
            printf("\n  Warning: The current sample rate may be too low. "
                   "For %u transfers and\n"
                   "           %u samples per buffer, a sample rate >= %"
                   PRIu64" Hz is\n           recommended to avoid timeouts.\n\n",
                   n_xfers, samp_per_buf, samplerate_min);
        } else {
            printf("\n  Warning: The current configuraion with %u transfers and"
                   "%u samples per buffer requires a sample rate above 40MHz.\n"
                   "Timeouts will likely occur with these settings.\n",
                   n_xfers, samp_per_buf);
        }
    }
}

/*   */
static int validate_stream_params(struct cli_state *s, struct rxtx_data *rxtx,
                                  const char *argv0)
{
    int status = 0;
    pthread_mutex_lock(&rxtx->data_mgmt.lock);

    /* These items will have been checked when the parameter was set */
    assert(rxtx->data_mgmt.samples_per_buffer >= RXTX_SAMPLES_MIN);
    assert(rxtx->data_mgmt.samples_per_buffer % RXTX_SAMPLES_MIN == 0);
    assert(rxtx->data_mgmt.num_buffers >= RXTX_BUFFERS_MIN);

    if (rxtx->data_mgmt.num_transfers >= rxtx->data_mgmt.num_buffers) {
        cli_err(s, argv0,
                "The 'xfers' parameter (%u) must be < the 'buffers'"
                " parameter (%u).", rxtx->data_mgmt.num_transfers,
                rxtx->data_mgmt.num_buffers);

        status = CMD_RET_INVPARAM;
    }

    pthread_mutex_unlock(&rxtx->data_mgmt.lock);
    return status;
};

int rxtx_cmd_start_check(struct cli_state *s, struct rxtx_data *rxtx,
                         const char *argv0)
{
    int status = CMD_RET_UNKNOWN;
    int fpga_status;
    bool have_file;

    if (!cli_device_is_opened(s)) {
        return CMD_RET_NODEV;
    } else if (rxtx_get_state(rxtx) != RXTX_STATE_IDLE) {
        return CMD_RET_STATE;
    } else {
        fpga_status = bladerf_is_fpga_configured(s->dev);
        if (fpga_status < 0) {
            s->last_lib_error = fpga_status;
            status = CMD_RET_LIBBLADERF;
        } else if (fpga_status != 1) {
            status = CMD_RET_NOFPGA;
        } else {
            pthread_mutex_lock(&rxtx->file_mgmt.file_meta_lock);
            have_file = (rxtx->file_mgmt.path != NULL);
            pthread_mutex_unlock(&rxtx->file_mgmt.file_meta_lock);

            if (!have_file) {
                cli_err(s, argv0, "File not configured");
                status = CMD_RET_INVPARAM;
            } else {
                status = validate_stream_params(s, rxtx, argv0);
            }

            if (status == 0) {
                check_samplerate(s, rxtx);
            }
        }
    }

    return status;
}

int rxtx_cmd_stop(struct cli_state *s, struct rxtx_data *rxtx)
{
    int status;

    if (!cli_device_is_opened(s)) {
        status = CMD_RET_NODEV;
    } else if (rxtx_get_state(rxtx) != RXTX_STATE_RUNNING) {
        status = CMD_RET_STATE;
    } else {
        rxtx_submit_request(rxtx, RXTX_TASK_REQ_STOP);
        status = 0;
    }

    return status;
}

void rxtx_task_exec_idle(struct rxtx_data *rxtx, unsigned char *requests)
{
    /* Wait until we're asked to start or shutdown */
    while (!(*requests & (RXTX_TASK_REQ_START | RXTX_TASK_REQ_SHUTDOWN))) {

        pthread_mutex_lock(&rxtx->task_mgmt.lock);
        pthread_cond_wait(&rxtx->task_mgmt.signal_req, &rxtx->task_mgmt.lock);
        *requests = rxtx->task_mgmt.req;
        pthread_mutex_unlock(&rxtx->task_mgmt.lock);
    }

    if (*requests & RXTX_TASK_REQ_SHUTDOWN) {
        rxtx_set_state(rxtx, RXTX_STATE_SHUTDOWN);
    } else if (*requests & RXTX_TASK_REQ_START) {
        rxtx_set_state(rxtx, RXTX_STATE_START);
    } else {
        /* Bug */
        assert(0);
    }

    *requests = 0;
}

void rxtx_task_exec_running(struct rxtx_data *rxtx)
{
    int status = bladerf_stream(rxtx->data_mgmt.stream, rxtx->module);

    if (status < 0) {
        set_last_error(&rxtx->last_error, ETYPE_BLADERF, status);
    }

    rxtx_set_state(rxtx, RXTX_STATE_STOP);
}

void rxtx_task_exec_stop(struct rxtx_data *rxtx, unsigned char *requests,
                         struct bladerf *dev)
{
    int status;

    *requests = rxtx_get_requests(rxtx,
                                  RXTX_TASK_REQ_STOP | RXTX_TASK_REQ_SHUTDOWN);

    pthread_mutex_lock(&rxtx->data_mgmt.lock);
    bladerf_deinit_stream(rxtx->data_mgmt.stream);
    rxtx->data_mgmt.stream = NULL;
    pthread_mutex_unlock(&rxtx->data_mgmt.lock);

    pthread_mutex_lock(&rxtx->file_mgmt.file_lock);
    if (rxtx->file_mgmt.file != NULL) {
        fclose(rxtx->file_mgmt.file);
        rxtx->file_mgmt.file = NULL;
    }
    pthread_mutex_unlock(&rxtx->file_mgmt.file_lock);

    if (*requests & RXTX_TASK_REQ_SHUTDOWN) {
        rxtx_set_state(rxtx, RXTX_STATE_SHUTDOWN);
    } else {
        rxtx_set_state(rxtx, RXTX_STATE_IDLE);
    }

    status = bladerf_enable_module(dev, rxtx->module, false);
    if (status < 0) {
        set_last_error(&rxtx->last_error, ETYPE_BLADERF, status);
    }

    *requests = 0;
}
