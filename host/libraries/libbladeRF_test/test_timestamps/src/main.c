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
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <libbladeRF.h>
#include "conversions.h"
#include "test_timestamps.h"

#define OPTSTR "hd:s:S:t:B:v:n:x:T:"


#define DECLARE_TEST(name) \
    extern int test_fn_##name(struct bladerf *, struct app_params *); \
    const struct test test_##name = { #name, test_fn_##name }

#define TEST(name) &test_##name

struct test {
    const char *name;
    int (*run)(struct bladerf *dev, struct app_params *p);
};

DECLARE_TEST(rx_gaps);
DECLARE_TEST(rx_scheduled);
DECLARE_TEST(tx_onoff);
DECLARE_TEST(tx_onoff_nowsched);
DECLARE_TEST(tx_gmsk_bursts);
DECLARE_TEST(loopback_onoff);
DECLARE_TEST(loopback_onoff_zp);
DECLARE_TEST(format_mismatch);
DECLARE_TEST(readback);
DECLARE_TEST(print);

static const struct test *tests[] = {
    TEST(rx_gaps),
    TEST(rx_scheduled),
    TEST(tx_onoff),
    TEST(tx_onoff_nowsched),
    TEST(tx_gmsk_bursts),
    TEST(loopback_onoff),
    TEST(loopback_onoff_zp),
    TEST(format_mismatch),
    TEST(readback),
    TEST(print),
};


static const struct option long_options[] = {
    { "help",       no_argument,        0,      'h' },

    /* Device configuration */
    { "device",             required_argument,  0,      'd' },
    { "samplerate",         required_argument,  0,      's' },
    { "buflen",             required_argument,  0,      'B' },
    { "num-bufs",           required_argument,  0,      'n' },
    { "num-xfers",          required_argument,  0,      'x' },
    { "timeout",            required_argument,  0,      'T' },

    /* Test configuration */
    { "test",               required_argument,  0,      't' },

    /* Verbosity options */
    { "lib-verbosity",      required_argument,  0,      'v' },

    /* Seed to use for tests utilizing a PRNG */
    { "seed",               required_argument,  0,      'S' },

    { 0,                    0,                  0,      0   },
};

const struct numeric_suffix freq_suffixes[] = {
    { "K",   1000 },
    { "kHz", 1000 },
    { "M",   1000000 },
    { "MHz", 1000000 },
    { "G",   1000000000 },
    { "GHz", 1000000000 },
};

const struct numeric_suffix len_suffixes[] = {
    { "k",   1024 },
};

static void usage(const char *argv0)
{
    printf("Usage: %s [options]\n", argv0);
    printf("Run timestamp tests\n\n");

    printf("Device configuration options:\n");
    printf("    -d, --device <device>     Use the specified device. By default,\n");
    printf("                              any device found will be used.\n");
    printf("    -s, --samplerate <rate>   Set the specified sample rate.\n");
    printf("                              Default = %u\n", DEFAULT_SAMPLERATE);
    printf("    -B, --buflen <value>      Buffer length. Must be multiple of 1024.\n");
    printf("    -n, --num-bufs <value>    Number of buffers to use.\n");
    printf("    -x, --num-xfers <value>   Number of transfers to use.\n");
    printf("    -T, --timeout <value>     Timeout value (ms)\n\n");

    printf("Test configuration:\n");
    printf("    -t, --test <name>         Test name to run. Options are:\n");
    printf("         rx_gaps              Check for unexpected gaps in samples.\n");
    printf("         rx_scheduled         Perform reads at specific timestamps.\n");
    printf("         tx_onoff             Transmits ON-OFF bursts.\n");
    printf("                                Requires external verification.\n");
    printf("         tx_onoff_nowsched    Transmits ON-OFF bursts, switching between\n");
    printf("                                TX_NOW and scheduled transmissions.\n");
    printf("                                Requires external verification.\n");
    printf("         tx_gmsk_bursts       Transmits GMSK bursts.\n");
    printf("                                Requires external verification.\n");
    printf("         loopback_onoff       Transmits ON-OFF bursts which are verified\n");
    printf("         loopback_onoff_zp    Transmits ON-OFF bursts with zero-padding,\n");
    printf("                                which are verified via baseband loopback\n");
    printf("                                to the RX module.\n");
    printf("         format_mismatch      Exercise checking of conflicting formats.\n");
    printf("         readback             Read back timestamps for 10-20s to determine\n");
    printf("                                mean intra-readback time and look for any\n");
    printf("                                unexpected gaps.\n");
    printf("         print                Read and print timestamps for ~2 hours.\n");
    printf("\n");
    printf("    -S, --seed <value>        Seed to use for PRNG-based test cases.\n");
    printf("\n");

    printf("Misc options:\n");
    printf("    -h, --help                  Show this help text\n");
    printf("    -v, --lib-verbosity <level> Set libbladeRF verbosity (Default: warning)\n");
    printf("\n");
}

static void init_app_params(struct app_params *p)
{
    memset(p, 0, sizeof(p[0]));
    p->samplerate = 1000000;
    p->prng_seed = 1;

    p->num_buffers = 16;
    p->num_xfers = 8;
    p->buf_size = 64 * 1024;
    p->timeout_ms = 10000;
}

static void deinit_app_params(struct app_params *p)
{
    free(p->device_str);
    free(p->test_name);
}

static int handle_args(int argc, char *argv[], struct app_params *p)
{
    int c, idx;
    bool ok;
    bladerf_log_level log_level;

    /* Print help */
    if (argc == 1) {
        return 1;
    }

    while ((c = getopt_long(argc, argv, OPTSTR, long_options, &idx)) >= 0) {
        switch (c) {
            case 'v':
                log_level = str2loglevel(optarg, &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid log level: %s\n", optarg);
                    return -1;
                } else {
                    bladerf_log_set_verbosity(log_level);
                }
                break;

            case 'h':
                return 1;

            case 'd':
                if (p->device_str != NULL) {
                    fprintf(stderr, "Device already specified: %s\n",
                            p->device_str);
                    return -1;
                } else {
                    p->device_str = strdup(optarg);
                    if (p->device_str == NULL) {
                        perror("strdup");
                        return -1;
                    }
                }
                break;

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

            case 'B':
                p->buf_size = str2uint_suffix(optarg,
                                              1024,
                                              UINT_MAX,
                                              len_suffixes,
                                              ARRAY_SIZE(len_suffixes),
                                              &ok);

                if (!ok || (p->buf_size % 1024) != 0) {
                    fprintf(stderr, "Invalid buffer length: %s\n", optarg);
                    return -1;
                }

                break;

            case 'n':
                p->num_buffers = str2uint(optarg, 2, UINT_MAX, &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid number of buffers: %s\n", optarg);
                    return -1;
                }
                break;

            case 'x':
                p->num_xfers = str2uint(optarg, 1, UINT_MAX, &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid number of transfers: %s\n", optarg);
                    return -1;
                }
                break;

            case 'T':
                p->timeout_ms = str2uint(optarg, 0, UINT_MAX, &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid timeout: %s\n", optarg);
                    return -1;
                }
                break;

            case 't':
                p->test_name = strdup(optarg);
                if (p->test_name == NULL) {
                    perror("strdup");
                    return -1;
                }
                break;

            case 'S':
                p->prng_seed = str2uint64(optarg, 1, UINT64_MAX, &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid seed value: %s\n", optarg);
                    return -1;
                }
                break;

            default:
                return -1;
        }
    }

    if (p->num_buffers < (p->num_xfers + 1)) {
        fprintf(stderr, "Too many xfers, or too few buffers specified.\n");
        return -1;
    }

    if (p->test_name == NULL) {
        fprintf(stderr, "A test name must be specified via -t <test>.\n");
        return -1;
    }

    return 0;
}

static inline int apply_params(struct bladerf *dev, struct app_params *p)
{
    int status;

    randval_init(&p->prng_state, p->prng_seed);

    status = bladerf_set_sample_rate(dev, BLADERF_MODULE_RX, p->samplerate, NULL);
    if (status != 0) {
        fprintf(stderr, "Failed to set RX sample rate: %s\n",
                bladerf_strerror(status));
        return status;
    }

    status = bladerf_set_sample_rate(dev, BLADERF_MODULE_TX, p->samplerate, NULL);
    if (status != 0) {
        fprintf(stderr, "Failed to set TX sample rate: %s\n",
                bladerf_strerror(status));
        return status;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int status;
    struct app_params p;
    struct bladerf *dev = NULL;
    size_t i;

    init_app_params(&p);

    status = handle_args(argc, argv, &p);
    if (status != 0) {
        if (status == 1) {
            usage(argv[0]);
            status = 0;
        }
        goto out;
    }

    status = bladerf_open(&dev, p.device_str);
    if (status != 0) {
        fprintf(stderr, "Failed to open device: %s\n",
                bladerf_strerror(status));
        goto out;
    }

    status = apply_params(dev, &p);
    if (status != 0) {
        goto out;
    }

    for (i = 0; i < ARRAY_SIZE(tests); i++) {
        if (!strcasecmp(p.test_name, tests[i]->name)) {
            status = tests[i]->run(dev, &p);
            break;
        }
    }

    if (i >= ARRAY_SIZE(tests)) {
        fprintf(stderr, "Unknown test: %s\n", p.test_name);
        status = -1;
    }

out:
    if (dev != NULL) {
        bladerf_close(dev);
    }

    deinit_app_params(&p);
    return status;
}
