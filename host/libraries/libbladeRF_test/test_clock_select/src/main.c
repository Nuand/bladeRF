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
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include "libbladeRF.h"

#define CHECK_STATUS(fn) \
    do { \
        status = (fn); \
        if (status != 0) { \
            fprintf(stderr, "Error at %s:%d (%s): Status %d\n", __FILE__, __LINE__, __func__, status); \
            goto error; \
        } \
    } while (0)

static volatile bool done = false;
void handle_sigint(int sig) {
    fprintf(stderr, "\nSIGINT caught. Closing test...");
    done = true;
}

void usage(const char *progname) {
    fprintf(stderr, "Usage: %s [options]\n", progname);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -d, --device <device>    Specify the device string (defaults to 'auto')\n");
    fprintf(stderr, "  -l, --load <file>        Specify the FPGA bitstream file to load\n");
    fprintf(stderr, "  -i, --iterations <num>   Specify the number of test iterations (default 100)\n");
    fprintf(stderr, "  -p, --power_reset        Perform a power reset before each iteration if specified\n");
    fprintf(stderr, "  -r, --reopen             Reopen the device before each iteration if specified\n");
    fprintf(stderr, "  -v, --verbosity          Enable verbose logging\n");
    fprintf(stderr, "  -h, --help               Display this help message and exit\n");
}

static struct option long_options[] = {
    {"device",      required_argument   , 0, 'd'},
    {"load",        required_argument   , 0, 'l'},
    {"iterations",  required_argument   , 0, 'i'},
    {"power_reset", no_argument         , 0, 'p'},
    {"reopen",      no_argument         , 0, 'r'},
    {"verbosity",   no_argument         , 0, 'v'},
    {"help",        no_argument         , 0, 'h'},
    {0, 0, 0, 0 }
};

int main(int argc, char *argv[])
{
    int status = 0;
    struct bladerf *dev = NULL;
    const char *device_string = NULL;
    const char *fpga_file = NULL;
    int c;
    size_t num_test_iterations = 100;
    size_t num_failures = 0;
    bool power_reset = false;
    bool reopen = false;
    time_t start_time, current_time, last_print_time = 0;
    size_t i = 0;

    while ((c = getopt_long(argc, argv, "d:l:i:prvh", long_options, NULL)) != -1) {
        switch (c) {
            case 'd':
                device_string = optarg;
                break;

            case 'l':
                fpga_file = optarg;
                break;

            case 'i':
                num_test_iterations = atoi(optarg);
                if (num_test_iterations == 0) {
                    fprintf(stderr, "Invalid number of iterations: %s\n", optarg);
                    exit(EXIT_FAILURE);
                }
                break;

            case 'p':
                power_reset = true;
                break;

            case 'r':
                reopen = true;
                break;

            case 'v':
                printf("Setting verbosity to verbose\n");
                bladerf_log_set_verbosity(BLADERF_LOG_LEVEL_VERBOSE);
                break;

            case 'h':
                usage(argv[0]);
                exit(EXIT_SUCCESS);

            case '?':
                usage(argv[0]);
                exit(EXIT_FAILURE);

            default:
                printf("?? getopt returned character code 0%o ??\n", c);
        }
    }

    printf("Iterations:  %zu\n", num_test_iterations);
    printf("Device str:  %s\n", (device_string == NULL) ? "auto" : device_string);
    printf("FPGA file:   %s\n", fpga_file ? fpga_file : "None");
    printf("Force reopen:  %s\n", reopen ? "true" : "false");
    printf("Power reset: %s\n", power_reset ? "true" : "false");

    if (reopen == false)
        CHECK_STATUS(bladerf_open(&dev, device_string));

    time(&start_time);
    signal(SIGINT, handle_sigint);
    for (i = 0; i < num_test_iterations; i++) {
        time(&current_time);
        if (current_time - last_print_time >= 1) {
            printf("Failures: %zu/%zu of %zu\n", num_failures, i + 1, num_test_iterations);
            last_print_time = current_time;
        }

        if (power_reset) {
            const char* cmd_restart_bladerf =
                "cd ../libraries/libbladeRF_test/common/arduino/power-toggle &&"
                "./turn.sh off &&"
                "./turn.sh on";

            CHECK_STATUS(system(cmd_restart_bladerf));
        }

        if (reopen)
            CHECK_STATUS(bladerf_open(&dev, device_string));

        if (fpga_file)
            CHECK_STATUS(bladerf_load_fpga(dev, fpga_file));

        status = bladerf_set_clock_select(dev, CLOCK_SELECT_EXTERNAL);
        if (status == 0)
            status = bladerf_set_clock_select(dev, CLOCK_SELECT_ONBOARD);

        if (status != 0) {
            fprintf(stderr, "Failed to set clock select: %s\n", bladerf_strerror(status));
            num_failures ++;

            fprintf(stderr, "Attempting to reopen device...\n");
            bladerf_close(dev);
            dev = NULL;
            sleep(3);
            CHECK_STATUS(bladerf_open(&dev, device_string));
        }

        if (reopen) {
            bladerf_close(dev);
            dev = NULL;
        }

        if (done)
            break;
    }

error:
    printf("\nFailures: %zu/%zu\n", num_failures, i + 1);

    if (dev)
        bladerf_close(dev);

    if (status != 0 || num_failures > 0)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
