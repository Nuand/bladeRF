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

#include <libbladeRF.h>

#include "test.h"
#include "log.h"

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

/* For the sake of simplicity, we'll just bounce back and forth between
 * RX and TX when doing both. */
int test_run(struct test_params *p)
{
    int status;
    size_t n;

    struct bladerf *dev;

    uint8_t *buf_rx= NULL;
    uint8_t *buf_tx= NULL;

    bool rx_done = true;
    bool tx_done = true;

    unsigned int to_rx, to_tx;

    dev = initialize_device(p);
    if (dev == NULL) {
        return -1;
    }

    if (p->out_file) {
        buf_rx = (uint8_t*) calloc(p->block_size, 2 * sizeof(int16_t));
        if (buf_rx == NULL) {
            status = -1;
            perror("calloc");
            goto test_run_out;
        }

        status = bladerf_sync_config(dev,
                                     BLADERF_MODULE_RX,
                                     BLADERF_FORMAT_SC16_Q11,
                                     p->stream_buffer_count,
                                     p->stream_buffer_size,
                                     p->num_xfers,
                                     p->timeout_ms
                                    );

        if (status != 0) {
            log_error("Failed to intialize RX sync handle: %s\n",
                    bladerf_strerror(status));
            goto test_run_out;
        }

        status = bladerf_enable_module(dev, BLADERF_MODULE_RX, true);
        if (status != 0) {
            log_error("Failed to enable RX module: %s\n",
                      bladerf_strerror(status));
            goto test_run_out;
        }

        rx_done = false;
    }

    if (p->in_file) {
        buf_tx = (uint8_t*) calloc(p->block_size, 2 * sizeof(int16_t));
        if (buf_tx == NULL) {
            status = -1;
            perror("calloc");
            goto test_run_out;
        }

        status = bladerf_sync_config(dev,
                                     BLADERF_MODULE_TX,
                                     BLADERF_FORMAT_SC16_Q11,
                                     p->stream_buffer_count,
                                     p->stream_buffer_size,
                                     p->num_xfers,
                                     p->timeout_ms
                                    );

        if (status != 0) {
            log_error("Failed to initialize TX sync handle: %s\n",
                    bladerf_strerror(status));
            goto test_run_out;
        }

        status = bladerf_enable_module(dev, BLADERF_MODULE_TX, true);
        if (status != 0) {
            log_error("Failed to enable RX module: %s\n",
                      bladerf_strerror(status));
            goto test_run_out;
        }

        tx_done = false;
    }

    while (!rx_done || !tx_done) {
        if (!rx_done) {
            to_rx = (unsigned int)(p->rx_count > p->block_size ? p->block_size : p->rx_count);

            status = bladerf_sync_rx(dev, buf_rx, to_rx, NULL, 0);

            if (status != 0) {
                rx_done = true;
                log_error("RX failed: %s\n", bladerf_strerror(status));
            } else {
                log_verbose("RX'd %llu samples.\n", (unsigned long long)to_rx);

                n = fwrite(buf_rx, SAMPLE_SIZE_BYTES, to_rx, p->out_file);
                if (n != to_rx) {
                    rx_done = true;

                    status = ferror(p->out_file);
                    if (status != 0) {
                        log_error("Failed to write RX data to file: %s\n",
                                  strerror(status));
                    }
                } else {
                    p->rx_count -= to_rx;
                    rx_done = p->rx_count == 0;
                }
            }
        }

        if (!tx_done) {
            to_tx = (unsigned int) fread(buf_tx, SAMPLE_SIZE_BYTES, p->block_size, p->in_file);

            if (to_tx != 0) {
                status = bladerf_sync_tx(dev, buf_tx, to_tx, NULL, 0);
                if (status != 0) {
                    tx_done = true;
                    log_error("TX failed: %s\n", bladerf_strerror(status));
                } else {
                    log_verbose("TX'd %llu samples.\n",
                                (unsigned long long)to_tx);
                }

            } else {
                if (--p->tx_repetitions != 0 && feof(p->in_file) &&
                    !ferror(p->in_file)) {
                    fseek(p->in_file, 0, SEEK_SET);
                    log_debug("TX repetitions remaining: %u\n",
                                p->tx_repetitions);
                } else {
                    tx_done = true;
                }
            }
        }
    }

test_run_out:
    log_verbose("Disablng RX.\n");
    status = bladerf_enable_module(dev, BLADERF_MODULE_RX, false);
    if (status != 0) {
        log_error("Failed to disable RX module: %s\n",
                bladerf_strerror(status));
    }

    log_verbose("Disablng TX.\n");
    status = bladerf_enable_module(dev, BLADERF_MODULE_TX, false);
    if (status != 0) {
        log_error("Failed to disable RX module: %s\n",
                bladerf_strerror(status));
    }

    free(buf_rx);
    free(buf_tx);

    bladerf_close(dev);

    return status;
}
