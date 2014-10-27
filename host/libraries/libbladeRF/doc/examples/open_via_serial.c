/*
 * Example of how to open a device with the specified serial number
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
#include <string.h>
#include <libbladeRF.h>
#include "example_common.h"

/** [example_snippet] */
struct bladerf * open_bladerf_from_serial(const char *serial)
{
    int status;
    struct bladerf *dev;
    struct bladerf_devinfo info;

    /* Initialize all fields to "don't care" wildcard values.
     *
     * Immediately passing this to bladerf_open_with_devinfo() would cause
     * libbladeRF to open any device on any available backend. */
    bladerf_init_devinfo(&info);

    /* Specify the desired device's serial number, while leaving all other
     * fields in the info structure wildcard values */
    strncpy(info.serial, serial, BLADERF_SERIAL_LENGTH - 1);
    info.serial[BLADERF_SERIAL_LENGTH - 1] = '\0';

    status = bladerf_open_with_devinfo(&dev, &info);
    if (status == BLADERF_ERR_NODEV) {
        printf("No devices available with serial=%s\n", serial);
        return NULL;
    } else if (status != 0) {
        fprintf(stderr, "Failed to open device with serial=%s (%s)\n",
                serial, bladerf_strerror(status));
        return NULL;
    } else {
        return dev;
    }
}
/** [example_snippet] */

static int print_device_state(struct bladerf *dev)
{
    int status;
    unsigned int frequency, bandwidth;
    struct bladerf_rational_rate rate;

    status = bladerf_get_frequency(dev, BLADERF_MODULE_RX, &frequency);
    if (status != 0) {
        return status;
    } else {
        printf("  RX frequency: %u Hz\n", frequency);
    }

    status = bladerf_get_frequency(dev, BLADERF_MODULE_TX, &frequency);
    if (status != 0) {
        return status;
    } else {
        printf("  TX frequency: %u Hz\n", frequency);
    }

    status = bladerf_get_bandwidth(dev, BLADERF_MODULE_RX, &bandwidth);
    if (status != 0) {
        return status;
    } else {
        printf("  RX bandwidth: %u Hz\n", bandwidth);
    }

    status = bladerf_get_bandwidth(dev, BLADERF_MODULE_TX, &bandwidth);
    if (status != 0) {
        return status;
    } else {
        printf("  TX bandwidth: %u Hz\n", bandwidth);
    }

    status = bladerf_get_rational_sample_rate(dev, BLADERF_MODULE_RX, &rate);
    if (status != 0) {
        return status;
    } else {
        printf("  RX sample rate: %" PRIu64 " %" PRIu64 "/%" PRIu64" sps\n",
                rate.integer, rate.num, rate.den);
    }

    status = bladerf_get_rational_sample_rate(dev, BLADERF_MODULE_TX, &rate);
    if (status != 0) {
        return status;
    } else {
        printf("  TX sample rate: %" PRIu64 " %" PRIu64 "/%" PRIu64" sps\n",
                rate.integer, rate.num, rate.den);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int status = 0;
    struct bladerf *dev;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <serial #>\n", argv[0]);
        return 1;
    }

    dev = open_bladerf_from_serial(argv[1]);
    if (dev) {
        printf("Opened device successfully!\n");
        status = print_device_state(dev);
    }

    if (status != 0) {
        fprintf(stderr, "Error: %s\n", bladerf_strerror(status));
    }

    if (dev) {
        bladerf_close(dev);
    }

    return status;
}
