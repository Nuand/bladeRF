/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2015 Nuand LLC
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

/* This program repeatedly starts, runs, and stops RX/TX streams. */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <libbladeRF.h>

#include "devcfg.h"
#include "conversions.h"
#include "minmax.h"


#define OPTIONS DEVCFG_OPTIONS_BASE "r:S:"

const char help_prefix[] =
"Repeated stream test - Repeatedly start, run, and stop streams\n"
"Usage: libbladeRF_test_repeated_stream [options]\n\n"
"  --rx                      Run a receive stream.\n"
"  --tx                      Run a transmit stream.\n"
"  -r, --repeat <n>          Number of repetitions. Default: 1000\n"
"  -S, --samples <n>         Number of samples to process per iteration.\n"
"                            Default: 1000000\n";

#define OPTION_REPEAT       'r'
#define OPTION_NUM_SAMPLES  'S'
#define OPTION_RX           0xa0
#define OPTION_TX           0xa1


const struct option app_options[] = {
    { "repeat",   required_argument,  0,    OPTION_REPEAT       },
    { "samples",  required_argument,  0,    OPTION_NUM_SAMPLES  },
    { "rx",       no_argument,        0,    OPTION_RX           },
    { "tx",       no_argument,        0,    OPTION_TX           },
    { 0,          0,                  0,    0                   },
};

struct app_params {
    struct bladerf *device;
    struct devcfg cfg;
    unsigned int repeat;
    unsigned int num_samples;
    bool rx;
    bool tx;
};

static int handle_app_args(int argc, char *argv[],
                           const struct option *options, struct app_params *p)
{
    int c;
    bool ok;

    optind = 1;

    while ((c = getopt_long(argc, argv, OPTIONS, options, NULL)) != -1) {
        switch (c) {
            case OPTION_REPEAT:
                p->repeat = str2uint(optarg, 1, UINT_MAX, &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid repeat count: %s\n", optarg);
                    return -1;
                }
                break;

            case OPTION_NUM_SAMPLES:
                p->num_samples =
                    (unsigned int) str2double(optarg, 1, UINT_MAX, &ok);

                if (!ok) {
                    fprintf(stderr, "Invalid sample count: %s\n", optarg);
                    return -1;
                }
                break;

            case OPTION_RX:
                p->rx = true;
                break;

            case OPTION_TX:
                p->tx = true;
                break;
        }
    }

    if (!p->rx && !p->tx) {
        fprintf(stderr, "Error: --rx or --tx must be specified.\n");
        return -1;
    }

    return 0;
}

void * rx_task(void *arg)
{
    const struct app_params *p = (const struct app_params *) arg;
    unsigned int samples_left;
    unsigned int to_rx;
    unsigned int iteration;
    int status = 0;

    int16_t *samples = malloc(2 * sizeof(int16_t) * p->cfg.samples_per_buffer);
    if (!samples) {
        perror("malloc");
        goto out;
    }

    for (iteration = 0; iteration < p->repeat; iteration++) {
        status = devcfg_perform_sync_config(p->device,
                                            BLADERF_MODULE_RX,
                                            BLADERF_FORMAT_SC16_Q11,
                                            &p->cfg, true);
        if (status != 0) {
            goto out;
        }

        samples_left = p->num_samples;

        while (samples_left != 0) {
            to_rx = uint_min(samples_left, p->cfg.samples_per_buffer);
            status = bladerf_sync_rx(p->device, samples, to_rx,
                                     NULL, 2 * p->cfg.sync_timeout_ms);

            if (status != 0) {
                fprintf(stderr, "bladerf_sync_rx failed with: %s\n",
                        bladerf_strerror(status));
                goto out;
            }

            samples_left -= to_rx;
        }

        bladerf_enable_module(p->device, BLADERF_MODULE_RX, false);
        printf("RX iteration %u complete.\r\n", iteration + 1 );
    }

out:
    free(samples);
    return NULL;
}

void * tx_task(void *arg)
{
    const struct app_params *p = (const struct app_params *) arg;
    unsigned int samples_left;
    unsigned int to_tx;
    unsigned int iteration;
    int status = 0;

    int16_t *samples = calloc(p->cfg.samples_per_buffer, 2 * sizeof(int16_t));
    if (!samples) {
        perror("malloc");
        goto out;
    }

    for (iteration = 0; iteration < p->repeat; iteration++) {
        status = devcfg_perform_sync_config(p->device,
                                            BLADERF_MODULE_TX,
                                            BLADERF_FORMAT_SC16_Q11,
                                            &p->cfg, true);
        if (status != 0) {
            goto out;
        }

        samples_left = p->num_samples;

        while (samples_left != 0) {
            to_tx = uint_min(samples_left, p->cfg.samples_per_buffer);
            status = bladerf_sync_tx(p->device, samples, to_tx,
                                     NULL, 2 * p->cfg.sync_timeout_ms);

            if (status != 0) {
                fprintf(stderr, "bladerf_sync_tx failed with: %s\n",
                        bladerf_strerror(status));
                goto out;
            }

            samples_left -= to_tx;
        }

        bladerf_enable_module(p->device, BLADERF_MODULE_TX, false);
        printf("TX iteration %u complete.\r\n", iteration + 1 );
    }

out:
    free(samples);
    return NULL;
}

int main(int argc, char *argv[])
{
    int status = EXIT_FAILURE;
    struct option *options = NULL;
    struct app_params params;
    pthread_t rx_thread;
    pthread_t tx_thread;

    params.rx = false;
    params.tx = false;
    params.repeat = 1000;
    params.num_samples = 1000000;
    params.device = NULL;

    devcfg_init(&params.cfg);

    options = devcfg_get_long_options(app_options);
    if (options == NULL) {
        fprintf(stderr, "Failed to initialize options array!\n");
        return EXIT_FAILURE;
    }

    status = devcfg_handle_args(argc, argv, OPTIONS, options, &params.cfg);
    if (status < 0) {
        goto out;
    } else if (status == 1) {
        status = 0;
        devcfg_print_common_help(help_prefix);
        putchar('\n');
        goto out;
    }

    status = handle_app_args(argc, argv, options, &params);
    if (status != 0) {
        goto out;
    }

    status = bladerf_open(&params.device, params.cfg.device_specifier);
    if (status != 0) {
        fprintf(stderr, "Unable to open device: %s\n", bladerf_strerror(status));
        goto out;
    }

    status = devcfg_apply(params.device, &params.cfg);
    if (status != 0) {
        goto out;
    }

    if (params.rx) {
        status = pthread_create(&rx_thread, NULL, rx_task, &params);
        if (status != 0) {
            fprintf(stderr, "Failed to launch RX thread: %s\n", strerror(status));
            params.rx = false;
        }
    }

    if (params.tx) {
        status = pthread_create(&tx_thread, NULL, tx_task, &params);
        if (status != 0) {
            fprintf(stderr, "Failed to launch TX thread: %s\n", strerror(status));
            params.tx = false;
        }
    }

    if (params.rx) {
        pthread_join(rx_thread, NULL);
    }

    if (params.tx) {
        pthread_join(tx_thread, NULL);
    }

out:
    bladerf_close(params.device);
    free(options);
    return status;
}
