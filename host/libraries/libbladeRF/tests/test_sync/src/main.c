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
#include "log.h"
#include "test.h"

/* FIXME these should be provided in libbladeRF.h */
#define SAMPLERATE_MIN          160000u
#define SAMPLERATE_MAX          40000000u
#define FREQ_MIN                300000000u
#define FREQ_MAX                3000000000u

#define OPTSTR "hd:s:f:l:i:o:r:c:b:X:B:C:T:"
const struct option long_options[] = {
    { "help",           no_argument,        0,  'h' },

    /* Device configuration */
    { "device",         required_argument,  0,  'd' },
    { "samplerate",     required_argument,  0,  's' },
    { "frequency",      required_argument,  0,  'f' },
    { "loopback",       required_argument,  0,  'l' },

    /* Test configuration */
    { "input",          required_argument,  0,  'i' },
    { "output",         required_argument,  0,  'o' },
    { "tx-repetitions", required_argument,  0,  'r' },
    { "rx-count",       required_argument,  0,  'c' },
    { "block-size",     required_argument,  0,  'b' },

    /* Stream configuration */
    { "num-xfers",      required_argument,  0,  'X' },
    { "buffer-size",    required_argument,  0,  'B' },
    { "buffer-count",   required_argument,  0,  'C' },
    { "timeout",        required_argument,  0,  'T' },

    /* Verbosity options */
    { "verbosity",      required_argument,  0,  1,  },
    { "lib-verbosity",  required_argument,  0,  2,  },
    { 0,                0,                  0,  0   },
};

const struct numeric_suffix freq_suffixes[] = {
    { "K",   1000 },
    { "kHz", 1000 },
    { "M",   1000000 },
    { "MHz", 1000000 },
    { "G",   1000000000 },
    { "GHz", 1000000000 },
};

const unsigned int num_freq_suffixes = sizeof(freq_suffixes) / sizeof(freq_suffixes[0]);

const struct numeric_suffix size_suffixes[] = {
    { "K",  1024 },
    { "M",  1024 * 1024 },
    { "G",  1024 * 1024 * 1024 },
};

const unsigned int num_size_suffixes = sizeof(size_suffixes) / sizeof(size_suffixes[0]);

const struct numeric_suffix count_suffixes[] = {
    { "K", 1000 },
    { "M", 1000000 },
    { "G", 1000000000 },
};

const unsigned int num_count_suffixes = sizeof(count_suffixes) / sizeof(count_suffixes[0]);


static void print_usage(const char *argv0)
{
    printf("Usage: %s [options]\n", argv0);
    printf("libbladeRF_sync_test: RX/TX samples to/from specified files.\n");
    printf("\n");

    printf("Device configuration options:\n");
    printf("    -d. --device <device>       Use the specified device. By default,\n");
    printf("                                any device found will be used.\n");
    printf("    -f, --frequency <value>     Set the specified frequency. Default = %u.\n", DEFAULT_FREQUENCY);
    printf("    -s, --samplerate <value>    Set the specified sample rate. Default = %u.\n", DEFAULT_SAMPLERATE);
    printf("    -l, --loopback <mode>       Operate in the specified loopback mode. Options are:\n");
    printf("                                  bb_txlpf_rxvga2, bb_txlpf_rxlpf,\n");
    printf("                                  bb_txvga1_rxvga1, bb_txvga1_rxlpf,\n");
    printf("                                  rf_lna1, rf_lna2, rf_lna2, none\n");
    printf("                                The default is 'none'.\n");
    printf("\n");

    printf("Test configuration options:\n");
    printf("    -i, --input <file>          File to transmit samples from.\n");
    printf("    -o, --output <file>         File to receive samples to.\n");
    printf("    -r, --tx-repetitions <n>    # of times to repeat input file. Default = %u\n", DEFAULT_TX_REPETITIONS);
    printf("    -c, --rx-count <n>          # of samples to receive. Defauilt = %u.\n", DEFAULT_RX_COUNT);
    printf("    -b, --block-size <n>        # samples to RX/TX per sync call. Default = %u.\n", DEFAULT_BLOCK_SIZE);
    printf("\n");

    printf("Stream configuration options:\n");
    printf("    -X, --num-xfers <n>         # in-flight transfers. Default = %u.\n", DEFAULT_STREAM_XFERS);
    printf("    -B, --buffer-size <n>       # samples per stream buffer. Default = %u.\n", DEFAULT_STREAM_SAMPLES);
    printf("    -C, --buffer-count <n>      # of stream buffers. Default = %u.\n", DEFAULT_STREAM_BUFFERS);
    printf("    -T, --timeout <n>           Stream timeout, in ms. Default = %u.\n", DEFAULT_STREAM_TIMEOUT);

    printf("\n");

    printf("Misc options:\n");
    printf("    -h, --help                  Show this help text\n");
    printf("    --verbosity <level>         Set test verbosity (Default: warning)\n");
    printf("    --sync-verbosity <level>    Set libbladeRF_sync verbosity (Default: warning)\n");
    printf("    --lib-verbosity <level>     Set libbladeRF verbosity (Default: warning)\n");
    printf("\n");

    printf("Notes:\n");
    printf("    The provided files must contain binary SC16 Q11 samples.\n");
    printf("\n");
    printf("    The -f and -s options are applied to both RX and TX modules.\n");
    printf("\n");
    printf("    When transmitting, the input file size (in samples) should be a multiple\n");
    printf("    of <block-size>, and at least 1 block. Otherwise, samples will be truncated.\n");
    printf("\n");
}

int handle_cmdline(int argc, char *argv[], struct test_params *p)
{
    int c;
    int idx;
    bool ok;
    bladerf_log_level level;

    test_init_params(p);

    while ((c = getopt_long(argc, argv, OPTSTR, long_options, &idx)) >= 0) {
        switch (c) {

            case 1:
                level = str2loglevel(optarg, &ok);
                if (!ok) {
                    log_error("Invalid log level provided: %s\n", optarg);
                    return -1;
                } else {
                    log_set_verbosity(level);
                }
                break;

            case 2:
                level = str2loglevel(optarg, &ok);
                if (!ok) {
                    log_error("Invalid log level provided: %s\n", optarg);
                    return -1;
                } else {
                    bladerf_log_set_verbosity(level);
                }
                break;

            case 'h':
                return 1;

            case 'd':
                if (p->device_str != NULL) {
                    log_error("Device was already specified.\n");
                    return -1;
                }

                p->device_str = strdup(optarg);
                if (p->device_str == NULL) {
                    perror("strdup");
                    return -1;
                }
                break;

            case 's':
                p->samplerate = str2uint_suffix(optarg,
                                                SAMPLERATE_MIN, SAMPLERATE_MAX,
                                                freq_suffixes,
                                                num_freq_suffixes,
                                                &ok);
                if (!ok) {
                    log_error("Invalid sample rate: %s\n", optarg);
                    return -1;
                }
                break;

            case 'f':
                p->frequency = str2uint_suffix(optarg,
                                               FREQ_MIN, FREQ_MAX,
                                               freq_suffixes,
                                               num_freq_suffixes,
                                               &ok);
                if (!ok) {
                    log_error("Invalid frequency: %s\n", optarg);
                    return -1;
                }
                break;

            case 'l':
                if (str2loopback(optarg, &p->loopback) != 0) {
                    log_error("Invalid loopback mode: %s\n", optarg);
                    return -1;
                }
                break;

            case 'i':
                if (p->in_file) {
                    log_error("Input file already provided.\n");
                    return -1;
                }

                p->in_file = fopen(optarg, "rb");
                if (p->in_file == NULL) {
                    log_error("Failed to open input file - %s\n",
                            strerror(errno));
                    return -1;
                }
                break;

            case 'o':
                if (p->out_file) {
                    log_error("Output file already provided.\n");
                    return -1;
                }

                p->out_file = fopen(optarg, "wb");
                if (p->out_file == NULL) {
                    log_error("Failed to open output file - %s\n",
                            strerror(errno));
                    return -1;
                }
                break;

            case 'r':
                p->tx_repetitions = str2uint_suffix(optarg, 1, UINT_MAX,
                                                    count_suffixes,
                                                    num_count_suffixes,
                                                    &ok);
                if (!ok) {
                    log_error("Invalid TX repetition value: %s\n", optarg);
                    return -1;
                }

                break;

            case 'c':
                p->rx_count = str2uint_suffix(optarg, 1, UINT_MAX,
                                              count_suffixes,
                                              num_count_suffixes,
                                              &ok);
                if (!ok) {
                    log_error("Invalid RX count: %s\n", optarg);
                    return -1;
                }

                break;

            case 'b':
                p->block_size = str2uint_suffix(optarg, 1, UINT_MAX,
                                                size_suffixes,
                                                num_size_suffixes,
                                                &ok);

                if (!ok) {
                    log_error("Invalid block size: %s\n", optarg);
                    return -1;
                }
                break;

            case 'X':
                p->num_xfers = str2uint(optarg, 1, UINT_MAX, &ok);
                if (!ok) {
                    log_error("Invalid stream transfer count: %s\n", optarg);
                    return -1;
                }
                break;

            case 'B':
                p->stream_buffer_size = str2uint(optarg, 1, UINT_MAX, &ok);
                if (!ok) {
                    log_error("Invalid stream buffer size: %s\n", optarg);
                    return -1;
                }
                break;

            case 'C':
                p->stream_buffer_count = str2uint(optarg, 1, UINT_MAX, &ok);
                if (!ok) {
                    log_error("Invalid stream buffer count: %s\n", optarg);
                    return -1;
                }
                break;

            case 'T':
                p->timeout_ms = str2uint(optarg, 0, UINT_MAX, &ok);
                if (!ok) {
                    log_error("Invalid stream timeout: %s\n", optarg);
                    return -1;
                }
                break;
        }
    }

    if (p->in_file == NULL && p->out_file == NULL) {
        log_error("An input or output file is required.\n");
        return -1;
    }

    if (p->frequency == 0) {
        p->frequency = DEFAULT_FREQUENCY;
    }

    if (p->samplerate == 0) {
        p->samplerate = DEFAULT_SAMPLERATE;
    }

    if (p->tx_repetitions == 0) {
        p->tx_repetitions = DEFAULT_TX_REPETITIONS;
    }

    if (p->rx_count == 0) {
        p->rx_count = DEFAULT_RX_COUNT;
    }

    if (p->block_size == 0) {
        p->block_size = DEFAULT_BLOCK_SIZE;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int status;
    struct test_params p;

    status = handle_cmdline(argc, argv, &p);

    if (status == 0) {
        status = test_run(&p);
    } else if (status > 0) {
        print_usage(argv[0]);
        status = 0;
    }

    if (p.in_file) {
        fclose(p.in_file);
    }

    if (p.out_file) {
        fclose(p.out_file);
    }

    return status;
}
