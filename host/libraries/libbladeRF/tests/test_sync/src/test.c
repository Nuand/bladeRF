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
#include <libbladeRF.h>

#include "test.h"
#include "log.h"
#include "minmax.h"

struct task_args {
    struct bladerf *dev;
    struct test_params *p;
    pthread_t thread;
    int status;
    bool running;
};

void test_init_params(struct test_params *p)
{
    memset(p, 0, sizeof(*p));

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
            goto initialize_device_out;
        }
    }

    if (p->in_file) {
        status = init_module(dev, p, BLADERF_MODULE_TX);
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

    status = bladerf_enable_module(task->dev, BLADERF_MODULE_RX, true);
    if (status != 0) {
        log_error("Failed to enable RX module: %s\n", bladerf_strerror(status));
        goto rx_task_out;
    }

    while (!done) {
        to_rx = uint_min(p->block_size, p->rx_count);
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

    status = bladerf_enable_module(task->dev, BLADERF_MODULE_RX, false);
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

    status = bladerf_enable_module(task->dev, BLADERF_MODULE_TX, true);
    if (status != 0) {
        log_error("Failed to enable RX module: %s\n", bladerf_strerror(status));
        goto tx_task_out;
    }

    while (!done) {
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

    status = bladerf_enable_module(task->dev, BLADERF_MODULE_TX, false);
    if (status != 0) {
        log_error("Failed to disable TX module: %s\n", bladerf_strerror(status));
    }

    return NULL;
}

/* For the sake of simplicity, we'll just bounce back and forth between
 * RX and TX when doing both. */
int test_run(struct test_params *p)
{
    struct bladerf *dev;
    struct task_args rx_args, tx_args;

    dev = initialize_device(p);
    if (dev == NULL) {
        return -1;
    }

    if (p->in_file != NULL) {
        tx_args.dev = dev;
        tx_args.p = p;
        tx_args.status = 0;

        log_debug("Starting TX task\n");
        if (pthread_create(&tx_args.thread, NULL, tx_task, &tx_args) != 0) {
            fclose(p->in_file);
            p->in_file = NULL;
        }
    }

    if (p->out_file != NULL) {
        rx_args.dev = dev;
        rx_args.p = p;
        rx_args.status = 0;

        log_debug("Starting RX task\n");
        if (pthread_create(&rx_args.thread, NULL, rx_task, &rx_args) != 0) {
            fclose(p->out_file);
            p->out_file = NULL;
        }
    }

    log_debug("Running...\n");

    if (p->in_file != NULL) {
        log_debug("Joining TX task\n");
        pthread_join(tx_args.thread, NULL);
    }

    if (p->out_file != NULL) {
        log_debug("Joining RX task\n");
        pthread_join(rx_args.thread, NULL);
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
