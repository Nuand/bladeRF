/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2024 Nuand LLC
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
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include "libbladeRF.h"

int read_and_print_binary(const char *binary_path) {
    uint64_t frequency;
    float power;

    if (access(binary_path, F_OK) == -1) {
        fprintf(stderr, "Error: file %s does not exist\n", binary_path);
        return 1;
    }

    FILE *binaryFile = fopen(binary_path, "rb");
    if (!binaryFile) {
        fprintf(stderr, "Error opening binary file.\n");
        return 1;
    }

    printf("Frequency (Hz),Power (dBm)\n");
    while (fread(&frequency, sizeof(uint64_t), 1, binaryFile) && fread(&power, sizeof(float), 1, binaryFile)) {
        printf("%" PRIu64 ",%f\n", frequency, power);
    }

    fclose(binaryFile);
    return 0;
}
