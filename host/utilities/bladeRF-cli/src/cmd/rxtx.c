/*
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2013-2014 Nuand LLC
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
#include <pthread.h>
#include <errno.h>
#include <inttypes.h>

#include "host_config.h"

#if BLADERF_OS_WINDOWS || BLADERF_OS_OSX
#include "clock_gettime.h"
#else
#include <time.h>
#endif

#include "rxtx.h"
#include "rxtx_impl.h"
#include "cmd/cmd.h"
#include "rel_assert.h"
#include "minmax.h"
#include "thread.h"

/* A "seems good enough" arbitrary minimum */
#define RXTX_BUFFERS_MIN 4

/* Dictated by bladeRF's transfer sizes */
#define RXTX_SAMPLES_MIN 1024

#define NSEC_PER_SEC 1000000000

#define ARRAY_LEN(x) (sizeof(x) / sizeof(x[0]))

const struct numeric_suffix rxtx_time_suffixes[] = {
    { FIELD_INIT(.suffix, "s"), FIELD_INIT(.multiplier, 1000) },
    { FIELD_INIT(.suffix, "ms"), FIELD_INIT(.multiplier, 1) },
    { FIELD_INIT(.suffix, "m"), FIELD_INIT(.multiplier, 1) },
};

const struct numeric_suffix rxtx_kmg_suffixes[] = {
    { FIELD_INIT(.suffix, "K"), FIELD_INIT(.multiplier, 1024) },
    { FIELD_INIT(.suffix, "k"), FIELD_INIT(.multiplier, 1024) },
    { FIELD_INIT(.suffix, "M"), FIELD_INIT(.multiplier, 1024 * 1024)},
    { FIELD_INIT(.suffix, "m"), FIELD_INIT(.multiplier, 1024 * 1024)},
    { FIELD_INIT(.suffix, "G"), FIELD_INIT(.multiplier, 1024 * 1024 * 1024)},
    { FIELD_INIT(.suffix, "g"), FIELD_INIT(.multiplier, 1024 * 1024 * 1024)}
};

const size_t rxtx_time_suffixes_len = ARRAY_LEN(rxtx_time_suffixes);
const size_t rxtx_kmg_suffixes_len = ARRAY_LEN(rxtx_kmg_suffixes);

void rxtx_set_state(struct rxtx_data *rxtx, enum rxtx_state state)
{
    MUTEX_LOCK(&rxtx->task_mgmt.lock);
    rxtx->task_mgmt.state = state;
    pthread_cond_signal(&rxtx->task_mgmt.signal_state_change);
    MUTEX_UNLOCK(&rxtx->task_mgmt.lock);
}

enum rxtx_state rxtx_get_state(struct rxtx_data *rxtx)
{
    enum rxtx_state ret;

    MUTEX_LOCK(&rxtx->task_mgmt.lock);
    ret = rxtx->task_mgmt.state;
    MUTEX_UNLOCK(&rxtx->task_mgmt.lock);

    return ret;
}

int rxtx_set_file_path(struct rxtx_data *rxtx, const char *file_path)
{
    int status = 0;

    MUTEX_LOCK(&rxtx->file_mgmt.file_meta_lock);

    if (rxtx->file_mgmt.path) {
        free(rxtx->file_mgmt.path);
    }

    rxtx->file_mgmt.path = strdup(file_path);
    if (!rxtx->file_mgmt.path) {
        status = CLI_RET_MEM;
    }

    MUTEX_UNLOCK(&rxtx->file_mgmt.file_meta_lock);

    return status;
}

void rxtx_set_file_format(struct rxtx_data *rxtx, enum rxtx_fmt format)
{
    MUTEX_LOCK(&rxtx->file_mgmt.file_meta_lock);
    rxtx->file_mgmt.format = format;
    MUTEX_UNLOCK(&rxtx->file_mgmt.file_meta_lock);
}

void rxtx_submit_request(struct rxtx_data *rxtx, unsigned char req)
{
    MUTEX_LOCK(&rxtx->task_mgmt.lock);
    rxtx->task_mgmt.req |= req;
    pthread_cond_signal(&rxtx->task_mgmt.signal_req);
    MUTEX_UNLOCK(&rxtx->task_mgmt.lock);
}

unsigned char rxtx_get_requests(struct rxtx_data *rxtx, unsigned char mask)
{
    unsigned char ret;

    MUTEX_LOCK(&rxtx->task_mgmt.lock);
    ret = rxtx->task_mgmt.req;
    rxtx->task_mgmt.req &= ~mask;
    MUTEX_UNLOCK(&rxtx->task_mgmt.lock);

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

        case RXTX_STATE_START:
            printf("%sStarting%s", prefix, suffix);
            break;

        case RXTX_STATE_RUNNING:
            printf("%sRunning%s", prefix, suffix);
            break;

        case RXTX_STATE_STOP:
            printf("%sStopping%s", prefix, suffix);
            break;

        case RXTX_STATE_SHUTDOWN:
            printf("%sShutting down%s", prefix, suffix);
            break;

        case RXTX_STATE_FAIL:
            printf("%sFailed Initialization%s", prefix, suffix);
            break;

        default:
            printf("%sInvalid/Unknown (BUG)%s", prefix, suffix);
    }
}

void rxtx_print_file(struct rxtx_data *rxtx,
                     const char *prefix, const char *suffix)
{
    MUTEX_LOCK(&rxtx->file_mgmt.file_meta_lock);

    printf("%s%s%s", prefix,rxtx->file_mgmt.path != NULL ?
           rxtx->file_mgmt.path : "Not configured", suffix);

    MUTEX_UNLOCK(&rxtx->file_mgmt.file_meta_lock);
}

void rxtx_print_file_format(struct rxtx_data *rxtx,
                            const char *prefix, const char *suffix)
{
    enum rxtx_fmt fmt;

    MUTEX_LOCK(&rxtx->file_mgmt.file_meta_lock);
    fmt = rxtx->file_mgmt.format;
    MUTEX_UNLOCK(&rxtx->file_mgmt.file_meta_lock);

    switch (fmt) {
        case RXTX_FMT_CSV_SC16Q11:
            printf("%sSC16 Q11, CSV%s", prefix, suffix);
            break;
        case RXTX_FMT_BIN_SC16Q11:
            printf("%sSC16 Q11, Binary%s", prefix, suffix);
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
                printf("%s%s%s", prefix, cli_strerror(val, 0), suffix);
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
    unsigned int bufs, samps, xfers, timeout;

    MUTEX_LOCK(&rxtx->data_mgmt.lock);
    bufs = (unsigned int)rxtx->data_mgmt.num_buffers;
    samps = (unsigned int)rxtx->data_mgmt.samples_per_buffer;
    xfers = (unsigned int)rxtx->data_mgmt.num_transfers;
    timeout = rxtx->data_mgmt.timeout_ms;
    MUTEX_UNLOCK(&rxtx->data_mgmt.lock);

    printf("%s# Buffers: %u%s", prefix, bufs, suffix);
    printf("%s# Samples per buffer: %u%s", prefix, samps, suffix);
    printf("%s# Transfers: %u%s", prefix, xfers, suffix);
    printf("%sTimeout (ms): %u%s", prefix, timeout, suffix);
}

enum rxtx_fmt rxtx_str2fmt(const char *str)
{
    enum rxtx_fmt ret = RXTX_FMT_INVALID;

    if (!strcasecmp("csv", str)) {
        ret = RXTX_FMT_CSV_SC16Q11;
    } else if (!strcasecmp("bin", str)) {
        ret = RXTX_FMT_BIN_SC16Q11;
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
    MUTEX_INIT(&ret->param_lock);

    /* Initialize data management items */
    ret->data_mgmt.num_buffers = 32;
    ret->data_mgmt.samples_per_buffer = 32 * 1024;
    ret->data_mgmt.num_transfers = 16;
    ret->data_mgmt.timeout_ms = 1000;

    MUTEX_INIT(&ret->data_mgmt.lock);

    /* Initialize file management items */
    ret->file_mgmt.file = NULL;
    ret->file_mgmt.path = NULL;
    ret->file_mgmt.format = RXTX_FMT_BIN_SC16Q11;
    MUTEX_INIT(&ret->file_mgmt.file_lock);
    MUTEX_INIT(&ret->file_mgmt.file_meta_lock);

    /* Initialize task management */
    ret->task_mgmt.started = false;
    ret->task_mgmt.state = RXTX_STATE_IDLE;
    ret->task_mgmt.req = 0;
    MUTEX_INIT(&ret->task_mgmt.lock);
    pthread_cond_init(&ret->task_mgmt.signal_req, NULL);
    pthread_cond_init(&ret->task_mgmt.signal_done, NULL);
    pthread_cond_init(&ret->task_mgmt.signal_state_change, NULL);
    ret->task_mgmt.main_task_waiting = false;

    cli_error_init(&ret->last_error);

    ret->module = module;

    return ret;
}

int rxtx_startup(struct cli_state *s, bladerf_module module)
{
    int status;

    if (module != BLADERF_MODULE_RX && module != BLADERF_MODULE_TX) {
        return CLI_RET_INVPARAM;
    }

    if (module == BLADERF_MODULE_RX) {
        rxtx_set_state(s->rx, RXTX_STATE_INIT);
        status = pthread_create(&s->rx->task_mgmt.thread, NULL, rx_task, s);

        if (status < 0) {
            rxtx_set_state(s->rx, RXTX_STATE_FAIL);
        } else {
            status = rxtx_wait_for_state(s->rx, RXTX_STATE_IDLE, 0);
            if (status == 0) {
                s->rx->task_mgmt.started = true;
            }
        }

    } else {
        rxtx_set_state(s->tx, RXTX_STATE_INIT);
        status = pthread_create(&s->tx->task_mgmt.thread, NULL, tx_task, s);

        if (status < 0) {
            rxtx_set_state(s->tx, RXTX_STATE_FAIL);
        } else {
            status = rxtx_wait_for_state(s->tx, RXTX_STATE_IDLE, 0);
            if (status == 0) {
                s->tx->task_mgmt.started = true;
            }
        }
    }

    if (status < 0) {
        status = CLI_RET_UNKNOWN;
    }

    return status;
}

bool rxtx_task_running(struct rxtx_data *rxtx)
{
    return rxtx_get_state(rxtx) == RXTX_STATE_RUNNING;
}

void rxtx_shutdown(struct rxtx_data *rxtx)
{
    if (rxtx->task_mgmt.started && rxtx_get_state(rxtx) != RXTX_STATE_FAIL) {
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
        return CLI_RET_MEM;
    }

    *val = strchr(param, '=');

    if (!*val || strlen(&(*val)[1]) < 1) {
        cli_err(s, argv0, "No value provided for parameter \"%s\"\n", param);
        status = CLI_RET_INVPARAM;
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
                status = CLI_RET_INVPARAM;
            } else {
                rxtx_set_file_format(rxtx, fmt);
                status = 1;
            }
        } else if (!strcasecmp("buffers", param)) {
            tmp = str2uint_suffix(*val, RXTX_BUFFERS_MIN,
                                  UINT_MAX, rxtx_kmg_suffixes,
                                  rxtx_kmg_suffixes_len, &ok);

            if (!ok) {
                cli_err(s, argv0, RXTX_ERRMSG_VALUE(param, *val));
                status = CLI_RET_INVPARAM;
            } else {
                MUTEX_LOCK(&rxtx->data_mgmt.lock);
                rxtx->data_mgmt.num_buffers = tmp;
                MUTEX_UNLOCK(&rxtx->data_mgmt.lock);
                status = 1;
            }

        } else if (!strcasecmp("samples", param)) {
            tmp = str2uint_suffix(*val, RXTX_BUFFERS_MIN,
                                  UINT_MAX, rxtx_kmg_suffixes,
                                  rxtx_kmg_suffixes_len, &ok);

            if (!ok) {
                cli_err(s, argv0, RXTX_ERRMSG_VALUE(param, *val));
                status = CLI_RET_INVPARAM;
            } else if (tmp % RXTX_SAMPLES_MIN != 0) {
                cli_err(s, argv0,
                        "The '%s' paramter must be a multiple of %u.\n",
                        param, RXTX_SAMPLES_MIN);
                status = CLI_RET_INVPARAM;
            } else {
                MUTEX_LOCK(&rxtx->data_mgmt.lock);
                rxtx->data_mgmt.samples_per_buffer= tmp;
                MUTEX_UNLOCK(&rxtx->data_mgmt.lock);
                status = 1;
            }
        } else if (!strcasecmp("xfers", param)) {
            tmp = str2uint_suffix(*val, RXTX_BUFFERS_MIN - 1, UINT_MAX,
                                  rxtx_kmg_suffixes, rxtx_kmg_suffixes_len,
                                  &ok);

            if (!ok) {
                cli_err(s, argv0, RXTX_ERRMSG_VALUE(param, *val));
                status = CLI_RET_INVPARAM;
            } else {
                MUTEX_LOCK(&rxtx->data_mgmt.lock);
                rxtx->data_mgmt.num_transfers= tmp;
                MUTEX_UNLOCK(&rxtx->data_mgmt.lock);
                status = 1;
            }
        } else if (!strcasecmp("timeout", param)) {
            tmp = str2uint_suffix(*val, 1, UINT_MAX, rxtx_time_suffixes,
                                  rxtx_time_suffixes_len, &ok);

            if (!ok) {
                cli_err(s, argv0, RXTX_ERRMSG_VALUE(param, *val));
                status = CLI_RET_INVPARAM;
            } else {
                MUTEX_LOCK(&rxtx->data_mgmt.lock);
                rxtx->data_mgmt.timeout_ms = tmp;
                MUTEX_UNLOCK(&rxtx->data_mgmt.lock);
                status = 1;
            }
        }
    }

    return status;
}

/* Require a 10% margin on the sample rate to ensure we can keep up, and
 * warn if this margin is violated.
 *
 * We effectively ensuring:
 *
 * Min sample rate > (xfers / timeout period) * samples / buffer
 *                    ^
 *  1 xfer = buffer --'
 */
static void check_samplerate(struct cli_state *s, struct rxtx_data *rxtx)
{
    int status;
    uint64_t samplerate_min;        /* Min required sample rate */
    unsigned int samplerate_dev;    /* Device's current sample rate */
    unsigned int n_xfers, samp_per_buf, timeout_ms;

    MUTEX_LOCK(&rxtx->data_mgmt.lock);
    n_xfers = (unsigned int)rxtx->data_mgmt.num_transfers;
    samp_per_buf = (unsigned int)rxtx->data_mgmt.samples_per_buffer;
    timeout_ms = rxtx->data_mgmt.timeout_ms;
    MUTEX_UNLOCK(&rxtx->data_mgmt.lock);

    samplerate_min = (uint64_t)n_xfers * samp_per_buf * 1000 / timeout_ms;
    samplerate_min += (samplerate_min + 9) / 10;

    status = bladerf_get_sample_rate(s->dev, rxtx->module, &samplerate_dev);
    if (status < 0) {
        cli_err(s, "Error", "Failed read device's current sample rate. "
                            "Unable to perform sanity check.\n");
    } else if (samplerate_dev < samplerate_min) {
        if (samplerate_min <= 40000000) {
            printf("\n");
            printf("    Warning: The current sample rate may be too low. For %u transfers,\n"
                   "    %u samples per buffer, and a %u ms timeout, a sample rate\n"
                   "    over %"PRIu64" Hz may be required. Alternatively, the 'timeout'\n"
                   "    parameter could be increased, but may yield underruns.\n\n",
                   n_xfers, samp_per_buf, timeout_ms, samplerate_min);
        } else {
            printf("\n");
            printf("    Warning: The current configuraion with %u transfers,\n"
                   "    %u samples per buffer, and a %u ms timeout requires a\n"
                   "    sample rate above 40MHz. Timeouts may occur with these settings.\n\n",
                   n_xfers, samp_per_buf, timeout_ms);
        }
    }
}

/*   */
static int validate_stream_params(struct cli_state *s, struct rxtx_data *rxtx,
                                  const char *argv0)
{
    int status = 0;
    MUTEX_LOCK(&rxtx->data_mgmt.lock);

    /* These items will have been checked when the parameter was set */
    assert(rxtx->data_mgmt.samples_per_buffer >= RXTX_SAMPLES_MIN);
    assert(rxtx->data_mgmt.samples_per_buffer % RXTX_SAMPLES_MIN == 0);
    assert(rxtx->data_mgmt.num_buffers >= RXTX_BUFFERS_MIN);

    if (rxtx->data_mgmt.num_transfers >= rxtx->data_mgmt.num_buffers) {
        cli_err(s, argv0,
                "The 'xfers' parameter (%u) must be < the 'buffers'"
                " parameter (%u).\n", rxtx->data_mgmt.num_transfers,
                rxtx->data_mgmt.num_buffers);

        status = CLI_RET_INVPARAM;
    }

    MUTEX_UNLOCK(&rxtx->data_mgmt.lock);
    return status;
};

int rxtx_cmd_start_check(struct cli_state *s, struct rxtx_data *rxtx,
                         const char *argv0)
{
    int status = CLI_RET_UNKNOWN;
    bool have_file;

    if (rxtx_get_state(rxtx) != RXTX_STATE_IDLE) {
        return CLI_RET_STATE;
    } else {
        MUTEX_LOCK(&rxtx->file_mgmt.file_meta_lock);
        have_file = (rxtx->file_mgmt.path != NULL);
        MUTEX_UNLOCK(&rxtx->file_mgmt.file_meta_lock);

        if (!have_file) {
            cli_err(s, argv0, "File not configured\n");
            status = CLI_RET_INVPARAM;
        } else {
            status = validate_stream_params(s, rxtx, argv0);
        }

        if (status == 0) {
            check_samplerate(s, rxtx);
        }
    }

    return status;
}

int rxtx_cmd_stop(struct cli_state *s, struct rxtx_data *rxtx)
{
    int status;

    if (rxtx_get_state(rxtx) != RXTX_STATE_RUNNING) {
        status = CLI_RET_STATE;
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

        MUTEX_LOCK(&rxtx->task_mgmt.lock);
        pthread_cond_wait(&rxtx->task_mgmt.signal_req, &rxtx->task_mgmt.lock);
        *requests = rxtx->task_mgmt.req;
        MUTEX_UNLOCK(&rxtx->task_mgmt.lock);
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

void rxtx_task_exec_stop(struct rxtx_data *rxtx, unsigned char *requests)
{
    *requests = rxtx_get_requests(rxtx,
                                  RXTX_TASK_REQ_STOP | RXTX_TASK_REQ_SHUTDOWN);

    MUTEX_LOCK(&rxtx->file_mgmt.file_lock);
    if (rxtx->file_mgmt.file != NULL) {
        fclose(rxtx->file_mgmt.file);
        rxtx->file_mgmt.file = NULL;
    }
    MUTEX_UNLOCK(&rxtx->file_mgmt.file_lock);

    if (*requests & RXTX_TASK_REQ_SHUTDOWN) {
        rxtx_set_state(rxtx, RXTX_STATE_SHUTDOWN);
    } else {
        rxtx_set_state(rxtx, RXTX_STATE_IDLE);
    }

    *requests = 0;

    rxtx_release_wait(rxtx);
}

int rxtx_handle_wait(struct cli_state *s, struct rxtx_data *rxtx,
                     int argc, char **argv)
{
    int status = CLI_RET_UNKNOWN;
    bool ok;
    unsigned int timeout_ms = 0;
    struct timespec timeout_abs;
    enum rxtx_state state;

    static const struct numeric_suffix times[] = {
        { "ms", 1 },
        { "s", 1000 },
        { "m", 60 * 1000 },
        { "h", 60 * 60 * 1000 },
    };

    if (argc < 2 || argc > 3) {
        return CLI_RET_NARGS;
    }

    /* The start cmd should have waited until we entered the RUNNING state */
    state = rxtx_get_state(rxtx);
    if (state != RXTX_STATE_RUNNING) {
        return 0;
    }

    if (argc == 3) {
        timeout_ms = str2uint_suffix(argv[2], 0, UINT_MAX, times,
                                     sizeof(times)/sizeof(times[0]), &ok);

        if (!ok) {
            cli_err(s, argv[0], "Invalid wait timeout: \"%s\"\n", argv[2]);
            return CLI_RET_INVPARAM;
        }
    }

    /* Release the device lock (acquired in cmd_handle()) while we wait
     * because we won't be doing device-control operations during this time.
     * This ensures that when the associated rx/tx task completes, it will be
     * able to acquire the device control lock to release this wait. */
    MUTEX_UNLOCK(&s->dev_lock);

    if (timeout_ms != 0) {
        const unsigned int timeout_sec = timeout_ms / 1000;

        status = clock_gettime(CLOCK_REALTIME, &timeout_abs);
        if (status != 0) {
            goto out;
        }

        timeout_abs.tv_sec += timeout_sec;
        timeout_abs.tv_nsec += (timeout_ms % 1000) * 1000 * 1000;

        if (timeout_abs.tv_nsec >= NSEC_PER_SEC) {
            timeout_abs.tv_sec += timeout_abs.tv_nsec / NSEC_PER_SEC;
            timeout_abs.tv_nsec %= NSEC_PER_SEC;
        }

        MUTEX_LOCK(&rxtx->task_mgmt.lock);
        rxtx->task_mgmt.main_task_waiting = true;
        while (rxtx->task_mgmt.main_task_waiting) {
            status = pthread_cond_timedwait(&rxtx->task_mgmt.signal_done,
                                            &rxtx->task_mgmt.lock,
                                            &timeout_abs);

            if (status == ETIMEDOUT) {
                rxtx->task_mgmt.main_task_waiting = false;
            }
        }
        MUTEX_UNLOCK(&rxtx->task_mgmt.lock);

    } else {
        MUTEX_LOCK(&rxtx->task_mgmt.lock);
        rxtx->task_mgmt.main_task_waiting = true;
        while (rxtx->task_mgmt.main_task_waiting) {
            status = pthread_cond_wait(&rxtx->task_mgmt.signal_done,
                                       &rxtx->task_mgmt.lock);
        }
        MUTEX_UNLOCK(&rxtx->task_mgmt.lock);
    }

out:
    /* Re-acquire the device control lock, as the top-level command handler
     * will be unlocking this when it's done */
    MUTEX_LOCK(&s->dev_lock);

    /* Expected and OK condition */
    if (status == ETIMEDOUT) {
        status = 0;
    }

    if (status != 0) {
        status = CLI_RET_UNKNOWN;
    }

    return status;
}

bool rxtx_release_wait(struct rxtx_data *rxtx)
{
    bool was_waiting = false;

    MUTEX_LOCK(&rxtx->task_mgmt.lock);
    was_waiting = rxtx->task_mgmt.main_task_waiting;
    rxtx->task_mgmt.main_task_waiting = false;
    pthread_cond_signal(&rxtx->task_mgmt.signal_done);
    MUTEX_UNLOCK(&rxtx->task_mgmt.lock);

    return was_waiting;
}

int rxtx_wait_for_state(struct rxtx_data *rxtx, enum rxtx_state req_state,
                        unsigned int timeout_ms)
{
    int status = 0;
    struct timespec timeout_abs;

    if (timeout_ms != 0) {
        const unsigned int timeout_sec = timeout_ms / 1000;

        status  = clock_gettime(CLOCK_REALTIME, &timeout_abs);
        if (status != 0) {
            return -1;
        }

        timeout_abs.tv_sec += timeout_sec;
        timeout_abs.tv_nsec += (timeout_ms % 1000) * 1000 * 1000;

        if (timeout_abs.tv_nsec >= NSEC_PER_SEC) {
            timeout_abs.tv_sec += timeout_abs.tv_nsec / NSEC_PER_SEC;
            timeout_abs.tv_nsec %= NSEC_PER_SEC;
        }

        MUTEX_LOCK(&rxtx->task_mgmt.lock);
        while (rxtx->task_mgmt.state != req_state && status == 0) {
            status = pthread_cond_timedwait(&rxtx->task_mgmt.signal_state_change,
                                            &rxtx->task_mgmt.lock,
                                            &timeout_abs);
        }
        MUTEX_UNLOCK(&rxtx->task_mgmt.lock);

    } else {
        MUTEX_LOCK(&rxtx->task_mgmt.lock);
        while (rxtx->task_mgmt.state != req_state) {
            status = pthread_cond_wait(&rxtx->task_mgmt.signal_state_change,
                                       &rxtx->task_mgmt.lock);
        }
        MUTEX_UNLOCK(&rxtx->task_mgmt.lock);
    }

    if (status == 0) {
        return 0;
    } else {
        return -1;
    }
}
