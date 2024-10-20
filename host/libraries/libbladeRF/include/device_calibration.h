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
 * @param ch           bladeRF channel
 * @return 0 on success, BLADERF_ERR_* code on failure.
 */
int gain_cal_csv_to_bin(struct bladerf *dev, const char *csv_path, const char *binary_path, bladerf_channel ch);

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

/**
 * @brief Retrieve the gain correction gain delta between the current frequency and the target frequency.
 *
 * This function determines the gain correction difference between two frequencies,
 * using the calibration table associated with the specified channel.
 * The result is based on the gain correction values retrieved from the calibration table entries.
 *
 * @param dev                 Pointer to the bladeRF device structure.
 * @param freq                The target frequency for which the gain correction gain is to be determined.
 * @param ch                  The bladeRF channel to use when fetching the gain calibration table.
 * @param compensated_gain    The gain correction result
 *
 * @return 0 on success, BLADERF_ERR_* code on failure.
 */
int get_gain_correction(struct bladerf *dev,
                        bladerf_frequency freq,
                        bladerf_channel ch,
                        bladerf_gain *compensated_gain);


/**
 * Retrieves a gain calibration entry for a specified frequency by interpolating
 * between the nearest lower and higher frequency calibration entries in the given
 * calibration table.
 *
 * @param[in]  tbl    Pointer to the gain calibration table containing pre-calculated
 *                    gain correction values across various frequencies.
 * @param[in]  freq   The frequency for which the gain calibration entry is requested.
 * @param[out] result Pointer to a `struct bladerf_gain_cal_entry` that will be filled
 *                    with the interpolated gain calibration entry for the specified frequency.
 *                    This structure must be allocated by the caller.
 *
 * The function first identifies the calibration entries immediately below and above the
 * requested frequency. If the requested frequency matches a calibration point exactly,
 * that entry is directly copied into `result`. If the requested frequency falls between
 * two calibration points, a linear interpolation is performed to compute the gain
 * correction value for the requested frequency, and the result is stored in `result`.
 *
 * @note This function assumes that the calibration table is sorted by frequency.
 *
 * @return 0 on success, indicating that `result` has been populated with the interpolated
 *         gain correction value. If the function fails, a negative error code is returned:
 *         - BLADERF_ERR_INVAL if the `result` pointer is NULL,
 *         - BLADERF_ERR_UNEXPECTED if suitable floor and ceiling entries cannot be found
 *           in the calibration table.
 */
int get_gain_cal_entry(const struct bladerf_gain_cal_tbl *tbl,
                       bladerf_frequency freq,
                       struct bladerf_gain_cal_entry *result);

/**
 * Applies compensated gain given the current gain target and center frequency
 *
 * @param dev       The bladeRF device structure pointer.
 * @param ch        The bladeRF channel to use.
 * @param frequency The target frequency.
 */
int apply_gain_correction(struct bladerf *dev, bladerf_channel ch, bladerf_frequency frequency);

/**
 * @brief Frees the resources of a gain calibration table and resets its fields.
 *
 * This function frees the dynamically allocated memory for the 'entries' and 'file_path'
 * fields of the bladerf_gain_cal_tbl structure. After freeing these resources, it resets all fields
 * of the structure to their default values, ensuring the structure is clean and can be
 * reused or safely deallocated. The function logs a verbose message upon starting the
 * freeing process.
 *
 * @param tbl Pointer to the bladerf_gain_cal_tbl structure. If this pointer is NULL,
 *            the function returns immediately without performing any operations.
 *            It is assumed that the caller ensures the validity of this pointer.
 */
void gain_cal_tbl_free(struct bladerf_gain_cal_tbl *tbl);

#endif
