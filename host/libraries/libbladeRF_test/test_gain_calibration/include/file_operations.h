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

#endif
