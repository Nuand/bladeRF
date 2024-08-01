/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2023 Nuand LLC
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


#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "libbladeRF.h"
#include "board/board.h"
#include "device_calibration.h"
#include "log.h"
#include "common.h"

int gain_cal_csv_to_bin(const char *csv_path, const char *binary_path)
{
    uint64_t frequency;
    float power;
    char line[256];
    char current_dir[1000];
    int status = 0;

    FILE *csvFile = fopen(csv_path, "r");
    FILE *binaryFile = fopen(binary_path, "wb");
    if (!csvFile || !binaryFile) {
        status = BLADERF_ERR_NO_FILE;
        if (getcwd(current_dir, sizeof(current_dir)) != NULL) {
            log_error("Error opening calibration file: %s\n", strcat(current_dir, csv_path));
        } else {
            log_error("Error opening calibration file\n");
        }
        goto error;
    }

    /** Check for empty CSV */
    if (!fgets(line, sizeof(line), csvFile)) {
        status = BLADERF_ERR_INVAL;
        log_error("Error reading header from CSV file or file is empty.\n");
        goto error;
    }

    /** Transfer CSV data to binary */
    while (fgets(line, sizeof(line), csvFile)) {
        sscanf(line, "%lu,%f", &frequency, &power);
        fwrite(&frequency, sizeof(uint64_t), 1, binaryFile);
        fwrite(&power, sizeof(float), 1, binaryFile);
    }

error:
    if (csvFile)
        fclose(csvFile);
    if (binaryFile)
        fclose(binaryFile);
    return status;
}

static int gain_cal_tbl_init(struct bladerf_gain_cal_tbl *tbl, uint32_t num_entries) {
    if (tbl == NULL) {
        log_error("calibration table is NULL\n");
        return BLADERF_ERR_MEM;
    }

    tbl->version = (struct bladerf_version){0, 0, 0, NULL};
    tbl->n_entries = num_entries;
    tbl->start_freq = 0;
    tbl->stop_freq = 0;

    tbl->entries = malloc(num_entries * sizeof(struct bladerf_gain_cal_entry));
    if (tbl->entries == NULL) {
        log_error("failed to allocate memory for calibration table entries\n");
        return BLADERF_ERR_MEM;
    }

    tbl->state = BLADERF_GAIN_CAL_LOADED;
    return 0;
}

int load_gain_calibration(struct bladerf *dev, bladerf_channel ch, const char *binary_path) {
    int num_channels = 4;
    struct bladerf_gain_cal_tbl gain_tbls[num_channels];
    bladerf_gain current_gain;
    uint64_t frequency;
    float power;
    int entry_counter;
    int status = 0;

    FILE *binaryFile = fopen(binary_path, "rb");
    if (!binaryFile) {
        log_error("Error opening binary file.\n");
        status = BLADERF_ERR_NO_FILE;
        goto error;
    }

    status = dev->board->get_gain(dev, ch, &current_gain);
    if (status != 0) {
        log_error("Failed to get gain: %s\n", bladerf_strerror(status));
        goto error;
    }

    status = gain_cal_tbl_init(&gain_tbls[ch], (uint32_t) 10e3);
    if (status != 0) {
        log_error("Error initializing gain calibration table\n");
        status = BLADERF_ERR_MEM;
        goto error;
    }

    entry_counter = 0;
    while (fread(&frequency, sizeof(uint64_t), 1, binaryFile) && fread(&power, sizeof(float), 1, binaryFile)) {
        gain_tbls[ch].entries[entry_counter].freq = frequency;
        gain_tbls[ch].entries[entry_counter].gain_corr = power;
        entry_counter++;
    }

    gain_tbls[ch].start_freq = gain_tbls[ch].entries[0].freq;
    gain_tbls[ch].stop_freq = gain_tbls[ch].entries[entry_counter-1].freq;
    gain_tbls[ch].n_entries = entry_counter;
    gain_tbls[ch].ch = ch;
    gain_tbls[ch].state = BLADERF_GAIN_CAL_LOADED;
    gain_tbls[ch].enabled = true;
    gain_tbls[ch].gain_target = current_gain;

    free(dev->gain_tbls[ch].entries);
    dev->gain_tbls[ch] = gain_tbls[ch];

error:
    if (binaryFile)
        fclose(binaryFile);
    return status;
}
