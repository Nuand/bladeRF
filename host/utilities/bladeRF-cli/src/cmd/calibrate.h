/*
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2014 Nuand LLC
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
#ifndef CALIBRATE_H_
#define CALIBRATE_H_

#include <stdint.h>
#include <libbladeRF.h>
#include "common.h"

/* Calibration opereration bitmasks */
#define CAL_DC_RX_      (1 << 0)
#define CAL_DC_TX_      (1 << 1)
#define CAL_DC_AUTO_    (1 << 2)
#define CAL_DC          (CAL_DC_RX_ | CAL_DC_TX_ | CAL_DC_AUTO_)

#define CAL_DC_LMS_TUNING   ((1 << 16) | CAL_DC_RX_)
#define CAL_DC_LMS_RXLPF    ((1 << 17) | CAL_DC_RX_)
#define CAL_DC_LMS_RXVGA2   ((1 << 18) | CAL_DC_RX_)
#define CAL_DC_LMS_TXLPF    ((1 << 19) | CAL_DC_TX_)

#define CAL_DC_LMS_TX_ALL (CAL_DC_LMS_TUNING | CAL_DC_LMS_TXLPF)
#define CAL_DC_LMS_RX_ALL (CAL_DC_LMS_TUNING | CAL_DC_LMS_RXLPF | \
                           CAL_DC_LMS_RXVGA2)
#define CAL_DC_LMS_ALL    (CAL_DC_LMS_TX_ALL | CAL_DC_LMS_RX_ALL)

#define CAL_DC_AUTO_RX  (CAL_DC_AUTO_ | CAL_DC_RX_)
#define CAL_DC_AUTO_TX  (CAL_DC_AUTO_ | CAL_DC_TX_)
#define CAL_DC_AUTO     (CAL_DC_AUTO_RX | CAL_DC_AUTO_TX)

#define IS_CAL(cal, ops) ((ops & cal) == cal)
#define IS_DC_CAL(ops) ((ops & CAL_DC) != 0)
#define IS_RX_CAL(ops) ((ops & CAL_DC_RX_) != 0)
#define IS_TX_CAL(ops) ((ops & CAL_DC_TX_) != 0)

/**
 * Perform DC offset calibrations
 *
 * @param   state   CLI state handle
 * @param   ops     Bitwise OR of CAL_DC_* values, describing desired operation
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int calibrate_dc(struct cli_state *state, unsigned int ops);

/**
 * Generate a DC offset calibration table
 *
 * @param   state       CLI state handle
 * @param   module      Module to calibrate
 * @param   filename    Output filename for table file
 * @param   f_low       Lowest frequency in the table to start at
 * @param   f_inc       Frequency to increment by at each calibration step
 * @param   f_high      Max frequency in the calibration table
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int calibrate_dc_gen_tbl(struct cli_state *state, bladerf_module module,
                         const char *filename, unsigned int f_low,
                         unsigned f_inc, unsigned int f_high);


#endif

