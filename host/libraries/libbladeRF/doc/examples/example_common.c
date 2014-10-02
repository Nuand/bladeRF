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
#include <libbladeRF.h>
#include "example_common.h"

struct bladerf *example_init(const char *devstr)
{
    int status;
    struct bladerf *dev;

    printf("Opening and initializing device...\n\n");

    status = bladerf_open(&dev, devstr);
    if (status != 0) {
        fprintf(stderr, "Failed to open device: %s\n",
                bladerf_strerror(status));
        goto out;
    }

    status = bladerf_set_frequency(dev, BLADERF_MODULE_RX, EXAMPLE_RX_FREQ);
    if (status != 0) {
        fprintf(stderr, "Failed to set RX frequency: %s\n",
                bladerf_strerror(status));
        goto out;
    } else {
        printf("RX frequency: %u Hz\n", EXAMPLE_RX_FREQ);
    }

    status = bladerf_set_sample_rate(dev, BLADERF_MODULE_RX,
                                     EXAMPLE_SAMPLERATE, NULL);
    if (status != 0) {
        fprintf(stderr, "Failed to set RX sample rate: %s\n",
                bladerf_strerror(status));
        goto out;
    } else {
        printf("RX samplerate: %u sps\n", EXAMPLE_SAMPLERATE);
    }

    status = bladerf_set_bandwidth(dev, BLADERF_MODULE_RX,
                                   EXAMPLE_BANDWIDTH, NULL);
    if (status != 0) {
        fprintf(stderr, "Failed to set RX bandwidth: %s\n",
                bladerf_strerror(status));
        goto out;
    } else {
        printf("RX bandwidth: %u Hz\n", EXAMPLE_BANDWIDTH);
    }

    status = bladerf_set_lna_gain(dev, EXAMPLE_RX_LNA);
    if (status != 0) {
        fprintf(stderr, "Failed to set RX LNA gain: %s\n",
                bladerf_strerror(status));
        goto out;
    } else {
        printf("RX LNA Gain: Max\n");
    }

    status = bladerf_set_rxvga1(dev, EXAMPLE_RX_VGA1);
    if (status != 0) {
        fprintf(stderr, "Failed to set RX VGA1 gain: %s\n",
                bladerf_strerror(status));
        goto out;
    } else {
        printf("RX VGA1 gain: %d\n", EXAMPLE_RX_VGA1);
    }

    status = bladerf_set_rxvga2(dev, EXAMPLE_RX_VGA2);
    if (status != 0) {
        fprintf(stderr, "Failed to set RX VGA2 gain: %s\n",
                bladerf_strerror(status));
        goto out;
    } else {
        printf("RX VGA2 gain: %d\n\n", EXAMPLE_RX_VGA2);
    }

    status = bladerf_set_frequency(dev, BLADERF_MODULE_TX, EXAMPLE_TX_FREQ);
    if (status != 0) {
        fprintf(stderr, "Faield to set TX frequency: %s\n",
                bladerf_strerror(status));
        goto out;
    } else {
        printf("TX frequency: %u Hz\n", EXAMPLE_TX_FREQ);
    }

    status = bladerf_set_sample_rate(dev, BLADERF_MODULE_TX,
                                     EXAMPLE_SAMPLERATE, NULL);
    if (status != 0) {
        fprintf(stderr, "Failed to set TX sample rate: %s\n",
                bladerf_strerror(status));
        goto out;
    } else {
        printf("TX samplerate: %u sps\n", EXAMPLE_SAMPLERATE);
    }

    status = bladerf_set_bandwidth(dev, BLADERF_MODULE_TX,
                                   EXAMPLE_BANDWIDTH, NULL);
    if (status != 0) {
        fprintf(stderr, "Failed to set TX bandwidth: %s\n",
                bladerf_strerror(status));
        goto out;
    } else {
        printf("TX bandwidth: %u\n", EXAMPLE_BANDWIDTH);
    }

    status = bladerf_set_txvga1(dev, EXAMPLE_TX_VGA1);
    if (status != 0) {
        fprintf(stderr, "Failed to set TX VGA1 gain: %s\n",
                bladerf_strerror(status));
        goto out;
    } else {
        printf("TX VGA1 gain: %d\n", EXAMPLE_TX_VGA1);
    }

    status = bladerf_set_txvga2(dev, EXAMPLE_TX_VGA2);
    if (status != 0) {
        fprintf(stderr, "Failed to set TX VGA2 gain: %s\n",
                bladerf_strerror(status));
        goto out;
    } else {
        printf("TX VGA2 gain: %d\n\n", EXAMPLE_TX_VGA2);
    }

out:
    if (status != 0) {
        bladerf_close(dev);
        return NULL;
    } else {
        return dev;
    }
}
