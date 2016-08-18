/**
 * @brief   Filters the input signal with the given FIR filter coefficients
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
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "host_config.h"

#include "fir_filter.h"

#ifdef ENABLE_FIR_FILTER_DEBUG_MSG
#   define DBG(...) fprintf(stderr, "[FIR] " __VA_ARGS__)
#else
#   define DBG(...)
#endif

struct fir_filter {

    float *taps;        /* Filter taps */
    size_t length;      /* Length of the filter, in taps */

    /* Interpolation or decimation factor */
    unsigned int factor;

    /* Insertion points into "shift register" implementation that utilizes
     * two copies of data to achieve a contiguious shifting window. */
    size_t ins1;
    size_t ins2;

    /* Filter state buffers */
    int32_t *i;
    int32_t *q;
};

static void fir_reset(struct fir_filter *filt)
{
    size_t n;

    for (n = 0; n < (2 * filt->length); n++) {
        filt->i[n] = 0;
        filt->q[n] = 0;
    }

    /* Remember, state is 2 copies: array len is 2 * filt->length elts */
    filt->ins1  = 0;
    filt->ins2  = filt->length;
}

void fir_deinit(struct fir_filter *filt)
{
    if (filt) {
        free(filt->q);
        free(filt->i);
        free(filt->taps);
        free(filt);
    }
}

struct fir_filter * fir_init(const float *taps, size_t length)
{
    struct fir_filter *filt;

    filt = calloc(1, sizeof(filt[0]));
    if (!filt) {
        perror("calloc");
        return NULL;
    }

    /* Filter state is a sliding window implemented with 2 copies of data */
    filt->i = calloc(2 * length, sizeof(filt->i[0]));
    if (!filt->i) {
        perror("calloc");
        fir_deinit(filt);
        return NULL;
    }

    filt->q = calloc(2 * length, sizeof(filt->q[0]));
    if (!filt->i) {
        perror("calloc");
        fir_deinit(filt);
        return NULL;
    }

    filt->taps = calloc(length, sizeof(filt->taps[0]));
    if (!filt->taps) {
        perror("calloc");
        fir_deinit(filt);
        return NULL;
    }

    memcpy(filt->taps, taps, length * sizeof(filt->taps[0]));
    filt->length = length;

    fir_reset(filt);
    return filt;
}

void fir_process(struct fir_filter *f, int16_t *input,
                    struct complex_sample *output, size_t count)
{
    /* Index into input/output buffers */
    size_t inout_idx;

    /* Index into f->taps array */
    size_t t;

    /* Index into f->i and f->q state arrays */
    size_t s;

    for (inout_idx = 0; inout_idx < (2*count); inout_idx += 2) {
        int32_t i = input[inout_idx];
        int32_t q = input[inout_idx+1];
        float result_i = 0;
        float result_q = 0;

        /* Insert samples */
        f->i[f->ins1] = f->i[f->ins2] = i;
        f->q[f->ins1] = f->q[f->ins2] = q;

        /* Convolve */
        for (t = 0, s = f->ins2; t < f->length; t++, s--) {
            float tmp_i, tmp_q;

            tmp_i = f->taps[t] * f->i[s];
            tmp_q = f->taps[t] * f->q[s];

            result_i += tmp_i;
            result_q += tmp_q;
        }

        /* Update output sample */
        output[inout_idx/2].i = (int16_t) roundf(result_i);
        output[inout_idx/2].q = (int16_t) roundf(result_q);

        /* Advance insertion points */
        f->ins2++;
        if (f->ins2 == (2 * f->length)) {
            f->ins1 = 0;
            f->ins2 = f->length;
        } else {
            f->ins1++;
        }
    }
}

#ifdef FIR_FILTER_TEST
#include <stdbool.h>
#include "conversions.h"
#include "rx_ch_filter.h"
#include "utils.h"

int main(int argc, char *argv[])
{
    int status              = EXIT_FAILURE;
    FILE *infile            = NULL;
    FILE *outfile           = NULL;

    int16_t *inbuf = NULL, *tempbuf = NULL;
    struct complex_sample *outbuf = NULL;

    struct fir_filter *filt = NULL;

    static const unsigned int max_chunk_size = 1024 * 1024 * 1024;

    unsigned int chunk_size = 4096;
    size_t n_read, n_written;
    bool done = false;

    if (argc < 3 || argc > 4) {
        fprintf(stderr,
                "Filter sc16q11 samples from <infile> and write"
                "them to <outfile>.\n\n");

        fprintf(stderr,
                "Usage: %s <infile> <outfile> [# chunk size(samples)]\n",
                argv[0]);

        return EXIT_FAILURE;
    }

    if (argc == 4) {
        bool valid;
        chunk_size = str2uint(argv[3], 1, max_chunk_size, &valid);
        if (!valid) {
            fprintf(stderr, "Invalid chunk size: %s samples\n", argv[3]);
            return EXIT_FAILURE;
        }
    }

    filt = fir_init(rx_ch_filter, rx_ch_filter_len);
    if (!filt) {
        fprintf(stderr, "Failed to allocate filter.\n");
        return EXIT_FAILURE;
    }

    inbuf = calloc(2*sizeof(int16_t), chunk_size);
    if (!inbuf) {
        perror("calloc");
        goto out;
    }
    tempbuf = calloc(2*sizeof(int16_t), chunk_size);
    if (!tempbuf) {
        perror("calloc");
        goto out;
    }

    outbuf = calloc(sizeof(struct complex_sample), chunk_size);
    if (!outbuf) {
        perror("calloc");
        goto out;
    }

    infile = fopen(argv[1], "rb");
    if (!infile) {
        perror("Failed to open input file");
        goto out;
    }

    outfile = fopen(argv[2], "wb");
    if (!outfile) {
        perror("Failed to open output file");
        goto out;
    }

    while (!done) {
        n_read = fread(inbuf, 2*sizeof(int16_t), chunk_size, infile);
        done = n_read != chunk_size;

        fir_process(filt, inbuf, outbuf, n_read);
        //convert
        conv_struct_to_samples(outbuf, (unsigned int) n_read, tempbuf);

        n_written = fwrite(tempbuf, 2*sizeof(int16_t), n_read, outfile);
        if (n_written != n_read) {
            fprintf(stderr, "Short write encountered. Exiting.\n");
            status = -1;
            goto out;
        }
    }

    status = 0;

out:
    if (infile) {
        fclose(infile);
    }

    if (outfile) {
        fclose(outfile);
    }
    free(tempbuf);
    free(inbuf);
    free(outbuf);
    fir_deinit(filt);

    return status;
}
#endif
