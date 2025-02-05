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
#include <libbladeRF.h>

#define CHECK_STATUS(fn) \
    do { \
        status = (fn); \
        if (status != 0) { \
            fprintf(stderr, "Error at %s:%d (%s): Status %d\n", __FILE__, __LINE__, __func__, status); \
            goto error; \
        } \
    } while (0)

int main(int argc, char *argv[])
{
    int status;
    struct bladerf *dev = NULL;
    const char *fpga_file = "latest.rbf";
    const char *device_string = NULL;
    int c;

    bladerf_log_set_verbosity(BLADERF_LOG_LEVEL_ERROR);

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"device",      required_argument   , 0, 'd'},
            {"fpga",        required_argument   , 0, 'f'},
            {"verbosity",   no_argument         , 0, 'v'},
            {0, 0, 0, 0 }
        };

        c = getopt_long(argc, argv, "d:f:v", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
            case 'd':
                device_string = optarg;
                printf("Using device: %s\n", optarg);
                break;

            case 'f':
                fpga_file = optarg;
                printf("Using FPGA file: %s\n", fpga_file);
                break;

            case 'v':
                printf("Setting verbosity to verbose\n");
                bladerf_log_set_verbosity(BLADERF_LOG_LEVEL_VERBOSE);
                break;

            case '?':
                // getopt_long already printed an error message.
                exit(EXIT_FAILURE);

            default:
                printf("?? getopt returned character code 0%o ??\n", c);
        }
    }

    if (device_string == NULL) {
        fprintf(stderr, "No device specified.\n");
        exit(EXIT_FAILURE);
    }

    printf("Opening device...\n");
    CHECK_STATUS(bladerf_open(&dev, device_string));

    printf("Loading FPGA image...\n");
    CHECK_STATUS(bladerf_load_fpga(dev, fpga_file));

    printf("Setting sample rate to 10e6...\n");
    CHECK_STATUS(bladerf_set_sample_rate(dev, BLADERF_MODULE_RX, 10e6, NULL));

    printf("Reloading the FPGA image...\n");
    CHECK_STATUS(bladerf_load_fpga(dev, fpga_file));

    printf("Setting tuning mode to FPGA...\n");
    CHECK_STATUS(bladerf_set_tuning_mode(dev, BLADERF_TUNING_MODE_FPGA));

    printf("Setting sample rate again to 10e6...\n");
    CHECK_STATUS(bladerf_set_sample_rate(dev, BLADERF_MODULE_RX, 10e6, NULL));

    printf("Passed!\n");
error:
    if (dev) {
        bladerf_close(dev);
    }
    return status;
}
