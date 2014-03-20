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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <libbladeRF.h>

#include "rel_assert.h"
#include "test.h"
#include "log.h"
#include "minmax.h"

static struct task_args {
    struct bladerf *dev;
    pthread_mutex_t *dev_lock;

    struct test_params *p;
    pthread_t thread;
    int status;
    bool quit;
} rx_args, tx_args;

#if BLADERF_OS_WINDOWS
static void ctrlc_handler(int signal)
{
    rx_args.quit = true;
    tx_args.quit = true;
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
    rx_args.quit = true;
    tx_args.quit = true;
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

void test_init_params(struct test_params *p)
{
    memset(p, 0, sizeof(*p));

    p->loopback = BLADERF_LB_NONE;

    p->rx_count = DEFAULT_RX_COUNT;
    p->block_size = DEFAULT_BLOCK_SIZE;

    p->num_xfers = DEFAULT_STREAM_XFERS;
    p->stream_buffer_count = DEFAULT_STREAM_BUFFERS;
    p->stream_buffer_size = DEFAULT_STREAM_SAMPLES;
    p->timeout_ms = DEFAULT_STREAM_TIMEOUT;
}

static int init_module(struct bladerf *dev, struct test_params *p,
                       bladerf_module m)
{
    const char *m_str = m == BLADERF_MODULE_RX ? "RX" : "TX";
    int status;
    unsigned int samplerate_actual;
    unsigned int frequency_actual;

    status = bladerf_set_sample_rate(dev, m, p->samplerate, &samplerate_actual);

    if (status != 0) {
        log_error("Failed to set %s samplerate: %s\n",
                m_str, bladerf_strerror(status));
        return status;
    }

    status = bladerf_set_frequency(dev, m, p->frequency);

    if (status != 0) {
        log_error("Failed to set %s frequency: %s\n",
                m_str, bladerf_strerror(status));
        return status;
    }

    status = bladerf_get_frequency(dev, m, &frequency_actual);
    if (status != 0) {
        log_error("Failed to read back %s frequency: %s\n",
                m_str, bladerf_strerror(status));
        return status;
    }

    log_debug("%s Frequency = %u, %s Samplerate = %u\n",
              m_str, frequency_actual, m_str, samplerate_actual);

    return status;
}

static struct bladerf * initialize_device(struct test_params *p)
{
    struct bladerf *dev;
    int status = bladerf_open(&dev, p->device_str);
    int fpga_loaded;

    if (status != 0) {
        log_error("Failed to open device: %s\n",
                bladerf_strerror(status));
        return NULL;
    }

    fpga_loaded = bladerf_is_fpga_configured(dev);
    if (fpga_loaded < 0) {
        log_error("Failed to check FPGA state: %s\n",
                  bladerf_strerror(fpga_loaded));
        status = -1;
        goto initialize_device_out;
    } else if (fpga_loaded == 0) {
        log_error("The device's FPGA is not loaded.\n");
        status = -1;
        goto initialize_device_out;
    }

    if (p->out_file) {
        status = init_module(dev, p, BLADERF_MODULE_RX);
        if (status != 0) {
            log_error("Failed to init RX module: %s\n",
                      bladerf_strerror(status));
            goto initialize_device_out;
        }
    }

    if (p->in_file) {
        status = init_module(dev, p, BLADERF_MODULE_TX);
        if (status != 0) {
            log_error("Failed to init TX module: %s\n",
                      bladerf_strerror(status));
            goto initialize_device_out;
        }
    }

    status = bladerf_set_loopback(dev, p->loopback);
    if (status != 0) {
        log_error("Failed to set loopback mode: %s\n",
                  bladerf_strerror(status));
    } else {
        log_debug("Set loopback to %d\n", p->loopback);
    }

initialize_device_out:
    if (status != 0) {
        bladerf_close(dev);
        dev = NULL;
    }

    return dev;
}

void *rx_task(void *args)
{
    int status;
    uint8_t *samples;
    unsigned int to_rx;
    struct task_args *task = (struct task_args*) args;
    struct test_params *p = task->p;
    bool done = false;
    size_t n;

    samples = (uint8_t *)calloc(p->block_size, 2 * sizeof(int16_t));
    if (samples == NULL) {
        perror("calloc");
        return NULL;
    }

    status = bladerf_sync_config(task->dev,
                                 BLADERF_MODULE_RX,
                                 BLADERF_FORMAT_SC16_Q11,
                                 p->stream_buffer_count,
                                 p->stream_buffer_size,
                                 p->num_xfers,
                                 p->timeout_ms);

    if (status != 0) {
        log_error("Failed to initialize RX sync handle: %s\n",
                  bladerf_strerror(status));
        goto rx_task_out;
    }

    pthread_mutex_lock(task->dev_lock);
    status = bladerf_enable_module(task->dev, BLADERF_MODULE_RX, true);
    pthread_mutex_unlock(task->dev_lock);
    if (status != 0) {
        log_error("Failed to enable RX module: %s\n", bladerf_strerror(status));
        goto rx_task_out;
    }

    /* This assumption is made with the below cast */
    assert(p->block_size < UINT_MAX);
    while (!done && !task->quit) {
        to_rx = (unsigned int) u64_min(p->block_size, p->rx_count);
        status = bladerf_sync_rx(task->dev, samples, to_rx, NULL,
                                 SYNC_TIMEOUT_MS);

        if (status != 0) {
            log_error("RX failed: %s\n", bladerf_strerror(status));
            done = true;
        } else {
            log_verbose("RX'd %llu samples.\n", (unsigned long long)to_rx);
            n = fwrite(samples, 2 * sizeof(int16_t), to_rx, p->out_file);

            if (n != to_rx) {
                done = true;
                status = ferror(p->out_file);
                if (status != 0) {
                    log_error("Failed to write RX data to file: %s\n",
                              strerror(status));
                }
            } else {
                p->rx_count -= to_rx;
                done = p->rx_count == 0;
            }
        }
    }

rx_task_out:
    free(samples);

    pthread_mutex_lock(task->dev_lock);
    status = bladerf_enable_module(task->dev, BLADERF_MODULE_RX, false);
    pthread_mutex_unlock(task->dev_lock);
    if (status != 0) {
        log_error("Failed to disable RX module: %s\n", bladerf_strerror(status));
    }

    return NULL;
}

void *tx_task(void *arg)
{
    int status;
    uint8_t *samples;
    unsigned int to_tx;
    struct task_args *task = (struct task_args*) arg;
    struct test_params *p = task->p;
    bool done = false;

    samples = (uint8_t *)calloc(p->block_size, 2 * sizeof(int16_t));
    if (samples == NULL) {
        perror("calloc");
        return NULL;
    }

    status = bladerf_sync_config(task->dev,
                                 BLADERF_MODULE_TX,
                                 BLADERF_FORMAT_SC16_Q11,
                                 p->stream_buffer_count,
                                 p->stream_buffer_size,
                                 p->num_xfers,
                                 p->timeout_ms);

    if (status != 0) {
        log_error("Failed to initialize RX sync handle: %s\n",
                  bladerf_strerror(status));
        goto tx_task_out;
    }

    pthread_mutex_lock(task->dev_lock);
    status = bladerf_enable_module(task->dev, BLADERF_MODULE_TX, true);
    pthread_mutex_unlock(task->dev_lock);
    if (status != 0) {
        log_error("Failed to enable RX module: %s\n", bladerf_strerror(status));
        goto tx_task_out;
    }

    while (!done && !task->quit) {
        to_tx = (unsigned int) fread(samples, 2 * sizeof(int16_t),
                                     p->block_size, p->in_file);

        if (to_tx != 0) {
            status = bladerf_sync_tx(task->dev, samples, to_tx, NULL,
                                     SYNC_TIMEOUT_MS);

            if (status != 0) {
                log_error("TX failed: %s\n", bladerf_strerror(status));
                done = true;
            }

        } else {
            if (--p->tx_repetitions != 0 && feof(p->in_file) &&
                !ferror(p->in_file)) {

                if (fseek(p->in_file, 0, SEEK_SET) == -1) {
                    perror("fseek");
                    done = true;
                }
            } else {
                done = true;
            }
        }
    }

tx_task_out:
    free(samples);

    pthread_mutex_lock(task->dev_lock);
    status = bladerf_enable_module(task->dev, BLADERF_MODULE_TX, false);
    pthread_mutex_unlock(task->dev_lock);
    if (status != 0) {
        log_error("Failed to disable TX module: %s\n", bladerf_strerror(status));
    }

    return NULL;
}

/* For the sake of simplicity, we'll just bounce back and forth between
 * RX and TX when doing both. */
int test_run(struct test_params *p)
{
    int status;
    struct bladerf *dev;

    /* We must be sure to only make control calls
     * (e.g., bladerf_enable_module()) from a single context. This is done
     * by locking access to dev in such cases.
     *
     * We do not need to be holding the lock when making calls to
     * bladerf_sync_rx/tx, since we're only calling each of these functions from
     * a single context (as opposed to two threads both calling sync_rx).
     */
    pthread_mutex_t dev_lock;

    init_signal_handling();

    if (pthread_mutex_init(&dev_lock, NULL) != 0) {
        return -1;
    }

    dev = initialize_device(p);
    if (dev == NULL) {
        return -1;
    }


    if (p->in_file != NULL) {
        tx_args.dev = dev;
        tx_args.dev_lock = &dev_lock;
        tx_args.p = p;
        tx_args.status = 0;
        tx_args.quit = false;

        log_debug("Starting TX task\n");
        if (pthread_create(&tx_args.thread, NULL, tx_task, &tx_args) != 0) {
            fclose(p->in_file);
            p->in_file = NULL;
        }
    }

    if (p->out_file != NULL) {
        rx_args.dev = dev;
        rx_args.dev_lock = &dev_lock;
        rx_args.p = p;
        rx_args.status = 0;
        rx_args.quit = false;

        log_debug("Starting RX task\n");
        if (pthread_create(&rx_args.thread, NULL, rx_task, &rx_args) != 0) {
            fclose(p->out_file);
            p->out_file = NULL;
        }
    }

    log_debug("Running...\n");

    if (p->in_file != NULL) {
        pthread_join(tx_args.thread, NULL);
        log_debug("Joined TX task\n");
    }

    if (p->out_file != NULL) {
        pthread_join(rx_args.thread, NULL);
        log_debug("Joined  RX task\n");
    }

    status = bladerf_set_loopback(dev, BLADERF_LB_NONE);
    if (status != 0) {
        log_error("Failed to set loopback mode to 'NONE': %s\n",
                  bladerf_strerror(status));
    }

    bladerf_close(dev);

    if (p->in_file != NULL && tx_args.status != 0) {
        return -1;
    } else if (p->out_file != NULL && rx_args.status != 0) {
        return -1;
    } else {
        return 0;
    }
}
