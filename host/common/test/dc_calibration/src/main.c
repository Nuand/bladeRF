/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (c) 2015 Nuand LLC
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
#include <limits.h>

#include <libbladeRF.h>
#include "dc_calibration.h"
#include "conversions.h"

const struct numeric_suffix freq_suffixes[] = {
    { "G",      1000 * 1000 * 1000 },
    { "GHz",    1000 * 1000 * 1000 },
    { "M",      1000 * 1000 },
    { "MHz",    1000 * 1000 },
    { "k",      1000 } ,
    { "kHz",    1000 }
};
#define NUM_FREQ_SUFFIXES (sizeof(freq_suffixes) / sizeof(freq_suffixes[0]))

void usage(const char *argv0) {
    printf("Usage: %s <LMS6 cal>\n", argv0);
    printf("Usage: %s <rx|tx> <frequency_low> [<step> <count>]\n", argv0);
    printf("\n");
    printf("To perform LMS6 calibrations, provide a single argument that is\n");
    printf("one of the following:\n");
    printf("    lpf_tuning, tx_lpf, rx_lpf, rx_vga2, all\n");
    printf("\n");
    printf("Otherwise, specify either 'rx' or 'tx' and the desired frequency.\n");
    printf("A step size and count may be specified to calibrate over a range.\n");
    printf("The results will be printed to stdout.\n");
    printf("\n");
}

int main(int argc, char *argv[])
{
    int status = 0;
    struct bladerf *dev = NULL;
    bladerf_module module;

    if (argc < 2 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
        usage(argv[0]);
        return 0;
    }

    status = bladerf_open(&dev, NULL);
    if (status != 0) {
        fprintf(stderr, "Unable to open device: %s\n", bladerf_strerror(status));
        return EXIT_FAILURE;
    }

    if (argc == 2) {
        struct bladerf_lms_dc_cals cals;

        status = dc_calibration_lms6(dev, argv[1]);
        if (status == BLADERF_ERR_INVAL) {
            fprintf(stderr, "Invalid LMS6 module: %s\n", argv[1]);
            goto out;
        } else if (status != 0) {
            fprintf(stderr, "Calibration failed: %s\n", bladerf_strerror(status));
            goto out;
        }

        status = bladerf_lms_get_dc_cals(dev, &cals);
        if (status != 0) {
            fprintf(stderr, "Failed to read LMS6 DC cals: %s\n",
                    bladerf_strerror(status));
            goto out;
        }

        printf("LPF Tuning:     %d\n", cals.lpf_tuning);
        printf("TX LPF I:       %d\n", cals.tx_lpf_i);
        printf("TX LPF Q:       %d\n", cals.tx_lpf_q);
        printf("RX LPF I:       %d\n", cals.rx_lpf_i);
        printf("RX LPF Q:       %d\n", cals.rx_lpf_q);
        printf("RXVGA2 DC REF:  %d\n", cals.dc_ref);
        printf("RXVGA2 AI:      %d\n", cals.rxvga2a_i);
        printf("RXVGA2 AQ:      %d\n", cals.rxvga2a_q);
        printf("RXVGA2 BI:      %d\n", cals.rxvga2b_i);
        printf("RXVGA2 BQ:      %d\n", cals.rxvga2b_q);

    } else if (argc == 3 || argc == 5) {
        bool ok;
        unsigned int f_start;

        module = str2module(argv[1]);
        if (module == BLADERF_MODULE_INVALID) {
            fprintf(stderr, "Invalid module: %s\n", argv[1]);
            status = EXIT_FAILURE;
            goto out;
        }

        f_start = str2uint_suffix(argv[2],
                                  BLADERF_FREQUENCY_MIN, BLADERF_FREQUENCY_MAX,
                                  freq_suffixes, NUM_FREQ_SUFFIXES,
                                  &ok);

        if (!ok) {
            fprintf(stderr, "Invalid frequency: %s\n", argv[2]);
            status = EXIT_FAILURE;
            goto out;
        }

        if (argc == 3) {
            struct dc_calibration_params p;
            p.frequency = f_start;
            p.corr_i  = p.corr_q = 0;
            p.error_i = p.error_q = 0;

            status = dc_calibration(dev, module, &p, 1, false);
            if (status == 0) {
                printf("F=%10u, Corr_I=%4d, Corr_Q=%4d, Error_I=%4.2f, Error_Q=%4.2f\n",
                       p.frequency, p.corr_i, p.corr_q, p.error_i, p.error_q);
            } else {
                fprintf(stderr, "Calibration failed: %s\n",
                        bladerf_strerror(status));
            }
        } else {
            unsigned int f_inc, count, i;
            struct dc_calibration_params *p;

            f_inc = str2uint_suffix(argv[3], 1, UINT_MAX,
                                    freq_suffixes, NUM_FREQ_SUFFIXES, &ok);
            if (!ok) {
                fprintf(stderr, "Invalid frequency increment: %s\n", argv[3]);
                status = EXIT_FAILURE;
                goto out;
            }

            count = str2uint(argv[4], 1, UINT_MAX, &ok);
            if (!ok) {
                fprintf(stderr, "Invalid count: %s\n", argv[4]);
                status = EXIT_FAILURE;
                goto out;
            }

            p = calloc(count, sizeof(p[0]));
            for (i = 0; i < count; i++) {
                p[i].frequency = f_start + i * f_inc;
            }

            if (p[count - 1].frequency <= BLADERF_FREQUENCY_MAX) {
                status = dc_calibration(dev, module, p, count, true);
                if (status == 0) {
                    for (i = 0; i < count; i++) {
                        printf("F=%10u, Corr_I=%4d, Corr_Q=%4d, Error_I=%4.2f, Error_Q=%4.2f\n",
                               p[i].frequency, p[i].corr_i, p[i].corr_q,
                               p[i].error_i, p[i].error_q);
                    }
                } else {
                    fprintf(stderr, "Calibration failed: %s\n",
                            bladerf_strerror(status));
                }
            } else {
                fprintf(stderr,
                        "Error: Provided parameters yield out of range frequency.\n");
                status = EXIT_FAILURE;
            }

            free(p);
        }

    } else {
        fprintf(stderr, "Error: Invalid usage. Run with --help for more information.\n");
        status = EXIT_FAILURE;
    }

out:
    bladerf_close(dev);

    if (status != 0) {
        status = EXIT_FAILURE;
    }

    return status;
}
