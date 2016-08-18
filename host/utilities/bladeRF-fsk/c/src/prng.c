/**
 * @brief   XORshift Pseudorandom number generator
 *
 * https://en.wikipedia.org/wiki/Xorshift#xorshift.2A
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
#include "prng.h"

uint8_t * prng_fill(uint64_t *state_inout, size_t len)
{
    size_t i;
    size_t to_fill;
    uint8_t *ret, *curr;
    uint64_t state = *state_inout;

    assert(state != 0);

    ret = malloc(len);
    if (!ret) {
        return NULL;
    }

    curr = ret;

    /* Fill 64-bit words first */
    to_fill = (len / 8) * 8;

    for (i = 0; i < to_fill; i += 8, curr += 8) {
        state = prng_update(state);
        curr[0] = (state >> 0)  & 0xff;
        curr[1] = (state >> 8)  & 0xff;
        curr[2] = (state >> 16) & 0xff;
        curr[3] = (state >> 24) & 0xff;
        curr[4] = (state >> 32) & 0xff;
        curr[5] = (state >> 40) & 0xff;
        curr[6] = (state >> 48) & 0xff;
        curr[7] = (state >> 56) & 0xff;
    }

    /* Fill remaining partial words */
    to_fill = len - to_fill;
    if (to_fill != 0) {
        state = prng_update(state);
        for (i = 0; i < to_fill; i++) {
            *curr = state & 0xff;
            state >>= 8;
            curr++;
        }
    }

    return ret;
}

#ifdef PRNG_TEST

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

int main(int argc, char *argv[])
{
    int len;
    uint64_t seed;
    uint8_t *buf;
    FILE *out;
    ssize_t n_written;
    int status = 0;

    if (argc < 4 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
        fprintf(stderr, "Usage: %s <seed> <length> <output file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    seed = atol(argv[1]);
    if (seed == 0) {
        fprintf(stderr, "Seed must be non-zero.\n");
        return EXIT_FAILURE;
    }

    len = atoi(argv[2]);
    if (len <= 0) {
        fprintf(stderr, "Length must be >= 1.\n");
        return EXIT_FAILURE;
    }

    buf = prng_fill(&seed, len);
    if (!buf) {
        fprintf(stderr, "Failed to create PRNG data buffer.\n");
        return EXIT_FAILURE;
    }

    out = fopen(argv[3], "wb");
    if (!out) {
        fprintf(stderr, "Failed to open %s for writing: %s\n",
                argv[3], strerror(errno));
        goto out;
    }

    n_written = fwrite(buf, 1, len, out);
    if (n_written != len) {
        fprintf(stderr, "Failed to write PRNG data.\n");
        status = EXIT_FAILURE;
    }

    fclose(out);

out:
    free(buf);
    return status;
}
#endif
