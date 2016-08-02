/**
 * @file
 * @brief   CRC-32 implementation based upon examples provided in Chapter 14 of
 *          Hacker's Delight (2nd Edition) by Henry S. Warren, Jr.
 *
 * http://www.hackersdelight.org
 *
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2016 Nuand LLC
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

#ifndef CRC32_CALC_H_
#define CRC32_CALC_H_

#include <stdint.h>

/**
 * Compute return a CRC32 checksum computed over the `len` bytes of `data`.
 */
uint32_t crc32(void *data, size_t len);

#endif
