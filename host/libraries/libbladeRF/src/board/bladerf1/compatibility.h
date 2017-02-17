#ifndef __COMPATIBILITY_H__
#define __COMPATIBILITY_H__

#include <libbladeRF.h>

/**
 * Check if the firmware version is sufficient for the current libbladeRF
 * version. If it's not, the user will need to use the bootloader to flash a
 * newer version.
 *
 * @param       fw_version              Firmware version
 * @param[out]  required_fw_version     If not-NULL, populated with minimum
 *                                      required firmware version
 *
 * @return  0 if the FW version is sufficient for normal operation,
 *          BLADERF_ERR_UPDATE_FW if a firmware update is required,
 *          other BLADERF_ERR_* values on unexpected errors
 */
int bladerf1_version_check_fw(const struct bladerf_version *fw_version,
                              struct bladerf_version *required_fw_version);

/**
 * Check if the firmware and FPGA versions are sufficient and compatible.
 *
 * @param       fw_version              Firmware version
 * @param       fpga_version            Firmware version
 * @param[out]  required_fw_version     If not-NULL, populated with minimum
 *                                      required firmware version
 * @param[out]  required_fpga_version   If not-NULL, populated with minimum
 *                                      required FPGA version
 *
 * @return  0 if the FPGA version is sufficient for normal operation,
 *          BLADERF_ERR_UPDATE_FPGA if the firmware requires a newer FPGA,
 *          BLADERF_ERR_UPDATE_FW if a firmware update is required to support
 *              the currently loaded FPGA,
 *          other BLADERF_ERR_* values on unexpected errors
 */

int bladerf1_version_check(const struct bladerf_version *fw_version,
                           const struct bladerf_version *fpga_version,
                           struct bladerf_version *required_fw_version,
                           struct bladerf_version *required_fpga_version);

#endif
