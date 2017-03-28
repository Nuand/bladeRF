#include "host_config.h"

#include "helpers/version.h"

/* Firmware-FPGA compatibility tables
 *
 * This list should be kept in decending order, such that the most recent
 * versions are first, and the last entry should contain the earliest version
 * that libbladeRF supports.
 */

#define VERSION(major, minor, patch) { major, minor, patch, NULL }

static const struct compat fw_compat[] = {
    /*   Firmware       requires  >=        FPGA */
    { VERSION(2, 0, 0),                 VERSION(0, 6, 0) },
};

const struct version_compat_table bladerf2_fw_compat_table = {fw_compat, ARRAY_SIZE(fw_compat)};

static const struct compat fpga_compat[] = {
    /*    FPGA          requires >=        Firmware */
    { VERSION(0, 6, 0),                 VERSION(2, 0, 0) },
};

const struct version_compat_table bladerf2_fpga_compat_table = {fpga_compat, ARRAY_SIZE(fpga_compat)};
