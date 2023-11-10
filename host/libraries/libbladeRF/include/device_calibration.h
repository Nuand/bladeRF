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

#ifndef BLADERF2_CALIBRATION_H_
#define BLADERF2_CALIBRATION_H_

#include <stdint.h>
#include "libbladeRF.h"

/**
 * @brief Converts gain calibration CSV data to a binary format.
 *
 * This function reads frequency and gain data from a CSV file and writes it
 * to a binary file.
 *
 * @param csv_path     Path to the input CSV file.
 * @param binary_path  Path to the output binary file.
 * @return 0 on success, BLADERF_ERR_* code on failure.
 */
int gain_cal_csv_to_bin(const char *csv_path, const char *binary_path);

/**
 * @brief Loads gain calibration data from a binary file into a bladeRF device.
 *
 * This function reads frequency and gain calibration data from a binary file and
 * loads it into the specified bladeRF device.
 *
 * @param dev         Pointer to the bladeRF device structure.
 * @param ch          Channel for which the gain calibration data is loaded.
 * @param binary_path Path to the binary file containing frequency-gain data.
 * @return 0 on success, BLADERF_ERR_* code on failure.
 */
int load_gain_calibration(struct bladerf *dev, bladerf_channel ch, const char *binary_path);

#endif
