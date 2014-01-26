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
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>

#include "cmd.h"
#include "cmd/rxtx.h"
#include "script.h"
#include "interactive.h"

/* There's currently only ever 1 active cli_state */
static struct cli_state *cli_state;

static inline void ctrlc_handler_common(int signal)
{
    bool waiting = false;
    if (signal == SIGINT || signal == SIGTERM) {

        if (cli_state) {
            /* Unblock any rx/tx "wait" commands */
            waiting = rxtx_release_wait(cli_state->rx);
            waiting |= rxtx_release_wait(cli_state->tx);
        }

        if (!waiting) {
            /* Let out interactive support know we got a ctrl-C if we weren't just
             * waiting on an rx/tx wait command */
            interactive_ctrlc();
        }
    }
}

#if BLADERF_OS_WINDOWS
static void ctrlc_handler(int signal)
{
    ctrlc_handler_common(signal);
}

static void init_signal_handling()
{
    void *sigint_prev, *sigterm_prev;

    sigint_prev = signal(SIGINT, ctrlc_handler);
    sigterm_prev = signal(SIGTERM, ctrlc_handler);

    if (sigint_prev == SIG_ERR || sigterm_prev == SIG_ERR) {
        fprintf(stderr, "Warning: Failed to initialize Ctrl-C "
                        "handlers for rx/tx wait cmd.");
    }
}

#else
static void ctrlc_handler(int signal, siginfo_t *info, void *unused) {
    ctrlc_handler_common(signal);
}

static void init_signal_handling()
{
    struct sigaction sigact;

    sigemptyset(&sigact.sa_mask);
    sigact.sa_sigaction = ctrlc_handler;
    sigact.sa_flags = SA_SIGINFO;

    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);
}
#endif

struct cli_state *cli_state_create()
{
    cli_state = malloc(sizeof(*cli_state));

    if (cli_state) {
        cli_state->dev = NULL;
        cli_state->last_lib_error = 0;
        cli_state->scripts = NULL;

        pthread_mutex_init(&cli_state->dev_lock, NULL);

        cli_state->rx = rxtx_data_alloc(BLADERF_MODULE_RX);
        if (!cli_state->rx) {
            goto cli_state_create_fail;
        }

        cli_state->tx = rxtx_data_alloc(BLADERF_MODULE_TX);
        if (!cli_state->tx) {
            goto cli_state_create_fail;
        }

        if (rxtx_startup(cli_state, BLADERF_MODULE_RX)) {
            rxtx_data_free(cli_state->rx);
            cli_state->rx = NULL;
            goto cli_state_create_fail;
        }

        if (rxtx_startup(cli_state, BLADERF_MODULE_TX)) {
            rxtx_data_free(cli_state->tx);
            cli_state->tx = NULL;
            goto cli_state_create_fail;
        }
    }

    init_signal_handling();

    return cli_state;

cli_state_create_fail:
    cli_state_destroy(cli_state);
    return NULL;
}

void cli_state_destroy(struct cli_state *s)
{
    if (s) {
        cli_close_all_scripts(&s->scripts);

        if (s->rx) {
            rxtx_shutdown(s->rx);
            rxtx_data_free(s->rx);
            s->rx = NULL;
        }

        if (s->tx) {
            rxtx_shutdown(s->tx);
            rxtx_data_free(s->tx);
            s->tx = NULL;
        }

        if (s->dev) {
            bladerf_close(s->dev);
        }

        free(s);
    }
}

bool cli_device_is_opened(struct cli_state *s)
{
    return s->dev != NULL;
}

bool cli_device_is_streaming(struct cli_state *s)
{
    return cli_device_is_opened(s) &&
            (rxtx_task_running(s->rx) || rxtx_task_running(s->tx));
}

void cli_err(struct cli_state *s, const char *pfx, const char *format, ...)
{
    va_list arg_list;
    char lbuf[81];
    char *err;
	int ret;

    memset(lbuf, 0, sizeof(lbuf));

    /* If we're in a script, we can provide line number info */
    if (cli_script_loaded(s->scripts)) {
        ret = snprintf(lbuf, sizeof(lbuf), " (%s:%d)",
                       cli_script_file_name(s->scripts),
                       cli_script_line(s->scripts));

        if(ret < 0) {
            lbuf[0] = '\0';
        } else if(((size_t)ret) >= sizeof(lbuf)) {
            lbuf[sizeof(lbuf)-1] = '\0';
        }
    }

    /* +4 --> 1 newlines, 2 chars padding, NUL terminator */
    err = calloc(strlen(lbuf) + strlen(pfx) + strlen(format) + 4, 1);
    if (err) {
        strcat(err, pfx);
        strcat(err, lbuf);
        strcat(err, ": ");
        strcat(err, format);
        strcat(err, "\n");

        va_start(arg_list, format);
        vfprintf(stderr, err, arg_list);
        va_end(arg_list);
        free(err);
    } else {
        /* Just do the best we can if a memory allocation error occurs */
        fprintf(stderr, "\nYikes! Multiple errors occurred!\n");
    }
}

void cli_error_init(struct cli_error *e)
{
    pthread_mutex_init(&e->lock, NULL);
    e->type = ETYPE_CLI;
    e->value = 0;
}

void set_last_error(struct cli_error *e, enum error_type type, int error)
{
    pthread_mutex_lock(&e->lock);
    e->type = type;
    e->value = error;
    pthread_mutex_unlock(&e->lock);
}

void get_last_error(struct cli_error *e, enum error_type *type, int *error)
{
    pthread_mutex_lock(&e->lock);
    *type = e->type;
    *error = e->value;
    pthread_mutex_unlock(&e->lock);
}

FILE *expand_and_open(const char *filename, const char *mode)
{
    char *expanded_filename;
    FILE *ret;

    expanded_filename = interactive_expand_path(filename);
    if (expanded_filename == NULL) {
        return NULL;
    }

    ret = fopen(expanded_filename, mode);
    free(expanded_filename);
    return ret;
}
