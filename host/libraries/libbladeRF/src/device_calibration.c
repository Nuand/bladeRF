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

#define __round_int(x) (x >= 0 ? (int)(x + 0.5) : (int)(x - 0.5))

#define RETURN_ERROR_STATUS(_what, _status)                   \
    do {                                                      \
        log_error("%s: %s failed: %s\n", __FUNCTION__, _what, \
                  bladerf_strerror(_status));                 \
        return _status;                                       \
    } while (0)

#define CHECK_STATUS(_fn)                  \
    do {                                   \
        int _s = _fn;                      \
        if (_s < 0) {                      \
            RETURN_ERROR_STATUS(#_fn, _s); \
        }                                  \
    } while (0)


int gain_cal_csv_to_bin(const char *csv_path, const char *binary_path, bladerf_channel ch)
{
    uint64_t frequency;
    float power;
    uint64_t cw_freq;
    uint8_t chain;
    bladerf_gain gain;
    int32_t rssi;
    float vsg_power;
    uint64_t signal_freq, bladeRF_PXI_freq;


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
        if (BLADERF_CHANNEL_IS_TX(ch)) {
            sscanf(line, "%" SCNu8 ",%" SCNi32 ",%" SCNu64 ",%" SCNu64 ",%f",
                &chain, &gain, &cw_freq, &frequency, &power);

            if (chain == 0 && gain == 60) {
                fwrite(&frequency, sizeof(uint64_t), 1, binaryFile);
                fwrite(&power, sizeof(float), 1, binaryFile);
            }
        } else {
            sscanf(line, "%" SCNu8 ",%" SCNi32 ",%f,%" SCNu64 ",%" SCNu64 ",%" SCNi32 ",%f",
                &chain, &gain, &vsg_power, &signal_freq, &bladeRF_PXI_freq, &rssi, &power);

            if (chain == 0 && gain == 0) {
                fwrite(&bladeRF_PXI_freq, sizeof(uint64_t), 1, binaryFile);
                fwrite(&power, sizeof(float), 1, binaryFile);
            }
        }
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

static int normalize_gain_correction(struct bladerf_gain_cal_tbl *tbl, bladerf_gain ref_pwr) {
    if (tbl->state != BLADERF_GAIN_CAL_LOADED) {
        log_error("The gain calibration table is not loaded for channel %d\n", tbl->ch);
        return BLADERF_ERR_MEM;
    }

    for (size_t i = 0; i < tbl->n_entries; i++) {
        tbl->entries[i].gain_corr -= (double)ref_pwr;
    }

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

    switch (ch) {
    /** bladeRF calibration target is -30dBFS for RX */
    case BLADERF_CHANNEL_RX(0):
    case BLADERF_CHANNEL_RX(1):
        status = normalize_gain_correction(&gain_tbls[ch], -30);
        break;

    /** bladeRF calibration target is 0dBm for TX */
    case BLADERF_CHANNEL_TX(0):
    case BLADERF_CHANNEL_TX(1):
        status = normalize_gain_correction(&gain_tbls[ch], 0);
        break;

    default:
        log_error("Channel unknown: ch = %i?\n", ch);
        status = BLADERF_ERR_UNEXPECTED;
        goto error;
    }
    if (status != 0) {
        goto error;
    }

    free(dev->gain_tbls[ch].entries);
    dev->gain_tbls[ch] = gain_tbls[ch];

error:
    if (binaryFile)
        fclose(binaryFile);
    return status;
}

static void find_floor_ceil_entries_by_frequency(const struct bladerf_gain_cal_tbl *tbl, bladerf_frequency freq,
                                                 struct bladerf_gain_cal_entry **floor, struct bladerf_gain_cal_entry **ceil) {
    int mid = 0;
    *floor = NULL;
    *ceil = NULL;

    if (tbl == NULL || tbl->entries == NULL || tbl->n_entries == 0) {
        return;
    }

    int32_t low = 0;
    int32_t high = tbl->n_entries - 1;

    /* Binary search for the entry with the closest frequency to 'freq' */
    while (low <= high && high >= 0) {
        mid = (low + high) / 2;
        if (tbl->entries[mid].freq == freq) {
            *floor = &tbl->entries[mid];
            *ceil = &tbl->entries[mid];
            return;
        } else if (tbl->entries[mid].freq < freq) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    /* At this point, 'low' points to the first entry greater than 'freq',
       and 'high' points to the last entry less than 'freq'. */
    if ((uint32_t)low < tbl->n_entries) {
        *ceil = &tbl->entries[low];
    }

    /* If 'high' is negative, then there are no entries less than 'freq' */
    *floor = (high >= 0) ? &tbl->entries[high] : &tbl->entries[0];
}

int get_gain_cal_entry(const struct bladerf_gain_cal_tbl *tbl, bladerf_frequency freq, struct bladerf_gain_cal_entry *result) {
    struct bladerf_gain_cal_entry *floor_entry, *ceil_entry;

    if (tbl == NULL || result == NULL) {
        return BLADERF_ERR_INVAL;
    }

    find_floor_ceil_entries_by_frequency(tbl, freq, &floor_entry, &ceil_entry);
    if (!floor_entry || !ceil_entry) {
        log_error("Could not find ceil or floor entries in the calibration table\n");
        return BLADERF_ERR_UNEXPECTED;
    }

    if (floor_entry->freq == ceil_entry->freq) {
        result->freq = freq;
        result->gain_corr = floor_entry->gain_corr;
        return 0;
    }

    double interpolated_gain_corr = floor_entry->gain_corr +
                                    (freq - floor_entry->freq) *
                                    (ceil_entry->gain_corr - floor_entry->gain_corr) /
                                    (ceil_entry->freq - floor_entry->freq);

    result->freq = freq;
    result->gain_corr = interpolated_gain_corr;
    return 0;
}

int get_gain_correction(struct bladerf *dev, bladerf_frequency freq, bladerf_channel ch, bladerf_gain *compensated_gain) {
    int status = 0;
    struct bladerf_gain_cal_tbl *cal_table = &dev->gain_tbls[ch];
    struct bladerf_gain_cal_entry entry_next;

    CHECK_STATUS(get_gain_cal_entry(cal_table, freq, &entry_next));

    *compensated_gain = __round_int(cal_table->gain_target - entry_next.gain_corr);

    log_verbose("Target gain:  %i, Compen. gain: %i\n", dev->gain_tbls[ch].gain_target, *compensated_gain);
    return status;
}

int apply_gain_correction(struct bladerf *dev, bladerf_channel ch, bladerf_frequency frequency) {
    struct bladerf_range const *gain_range = NULL;
    bladerf_frequency current_frequency;
    bladerf_gain gain_compensated;

    if (dev->gain_tbls[ch].enabled == false) {
        log_error("Gain compensation disabled. Can't apply gain correction.\n");
        return BLADERF_ERR_UNEXPECTED;
    }

    CHECK_STATUS(dev->board->get_gain_range(dev, ch, &gain_range));
    CHECK_STATUS(dev->board->get_frequency(dev, ch, &current_frequency));
    CHECK_STATUS(get_gain_correction(dev, frequency, ch, &gain_compensated));

    if (gain_compensated > gain_range->max || gain_compensated < gain_range->min) {
        log_warning("Power compensated gain out of range [%i:%i]: %i\n",
            gain_range->min, gain_range->max, gain_compensated);
        gain_compensated = (gain_compensated > gain_range->max) ? gain_range->max : gain_range->min;
        log_warning("Gain clamped to: %i\n", gain_compensated);
    }

    CHECK_STATUS(dev->board->set_gain(dev, ch, gain_compensated););

    return 0;
}
