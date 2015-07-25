/**
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014-2015 Nuand LLC
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

#include "version_compat.h"
#include "bladerf_priv.h"
#include "rel_assert.h"
#include "log.h"

#define VERSION(major, minor, patch) { major, minor, patch, NULL }
#define OLDEST_SUPPORTED(tbl) (&tbl[ARRAY_SIZE(tbl) - 1].ver)

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

static const struct compat fw_compat_tbl[] = {
    /*   Firmware       requires  >=        FPGA */
    { VERSION(1, 8, 0),                 VERSION(0, 0, 2) },
    { VERSION(1, 7, 1),                 VERSION(0, 0, 2) },
    { VERSION(1, 7, 0),                 VERSION(0, 0, 2) },
    { VERSION(1, 6, 1),                 VERSION(0, 0, 2) },
    { VERSION(1, 6, 0),                 VERSION(0, 0, 1) },
};

static const struct compat fpga_compat_tbl[] = {
    /*    FPGA          requires >=        Firmware */
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

static const struct compat * find_fw_match(const struct bladerf *dev)
{
    size_t i;
    const struct compat *newest_fw = &fw_compat_tbl[0];

    /* Version is newer than what's in our table - complain */
    if (version_less_than(&newest_fw->ver,
                          dev->fw_version.major,
                          dev->fw_version.minor,
                          dev->fw_version.patch)) {

        log_info("Firmware version (v%u.%u.%u) is newer than entries in "
                 "libbladeRF's compatibility table. Please update libbladeRF "
                 "if problems arise.\n",
                 dev->fw_version.major,
                 dev->fw_version.minor,
                 dev->fw_version.patch);

        return newest_fw;
    }

    for (i = 0; i < ARRAY_SIZE(fw_compat_tbl); i++) {
        if (version_equal(&dev->fw_version, &fw_compat_tbl[i].ver)) {
            return &fw_compat_tbl[i];
        }
    }

    return NULL;
}

static const struct compat * find_fpga_match(const struct bladerf *dev)
{
    size_t i;
    const struct compat *newest_fpga = &fpga_compat_tbl[0];

    /* Device's FPGA is newer than what's in our table - complain */
    if (version_less_than(&newest_fpga->ver,
                          dev->fpga_version.major,
                          dev->fpga_version.minor,
                          dev->fpga_version.patch)) {


        log_info("FPGA version (v%u.%u.%u) is newer than entries in "
                 "libbladeRF's compatibility table. Please update libbladeRF "
                 "if problems arise.\n",
                 dev->fpga_version.major,
                 dev->fpga_version.minor,
                 dev->fpga_version.patch);

        return newest_fpga;
    }

    for (i = 0; i < ARRAY_SIZE(fpga_compat_tbl); i++) {
        if (version_equal(&dev->fpga_version, &fpga_compat_tbl[i].ver)) {
            return &fpga_compat_tbl[i];
        }
    }

    return NULL;
}

static inline void required_values(const struct compat *entry,
                                   unsigned int *major,
                                   unsigned int *minor,
                                   unsigned int *patch)
{
    *major = entry->requires.major;
    *minor = entry->requires.minor;
    *patch = entry->requires.patch;
}

int version_check_fw(const struct bladerf *dev)
{
    static const struct bladerf_version *ver = OLDEST_SUPPORTED(fw_compat_tbl);

    if (version_greater_or_equal(&dev->fw_version,
                                 ver->major, ver->minor, ver->patch)) {
        return 0;
    } else {
        return BLADERF_ERR_UPDATE_FW;
    }
}

void version_required_fw(const struct bladerf *dev,
                         struct bladerf_version *version, bool by_fpga)
{
    if (by_fpga) {
        const struct compat *fpga = find_fpga_match(dev);

        if (fpga == NULL) {
            log_debug("%s is missing FPGA version compat table entry?",
                      __FUNCTION__);
            memset(version, 0, sizeof(version[0]));
            assert(!"BUG!");
        } else {
            memcpy(version, &fpga->requires, sizeof(version[0]));
        }

    } else {
        const struct bladerf_version *required_version =
                                            OLDEST_SUPPORTED(fw_compat_tbl);
        memcpy(version, required_version, sizeof(version[0]));
    }
}


int version_check_fpga(const struct bladerf *dev)
{
    const struct compat *fw = find_fw_match(dev);
    const struct compat *fpga = find_fpga_match(dev);
    unsigned int major, minor, patch;

    /* We didn't find a compatible firmware or FPGA versions in our list. */
    if (fw == NULL) {
        return BLADERF_ERR_UPDATE_FW;
    } else if (fpga == NULL) {
        return BLADERF_ERR_UPDATE_FPGA;
    }

    /* Check if the FPGA meets the minimum requirements dictated by the
     * firmware version */
    required_values(fw, &major, &minor, &patch);
    if (version_less_than(&dev->fpga_version, major, minor, patch)) {
        return BLADERF_ERR_UPDATE_FPGA;
    }

    /* Check if the firmware version meets the minimum requirements dictated
     * by the FPGA version */
    required_values(fpga, &major, &minor, &patch);
    if (version_less_than(&dev->fw_version, major, minor, patch)) {
        return BLADERF_ERR_UPDATE_FW;
    }

    return 0;
}

void version_required_fpga(struct bladerf *dev, struct bladerf_version *version)
{
    const struct compat *entry = find_fw_match(dev);

    if (entry == NULL) {
        /* If this happens, someone didn't test the code before committing,
         * and jynik will be forced to bring out the git blame bat. */
        log_debug("%s called before FW version check?", __FUNCTION__);
        memset(version, 0, sizeof(version[0]));
        assert(!"BUG!");
    } else {
        memcpy(version, &entry->requires, sizeof(version[0]));
    }
}

bool version_equal(const struct bladerf_version *v1,
                   const struct bladerf_version *v2)
{
    return v1->major == v2->major &&
           v1->minor == v2->minor &&
           v1->patch == v2->patch;
}

bool version_greater_or_equal(const struct bladerf_version *version,
                              unsigned int major, unsigned int minor,
                              unsigned int patch)
{
    if (version->major > major) {
        return true;
    } else if ( (version->major == major) && (version->minor > minor) ) {
        return true;
    } else if ((version->major == major) &&
               (version->minor == minor) &&
               (version->patch >= patch) ) {
        return true;
    } else {
        return false;
    }
}

bool version_less_than(const struct bladerf_version *version,
                       unsigned int major, unsigned int minor,
                       unsigned int patch)
{
    return !version_greater_or_equal(version, major, minor, patch);
}

