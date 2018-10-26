/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2015 Nuand LLC
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

/* This program is intended to acquire some rough estimates of the duration of
 * our frequency tuning operations.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <libbladeRF.h>
#include "test_common.h"

#define ITERATIONS 2500u
#define FIXED_FREQ 2405000000u

double fixed_retune(struct bladerf *dev)
{
    int status;
    struct timespec start, end;
    unsigned int i;

    status = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    if (status != 0) {
        fprintf(stderr, "Failed to get end time. Erroring out.\n");
        return -1;
    }

    for (i = 0; i < ITERATIONS; i++) {
        status = bladerf_set_frequency(dev, BLADERF_MODULE_RX, FIXED_FREQ);
        if (status != 0) {
            fprintf(stderr, "Failed to set frequency (%u): %s\n",
                    FIXED_FREQ, bladerf_strerror(status));
            return -1;
        }
    }

    status = clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    if (status != 0) {
        fprintf(stderr, "Failed to get end time. Erroring out.\n");
        return -1;
    }

    return calc_avg_duration(&start, &end, ITERATIONS);
}

double random_retune(struct bladerf *dev)
{
    int status;
    struct timespec start, end;
    unsigned int i;
    unsigned int freq;
    uint64_t prng;

    randval_init(&prng, 1);

    status = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    if (status != 0) {
        fprintf(stderr, "Failed to get end time. Erroring out.\n");
        return -1;
    }

    for (i = 0; i < ITERATIONS; i++) {
        freq = get_rand_freq(&prng, false);
        status = bladerf_set_frequency(dev, BLADERF_MODULE_RX, freq);
        if (status != 0) {
            fprintf(stderr, "Failed to set frequency (%u): %s\n",
                    FIXED_FREQ, bladerf_strerror(status));
            return -1;
        }
    }

    status = clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    if (status != 0) {
        fprintf(stderr, "Failed to get end time. Erroring out.\n");
        return -1;
    }

    return calc_avg_duration(&start, &end, ITERATIONS);
}

double quick_retune(struct bladerf *dev)
{
    const char *board_name;
    bladerf_frequency center_freq;
    const bladerf_frequency center_fdev = 1.0e6; /* 1 MHz deviation */
    int status;
    struct bladerf_quick_tune f1, f2;
    struct timespec start, end;
    unsigned int i;

    board_name = bladerf_get_board_name(dev);

    /* Choosing frequencies that cross the low/high band threshold */
    if (strcmp(board_name, "bladerf1") == 0) {
        center_freq = 1.500e9;
    } else if (strcmp(board_name, "bladerf2") == 0) {
        center_freq = 3.000e9;
    } else {
        fprintf(stderr, "Unknown board name: %s\n", board_name);
        return -1;
    }

    status = bladerf_set_frequency(dev, BLADERF_MODULE_RX,
                                   center_freq - center_fdev);
    if (status != 0) {
        return -1;
    }

    status = bladerf_get_quick_tune(dev, BLADERF_MODULE_RX, &f1);
    if (status != 0) {
        return -1;
    }

    status = bladerf_set_frequency(dev, BLADERF_MODULE_RX,
                                   center_freq + center_fdev);
    if (status != 0) {
        return -1;
    }

    status = bladerf_get_quick_tune(dev, BLADERF_MODULE_RX, &f2);
    if (status != 0) {
        return -1;
    }

    status = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    if (status != 0) {
        fprintf(stderr, "Failed to get end time. Erroring out.\n");
        return -1;
    }

    for (i = 0; i < (ITERATIONS / 2); i++) {
        status = bladerf_schedule_retune(dev, BLADERF_MODULE_RX,
                                         BLADERF_RETUNE_NOW,
                                         0, &f1);
        if (status != 0) {
            fprintf(stderr, "Failed to tune to f1: %s\n",
                    bladerf_strerror(status));
            return -1;
        }

        status = bladerf_schedule_retune(dev, BLADERF_MODULE_RX,
                                         BLADERF_RETUNE_NOW,
                                         0, &f2);
        if (status != 0) {
            fprintf(stderr, "Failed to tune to f2: %s\n",
                    bladerf_strerror(status));
            return -1;
        }
    }

    status = clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    if (status != 0) {
        fprintf(stderr, "Failed to get end time. Erroring out.\n");
        return -1;
    }

    return calc_avg_duration(&start, &end, ITERATIONS);
}

int main(int argc, char *argv[])
{
    int status;
    struct bladerf *dev = NULL;
    const char *devstr = NULL;
    double duration_host, duration_fpga;
    const char *board_name;

    if (argc > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
        fprintf(stderr, "Usage: %s [device string]\n", argv[0]);
        return 1;
    } else {
        devstr = argv[1];
    }

    status = bladerf_open(&dev, devstr);
    if (status != 0) {
        fprintf(stderr, "Unable to open device: %s\n",
                bladerf_strerror(status));
        return status;
    }

    board_name = bladerf_get_board_name(dev);

    printf("Re-tuning with fixed frequency...\n");

    status = bladerf_set_tuning_mode(dev, BLADERF_TUNING_MODE_HOST);
    if (status != 0) {
        fprintf(stderr, "Failed to switch to host-based tuning mode: %s\n",
                bladerf_strerror(status));
        status = -1;
        goto out;
    }

    duration_host = fixed_retune(dev);

    if (duration_host < 0) {
        status = (int) duration_host;
        goto out;
    } else {
        printf("  Host tuning:    %fs\n", duration_host);
    }

    if (strcmp(board_name, "bladerf1") == 0) {
        status = bladerf_set_tuning_mode(dev, BLADERF_TUNING_MODE_FPGA);
        if (status != 0) {
            fprintf(stderr, "Failed to switch to FPGA-based tuning mode: %s\n",
                    bladerf_strerror(status));
            status = -1;
            goto out;
        } else {
            duration_fpga = fixed_retune(dev);

            if (duration_fpga < 0) {
                status = (int) duration_fpga;
                goto out;
            } else {
                printf("  FPGA tuning:    %fs\n", duration_fpga);
            }

            printf("  Speedup factor: %f\n", duration_host / duration_fpga);
        }
    }



    printf("Re-tuning with random frequencies...\n");

    status = bladerf_set_tuning_mode(dev, BLADERF_TUNING_MODE_HOST);
    if (status != 0) {
        fprintf(stderr, "Failed to switch to host-based tuning mode: %s\n",
                bladerf_strerror(status));
        status = -1;
        goto out;
    }

    duration_host = random_retune(dev);

    if (duration_host < 0) {
        status = (int) duration_host;
        goto out;
    } else {
        printf("  Host tuning:    %fs\n", duration_host);
    }

    if (strcmp(board_name, "bladerf1") == 0) {
        status = bladerf_set_tuning_mode(dev, BLADERF_TUNING_MODE_FPGA);
        if (status != 0) {
            fprintf(stderr, "Failed to switch to FPGA-based tuning mode: %s\n",
                    bladerf_strerror(status));
            status = -1;
            goto out;
        } else {
            duration_fpga = fixed_retune(dev);

            if (duration_fpga < 0) {
                status = (int) duration_fpga;
                goto out;
            } else {
                printf("  FPGA tuning:    %fs\n", duration_fpga);
            }

            printf("  Speedup factor: %f\n\n", duration_host / duration_fpga);
        }
    }

    printf("Performing quick-tune...\n");

    status = bladerf_set_tuning_mode(dev, BLADERF_TUNING_MODE_HOST);
    if (status != 0) {
        fprintf(stderr, "Failed to switch to host-based tuning mode: %s\n",
                bladerf_strerror(status));
        status = -1;
        goto out;
    }

    duration_host = quick_retune(dev);

    if (duration_host < 0) {
        status = (int) duration_host;
        goto out;
    } else {
        printf("  Host tuning:    %fs\n", duration_host);
    }

    if (strcmp(board_name, "bladerf1") == 0) {
        status = bladerf_set_tuning_mode(dev, BLADERF_TUNING_MODE_FPGA);
        if (status != 0) {
            fprintf(stderr, "Failed to switch to fpga-based tuning mode: %s\n",
                    bladerf_strerror(status));
            status = -1;
            goto out;
        } else {
            duration_fpga = quick_retune(dev);

            if (duration_fpga < 0) {
                status = (int) duration_fpga;
                goto out;
            } else {
                printf("  FPGA tuning:    %fs\n", duration_fpga);
            }

            printf("  Speedup factor: %f\n\n", duration_host / duration_fpga);
        }
    }

out:
    bladerf_close(dev);
    return status;
}
