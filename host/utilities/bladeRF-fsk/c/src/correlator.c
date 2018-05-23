/**
 * @brief   Cross correlates the input waveform with the preamble waveform and
 *          detects peaks
 *
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2016 Nuand LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <inttypes.h>

#include "correlator.h"
#include "host_config.h"

#ifdef ENABLE_CORR_DEBUG_MSG
#   define DBG(...) fprintf(stderr, "[Corr]  " __VA_ARGS__)
#else
#   define DBG(...)
#endif

#define COUNTDOWN_INACTIVE  (UINT_MAX)

#define LOG_FILE_SUFFIX  "_correlator_log.csv"

struct complexf {
    float real;
    float imag;
};

struct correlator {
    struct complexf *ref;       /* Reference signal to correlate against */
    size_t len;                 /* Length of reference sig, insamples */

    float threshold_pwr;        /* Correlation power threshold */

    float max;                  /* Max correlation value we've seen so far */

    unsigned int num_counts;    /* Number of samples to observe after a
                                 * detected correlation and before asserting
                                 * a match. This is intended to ensure we
                                 * we report the local maxima.
                                 */

    unsigned int countdown;     /* Countdown until a value exceeding the
                                 * max value is reported */

    uint64_t match_timestamp;   /* Timestamp when we found our max
                                 * correlation value */

    struct complexf *buf;       /* Sample buffer */
    struct complexf *ins1;      /* Insertion points */
    struct complexf *ins2;
    struct complexf *end;       /* Pointer to element past buf */

#   ifdef LOG_CORRELATOR_OUTPUT
    FILE *out;
#   endif
};

#ifdef LOG_CORRELATION_SIGNAL
static void save_reference_sig(struct complexf *ref, size_t len)
{
    size_t i;
    FILE *out = fopen("corr_sig.csv", "w");
    if (out == NULL) {
        perror("fopen");
        return;
    }

    for (i = 0; i < len; i++) {
        fprintf(out, "%f,%f\n", ref[i].real, ref[i].imag);
    }

    fclose(out);
}
#endif

struct correlator *corr_init(uint8_t *syms, size_t n, unsigned int sps)
{
    int status = -1;
    size_t i;
    struct correlator *ret = NULL;
    struct fsk_handle *fsk = NULL;
    struct complex_sample *raw_samples = NULL;
#ifdef LOG_CORRELATOR_OUTPUT
    char log_name[] = "correlator_log.csv";
#endif

    if (n < 8 || (n % 8 != 0)) {
        fprintf(stderr, "Number of correlation symbols must be a multiple of 8.\n");
        return NULL;
    } else if (sps < 1) {
        fprintf(stderr, "SPS must be >= 1.\n");
        return NULL;
    }

    ret = calloc(1, sizeof(ret[0]));
    if (ret == NULL) {
        perror("malloc");
        goto out;
    }

#   ifdef LOG_CORRELATOR_OUTPUT
    ret->out = fopen(log_name, "w");
    if (ret->out == NULL) {
        perror("fopen");
        goto out;
    }
#   endif

    //Length is the ceiling of (sps*n/DECIMATION_FACTOR)
    ret->len = 1 + (sps*n-1)/DECIMATION_FACTOR;

    ret->ref = malloc(ret->len * sizeof(ret->ref[0]));
    if (ret->ref == NULL) {
        perror("malloc");
        goto out;
    }

    /* Buffer is 2x as a means to implement a shift register using
     * moving insertion points */
    ret->buf = malloc(2 * ret->len * sizeof(ret->buf[0]));
    if (ret->buf == NULL) {
        perror("malloc");
        goto out;
    }

    raw_samples = malloc(sps * n * sizeof(raw_samples[0]));
    if (raw_samples == NULL) {
        perror("malloc");
        goto out;
    }

    //generate preamble waveform
    fsk = fsk_init();
    if (fsk == NULL){
        fprintf(stderr, "Couldn't initialize FSK modulator\n");
        goto out;
    }
    fsk_mod(fsk, syms, (int)n/8, raw_samples);
    //Convert sc16q11 to complexf and decimate
       for (i = 0; i < ret->len; i ++){
        ret->ref[i].real = raw_samples[i*DECIMATION_FACTOR].i/2048.0f;
        ret->ref[i].imag = raw_samples[i*DECIMATION_FACTOR].q/2048.0f;
       }

    /* Take the complex conjugate of our modulated reference signal
     * to avoid needing to do this each time we perform a dot product
     * operation later */
    for (i = 0; i < ret->len; i++) {
        ret->ref[i].imag = -ret->ref[i].imag;
    }

    ret->end = &ret->buf[2 * ret->len];
    //Maximum power is ret->len/DECIMATION_FACTOR * ret->len/DECIMATION_FACTOR
    ret->threshold_pwr = ret->len * ret->len * 0.5625f;

    ret->num_counts = sps/DECIMATION_FACTOR - 1;

    corr_reset(ret);

#   ifdef LOG_CORRELATION_SIGNAL
    save_reference_sig(ret->ref, ret->len);
#   endif

    status = 0;

    DBG("Correlator decimation factor = %u\n", DECIMATION_FACTOR);
    DBG("Correlator length: %zd symbols (%zd samples)\n",
        n, ret->len);

    DBG("Correlator power threshold: %f\n", ret->threshold_pwr);


out:
    if (status != 0) {
        corr_deinit(ret);
        ret = NULL;
    }

    fsk_close(fsk);
    free(raw_samples);

    return ret;
}

void corr_deinit(struct correlator *corr)
{
    if (corr) {
        free(corr->ref);
        free(corr->buf);

#       ifdef LOG_CORRELATOR_OUTPUT
        if (corr->out != NULL) {
            fclose(corr->out);
        }
#       endif

        free(corr);
    }
}

static inline void reset_insertion_points(struct correlator *corr)
{
    corr->ins1 = corr->buf;
    corr->ins2 = corr->buf + corr->len;
}

void corr_reset(struct correlator *corr)
{
    size_t i;

    if (corr != NULL) {
        reset_insertion_points(corr);
        corr->match_timestamp = CORRELATOR_NO_RESULT;
        corr->max = corr->threshold_pwr;
        corr->countdown = COUNTDOWN_INACTIVE;

        for (i = 0; i < (2 * corr->len); i++) {
            corr->buf[i].real = corr->buf[i].imag = 0.0f;
        }
    }
}

uint64_t corr_process(struct correlator *corr,
                      const struct complex_sample *samples, size_t n,
                      uint64_t timestamp)
{
    size_t i, j;

    uint64_t detected = CORRELATOR_NO_RESULT;

    for (i = 0; i < n; i += DECIMATION_FACTOR) {
        struct complexf *ref = corr->ref + corr->len - 1;
        struct complexf *buf = corr->ins2;
        struct complexf result;
        float result_pwr;

        /* Insert sample */
        //Scale by 1/2048
        corr->ins1->real = corr->ins2->real = samples[i].i/2048.0f;
        corr->ins1->imag = corr->ins2->imag = samples[i].q/2048.0f;

        result.real = result.imag = 0;

        /* Cross correlate */
        for (j = 0; j < corr->len; j ++) {
            result.real += ref->real * buf->real - ref->imag * buf->imag;
            result.imag += ref->real * buf->imag + ref->imag * buf->real;

            ref--;
            buf--;
        }

        result_pwr = result.real * result.real + result.imag * result.imag;

#       ifdef LOG_CORRELATOR_OUTPUT
        fprintf(corr->out, "%f, %"PRIu64"\n", result_pwr, timestamp);
#       endif

        if (result_pwr > corr->max) {
            corr->max = result_pwr;
            corr->countdown = corr->num_counts;
            corr->match_timestamp = timestamp;

            DBG("Got a match at %"PRIu64", result_pwr=%f. Resetting countdown.\n",
                timestamp, result_pwr);

        } else if (corr->countdown != COUNTDOWN_INACTIVE) {
            //Find the peak

            if (--corr->countdown == 0) {
                /* We have a result! Exit early with it */
                detected = corr->match_timestamp;

                DBG("Countdown complete. Acquired at: %"PRIu64"\n", detected);

                corr_reset(corr);
                return detected;
            } else {
                DBG("Countdown @ %u\n", corr->countdown);
            }
        }

        /* Update insertion points */
        corr->ins2++;
        if (corr->ins2 == corr->end) {
            reset_insertion_points(corr);
        } else {
            corr->ins1++;
        }

        /* Update record of which timestamp we're on...*/
        timestamp += 2;
    }

    return detected;
}

#ifdef CORRELATOR_TEST
#include <string.h>
#include <errno.h>
#include "utils.h"

static uint8_t code_a[] = { 0x2E, 0x69, 0x2C, 0xF0 };

int main(int argc, char *argv[])
{
    FILE *in = NULL;
    int16_t *raw_samples = NULL;
    struct complex_sample *samples = NULL;
    struct correlator *corr = NULL;
    uint64_t timestamp = 0;
    uint64_t acquisition = CORRELATOR_NO_RESULT;
    int sps;
    int i;
    int status = -1;

    int num_samples;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <CSV input file> <sps>\n", argv[0]);
        return EXIT_FAILURE;
    }

    num_samples = load_samples_from_csv_file(argv[1], false, 0, &raw_samples);
    if (num_samples < 0){
        fprintf(stderr, "Couldn't load samples from file\n");
        return 1;
    }
    printf("Loaded %d samples\n", num_samples);

    sps = atoi(argv[2]);

    samples = calloc(num_samples, sizeof(samples[0]));
    if (samples == NULL) {
        perror("calloc");
        goto out;
    }

    corr = corr_init(code_a, 8 * sizeof(code_a), sps);
    if (corr == NULL) {
        fprintf(stderr, "Failed to initialize correlator.\n");
        goto out;
    }

    //Convert sc16q11 to complex_sample
    for (i = 0; i < num_samples; ++i) {
        samples[i].i = raw_samples[(2*i)];
        samples[i].q = raw_samples[(2*i)+1];
    }

    acquisition = corr_process(corr, samples, num_samples, timestamp);

    if (acquisition == CORRELATOR_NO_RESULT) {
        printf("Correlation symbols not found.\n");
    } else {
        printf("Acquired Code. Acquisition = %"PRIu64"\n", acquisition);
    }

    status = 0;

out:
    corr_deinit(corr);

    if (in != NULL) {
        fclose(in);
    }

    free(raw_samples);
    free(samples);
    return status;
}
#endif
