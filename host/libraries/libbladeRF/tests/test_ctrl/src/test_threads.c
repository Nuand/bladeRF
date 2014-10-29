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

/* This test is intended to execise one or more threads making control calls
 * while concurrently runing full duplex streams via the sync interface */

#include <pthread.h>
#include "test_ctrl.h"

DECLARE_TEST_CASE(threads);

struct sync_task {
    struct bladerf *dev;
    pthread_t thread;
    pthread_mutex_t lock;
    bool launched;
    bool run;
    int status;
    bladerf_module module;
    int (*fn)(struct bladerf *dev, void *samples, unsigned int num_samples,
              struct bladerf_metadata *meata, unsigned int timeout_ms);
};

struct thread_test_case {
    const unsigned int iterations;
    const bool quiet;
    const struct test_case *test;
};

struct thread_state {
    bool launched;
    struct bladerf *dev;
    struct app_params *p;
    unsigned int failures;
    const struct thread_test_case *tc;
    pthread_t thread;
};

static const struct thread_test_case tc[] = {
    { 100,  true,   &test_case_xb200 },
    { 75,   true,   &test_case_gain },
    { 25,   true,   &test_case_bandwidth },
    { 1,    true,   &test_case_correction },
    { 1,    false,  &test_case_frequency },
};

static inline void get_sync_task_state(struct sync_task *t, bool *run)
{
    pthread_mutex_lock(&t->lock);
    *run = t->run;
    pthread_mutex_unlock(&t->lock);
}

static void * stream_task(void *arg)
{
    int status;
    struct sync_task *t = (struct sync_task *) arg;
    bool run;
    int16_t *samples = NULL;

    samples = calloc(DEFAULT_BUF_LEN, 2 * sizeof(int16_t));
    if (samples == NULL) {
        status = BLADERF_ERR_MEM;
        goto out;
    }

    status = bladerf_sync_config(t->dev, t->module,
                                 BLADERF_FORMAT_SC16_Q11,
                                 DEFAULT_NUM_BUFFERS,
                                 DEFAULT_BUF_LEN,
                                 DEFAULT_NUM_XFERS,
                                 DEFAULT_TIMEOUT_MS);
    if (status != 0) {
        goto out;
    }

    status = bladerf_enable_module(t->dev, t->module, true);
    if (status != 0) {
        goto out;
    }

    get_sync_task_state(t, &run);
    while (run && status == 0) {
        status = t->fn(t->dev, samples, DEFAULT_BUF_LEN, NULL,
                       DEFAULT_TIMEOUT_MS);

        if (status != 0) {
            PR_ERROR("%s failed with: %s\n",
                     (t->module == BLADERF_MODULE_RX ? "RX" : "TX"),
                     bladerf_strerror(status));
        }

        get_sync_task_state(t, &run);
    }


out:
    if (status == 0) {
        status = bladerf_enable_module(t->dev, t->module, false);
    } else {
        bladerf_enable_module(t->dev, t->module, false);
    }

    free(samples);
    pthread_mutex_lock(&t->lock);
    t->status = status;
    pthread_mutex_unlock(&t->lock);
    return NULL;
}

static void init_task(struct sync_task *t, struct bladerf *dev, bladerf_module m)
{
    t->dev = dev;
    pthread_mutex_init(&t->lock, NULL);
    t->launched = false;
    t->run = true;
    t->status = 0;
    t->module = m;

    if (m == BLADERF_MODULE_RX) {
        t->fn = bladerf_sync_rx;
    } else {
        t->fn = bladerf_sync_tx;
    }
}

static int launch_task(struct sync_task *t)
{
    int status = pthread_create(&t->thread, NULL, stream_task, t);
    if (status == 0) {
        t->launched = true;
    }

    return status;
}

static inline int deinit_task(struct sync_task *t)
{
    if (t->launched) {
        pthread_mutex_lock(&t->lock);
        t->run = false;
        pthread_mutex_unlock(&t->lock);
        pthread_join(t->thread, NULL);
        return t->status;
    } else {
        return 0;
    }
}


void * run_test_fn(void *arg)
{
    unsigned int i;
    struct thread_state *s = (struct thread_state *) arg;
    for (i = 0; i < s->tc->iterations; i++) {
        s->failures += s->tc->test->fn(s->dev, s->p, s->tc->quiet);
    }
    return NULL;
}

unsigned int test_threads(struct bladerf *dev, struct app_params *p, bool quiet)
{
    int status;
    unsigned int failures = 0;
    size_t i;
    struct sync_task rx, tx;
    struct thread_state *threads = NULL;
    const size_t num_threads = ARRAY_SIZE(tc);

    PRINT("%s: Running full-duplex stream with multiple control threads...\n",
          __FUNCTION__);
    PRINT("  Printing output from test_frequency for status...\n");

    init_task(&rx, dev, BLADERF_MODULE_RX);
    init_task(&tx, dev, BLADERF_MODULE_TX);

    threads = calloc(num_threads, sizeof(threads[0]));
    if (threads == NULL) {
        return 1;
    }

    for (i = 0; i < ARRAY_SIZE(tc); i++) {
        threads[i].launched = false;
        threads[i].dev = dev;
        threads[i].p = p;
        threads[i].failures = 0;
        threads[i].tc = &tc[i];
    }

    status = bladerf_set_loopback(dev, BLADERF_LB_BB_TXVGA1_RXVGA2);
    if (status != 0) {
        PR_ERROR("Failed to enable loopback: %s\n", bladerf_strerror(status));
        failures++;
        goto out;
    }

    if (launch_task(&rx) != 0) {
        PR_ERROR("%s: Failed to launch RX thread\n", __FUNCTION__);
        goto out;
    }

    if(launch_task(&tx) != 0) {
        PR_ERROR("%s: Failed to launch TX thread\n", __FUNCTION__);
        goto out;
    }

    for (i = 0; i < num_threads; i++) {
        if (!p->use_xb200 && threads[i].tc->test->fn == test_xb200) {
            continue;
        }

        status = pthread_create(&threads[i].thread, NULL,
                                run_test_fn, &threads[i]);

        if (status == 0) {
            PRINT("  Started test_%s thread...\n", threads[i].tc->test->name);
            threads[i].launched = true;
        } else {
            PR_ERROR("  Failed to start test_%s thread...\n",
                     threads[i].tc->test->name);
            failures++;
        }
    }

    for (i = 0; i < num_threads; i++) {
        if (threads[i].launched) {
            pthread_join(threads[i].thread, NULL);
            PRINT("\n  Joined test_%s thread.\n", threads[i].tc->test->name);
            failures += threads[i].failures;
        }
    }

out:
    deinit_task(&rx);
    deinit_task(&tx);

    status = bladerf_set_loopback(dev, BLADERF_LB_NONE);
    if (status != 0) {
        PR_ERROR("Failed to disable loopback: %s\n", bladerf_strerror(status));
        failures++;
        goto out;
    }

    free(threads);
    return failures;
}
