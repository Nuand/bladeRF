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
#ifndef BLADERF_CAL_TABLE_H_
#define BLADERF_CAL_TABLE_H_

#include <stdlib.h>
#include <stdint.h>
#include "bladerf_priv.h"

struct dc_cal_entry {
    unsigned int freq;          /* Frequency (Hz) associated with this entry */
    int16_t dc_i;
    int16_t dc_q;
};


struct dc_cal_tbl {
    uint32_t version;
    uint32_t n_entries;
    struct bladerf_lms_dc_cals reg_vals;

    unsigned int curr_idx;
    struct dc_cal_entry *entries;  /* Sorted (increasing) by freq */
};

extern struct dc_cal_tbl rx_cal_test;

/**
 * Get the index of an (approximate) match from the specific dc cal table
 *
 * @param   tbl     Table to search
 * @param   freq    Desired frequency
 *
 * @param   Index into tbl->entries[]
 */
unsigned int dc_cal_tbl_lookup(const struct dc_cal_tbl *tbl, unsigned int freq);

/**
 * Get the DC cal values associated with the specified frequencies. If the
 * specified frequency is not in the table, the DC calibration values will
 * be interpolated from surrounding entries.
 *
 * @param[in]  tbl      Table to search
 * @param[in]  freq     Desired frequency
 * @param[out] vals     Found or interpolated DC calibration values
 */
void dc_cal_tbl_vals(const struct dc_cal_tbl *tbl, unsigned int freq,
                     int16_t *dc_i, int16_t *dc_q);

/**
 * Load a DC calibration table from the provided data
 *
 * @param   buf   Packed table data
 * @param   len   Length of packed data, in bytes
 *
 * @return Loaded table
 */
struct dc_cal_tbl * dc_cal_tbl_load(uint8_t *buf, size_t buf_len);

/**
 * Free a DC calibration table
 *
 * @param   tbl     Pointer to table to free
 *
 * The table pointer will be set to NULL after freeing it.
 */
void dc_cal_tbl_free(struct dc_cal_tbl **tbl);

#endif
