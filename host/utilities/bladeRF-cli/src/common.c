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

#include "cmd.h"
#include "cmd/rxtx.h"

struct cli_state *cli_state_create()
{
    struct cli_state *ret = malloc(sizeof(*ret));

    if (ret) {
        ret->dev = NULL;
        ret->last_lib_error = 0;
        ret->script = NULL;
        ret->lineno = 0;

        ret->rx = rxtx_data_alloc(BLADERF_MODULE_RX);
        if (!ret->rx) {
            goto cli_state_create_fail;
        }

        ret->tx = rxtx_data_alloc(BLADERF_MODULE_TX);
        if (!ret->tx) {
            goto cli_state_create_fail;
        }

        if (rxtx_startup(ret, BLADERF_MODULE_RX)) {
            rxtx_data_free(ret->rx);
            ret->rx = NULL;
            goto cli_state_create_fail;
        }

        if (rxtx_startup(ret, BLADERF_MODULE_TX)) {
            rxtx_data_free(ret->tx);
            ret->tx = NULL;
            goto cli_state_create_fail;
        }
    }

    return ret;

cli_state_create_fail:
    cli_state_destroy(ret);
    return NULL;
}

void cli_state_destroy(struct cli_state *s)
{
    if (s) {
        if (cli_device_is_opened(s)) {
            if (rxtx_task_running(s->rx)) {
                rxtx_shutdown(s->rx);
                rxtx_data_free(s->rx);
            }

            if (rxtx_task_running(s->tx)) {
                rxtx_shutdown(s->tx);
                rxtx_data_free(s->tx);
            }

            bladerf_close(s->dev);
        }

        free(s);
    }
}

bool cli_device_is_opened(struct cli_state *s)
{
    return s->dev != NULL;
}

bool cli_device_in_use(struct cli_state *s)
{
    return cli_device_is_opened(s) &&
            (rxtx_task_running(s->rx) || rxtx_task_running(s->tx));
}

void cli_err(struct cli_state *s, const char *pfx, const char *format, ...)
{
    va_list arg_list;
    char lbuf[32];
    char *err;
	int ret;

    memset(lbuf, 0, sizeof(lbuf));

    /* If we're in a script, we can provide line number info */
    if (s->script) {
        ret = snprintf(lbuf, sizeof(lbuf), " (line %d)", s->lineno);
        if(ret < 0) {
            lbuf[0] = '\0';
        } else if(((size_t)ret) >= sizeof(lbuf)) {
            lbuf[sizeof(lbuf)-1] = '\0';
        }
    }

    /* +6 --> 3 newlines, 2 chars padding, NUL terminator */
    err = calloc(strlen(lbuf) + strlen(pfx) + strlen(format) + 6, 1);
    if (err) {
        strcat(err, "\n");
        strcat(err, pfx);
        strcat(err, lbuf);
        strcat(err, ": ");
        strcat(err, format);
        strcat(err, "\n\n");

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
