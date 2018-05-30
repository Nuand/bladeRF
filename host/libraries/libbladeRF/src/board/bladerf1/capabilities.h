/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2015-2017 Nuand LLC
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

/* This file defines device capabilities added across libbladeRF, FX3, and FPGA
 * versions that we can check for */

#ifndef BLADERF1_CAPABILITIES_H_
#define BLADERF1_CAPABILITIES_H_

#include <stdint.h>

#include "board/board.h"

/**
 * Convenience wrapper for testing capabilities mask
 */
static inline bool have_cap(uint64_t capabilities, uint64_t capability)
{
    return (capabilities & capability) != 0;
}

/**
 * Determine device's firmware capabilities.
 *
 * @param[in]   fw_version  Firmware version
 *
 * @return  Capabilities bitmask
 */
uint64_t bladerf1_get_fw_capabilities(const struct bladerf_version *fw_version);

/**
 * Add capability bits based upon FPGA version stored in the device handle
 *
 * @param[in]   fpga_version    FPGA version
 *
 * @return  Capabilities bitmask
 */
uint64_t bladerf1_get_fpga_capabilities(
    const struct bladerf_version *fpga_version);

#endif
