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
#include <getopt.h>

#include "conversions.h"

/* TODO Make these configurable */
#define BUFFER_SIZE 4096
#define NUM_BUFFERS 128
#define NUM_XFERS   31
#define TIMEOUT_MS  2500

#define RESET_EXPECTED  UINT32_MAX

#define OPTSTR "hs:i:t:v"
const struct option long_options[] = {
    { "help",           no_argument,        0,          'h' },
    { "device",         required_argument,  0,          'd' },
    { "samplerate",     required_argument,  0,          's' },
    { "iterations",     required_argument,  0,          'i' },
    { "verbose",        no_argument,        0,          'v' },
};

const struct numeric_suffix freq_suffixes[] = {
    { "K",   1000 },
    { "kHz", 1000 },
    { "M",   1000000 },
    { "MHz", 1000000 },
    { "G",   1000000000 },
    { "GHz", 1000000000 },
};

const struct numeric_suffix count_suffixes[] = {
    { "K", 1000 },
    { "M", 1000000 },
    { "G", 1000000000 },
};

struct app_params {
    unsigned int samplerate;
    unsigned int iterations;
    char *device_str;
};

static void print_usage(const char *argv0)
{
    printf("Usage: %s [options]\n", argv0);
    printf("libbladerf_test_discont: Test for discontinuities.\n");
    printf("\n");
    printf("Options:\n");
    printf("    -s, --samplerate <value>    Use the specified sample rate.\n");
    printf("    -t, --test <test name>      Run the specified test.\n");
    printf("    -i, --iterations <count>    Run the specified number of iterations\n");
    printf("    -d, --device <devstr>       Device argument string\n");
    printf("    -h, --help                  Print this help text.\n");
    printf("\n");
    printf("Available tests:\n");
    printf("    rx_counter  -   Received samples are derived from the internal\n"
           "                    FPGA counter. Gaps are reported.\n");
    printf("\n");
}

int handle_cmdline(int argc, char *argv[], struct app_params *p)
{
    int c, idx;
    bool ok;

    memset(p, 0, sizeof(p[0]));

    p->samplerate = 1000000;
    p->iterations = 10000;
    p->device_str = NULL;

    while ((c = getopt_long(argc, argv, OPTSTR, long_options, &idx)) >= 0) {
        switch (c) {
            case 's':
                p->samplerate = str2uint_suffix(optarg,
                                                BLADERF_SAMPLERATE_MIN,
                                                BLADERF_SAMPLERATE_REC_MAX,
                                                freq_suffixes,
                                                ARRAY_SIZE(freq_suffixes),
                                                &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid sample rate: %s\n", optarg);
                    return -1;
                }
                break;

            case 'i':
                p->iterations = str2uint_suffix(optarg,
                                                1, UINT_MAX,
                                                count_suffixes,
                                                ARRAY_SIZE(count_suffixes),
                                                &ok);

                if (!ok) {
                    fprintf(stderr, "Invalid # iterations: %s\n", optarg);
                    return -1;
                }
                break;

            case 'd':
                p->device_str = optarg;
                break;

            case 'v':
                bladerf_log_set_verbosity(BLADERF_LOG_LEVEL_VERBOSE);
                break;

            case 'h':
                print_usage(argv[0]);
                return 1;
        }
    }

    return 0;
}

int run_test(struct bladerf *dev, struct app_params *p)
{
    int status;
    uint32_t gpio_val, gpio_backup;
    unsigned int i, j;
    uint32_t *data = NULL;
    unsigned int discontinuities = 0;
    unsigned int sample_num = 0;
    unsigned int count_exp = RESET_EXPECTED;
    const unsigned int update_interval = p->samplerate / BUFFER_SIZE;

    status = bladerf_sync_config(dev,
                                 BLADERF_MODULE_RX,
                                 BLADERF_FORMAT_SC16_Q11,
                                 NUM_BUFFERS,
                                 BUFFER_SIZE,
                                 NUM_XFERS,
                                 TIMEOUT_MS);

    if (status != 0) {
        fprintf(stderr, "Failed to configure RX sync i/f: %s\n",
                bladerf_strerror(status));
        return status;
    }

    status = bladerf_config_gpio_read(dev, &gpio_val);
    if (status != 0) {
        fprintf(stderr, "Failed to read device IO configuration: %s\n",
                bladerf_strerror(status));
        return status;
    }

    /* TODO use API macro (in upcoming changeset) */
    gpio_backup = gpio_val;
    gpio_val |= 0x200;

    status = bladerf_config_gpio_write(dev, gpio_val);
    if (status != 0) {
        fprintf(stderr, "Failed to write device IO configuration: %s\n",
                bladerf_strerror(status));
        return status;
    }

    data = malloc(BUFFER_SIZE * sizeof(data[0]));
    if (data == NULL) {
        perror("malloc");
        status = BLADERF_ERR_UNEXPECTED;
        goto out;
    }

    status = bladerf_enable_module(dev, BLADERF_MODULE_RX, true);
    if (status != 0) {
        fprintf(stderr, "Failed to enable RX module: %s\n",
                bladerf_strerror(status));
        goto out;
    }

    printf("Running %u iterations.\n\n", p->iterations);

    for (i = 0, status = 0; i < p->iterations && status == 0; i++) {
        if (i % update_interval == 0) {
            printf("\rCurrent iteration %10u / %-10u", i, p->iterations);
            fflush(stdout);
        }

        status = bladerf_sync_rx(dev, data, BUFFER_SIZE, NULL, TIMEOUT_MS);
        if (status != 0) {
            fprintf(stderr, "\nRX failed: %s\n", bladerf_strerror(status));
        }

        for (j = 0; j < BUFFER_SIZE; j++) {
            if (count_exp != RESET_EXPECTED) {
                if (count_exp != data[j]) {
                    fprintf(stderr, "\nDiscontinuity @ sample %u\n",
                            sample_num + j);
                    fprintf(stderr, "   Expected 0x%08x, Got 0x%08x\n",
                            count_exp, data[j]);

                    count_exp = UINT32_MAX;
                    discontinuities++;
                } else {
                    count_exp++;
                }
            } else {
                count_exp = data[j] + 1;
            }
        }

        sample_num += BUFFER_SIZE;
    }

    printf("\n\nDone. %u discontinuities encountered.\n", discontinuities);

out:
    if (bladerf_config_gpio_write(dev, gpio_backup) != 0) {
        fprintf(stderr, "Failed to restore device IO configuration\n");
    }

    free(data);
    return status;
}

int main(int argc, char *argv[])
{
    int status;
    struct app_params params;
    struct bladerf *dev;

    status = handle_cmdline(argc, argv, &params);
    if (status != 0) {
        return status != 1 ? status : 0;
    }

    status = bladerf_open(&dev, params.device_str);
    if (status != 0) {
        fprintf(stderr, "Failed to open device: %s\n",
                bladerf_strerror(status));
        return -1;
    }

    status = bladerf_set_sample_rate(dev, BLADERF_MODULE_RX,
                                     params.samplerate, NULL);
    if (status != 0) {
        fprintf(stderr, "Failed to set RX samplerate: %s\n",
                bladerf_strerror(status));
        goto out;
    }

    status = bladerf_set_sample_rate(dev, BLADERF_MODULE_TX,
                                     params.samplerate, NULL);
    if (status != 0) {
        fprintf(stderr, "Failed to set TX samplerate: %s\n",
                bladerf_strerror(status));
        goto out;
    }

    status = run_test(dev, &params);

out:
    bladerf_close(dev);
    return status;
}
