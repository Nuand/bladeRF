#include <libbladeRF.h>

#include "log.h"
#include "rel_assert.h"
#include "host_config.h"

#include "helpers/version.h"

/* Firmware-FPGA compatibility tables
 *
 * This list should be kept in decending order, such that the most recent
 * versions are first, and the last entry should contain the earliest version
 * that libbladeRF supports.
 */
struct compat {
    struct bladerf_version ver;
    struct bladerf_version requires;
};

#define VERSION(major, minor, patch) { major, minor, patch, NULL }
#define OLDEST_SUPPORTED(tbl) (&tbl[ARRAY_SIZE(tbl) - 1].ver)

static const struct compat fw_compat_tbl[] = {
    /*   Firmware       requires  >=        FPGA */
    { VERSION(2, 0, 0),                 VERSION(0, 0, 2) },
    { VERSION(1, 9, 1),                 VERSION(0, 0, 2) },
    { VERSION(1, 9, 0),                 VERSION(0, 0, 2) },
    { VERSION(1, 8, 1),                 VERSION(0, 0, 2) },
    { VERSION(1, 8, 0),                 VERSION(0, 0, 2) },
    { VERSION(1, 7, 1),                 VERSION(0, 0, 2) },
    { VERSION(1, 7, 0),                 VERSION(0, 0, 2) },
    { VERSION(1, 6, 1),                 VERSION(0, 0, 2) },
    { VERSION(1, 6, 0),                 VERSION(0, 0, 1) },
};

static const struct compat fpga_compat_tbl[] = {
    /*    FPGA          requires >=        Firmware */
    { VERSION(0, 7, 1),                 VERSION(1, 6, 1) },
    { VERSION(0, 7, 0),                 VERSION(1, 6, 1) },
    { VERSION(0, 6, 0),                 VERSION(1, 6, 1) },
    { VERSION(0, 5, 0),                 VERSION(1, 6, 1) },
    { VERSION(0, 4, 1),                 VERSION(1, 6, 1) },
    { VERSION(0, 4, 0),                 VERSION(1, 6, 1) },
    { VERSION(0, 3, 5),                 VERSION(1, 6, 1) },
    { VERSION(0, 3, 4),                 VERSION(1, 6, 1) },
    { VERSION(0, 3, 3),                 VERSION(1, 6, 1) },
    { VERSION(0, 3, 2),                 VERSION(1, 6, 1) },
    { VERSION(0, 3, 1),                 VERSION(1, 6, 1) },
    { VERSION(0, 3, 0),                 VERSION(1, 6, 1) },
    { VERSION(0, 2, 0),                 VERSION(1, 6, 1) },
    { VERSION(0, 1, 2),                 VERSION(1, 6, 1) },
    { VERSION(0, 1, 1),                 VERSION(1, 6, 1) },
    { VERSION(0, 1, 0),                 VERSION(1, 6, 1) },
    { VERSION(0, 0, 6),                 VERSION(1, 6, 1) },
    { VERSION(0, 0, 5),                 VERSION(1, 6, 1) },
    { VERSION(0, 0, 4),                 VERSION(1, 6, 1) },
    { VERSION(0, 0, 3),                 VERSION(1, 6, 1) },
    { VERSION(0, 0, 2),                 VERSION(1, 6, 1) },
    { VERSION(0, 0, 1),                 VERSION(1, 6, 0) },
};

static const struct compat * find_fw_compat(const struct bladerf_version *fw_version)
{
    const struct compat *newest_compat = &fw_compat_tbl[0];
    size_t i;

    /* Version is newer than what's in our table - complain */
    if (version_less_than(&newest_compat->ver, fw_version)) {
        log_info("Firmware version (v%u.%u.%u) is newer than entries in "
                 "libbladeRF's compatibility table. Please update libbladeRF "
                 "if problems arise.\n",
                 fw_version->major, fw_version->minor, fw_version->patch);
        return newest_compat;
    }

    for (i = 0; i < ARRAY_SIZE(fw_compat_tbl); i++) {
        if (version_equal(fw_version, &fw_compat_tbl[i].ver)) {
            return &fw_compat_tbl[i];
        }
    }

    return NULL;
}

static const struct compat * find_fpga_compat(const struct bladerf_version *fpga_version)
{
    const struct compat *newest_compat = &fpga_compat_tbl[0];
    size_t i;

    /* Device's FPGA is newer than what's in our table - complain */
    if (version_less_than(&newest_compat->ver, fpga_version)) {
        log_info("FPGA version (v%u.%u.%u) is newer than entries in "
                 "libbladeRF's compatibility table. Please update libbladeRF "
                 "if problems arise.\n",
                 fpga_version->major, fpga_version->minor, fpga_version->patch);
        return newest_compat;
    }

    for (i = 0; i < ARRAY_SIZE(fpga_compat_tbl); i++) {
        if (version_equal(fpga_version, &fpga_compat_tbl[i].ver)) {
            return &fpga_compat_tbl[i];
        }
    }

    return NULL;
}

int bladerf1_version_check_fw(const struct bladerf_version *fw_version,
                              struct bladerf_version *required_fw_version)
{
    static const struct bladerf_version *oldest_version = OLDEST_SUPPORTED(fw_compat_tbl);

    if (required_fw_version) {
        *required_fw_version = *oldest_version;
    }

    if (version_greater_or_equal(fw_version, oldest_version)) {
        return 0;
    }

    return BLADERF_ERR_UPDATE_FW;
}

int bladerf1_version_check(const struct bladerf_version *fw_version,
                           const struct bladerf_version *fpga_version,
                           struct bladerf_version *required_fw_version,
                           struct bladerf_version *required_fpga_version)
{
    const struct compat *fw_compat = find_fw_compat(fw_version);
    const struct compat *fpga_compat = find_fpga_compat(fpga_version);

    if (fw_compat == NULL) {
        log_debug("%s is missing FW version compat table entry?",
                  __FUNCTION__);
        assert(!"BUG!");
    } else if (fpga_compat == NULL) {
        log_debug("%s is missing FPGA version compat table entry?",
                  __FUNCTION__);
        assert(!"BUG!");
    }

    if (required_fw_version) {
        *required_fw_version = fpga_compat->requires;
    }
    if (required_fpga_version) {
        *required_fpga_version = fw_compat->requires;
    }

    /* Check if the FPGA meets the minimum requirements dictated by the
     * firmware version */
    if (version_less_than(fpga_version, &fw_compat->requires)) {
        return BLADERF_ERR_UPDATE_FPGA;
    }

    /* Check if the firmware version meets the minimum requirements dictated
     * by the FPGA version */
    if (version_less_than(fw_version, &fpga_compat->requires)) {
        return BLADERF_ERR_UPDATE_FW;
    }

    return 0;
}

