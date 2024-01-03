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

int test_serial_based_autoload(struct bladerf *dev, bladerf_channel ch) {
    int status = 0;
    char new_filename[255];
    char old_filename[255];
    const char *output_dir = "./output/";
    char *cal_bin = (char *)malloc(255);
    char *serial = (char *)malloc(255);
    if (serial == NULL || cal_bin == NULL) {
        status = BLADERF_ERR_MEM;
        goto out;
    }

    status = bladerf_get_serial(dev, serial);
    if (status != 0) {
        goto out;
    }

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        strncpy(cal_bin, "tx_sweep.tbl", 255);
        snprintf(new_filename, sizeof(new_filename), "%s%s_tx_gain_cal.tbl", output_dir, serial);
    } else {
        strncpy(cal_bin, "rx_sweep.tbl", 255);
        snprintf(new_filename, sizeof(new_filename), "%s%s_rx_gain_cal.tbl", output_dir, serial);
    }

    snprintf(old_filename, sizeof(old_filename), "%s%s", output_dir, cal_bin);

    if (rename(old_filename, new_filename) != 0) {
        status = BLADERF_ERR_IO;
        goto out;
    }

    status = bladerf_load_gain_calibration(dev, ch, NULL);
    if (status != 0) {
        goto out;
    }

out:
    free(serial);
    free(cal_bin);
    return status;
}
