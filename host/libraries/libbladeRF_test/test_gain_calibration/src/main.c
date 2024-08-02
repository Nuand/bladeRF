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
#include <math.h>
#include <unistd.h>
#include "libbladeRF.h"
#include "file_operations.h"

#include "log.h"
#include "helpers/file.h"

#define CHECK(func_call) do { \
    status = (func_call); \
    if (status != 0) { \
        fprintf(stderr, "Failed at %s: %s\n", #func_call, bladerf_strerror(status)); \
        status = EXIT_FAILURE; \
        goto out; \
    } \
} while (0)

static int init_sync(struct bladerf *dev)
{
    int status;
    const unsigned int num_buffers   = 512;
    const unsigned int buffer_size   = 16*2048; /* Must be a multiple of 1024 */
    const unsigned int num_transfers = 64;
    const unsigned int timeout_ms    = 3500;

    CHECK(bladerf_sync_config(dev, BLADERF_TX_X1, BLADERF_FORMAT_SC16_Q11,
                              num_buffers, buffer_size, num_transfers,
                              timeout_ms));

out:
    return status;
}

int main(int argc, char *argv[]) {
    int status;
    struct bladerf *dev;
    char *devstr = NULL;
    const char *rx_cal_file = "rx_sweep_0.csv";
    const char *tx_cal_file = "tx_sweep_60_fine.csv";
    bladerf_channel ch;

    const struct bladerf_range *freq_range;
    const bladerf_gain TARGET_GAIN = 30;
    bladerf_gain gain;
    bladerf_gain gain_expected;
    const struct bladerf_gain_cal_tbl *cal[4];

    bladerf_log_set_verbosity(BLADERF_LOG_LEVEL_INFO);

    CHECK(bladerf_open(&dev, devstr));

    ch = BLADERF_CHANNEL_RX(0);
    CHECK(bladerf_load_gain_calibration(dev, ch, rx_cal_file));
    CHECK(bladerf_get_gain_calibration(dev, ch, &cal[ch]));

    ch = BLADERF_CHANNEL_TX(0);
    CHECK(bladerf_load_gain_calibration(dev, ch, tx_cal_file));
    CHECK(bladerf_get_gain_calibration(dev, ch, &cal[ch]));

    for (int i = 0; i < 4; i++) {
        CHECK(bladerf_print_gain_calibration(dev, i, false));
    }

    /**
     * Monitor calibrated gain over frequency
     */
    CHECK(init_sync(dev));
    CHECK(bladerf_set_gain(dev, ch, TARGET_GAIN));

    CHECK(bladerf_enable_module(dev, ch, true));
    for (size_t entry_idx = 0; entry_idx < cal[ch]->n_entries; entry_idx++) {
        CHECK(bladerf_set_frequency(dev, ch, cal[ch]->entries[entry_idx].freq));
        CHECK(bladerf_get_gain(dev, ch, &gain));
        gain_expected = (int)round(TARGET_GAIN - cal[ch]->entries[entry_idx].gain_corr);

        if (gain != gain_expected) {
            fprintf(stderr, "[Error] Gain compensation failed TX frequency sweep\n");
            fprintf(stderr, "   Frequency:       %" PRIu64 "\n", cal[ch]->entries[entry_idx].freq);
            fprintf(stderr, "   Target gain:     %i\n", TARGET_GAIN);
            fprintf(stderr, "   Gain correction: %f\n", cal[ch]->entries[entry_idx].gain_corr);
            fprintf(stderr, "   Expected gain:   %i\n", gain_expected);
            goto out;
        }

        printf("\033[2K\rTX Frequency: %0.0f MHz, Gain: %i", cal[ch]->entries[entry_idx].freq/1e6, gain);
        fflush(stdout);
    }
    printf("\n");

out:
    bladerf_close(dev);
    return status;
}

