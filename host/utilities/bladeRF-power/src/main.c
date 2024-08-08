/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2024 Nuand LLC
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
 *
 * This program is intended to verify that C programs build against
 * libbladeRF without any unintended dependencies.
 */
#include <stdio.h>
#include <getopt.h>
#include "libbladeRF.h"
#include "init.h"
#include "helpers.h"
#include "conversions.h"

#define CHECK(fn) do { \
    status = fn; \
    if (status != 0) { \
        fprintf(stderr, "[Error] %s:%d: %s - %s\n", __FILE__, __LINE__, #fn, bladerf_strerror(status)); \
        fprintf(stderr, "Exiting.\n"); \
        goto error; \
    } \
} while (0)

#define OPTSTR "d:c:l:trf:s:v:h"
struct option long_options[] = {
    { "device",     required_argument,  NULL,   'd' },
    { "channel",    required_argument,  NULL,   'c' },
    { "load",       required_argument,  NULL,   'l' },
    { "tx",         no_argument,        NULL,   't' },
    { "rx",         no_argument,        NULL,   'r' },
    { "frequency",  required_argument,  NULL,   'f' },
    { "sample-rate",required_argument,  NULL,   's' },
    { "verbosity",  optional_argument,  NULL,   'v' },
    { "help",       no_argument,        NULL,   'h' },
    { NULL,         0,                  NULL,   0   },
};

int main(int argc, char *argv[])
{
    int status = 0;
    struct bladerf *dev = NULL;
    struct bladerf_devinfo devinfo;
    char *devstr = NULL;

    int opt = 0;
    int opt_ind = 0;
    bool ok;
    bladerf_log_level log_level = BLADERF_LOG_LEVEL_INFO;

    struct test_params test;
    init_params(&test);

    while (opt != -1) {
        opt = getopt_long(argc, argv, OPTSTR, long_options, &opt_ind);

        switch (opt) {
            case 'd':
                devstr = optarg;
                break;

            case 'c':
                test.channel = str2int(optarg, 0, INT32_MAX, &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid channel: %s\n", optarg);
                    return -1;
                }
                break;

            case 'f':
                test.frequency = str2uint64_suffix(optarg, 0, UINT64_MAX, freq_suffixes,
                    NUM_FREQ_SUFFIXES, &ok);
                printf("Frequency: %" PRIu64 "\n", test.frequency);
                break;

            case 't':
            case 'r':
                if (test.direction != DIRECTION_UNSET) {
                    fprintf(stderr, "Only one direction may be specified.\n");
                    test.direction = ask_direction();
                    break;
                }
                test.direction = (opt == 't') ? BLADERF_TX : BLADERF_RX;
                break;

            case 'l':
                test.gain_cal_file = optarg;
                break;

            case 's':
                test.samp_rate = str2uint_suffix(optarg, 0, UINT32_MAX, freq_suffixes,
                    NUM_FREQ_SUFFIXES, &ok);
                printf("Sample rate: %i\nHz", test.samp_rate);
                break;

            case 'v':
                log_level = str2loglevel(optarg, &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid log level: %s\n", optarg);
                    return -1;
                }
                break;

            case 'h':
                printf("Usage: %s [options]\n", argv[0]);
                printf("  -d, --device <str>        Specify the device to open.\n");
                printf("  -c, --channel <num>       Specify the channel to use.\n");
                printf("  -l, --load <file>         Load a specified gain cal file (.csv or .tbl).\n");
                printf("  -t, --tx                  Transmit mode. Can't be combined with --rx.\n");
                printf("  -r, --rx                  Receive mode. Can't be combined with --tx.\n");
                printf("  -f, --frequency <freq>    Set the initial frequency (in Hz).\n");
                printf("  -s, --sample-rate <rate>  Set the initial sample rate (in Hz).\n");
                printf("  -v, --verbosity <level>   Set the libbladeRF verbosity level (e.g., verbose, debug).\n");
                printf("  -h, --help                Display this help text and exit.\n");
                printf("\n");
                return 0;

            default:
                break;
        }
    }

    bladerf_log_set_verbosity(log_level);

    CHECK(bladerf_open(&dev, devstr));
    CHECK(bladerf_get_devinfo(dev, &devinfo));
    printf("Device: %s\n", devinfo.serial);

    if (test.direction == DIRECTION_UNSET) {
        test.direction = ask_direction();
    }

    CHECK(dev_init(dev, test.direction, &test));
    CHECK(start_streaming(dev, &test));

error:
    if (dev) bladerf_close(dev);
    return status;
}
