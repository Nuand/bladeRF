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

/* This program is intended to acquire some rough estimates of FPGA peripheral
 * access times, for use in determining if change yield significant improvements
 * or performance regressions. */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <libbladeRF.h>
#include "test_common.h"

#define ITERATIONS 10000

int time_lms_reads(struct bladerf *dev, double *duration)
{
    int status;
    struct timespec start, end;
    unsigned int i;
    uint8_t data;

    status = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    if (status != 0) {
        fprintf(stderr, "Failed to get start time. Erroring out.\n");
        return -1;
    }

    for (i = 0; i < ITERATIONS; i++) {
        status = bladerf_lms_read(dev, 0x04, &data);
        if (status != 0) {
            fprintf(stderr, "LMS Read failed: %s\n",
                    bladerf_strerror(status));
            return -1;
        }
    }

    status = clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    if (status != 0) {
        fprintf(stderr, "Failed to get end time. Erroring out.\n");
        return -1;
    }

    *duration = calc_avg_duration(&start, &end, ITERATIONS);
    return 0;
}

int time_lms_writes(struct bladerf *dev, double *duration)
{
    int status;
    struct timespec start, end;
    unsigned int i;

    status = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    if (status != 0) {
        fprintf(stderr, "Failed to get start time. Erroring out.\n");
        return -1;
    }

    for (i = 0; i < ITERATIONS; i++) {
        status = bladerf_lms_write(dev, 0x04, 0xaa);
        if (status != 0) {
            fprintf(stderr, "LMS Write failed: %s\n",
                    bladerf_strerror(status));
            return -1;
        }
    }

    status = clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    if (status != 0) {
        fprintf(stderr, "Failed to get end time. Erroring out.\n");
        return -1;
    }

    *duration = calc_avg_duration(&start, &end, ITERATIONS);
    return 0;
}

int time_si5338_reads(struct bladerf *dev, double *duration)
{
    int status;
    struct timespec start, end;
    unsigned int i;
    uint8_t data;

    status = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    if (status != 0) {
        fprintf(stderr, "Failed to get start time. Erroring out.\n");
        return -1;
    }

    for (i = 0; i < ITERATIONS; i++) {
        status = bladerf_si5338_read(dev, 0x00, &data);
        if (status != 0) {
            fprintf(stderr, "LMS Read failed: %s\n",
                    bladerf_strerror(status));
            return -1;
        }
    }

    status = clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    if (status != 0) {
        fprintf(stderr, "Failed to get end time. Erroring out.\n");
        return -1;
    }

    *duration = calc_avg_duration(&start, &end, ITERATIONS);
    return 0;
}

int time_si5338_writes(struct bladerf *dev, double *duration)
{
    int status;
    struct timespec start, end;
    unsigned int i;

    status = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    if (status != 0) {
        fprintf(stderr, "Failed to get start time. Erroring out.\n");
        return -1;
    }

    for (i = 0; i < ITERATIONS; i++) {
        status = bladerf_si5338_write(dev, 0x00, 0xaa);
        if (status != 0) {
            fprintf(stderr, "LMS Read failed: %s\n",
                    bladerf_strerror(status));
            return -1;
        }
    }

    status = clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    if (status != 0) {
        fprintf(stderr, "Failed to get end time. Erroring out.\n");
        return -1;
    }

    *duration = calc_avg_duration(&start, &end, ITERATIONS);
    return 0;
}

int time_rfic_reads(struct bladerf *dev, double *duration)
{
    int status;
    struct timespec start, end;
    unsigned int i;
    uint8_t data;

    status = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    if (status != 0) {
        fprintf(stderr, "Failed to get start time. Erroring out.\n");
        return -1;
    }

    for (i = 0; i < ITERATIONS; i++) {
        status = bladerf_get_rfic_register(dev, 0x00, &data);
        if (status != 0) {
            fprintf(stderr, "RFIC Read failed: %s\n",
                    bladerf_strerror(status));
            return -1;
        }
    }

    status = clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    if (status != 0) {
        fprintf(stderr, "Failed to get end time. Erroring out.\n");
        return -1;
    }

    *duration = calc_avg_duration(&start, &end, ITERATIONS);
    return 0;
}

int time_rfic_writes(struct bladerf *dev, double *duration)
{
    int status;
    struct timespec start, end;
    unsigned int i;

    status = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    if (status != 0) {
        fprintf(stderr, "Failed to get start time. Erroring out.\n");
        return -1;
    }

    for (i = 0; i < ITERATIONS; i++) {
        status = bladerf_set_rfic_register(dev, 0x00, 0xaa);
        if (status != 0) {
            fprintf(stderr, "RFIC Read failed: %s\n",
                    bladerf_strerror(status));
            return -1;
        }
    }

    status = clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    if (status != 0) {
        fprintf(stderr, "Failed to get end time. Erroring out.\n");
        return -1;
    }

    *duration = calc_avg_duration(&start, &end, ITERATIONS);
    return 0;
}

int test_bladeRF1(struct bladerf *dev) {
    int status;
    double duration;

    printf("Timing LMS6002D reads over %u iterations...\n", ITERATIONS);
    status = time_lms_reads(dev, &duration);
    if (status != 0) {
        goto out;
    } else {
        printf("  Average access time: %f s\n\n", duration);
    }

    printf("Timing LMS6002D writes over %u iterations...\n", ITERATIONS);
    status = time_lms_writes(dev, &duration);
    if (status != 0) {
        goto out;
    } else {
        printf("  Average access time: %f s\n\n", duration);
    }

    printf("Timing SI5338 reads over %u iterations...\n", ITERATIONS);
    status = time_si5338_reads(dev, &duration);
    if (status != 0) {
        goto out;
    } else {
        printf("  Average access time: %f s\n\n", duration);
    }

    printf("Timing SI5338 writes over %u iterations...\n", ITERATIONS);
    status = time_si5338_writes(dev, &duration);
    if (status != 0) {
        goto out;
    } else {
        printf("  Average access time: %f s\n\n", duration);
    }

out:
    return status;
}

int test_bladeRF2(struct bladerf *dev) {
    int status;
    double duration;

    printf("Timing RFIC reads over %u iterations...\n", ITERATIONS);
    status = time_rfic_reads(dev, &duration);
    if (status != 0) {
        goto out;
    } else {
        printf("  Average access time: %f s\n\n", duration);
    }

    printf("Timing RFIC writes over %u iterations...\n", ITERATIONS);
    status = time_rfic_writes(dev, &duration);
    if (status != 0) {
        goto out;
    } else {
        printf("  Average access time: %f s\n\n", duration);
    }

out:
    return status;
}

int main(int argc, char *argv[])
{
    int status;
    struct bladerf *dev = NULL;
    const char *devstr = NULL;
    bladerf_fpga_size fpga_size;

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

    status = bladerf_get_fpga_size(dev, &fpga_size);
    if (status != 0) {
        fprintf(stderr, "Unable to query FPGA size: %s\n",
                bladerf_strerror(status));
        return status;
    }

    switch (fpga_size) {
        default:
        case BLADERF_FPGA_40KLE:
        case BLADERF_FPGA_115KLE:
            status = test_bladeRF1(dev);
            break;

        case BLADERF_FPGA_A4:
        case BLADERF_FPGA_A9:
            status = test_bladeRF2(dev);
            break;
    }
    bladerf_close(dev);
    return status;
}
