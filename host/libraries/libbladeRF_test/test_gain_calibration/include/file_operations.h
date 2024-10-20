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
#ifndef FILE_OPERATIONS_H_
#define FILE_OPERATIONS_H_

#include "libbladeRF.h"

/**
 * @brief Read frequency-power data from a binary file and print it.
 *
 * Primarily useful for debugging. This function reads pairs of frequency (uint64_t)
 * and power (float) values from a binary file and prints them to the standard output.
 *
 * @param[in] binary_path Path to the binary file containing the frequency-power data.
 * @return Returns 0 on success, and 1 if there's an error opening the file.
 */
int read_and_print_binary(const char *binary_path);

/**
 * @brief Renames a calibration file based on the serial number of a BladeRF device and loads gain calibration.
 *
 * This function first retrieves the serial number of the specified BladeRF device. It then constructs a new filename
 * for the calibration file by appending the serial number to the original filename. After renaming the file, it
 * proceeds to load the gain calibration for the specified channel. If any step fails, the function returns an error.
 *
 * @param dev Pointer to a bladerf structure representing an open BladeRF device.
 * @param ch  Specifies the BladeRF channel for which the gain calibration is to be loaded.
 *
 * @return Returns 0 on successful execution and renaming of the file and loading of gain calibration.
 *         If an error occurs at any step (memory allocation failure, failure to get the serial number,
 *         failure in renaming the file, or failure in loading the gain calibration), the function returns -1.
 *
 * @note The function assumes the presence of "./output/" directory and "rx_sweep.bin" file in it.
 *       The memory allocated for the serial number is freed before the function returns.
 */
int test_serial_based_autoload(struct bladerf *dev, bladerf_channel ch);

#endif
