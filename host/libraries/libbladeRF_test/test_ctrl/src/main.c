/*
 * This program performs some simple operations with the various libbladeRF
 * "control" functions. It is mainly intended to only check for the presence
 * of grossly erroneous code in these functions, that would cause crashes or
 * very bizarre results.
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
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "conversions.h"
#include "test_ctrl.h"

static const struct test_case *tests[] = {
    // clang-format off
    &test_case_sampling,
    &test_case_lpf_mode,
    &test_case_enable_module,
    &test_case_loopback,
    &test_case_rx_mux,
    &test_case_xb200,
    &test_case_correction,
    &test_case_samplerate,
    &test_case_bandwidth,
    &test_case_gain,
    &test_case_frequency,
    &test_case_threads,
    // clang-format on
};

#define OPTARG "d:ft:s:T:v:hL"
static const struct option long_options[] = {
    // clang-format off
    { "device",     required_argument,  0,      'd'},
    { "fast",       no_argument,        0,      'f'},
    { "test",       required_argument,  0,      't'},
    { "seed",       required_argument,  0,      's'},
    { "help",       no_argument,        0,      'h'},
    { "xb200",      no_argument,        0,       1 },
    { "tuningmode", required_argument,  0,      'T'},
    { "list-tests", no_argument,        0,      'L'},
    { "verbosity",  required_argument,  0,      'v'},
    { 0,            0,                  0,       0 },
    // clang-format on
};

struct stats {
    bool ran;
    size_t failures;
};

void usage(const char *argv0)
{
    printf("Usage: %s [options]\n", argv0);
    printf("Exercise various libbladeRF control functions.\n");
    printf("\nOptions:\n");
    printf("  -d, --device <str>            Open the specified device.\n");
    printf("                                By default, any device is used.\n");
    printf("  -f, --fast                    Run fast test.\n");
    printf("  -t, --test <name>             Run specified test.\n");
    printf("  -s, --seed <seed>             Use specified seed for PRNG.\n");
    printf("  -h, --help                    Show this text.\n");
    printf("  --xb200                       Test XB-200 functionality.\n");
    printf("                                Device must be present.\n");
    printf("  --tuningmode <mode>           Tuning mode. 'fpga' or 'host'\n");
    printf("  -L, --list-tests              List available tests.\n");
    printf("  -v, --verbosity <level>       Set libbladeRF verbosity level.\n");
}

void list_tests()
{
    size_t i;

    printf("Available tests:\n");
    printf("-------------------------\n");
    for (i = 0; i < ARRAY_SIZE(tests); i++) {
        printf("    %s\n", tests[i]->name);
    }
    printf("\n");
}

int get_params(int argc, char *argv[], struct app_params *p)
{
    int c, idx;
    bool ok;
    bladerf_log_level level;

    memset(p, 0, sizeof(p[0]));
    p->randval_seed   = 1;
    p->tuning_mode    = BLADERF_TUNING_MODE_INVALID;
    p->module_enabled = false;
    p->fast_test      = false;

    while ((c = getopt_long(argc, argv, OPTARG, long_options, &idx)) != -1) {
        switch (c) {
            case 'd':
                if (p->device_str) {
                    fprintf(stderr, "Device already specified!\n");
                    return -1;
                } else {
                    p->device_str = strdup(optarg);
                    if (NULL == p->device_str) {
                        perror("strdup");
                        return -1;
                    }
                }
                break;

            case 'f':
                p->fast_test = true;
                break;

            case 't':
                if (p->test_name) {
                    fprintf(stderr, "Test already specified!\n");
                    return -1;
                } else {
                    p->test_name = strdup(optarg);
                    if (NULL == p->test_name) {
                        perror("strdup");
                        return -1;
                    }
                }
                break;

            case 's':
                p->randval_seed = str2uint64(optarg, 1, UINT64_MAX, &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid seed value: %s\n", optarg);
                    return -1;
                }
                break;

            case 'h':
                usage(argv[0]);
                return 1;

            case 1:
                p->use_xb200 = true;
                break;

            case 'L':
                list_tests();
                return 1;

            case 'T':
                if (0 == strcmp(optarg, "fpga")) {
                    p->tuning_mode = BLADERF_TUNING_MODE_FPGA;
                } else if (0 == strcmp(optarg, "host")) {
                    p->tuning_mode = BLADERF_TUNING_MODE_HOST;
                } else {
                    fprintf(stderr, "Invalid tuning mode: %s\n", optarg);
                    return -1;
                }
                break;

            case 'v':
                level = str2loglevel(optarg, &ok);
                if (ok) {
                    bladerf_log_set_verbosity(level);
                } else {
                    fprintf(stderr, "Invalid log level: %s\n", optarg);
                    return -1;
                }
                break;

            default:
                return -1;
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    struct app_params p;
    struct bladerf *dev = NULL;
    struct stats *stats = NULL;
    bool pass           = true;
    bladerf_xb expected, attached;
    size_t i;
    int status;

    status = get_params(argc, argv, &p);
    if (status != 0) {
        if (1 == status) {
            status = 0;
        }
        goto out;
    }

    for (i = 0; i < ARRAY_SIZE(tests); i++) {
        if (NULL == p.test_name || !strcasecmp(p.test_name, tests[i]->name)) {
            break;
        }
    }

    if (i >= ARRAY_SIZE(tests)) {
        fprintf(stderr, "Invalid test: %s\n", p.test_name);
        status = -1;
        goto out;
    }

    stats = calloc(ARRAY_SIZE(tests), sizeof(stats[0]));
    if (NULL == stats) {
        perror("calloc");
        status = -1;
        goto out;
    }

    status = bladerf_open(&dev, p.device_str);
    if (status != 0) {
        fprintf(stderr, "Failed to open device: %s\n",
                bladerf_strerror(status));
        goto out;
    }

    if (p.use_xb200) {
        expected = BLADERF_XB_200;

        status = bladerf_expansion_attach(dev, BLADERF_XB_200);
        if (status != 0) {
            fprintf(stderr, "Failed to attach XB-200: %s\n",
                    bladerf_strerror(status));
            goto out;
        }
    } else {
        expected = BLADERF_XB_NONE;
    }

    if (p.tuning_mode != BLADERF_TUNING_MODE_INVALID) {
        status = bladerf_set_tuning_mode(dev, p.tuning_mode);
        if (status != 0) {
            fprintf(stderr, "Failed to set tuning mode: %s\n",
                    bladerf_strerror(status));
            goto out;
        }
    }

    status = bladerf_expansion_get_attached(dev, &attached);
    if (status != 0) {
        fprintf(stderr, "Failed to query attached expansion board: %s\n",
                bladerf_strerror(status));
        goto out;
    }

    if (expected != attached) {
        fprintf(stderr, "Unexpected expansion board readback: %d\n", attached);
        status = -1;
        goto out;
    }

    for (i = 0; i < ARRAY_SIZE(tests); i++) {
        if (NULL == p.test_name || !strcasecmp(p.test_name, tests[i]->name)) {
            randval_init(&p.randval_state, p.randval_seed);
            stats[i].ran      = true;
            stats[i].failures = tests[i]->fn(dev, &p, false);
        }
    }

    puts("\nFailure counts");
    puts("--------------------------------------------");
    for (i = 0; i < ARRAY_SIZE(tests); i++) {
        if (stats[i].ran) {
            printf("%16s %zu\n", tests[i]->name, stats[i].failures);
        }

        if (stats[i].failures != 0) {
            pass = false;
        }
    }
    puts("");

    status = pass ? 0 : -1;

out:
    if (dev) {
        bladerf_close(dev);
    }

    free(p.device_str);
    free(p.test_name);
    free(stats);
    return status;
}
