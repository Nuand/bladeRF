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

#ifndef HELPERS_VERSION_H_
#define HELPERS_VERSION_H_

#include <libbladeRF.h>

#define BLADERF_VERSION_STR_MAX 32

/**
 * Test if two versions are equal. The "describe" field is not checked.
 *
 * @return true if equal, false otherwise
 */
bool version_equal(const struct bladerf_version *v1,
                   const struct bladerf_version *v2);

/**
 * Check if version v1 is greater than or equal to version v2.
 *
 * @param[in]   v1  First version
 * @param[in]   v2  Second version
 *
 * @return true for greater or equal than, false otherwise
 */
bool version_greater_or_equal(const struct bladerf_version *v1,
                              const struct bladerf_version *v2);

/**
 * Check if version v1 is less than version v2.
 *
 * @param[in]   v1  First version
 * @param[in]   v2  Second version
 *
 * @return true for less than, false otherwise
 */
bool version_less_than(const struct bladerf_version *v1,
                       const struct bladerf_version *v2);

/**
 * Check if version in the provided structure is greater or equal to
 * the version specified by the provided major, minor, and patch values
 *
 * @param[in]   version     Version structure to check
 * @param[in]   major       Minor version
 * @param[in]   minor       Minor version
 * @param[in]   patch       Patch version
 *
 * @return true for greater or equal than, false otherwise
 */
bool version_fields_greater_or_equal(const struct bladerf_version *version,
                                     unsigned int major,
                                     unsigned int minor,
                                     unsigned int patch);

/**
 * Check if version in the provided structure is less than
 * the version specied by the provided major, minor, and patch values
 *
 * @param[in]   version     Version structure to check
 * @param[in]   major       Minor version
 * @param[in]   minor       Minor version
 * @param[in]   patch       Patch version
 *
 * @return true for less than, false otherwise
 */
bool version_fields_less_than(const struct bladerf_version *version,
                              unsigned int major,
                              unsigned int minor,
                              unsigned int patch);


/* Version compatibility table structure. */
struct version_compat_table {
    const struct compat {
        struct bladerf_version ver;
        struct bladerf_version requires;
    } * table;
    unsigned int len;
};

/**
 * Check if the firmware version is sufficient for the current libbladeRF
 * version. If it's not, the user will need to use the bootloader to flash a
 * newer version.
 *
 * @param[in]   fw_compat_table         Firmware-FPGA version compat. table
 * @param[in]   fw_version              Firmware version
 * @param[out]  required_fw_version     If not-NULL, populated with minimum
 *                                      required firmware version
 *
 * @return  0 if the FW version is sufficient for normal operation,
 *          BLADERF_ERR_UPDATE_FW if a firmware update is required.
 */
int version_check_fw(const struct version_compat_table *fw_compat_table,
                     const struct bladerf_version *fw_version,
                     struct bladerf_version *required_fw_version);

/**
 * Check if the firmware and FPGA versions are sufficient and compatible.
 *
 * @param[in]   fw_compat_table         Firmware-FPGA version compat. table
 * @param[in]   fpga_compat_table       FPGA-Firmware version compat. table
 * @param[in]   fw_version              Firmware version
 * @param[in]   fpga_version            Firmware version
 * @param[out]  required_fw_version     If not-NULL, populated with minimum
 *                                      required firmware version
 * @param[out]  required_fpga_version   If not-NULL, populated with minimum
 *                                      required FPGA version
 *
 * @return  0 if the FPGA version is sufficient for normal operation,
 *          BLADERF_ERR_UPDATE_FPGA if the firmware requires a newer FPGA,
 *          BLADERF_ERR_UPDATE_FW if a firmware update is required to support
 *          the currently loaded FPGA.
 */
int version_check(const struct version_compat_table *fw_compat_table,
                  const struct version_compat_table *fpga_compat_table,
                  const struct bladerf_version *fw_version,
                  const struct bladerf_version *fpga_version,
                  struct bladerf_version *required_fw_version,
                  struct bladerf_version *required_fpga_version);

#endif
