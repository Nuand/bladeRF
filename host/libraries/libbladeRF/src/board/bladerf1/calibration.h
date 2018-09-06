/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014 Nuand LLC
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

#ifndef BLADERF1_CALIBRATION_H_
#define BLADERF1_CALIBRATION_H_

#include <stdint.h>

#include <libbladeRF.h>

struct dc_cal_entry {
    unsigned int freq; /* Frequency (Hz) associated with this entry */
    int16_t dc_i;
    int16_t dc_q;

    int16_t max_dc_i;
    int16_t max_dc_q;
    int16_t mid_dc_i;
    int16_t mid_dc_q;
    int16_t min_dc_i;
    int16_t min_dc_q;
};

struct dc_cal_tbl {
    uint32_t version;
    uint32_t n_entries;
    struct bladerf_lms_dc_cals reg_vals;

    unsigned int curr_idx;
    struct dc_cal_entry *entries; /* Sorted (increasing) by freq */
};

extern struct dc_cal_tbl rx_cal_test;

/**
 * Get the index of an (approximate) match from the specific dc cal table
 *
 * @param[in]   tbl     Table to search
 * @param[in]   freq    Desired frequency
 *
 * @return index into tbl->entries[].
 */
unsigned int dc_cal_tbl_lookup(const struct dc_cal_tbl *tbl, unsigned int freq);

/**
 * Get the DC cal values associated with the specified frequencies. If the
 * specified frequency is not in the table, the DC calibration values will
 * be interpolated from surrounding entries.
 *
 * @param[in]   tbl      Table to search
 * @param[in]   freq     Desired frequency
 * @param[out]  entry    Found or interpolated DC calibration values
 */
void dc_cal_tbl_entry(const struct dc_cal_tbl *tbl,
                      unsigned int freq,
                      struct dc_cal_entry *entry);

/**
 * Load a DC calibration table from the provided data
 *
 * @param[in]   buf   Packed table data
 * @param[in]   len   Length of packed data, in bytes
 *
 * @return Loaded DC calibration table, or NULL on error
 */
struct dc_cal_tbl *dc_cal_tbl_load(const uint8_t *buf, size_t buf_len);

/**
 * Load a DC calibration table from an image file
 *
 * @param[in]   dev         bladeRF device handle
 * @param[out]  tbl         DC calibration Table
 * @param[in]   img_file    Path to image file
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int dc_cal_tbl_image_load(struct bladerf *dev,
                          struct dc_cal_tbl **tbl, const char *img_file);

/**
 * Free a DC calibration table
 *
 * @param[inout]    tbl     Pointer to table to free
 *
 * The table pointer will be set to NULL after freeing it.
 */
void dc_cal_tbl_free(struct dc_cal_tbl **tbl);

#endif
