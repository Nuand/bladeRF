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
#include <limits.h>
#include "libbladeRF.h"
#include "board/board.h"
#include "helpers/version.h"
#include "device_calibration.h"
#include "log.h"
#include "common.h"

#define GAIN_CAL_HEADER_RX "RX Chain,RX Gain,VSG Power into bladeRF RX (dBm),Frequency of signal (Hz),Frequency of bladeRF+PXI (Hz),AD9361 RSSI register value,Power of Signal from Full Scale (dBFS)\0"
#define GAIN_CAL_HEADER_TX "TX Chain,TX Gain,Frequency of Signal (Hz),Frequency of bladeRF+PXI (Hz),VSA Measured Power (dBm)\0"

#define GAIN_CAL_VERSION (struct bladerf_version) { \
    .describe = "gain calibration table", \
    .major = 1, \
    .minor = 0, \
    .patch = 0, \
}

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

static size_t count_csv_entries(const char *filename) {
    char line[1024]; // Adjust buffer size as needed
    int count = 0;

    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        return -1;
    }

    while (fgets(line, sizeof(line), file)) {
        count++;
    }

    fclose(file);

    return count - 2; // Subtract 2 to account for the serial number and header lines
}

int gain_cal_csv_to_bin(struct bladerf *dev, const char *csv_path, const char *binary_path, bladerf_channel ch)
{
    int status = 0;
    struct bladerf_image *image;
    size_t data_size;
    size_t entry_size;
    size_t offset = 0;

    char line[256];
    char current_dir[1000];
    char expected_header[1024];
    char device_serial[BLADERF_SERIAL_LENGTH];
    char csv_serial[BLADERF_SERIAL_LENGTH];

    uint64_t frequency;
    float power;
    uint64_t cw_freq;
    uint8_t chain;
    bladerf_gain gain;
    int32_t rssi;
    float vsg_power;
    uint64_t signal_freq;

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

    strncpy(device_serial, dev->ident.serial, BLADERF_SERIAL_LENGTH);
    device_serial[BLADERF_SERIAL_LENGTH - 1] = '\0';

    if (!fgets(line, sizeof(line), csvFile)) {
        status = BLADERF_ERR_INVAL;
        log_error("Error reading serial number from CSV file or file is empty.\n");
        goto error;
    }

    sscanf(line, "Serial: %s", csv_serial);
    if (strcmp(device_serial, csv_serial) != 0) {
        log_warning("Gain calibration file serial (%s) does not match device serial (%s)\n", csv_serial, device_serial);
    }

    size_t num_entries = count_csv_entries(csv_path);
    if (num_entries == 0) {
        status = BLADERF_ERR_INVAL;
        log_error("Error reading header from CSV file or file is empty.\n");
        goto error;
    }

    entry_size = (BLADERF_CHANNEL_IS_TX(ch))
        ? sizeof(chain) + sizeof(gain) + sizeof(cw_freq) + sizeof(frequency) + sizeof(power)
        : sizeof(chain) + sizeof(gain) + sizeof(vsg_power) + sizeof(signal_freq) + sizeof(frequency) + sizeof(rssi) + sizeof(power);

    data_size = num_entries * entry_size + BLADERF_SERIAL_LENGTH;

    image = bladerf_alloc_image(dev, BLADERF_IMAGE_TYPE_GAIN_CAL, 0xffffffff, data_size);
    if (image == NULL) {
        log_error("Failed to allocate image\n");
        status = BLADERF_ERR_MEM;
        goto error;
    }

    if (!fgets(line, sizeof(line), csvFile)) {
        status = BLADERF_ERR_INVAL;
        log_error("Error reading header from CSV file or file is empty.\n");
        goto error;
    }

    strncpy(expected_header, (BLADERF_CHANNEL_IS_TX(ch)) ? GAIN_CAL_HEADER_TX : GAIN_CAL_HEADER_RX, sizeof(expected_header));
    if (strncmp(line, expected_header, strlen(expected_header)) != 0) {
        status = BLADERF_ERR_INVAL;
        log_error("CSV format does not match expected %s headers\n", (BLADERF_CHANNEL_IS_TX(ch)) ? "TX" : "RX");
        goto error;
    }

    image->version = GAIN_CAL_VERSION;

    memcpy(&image->data[offset], device_serial, BLADERF_SERIAL_LENGTH);
    offset += BLADERF_SERIAL_LENGTH;

    for (size_t i = 0; i < num_entries; i++) {
        if (!fgets(line, sizeof(line), csvFile)) {
            break;
        }

        if (BLADERF_CHANNEL_IS_TX(ch)) {
            sscanf(line, "%" SCNu8 ",%" SCNi32 ",%" SCNu64 ",%" SCNu64 ",%f",
                &chain, &gain, &cw_freq, &frequency, &power);

            memcpy(&image->data[offset], &chain, sizeof(chain));
            offset += sizeof(chain);
            memcpy(&image->data[offset], &gain, sizeof(gain));
            offset += sizeof(gain);
            memcpy(&image->data[offset], &cw_freq, sizeof(cw_freq));
            offset += sizeof(cw_freq);
            memcpy(&image->data[offset], &frequency, sizeof(frequency));
            offset += sizeof(frequency);
            memcpy(&image->data[offset], &power, sizeof(power));
            offset += sizeof(power);
        } else {
            sscanf(line, "%" SCNu8 ",%" SCNi32 ",%f,%" SCNu64 ",%" SCNu64 ",%" SCNi32 ",%f",
                &chain, &gain, &vsg_power, &signal_freq, &frequency, &rssi, &power);

            memcpy(&image->data[offset], &chain, sizeof(chain));
            offset += sizeof(chain);
            memcpy(&image->data[offset], &gain, sizeof(gain));
            offset += sizeof(gain);
            memcpy(&image->data[offset], &vsg_power, sizeof(vsg_power));
            offset += sizeof(vsg_power);
            memcpy(&image->data[offset], &signal_freq, sizeof(signal_freq));
            offset += sizeof(signal_freq);
            memcpy(&image->data[offset], &frequency, sizeof(frequency));
            offset += sizeof(frequency);
            memcpy(&image->data[offset], &rssi, sizeof(rssi));
            offset += sizeof(rssi);
            memcpy(&image->data[offset], &power, sizeof(power));
            offset += sizeof(power);
        }
    }

    log_debug("Writing image to file: %s\n", binary_path);
    bladerf_image_write(dev, image, binary_path);
    bladerf_free_image(image);

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
    tbl->file_path_len = PATH_MAX;

    tbl->entries = malloc(num_entries * sizeof(struct bladerf_gain_cal_entry));
    if (tbl->entries == NULL) {
        log_error("failed to allocate memory for calibration table entries\n");
        return BLADERF_ERR_MEM;
    }

    tbl->file_path = malloc(tbl->file_path_len + 1);
    if (tbl->file_path == NULL) {
        log_error("failed to allocate memory for calibration table file path\n");
        return BLADERF_ERR_MEM;
    }

    tbl->state = BLADERF_GAIN_CAL_LOADED;
    return 0;
}

void gain_cal_tbl_free(struct bladerf_gain_cal_tbl *tbl) {
    log_verbose("Freeing gain calibration table\n");

    if (tbl->entries != NULL) {
        free(tbl->entries);
        tbl->entries = NULL;
    }

    if (tbl->file_path != NULL) {
        free(tbl->file_path);
        tbl->file_path = NULL;
    }

    tbl->version = (struct bladerf_version){0, 0, 0, NULL};
    tbl->enabled = false;
    tbl->ch = 0;
    tbl->n_entries = 0;
    tbl->start_freq = 0;
    tbl->stop_freq = 0;
    tbl->gain_target = 0;
    tbl->file_path_len = 0;
    tbl->state = BLADERF_GAIN_CAL_UNLOADED;
}

int load_gain_calibration(struct bladerf *dev, bladerf_channel ch, const char *binary_path) {
    int num_channels = 4;
    struct bladerf_gain_cal_tbl gain_tbls[num_channels];
    bladerf_gain current_gain;
    uint64_t frequency;
    float power;
    size_t entry_counter;
    size_t offset;
    int status = 0;

    uint64_t cw_freq;
    uint8_t chain;
    bladerf_gain gain;
    int32_t rssi;
    float vsg_power;
    bladerf_frequency signal_freq;

    struct bladerf_image *image = NULL;
    size_t entry_size;
    size_t num_entries;
    char device_serial[BLADERF_SERIAL_LENGTH];
    char file_serial[BLADERF_SERIAL_LENGTH];

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

    entry_size = (BLADERF_CHANNEL_IS_TX(ch))
        ? sizeof(chain) + sizeof(gain) + sizeof(cw_freq) + sizeof(frequency) + sizeof(power)
        : sizeof(chain) + sizeof(gain) + sizeof(vsg_power) + sizeof(signal_freq) + sizeof(frequency) + sizeof(rssi) + sizeof(power);

    image = bladerf_alloc_image(dev, BLADERF_IMAGE_TYPE_GAIN_CAL, 0, 0);
    status = bladerf_image_read(image, binary_path);
    if (status != 0) {
        log_error("Failed to read image: %s\n", bladerf_strerror(status));
        goto error;
    }

    if (version_equal(&image->version, &GAIN_CAL_VERSION) == false) {
        log_error("Expected gain calibration table: v%i.%i.%i\n",
            GAIN_CAL_VERSION.major, GAIN_CAL_VERSION.minor, GAIN_CAL_VERSION.patch);
        log_error("Imported gain calibration table: v%i.%i.%i\n",
            image->version.major, image->version.minor, image->version.patch);
        status = BLADERF_ERR_INVAL;
        goto error;
    }

    strncpy(device_serial, dev->ident.serial, BLADERF_SERIAL_LENGTH);
    device_serial[BLADERF_SERIAL_LENGTH - 1] = '\0';
    memcpy(file_serial, image->data, BLADERF_SERIAL_LENGTH);
    file_serial[BLADERF_SERIAL_LENGTH - 1] = '\0';

    if (strcmp(device_serial, file_serial) != 0) {
        log_warning("Calibration file serial (%s) does not match device serial (%s)\n", file_serial, device_serial);
    }

    offset = BLADERF_SERIAL_LENGTH;
    entry_counter = 0;
    num_entries = (image->length - BLADERF_SERIAL_LENGTH) / entry_size;
    for (uint64_t i = 0; i < num_entries; i++) {
        if (BLADERF_CHANNEL_IS_TX(ch)) {
            memcpy(&chain, &image->data[offset], sizeof(chain));
            offset += sizeof(chain);
            memcpy(&gain, &image->data[offset], sizeof(gain));
            offset += sizeof(gain);
            memcpy(&cw_freq, &image->data[offset], sizeof(cw_freq));
            offset += sizeof(cw_freq);
            memcpy(&frequency, &image->data[offset], sizeof(frequency));
            offset += sizeof(frequency);
            memcpy(&power, &image->data[offset], sizeof(power));
            offset += sizeof(power);
        } else {
            memcpy(&chain, &image->data[offset], sizeof(chain));
            offset += sizeof(chain);
            memcpy(&gain, &image->data[offset], sizeof(gain));
            offset += sizeof(gain);
            memcpy(&vsg_power, &image->data[offset], sizeof(vsg_power));
            offset += sizeof(vsg_power);
            memcpy(&signal_freq, &image->data[offset], sizeof(signal_freq));
            offset += sizeof(signal_freq);
            memcpy(&frequency, &image->data[offset], sizeof(frequency));
            offset += sizeof(frequency);
            memcpy(&rssi, &image->data[offset], sizeof(rssi));
            offset += sizeof(rssi);
            memcpy(&power, &image->data[offset], sizeof(power));
            offset += sizeof(power);
        }

        if (BLADERF_CHANNEL_IS_TX(ch) && chain == 0 && gain == 60) {
            gain_tbls[ch].entries[entry_counter].freq = frequency;
            gain_tbls[ch].entries[entry_counter].gain_corr = power;
            entry_counter++;
        }

        if (!BLADERF_CHANNEL_IS_TX(ch) && chain == 0 && gain == 0) {
            gain_tbls[ch].entries[entry_counter].freq = frequency;
            gain_tbls[ch].entries[entry_counter].gain_corr = power - vsg_power;
            entry_counter++;
        }
    }

    if (entry_counter == 0) {
        log_error("No valid entries found: %s\n", binary_path);
        status = BLADERF_ERR_UNEXPECTED;
        goto error;
    }

    gain_tbls[ch].version = image->version;
    gain_tbls[ch].start_freq = gain_tbls[ch].entries[0].freq;
    gain_tbls[ch].stop_freq = gain_tbls[ch].entries[entry_counter-1].freq;
    gain_tbls[ch].n_entries = entry_counter;
    gain_tbls[ch].ch = ch;
    gain_tbls[ch].state = BLADERF_GAIN_CAL_LOADED;
    gain_tbls[ch].enabled = true;
    gain_tbls[ch].gain_target = current_gain;
    strncpy(gain_tbls[ch].file_path, binary_path, gain_tbls[ch].file_path_len);

    gain_cal_tbl_free(&dev->gain_tbls[ch]);
    dev->gain_tbls[ch] = gain_tbls[ch];

error:
    if (status != 0) {
        log_error("binary_path: %s\n", binary_path);
    }

    if (binaryFile)
        fclose(binaryFile);
    if (image)
        bladerf_free_image(image);
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
