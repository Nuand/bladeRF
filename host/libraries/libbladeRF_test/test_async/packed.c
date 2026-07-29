/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2026 Nuand LLC
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <libbladeRF.h>

#include "streaming/format.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static int test_format_sizes(void)
{
    size_t const num_samples = 8;

    if (samples_to_bytes(BLADERF_FORMAT_SC16_Q11_PACKED, num_samples) !=
            4 * num_samples ||
        samples_to_wire_bytes(BLADERF_FORMAT_SC16_Q11_PACKED, num_samples) !=
            3 * num_samples ||
        bytes_to_samples(BLADERF_FORMAT_SC16_Q11_PACKED, 3 * num_samples) !=
            num_samples) {
        fprintf(stderr, "packed logical or wire size is incorrect\n");
        return 1;
    }

    return 0;
}

static int test_in_place_round_trip(void)
{
    int16_t const samples[] = {
        0,     0,    1,     -1, -2048, 2047, 0x123, -0x234,
        -1,    -2048, 2047, 1,  -1024, 1024, 42,    -42,
    };
    uint8_t const expected[] = {
        0x00, 0x00, 0x00, 0x01, 0xf0, 0xff, 0x00, 0xf8,
        0x7f, 0x23, 0xc1, 0xdc, 0xff, 0x0f, 0x80, 0xff,
        0x17, 0x00, 0x00, 0x0c, 0x40, 0x2a, 0x60, 0xfd,
    };
    int16_t buffer[ARRAY_SIZE(samples)];

    memcpy(buffer, samples, sizeof(samples));
    sc16q11_pack_in_place(buffer, ARRAY_SIZE(samples) / 2);

    if (memcmp(buffer, expected, sizeof(expected)) != 0) {
        fprintf(stderr, "in-place packed bytes differ\n");
        return 1;
    }

    sc16q11_unpack_in_place(buffer, ARRAY_SIZE(samples) / 2);
    if (memcmp(buffer, samples, sizeof(samples)) != 0) {
        fprintf(stderr, "in-place packed round trip failed\n");
        return 1;
    }

    return 0;
}

int main(void)
{
    return test_format_sizes() + test_in_place_round_trip();
}
