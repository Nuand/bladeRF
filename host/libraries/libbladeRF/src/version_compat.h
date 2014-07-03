/**
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

#ifndef VERSION_COMPAT_H_
#define VERSION_COMPAT_H_

#include <libbladeRF.h>

/**
 * Check if device's firmware version is sufficient for the current libbladeRF
 * version. If it's not, the user will need to use the bootloader to flash
 * a newer version.
 *
 * @param   dev             Device handle
 *
 * @return  0 if the FW version is sufficient for normal operation,
 *          BLADERF_ERR_UPDATE_FW if a firmware update is required,
 *          other BLADERF_ERR_* values on unexpected errors
 */
int version_check_fw(const struct bladerf *dev);

/**
 * Get the minimum required firmware version
 *
 * @param[in]   dev         Device to check
 * @param[in]   by_fpga     If true, this function returns the minimum firmware
 *                          version required by the FPGA. This assumes that
 *                          the device structure's FPGA version info is
 *                          filled in. If false, this function retrieves
 *                          the minimum required firmware version required
 *                          by libbladeRF.
 * @param[out]  version     Written with firmware version. `describe` field
 *                          will be NULL.
 */
void version_required_fw(const struct bladerf *dev,
                         struct bladerf_version *version, bool by_fpga);


/**
 * Check if device's FPGA version is sufficient, considering the current
 * library version and device's firmware version. Also test if loaded FPGA
 * requires a more recent firmware version.
 *
 * @param   dev             Device handle
 *
 * @return  0 if the FPGA version is sufficient for normal operation,
 *          BLADERF_ERR_UPDATE_FPGA if the firmware requires a newer FPGA,
 *          BLADERF_ERR_UPDATE_FW if a firmware update is required to support
 *              the currently loaded FPGA,
 *          other BLADERF_ERR_* values on unexpected errors
 */
int version_check_fpga(const struct bladerf *dev);

/**
 * Get the minimum required FPGA version, based upon a device's firmware version
 *
 * @param[in]   dev         Device to check
 * @param[out]  version     Written with FPGA version. `describe` field
 *                          will be NULL.
 */
void version_required_fpga(struct bladerf *dev, struct bladerf_version *version);

/**
 * Test if two versions are equal. The "describe" field is not checked
 *
 * @return true if equal, false otherwise
 */
bool version_equal(const struct bladerf_version *v1,
                   const struct bladerf_version *v2);

/**
 * Check if version in the provided structure is greater or equal to
 * the version specified by the provided major, minor, and patch values
 *
 * @param       version     Version structure to check
 * @param       major       Minor version
 * @param       minor       Minor version
 * @param       patch       Patch version
 *
 * @return true for greater or equal, false otherwise
 */
bool version_greater_or_equal(const struct bladerf_version *version,
                              unsigned int major, unsigned int minor,
                              unsigned int patch);

/**
 * Check if version in the provided structure is less than
 * the version specied by the provided major, minor, and patch values
 *
 * @param       version     Version structure to check
 * @param       major       Minor version
 * @param       minor       Minor version
 * @param       patch       Patch version
 *
 * @return true for less than, false otherwise
 */
bool version_less_than(const struct bladerf_version *version,
                       unsigned int major, unsigned int minor,
                       unsigned int patch);
#endif
