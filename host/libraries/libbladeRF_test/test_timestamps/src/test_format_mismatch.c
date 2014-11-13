/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014 Nuand LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* This test exercises some error-checking with repect to the RX and TX
 * modules being configured with conflicting formats */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <pthread.h>
#include <libbladeRF.h>
#include "test_timestamps.h"

const struct interfaces {
    bool rx_sync;
    bool tx_sync;
} ifs[] = {
    { true,     true  },
    { true,     false },
    { false,    true  },
    { false,    false },
};

const struct test_case {
    bladerf_format rx_fmt;
    bladerf_format tx_fmt;
    int exp_status;
} tests[] = {
    { BLADERF_FORMAT_SC16_Q11, BLADERF_FORMAT_SC16_Q11, 0 },
    { BLADERF_FORMAT_SC16_Q11, BLADERF_FORMAT_SC16_Q11_META, BLADERF_ERR_INVAL },
    { BLADERF_FORMAT_SC16_Q11_META, BLADERF_FORMAT_SC16_Q11, BLADERF_ERR_INVAL  },
    { BLADERF_FORMAT_SC16_Q11_META, BLADERF_FORMAT_SC16_Q11_META, 0 },
};

typedef enum {
   TASK_UNINITIALIZED,
   TASK_RUNNING,
   TASK_SHUTDOWN_REQUESTED,
   TASK_STOPPED,
} task_state;

static struct async_task
{
    int exit_status;
    struct bladerf *dev;
    bladerf_module module;
    bladerf_format fmt;
    struct app_params *p;
    task_state state;

    void **buffers;
    size_t curr_buf_idx;
    size_t n_buffers;

    pthread_t thread;
    pthread_mutex_t lock;
} rx_task, tx_task;

void *callback(struct bladerf *dev, struct bladerf_stream *stream,
               struct bladerf_metadata *meta, void *samples, size_t num_samples,
               void *user_data)

{
    struct async_task *t = (struct async_task *) user_data;
    void *ret;


    pthread_mutex_lock(&t->lock);

    t->curr_buf_idx = (t->curr_buf_idx + 1) % t->n_buffers;
    ret = t->buffers[t->curr_buf_idx];

    switch (t->state) {
        case TASK_UNINITIALIZED:
            t->state = TASK_RUNNING;
            break;

        case TASK_RUNNING:
            break;

        case TASK_SHUTDOWN_REQUESTED:
            ret = BLADERF_STREAM_SHUTDOWN;
            break;

        default:
            fprintf(stderr, "Unexpected task state: %d\n", t->state);
            ret = BLADERF_STREAM_SHUTDOWN;
    }

    pthread_mutex_unlock(&t->lock);
    return ret;
}

static void *stream_task(void *arg)
{
    int status;
    struct bladerf_stream *stream;
    struct async_task *t = (struct async_task *) arg;

    status = bladerf_init_stream(&stream, t->dev, callback, &t->buffers,
                                 t->n_buffers, t->fmt, t->p->buf_size,
                                 t->p->num_xfers, t);

    if (status != 0) {
        t->exit_status = BLADERF_ERR_UNEXPECTED;
        fprintf(stderr, "Failed to initialized %s stream: %s\n",
                (t->module == BLADERF_MODULE_RX) ? "RX" : "TX",
                bladerf_strerror(status));
        goto out;
    }

    status = bladerf_enable_module(t->dev, t->module, true);
    if (status != 0) {
        fprintf(stderr, "Failed to enable %s module: %s\n",
                t->module == BLADERF_MODULE_RX ? "RX" : "TX",
                bladerf_strerror(status));
        goto out;
    }

    status = bladerf_stream(stream, t->module);
    bladerf_deinit_stream(stream);

out:
    pthread_mutex_lock(&t->lock);
    t->state = TASK_STOPPED;
    t->exit_status = status;
    pthread_mutex_unlock(&t->lock);

    return NULL;
}

static int enable_modules(struct bladerf *dev, bool enable)
{
    int status;

    status = bladerf_enable_module(dev, BLADERF_MODULE_RX, enable);
    if (status != 0) {
        fprintf(stderr, "Failed to %s RX module: %s\n",
                enable ? "enable" : "disable",
                bladerf_strerror(status));
        return status;
    }

    status = bladerf_enable_module(dev, BLADERF_MODULE_TX, enable);
    if (status != 0) {
        fprintf(stderr, "Failed to %s TX module: %s\n",
                enable ? "enable" : "disable",
                bladerf_strerror(status));
        return status;
    }

    return status;
}

static int init_if(struct bladerf *dev, struct app_params *p, bladerf_module m,
                   bool sync, bladerf_format fmt, int exp_status)
{
    int status;
    struct async_task *t;

    if (sync) {
        status = bladerf_sync_config(dev, m, fmt, p->num_buffers, p->buf_size,
                                     p->num_xfers, p->timeout_ms);

        if (status != exp_status) {
            fprintf(stderr,
                    "Uexpected status when configuring sync %s: %s\n",
                    m == BLADERF_MODULE_RX ? "RX" : "TX",
                    bladerf_strerror(status));
            status = -1;
        } else {
            status = 0;
        }
    } else {

        task_state state;

        switch (m) {
            case BLADERF_MODULE_RX:
                t = &rx_task;
                break;

            case BLADERF_MODULE_TX:
                t = &tx_task;
                break;

            default:
                fprintf(stderr, "Invalid module: %d\n", m);
                return -1;
        }

        t->fmt = fmt;
        t->state = state = TASK_UNINITIALIZED;

        status = pthread_create(&t->thread, NULL, stream_task, t);
        if (status != 0) {
            fprintf(stderr, "Failed to launch %s task\n",
                    m == BLADERF_MODULE_RX ? "RX" : "TX");
            return status;
        }

        do {
            usleep(25000);
            pthread_mutex_lock(&t->lock);
            state = t->state;
            pthread_mutex_unlock(&t->lock);
        } while (state == TASK_UNINITIALIZED);

        /* This may have already stopped if an invalid configuration was
         * detected. */
        if (state == TASK_STOPPED) {
            pthread_join(t->thread, NULL);
            if (t->exit_status != exp_status) {
                fprintf(stderr,
                        "Uexpected status when configuring async %s: %s\n",
                        m == BLADERF_MODULE_RX ? "RX" : "TX",
                        bladerf_strerror(status));
                status = -1;
            } else {
                status = 0;
            }
        }
    }

    return status;
}

static int deinit_if(struct bladerf *dev, bladerf_module m, bool sync,
                     int exp_status)
{
    int status = 0;

    /* Nothing to do for sync i/f -- errors are reported during the config */
    if (!sync) {
        struct async_task *t;
        task_state state;

        switch (m) {
            case BLADERF_MODULE_RX:
                t = &rx_task;
                break;

            case BLADERF_MODULE_TX:
                t = &tx_task;
                break;

            default:
                fprintf(stderr, "Invalid module: %d\n", m);
                return -1;
        }

        pthread_mutex_lock(&t->lock);
        state = t->state;
        pthread_mutex_unlock(&t->lock);

        while (state != TASK_STOPPED) {
            pthread_mutex_lock(&t->lock);
            t->state = TASK_SHUTDOWN_REQUESTED;
            pthread_mutex_unlock(&t->lock);

            usleep(25000);

            pthread_mutex_lock(&t->lock);
            state = t->state;
            pthread_mutex_unlock(&t->lock);
        }

        pthread_join(t->thread, NULL);

        if (t->exit_status != exp_status) {
            fprintf(stderr,
                    "Uexpected status when deinitializing async %s: %s\n",
                    m == BLADERF_MODULE_RX ? "RX" : "TX",
                    bladerf_strerror(t->exit_status));
            status = -1;
        } else {
            status = 0;
        }
    }

    return status;
}

static void print_test_info(int n, const struct interfaces *i,
                            const struct test_case *t)
{
    printf("\nTest case %d\n", n + 1);
    printf("-----------------------------------\n");
    printf(" RX i/f: %s\n", i->rx_sync ? "sync" : "async");
    printf(" RX fmt: %s\n", t->rx_fmt == BLADERF_FORMAT_SC16_Q11 ?
                            "SC16 Q11" : "SC16 Q11 + Metadata");
    printf(" TX i/f: %s\n", i->tx_sync ? "sync" : "async");
    printf(" TX fmt: %s\n", t->tx_fmt == BLADERF_FORMAT_SC16_Q11 ?
                            "SC16 Q11" : "SC16 Q11 + Metadata");
}

static void init_task_info(struct bladerf *dev, struct app_params *p,
                           bladerf_module m, struct async_task *t)
{
    memset(t, 0, sizeof(t[0]));

    t->state = TASK_UNINITIALIZED;
    t->dev = dev;
    t->module = m;
    t->p = p;
    t->n_buffers = 16;
    t->curr_buf_idx = 0;

    pthread_mutex_init(&t->lock, NULL);
}

int test_fn_format_mismatch(struct bladerf *dev, struct app_params *p)
{
    int status;
    size_t i, j;
    bladerf_loopback lb_backup;

    init_task_info(dev, p, BLADERF_MODULE_RX, &rx_task);
    init_task_info(dev, p, BLADERF_MODULE_TX, &tx_task);

    status = bladerf_get_loopback(dev, &lb_backup);
    if (status != 0) {
        fprintf(stderr, "Failed to get current loopback mode.\n");
        return status;
    }

    status = bladerf_set_loopback(dev, BLADERF_LB_BB_TXVGA1_RXVGA2);
    if (status != 0) {
        fprintf(stderr, "Failed to set loopback mode.\n");
        return status;
    }

    for (i = 0; i < ARRAY_SIZE(ifs); i++) {

        for (j = 0; j < ARRAY_SIZE(tests); j++) {
            const size_t test_case = i * ARRAY_SIZE(tests) + j;
            assert(test_case <= INT_MAX);
            print_test_info((int) test_case, &ifs[i], &tests[j]);

            status = enable_modules(dev, false);
            if (status != 0) {
                goto fail;
            }

            /* Init RX first, then TX */
            printf("RX -> TX...");

            status = init_if(dev, p, BLADERF_MODULE_RX, ifs[i].rx_sync,
                             tests[j].rx_fmt, 0);

            if (status != 0) {
                goto fail;
            }

            status = init_if(dev, p, BLADERF_MODULE_TX, ifs[i].tx_sync,
                             tests[j].tx_fmt, tests[j].exp_status);
            if (status != 0) {
                goto fail;
            }

            status = deinit_if(dev, BLADERF_MODULE_RX, ifs[i].rx_sync, 0);
            if (status != 0) {
                goto fail;
            }

            status = deinit_if(dev, BLADERF_MODULE_TX, ifs[i].tx_sync,
                               tests[j].exp_status);
            if (status != 0) {
                goto fail;
            }

            status = enable_modules(dev, false);
            if (status != 0) {
                goto fail;
            }

            printf("Pass.\n");

            /* Init TX first, then RX */
            printf("TX -> RX...");

            status = init_if(dev, p, BLADERF_MODULE_TX, ifs[i].tx_sync,
                             tests[j].tx_fmt, 0);

            if (status != 0) {
                goto fail;
            }

            status = init_if(dev, p, BLADERF_MODULE_RX, ifs[i].rx_sync,
                             tests[j].rx_fmt, tests[j].exp_status);
            if (status != 0) {
                goto fail;
            }

            status = deinit_if(dev, BLADERF_MODULE_TX, ifs[i].tx_sync, 0);
            if (status != 0) {
                goto fail;
            }

            status = deinit_if(dev, BLADERF_MODULE_RX, ifs[i].rx_sync,
                               tests[j].exp_status);
            if (status != 0) {
                goto fail;
            }

            status = enable_modules(dev, false);
            if (status != 0) {
                goto fail;
            }

            printf("Pass.\n");

        }
    }

fail:
    if (status != 0) {
        printf("Fail.\n");
    }
    printf("\n");

    if (bladerf_set_loopback(dev, lb_backup) != 0) {
        fprintf(stderr, "Failed to restore loopback settings.\n");
    }

    return status;
}
