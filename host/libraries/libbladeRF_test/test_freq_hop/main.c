/*
 * This program tunes to random frequencies, optionally while performing
 * RX/TX calls within the same thread. This was written to investigate
 * lockups reported with the bladeRF and third-party software.
 *
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
#include <getopt.h>
#include <inttypes.h>
#include <libbladeRF.h>

#include "conversions.h"
#include "test_common.h"
#include "devcfg.h"

#define TEST_OPTIONS_STR    DEVCFG_OPTIONS_BASE"i:S:"

struct app_params {
    struct devcfg dev_config;
    bool rx;
    bool tx;
    uint64_t iterations;
    uint64_t randval_seed;
    uint64_t randval_state;
};

static struct option app_long_options[] = {
    { "rx",         no_argument,        0,      1 },
    { "tx",         no_argument,        0,      2 },
    { "iterations", required_argument,  0,      'i' },
    { "seed",       required_argument,  0,      'S' },
    { NULL,         0,                  0,      0 },
};

int app_handle_args(int argc, char **argv,
                    struct option *long_options, struct app_params *p)
{
    int c;
    bool ok;

    optind = 1;
    c = getopt_long(argc, argv, TEST_OPTIONS_STR, long_options, NULL);
    while (c >= 0) {

        switch (c) {
            case 1:
                p->rx = true;
                break;

            case 2:
                p->tx = true;
                break;

            case 'i':
                p->iterations = str2uint64(optarg, 1, UINT64_MAX, &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid # iterations: %s\n", optarg);
                    return -1;
                }
                break;

            case 'S':
                p->randval_seed = str2uint64(optarg, 0, UINT64_MAX, &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid seed value: %s\n", optarg);
                    return -1;
                }
                break;
        }

        c = getopt_long(argc, argv, TEST_OPTIONS_STR, long_options, NULL);
    }

    return 0;
}

void print_usage(const char *argv0)
{
    printf("%s: Exercise frequency changes within an RX/TX thread\n", argv0);
    printf("\n");
    printf("Test-specific options:\n");
    printf("  -i, --iterations <value>  Number of iterations to run.\n");
    printf("  -S, --seed <value>        PRNG seed for random frequencies.\n");
    printf("  --rx                      Enable bladerf_sync_rx() calls.\n");
    printf("  --tx                      Enable bladerf_sync_tx() calls.\n");
    printf("                             Requires device to be in loopback mode.\n");
    printf("\n");
    devcfg_print_common_help("Device configuration arguments\n");
    printf("\n");
}

int run_test(struct bladerf *dev, struct app_params *p)
{
    int status = 0;
    int disable_status;
    uint64_t iteration;

    int16_t *rx_samples = NULL;
    int16_t *tx_samples = NULL;

    if (p->rx) {
        rx_samples = calloc(p->dev_config.samples_per_buffer, 2 * sizeof(int16_t));
        if (rx_samples == NULL) {
            status = -1;
            goto out;
        }

        status = devcfg_perform_sync_config(dev, BLADERF_MODULE_RX,
                                          BLADERF_FORMAT_SC16_Q11,
                                          &p->dev_config, true);
        if (status != 0) {
            status = -1;
            goto out;
        }
    }

    if (p->tx) {
        tx_samples = calloc(p->dev_config.samples_per_buffer, 2 * sizeof(int16_t));
        if (tx_samples == NULL) {
            status = -1;
            goto out;
        }

        status = devcfg_perform_sync_config(dev, BLADERF_MODULE_RX,
                                          BLADERF_FORMAT_SC16_Q11,
                                          &p->dev_config, true);
        if (status != 0) {
            status = -1;
            goto out;
        }
    }

    for (iteration = 0; iteration < p->iterations; iteration++) {
        if (iteration % 50 == 0) {
            printf("\rIteration: %16"PRIu64" of %-16"PRIu64,
                   iteration, p->iterations);
            fflush(stdout);
        }

        /* Tune the RX module if we're RX'ing data, or if no data
         * reception/transmission has been requested */
        if (p->rx || (!p->rx && !p->tx)) {
            unsigned int freq = get_rand_freq(&p->randval_state,
                                              p->dev_config.enable_xb200);

            status = bladerf_set_frequency(dev, BLADERF_MODULE_RX, freq);
            if (status != 0) {
                fprintf(stderr,
                        "Failed to set RX frequency to %u @ iteration %"PRIu64": %s\n",
                        freq, iteration, bladerf_strerror(status));
                status = -1;
                goto out;
            }

            if (p->rx) {
                /* Just to exercise partial buffer logic */
                const unsigned int to_rx = p->dev_config.samples_per_buffer;
                status = bladerf_sync_rx(dev, rx_samples, to_rx, NULL,
                                         p->dev_config.sync_timeout_ms);

                if (status != 0) {
                    fprintf(stderr,
                            "Failed to RX samples @ iteration %"PRIu64": %s\n",
                            iteration, bladerf_strerror(status));
                    status = -1;
                    goto out;
                }
            }
        }

        if (p->tx) {
            unsigned int freq = get_rand_freq(&p->randval_state,
                                              p->dev_config.enable_xb200);

            const unsigned int to_tx = p->dev_config.samples_per_buffer;

            status = bladerf_set_frequency(dev, BLADERF_MODULE_TX, freq);
            if (status != 0) {
                fprintf(stderr,
                        "Failed to set TX frequency to %u @ iteration %"PRIu64": %s\n",
                        freq, iteration, bladerf_strerror(status));
                status = -1;
                goto out;
            }

            /* Just to exercise partial buffer logic */
            status = bladerf_sync_rx(dev, tx_samples, to_tx, NULL,
                                     p->dev_config.sync_timeout_ms);

            if (status != 0) {
                fprintf(stderr,
                        "Failed to RX samples @ iteration %"PRIu64": %s\n",
                        iteration, bladerf_strerror(status));
                status = -1;
                goto out;
            }
        }
    }


out:
    printf("\n");

    if (p->rx) {
        disable_status = bladerf_enable_module(dev, BLADERF_MODULE_RX, false);
        if (disable_status != 0) {
            fprintf(stderr, "Failed to disable RX module: %s\n",
                    bladerf_strerror(status));

            if (status == 0) {
                status = -1;
            }
        }
    }

    if (p->tx) {
        disable_status = bladerf_enable_module(dev, BLADERF_MODULE_TX, false);
        if (disable_status != 0) {
            fprintf(stderr, "Failed to disable TX module: %s\n",
                    bladerf_strerror(status));

            if (status == 0) {
                status = -1;
            }
        }
    }

    free(rx_samples);
    free(tx_samples);
    return status;
}

int main(int argc, char *argv[])
{
    int status;
    struct bladerf *dev = NULL;
    struct app_params params;
    struct option *options = NULL;

    devcfg_init(&params.dev_config);
    params.iterations = 5000;
    params.randval_seed = 1;
    params.rx = false;
    params.tx = false;

    options = devcfg_get_long_options(app_long_options);
    if (options == NULL) {
        status = -1;
        goto error_no_dev;
    }

    status = devcfg_handle_args(argc, argv,
                                TEST_OPTIONS_STR, options,
                                &params.dev_config);
    if (status < 0) {
        status = -1;
        goto error_no_dev;
    } else if (status > 0) {
        print_usage(argv[0]);
        status = 0;
        goto error_no_dev;
    }

    randval_init(&params.randval_state, params.randval_seed);

    status = app_handle_args(argc, argv, options, &params);
    if (status != 0) {
        status = -1;
        goto error_no_dev;
    }

    if (params.tx && params.dev_config.loopback == BLADERF_LB_NONE) {
        fprintf(stderr, "--tx requires the device to be put in a loopback mode.\n");
        status = -1;
        goto error_no_dev;
    }

    status = bladerf_open(&dev, params.dev_config.device_specifier);
    if (status != 0) {
        fprintf(stderr, "Unable to open device: %s\n",
                bladerf_strerror(status));
        status = -1;
        goto error_no_dev;
    }

    status = devcfg_apply(dev, &params.dev_config);
    if (status == 0) {
        status = run_test(dev, &params);
    }


    bladerf_close(dev);

error_no_dev:
    devcfg_deinit(&params.dev_config);
    free(options);
    return status;
}
