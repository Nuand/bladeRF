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

#include "log.h"
#include "rel_assert.h"

#include "helpers/version.h"

bool version_equal(const struct bladerf_version *v1,
                   const struct bladerf_version *v2)
{
    return v1->major == v2->major &&
           v1->minor == v2->minor &&
           v1->patch == v2->patch;
}

bool version_greater_or_equal(const struct bladerf_version *v1,
                              const struct bladerf_version *v2)
{
    return version_fields_greater_or_equal(v1, v2->major, v2->minor, v2->patch);
}

bool version_less_than(const struct bladerf_version *v1,
                       const struct bladerf_version *v2)
{
    return version_fields_less_than(v1, v2->major, v2->minor, v2->patch);
}

bool version_fields_greater_or_equal(const struct bladerf_version *version,
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

bool version_fields_less_than(const struct bladerf_version *version,
                              unsigned int major, unsigned int minor,
                              unsigned int patch)
{
    return !version_fields_greater_or_equal(version, major, minor, patch);
}

static const struct compat *find_fw_compat(const struct version_compat_table *fw_compat_table,
                                           const struct bladerf_version *fw_version)
{
    const struct compat *newest_compat = &fw_compat_table->table[0];
    size_t i;

    /* Version is newer than what's in our table - complain */
    if (version_less_than(&newest_compat->ver, fw_version)) {
        log_info("Firmware version (v%u.%u.%u) is newer than entries in "
                 "libbladeRF's compatibility table. Please update libbladeRF "
                 "if problems arise.\n",
                 fw_version->major, fw_version->minor, fw_version->patch);
        return newest_compat;
    }

    for (i = 0; i < fw_compat_table->len; i++) {
        if (version_equal(fw_version, &fw_compat_table->table[i].ver)) {
            return &fw_compat_table->table[i];
        }
    }

    return NULL;
}

static const struct compat *find_fpga_compat(const struct version_compat_table *fpga_compat_table,
                                             const struct bladerf_version *fpga_version)
{
    const struct compat *newest_compat = &fpga_compat_table->table[0];
    size_t i;

    /* Device's FPGA is newer than what's in our table - complain */
    if (version_less_than(&newest_compat->ver, fpga_version)) {
        log_info("FPGA version (v%u.%u.%u) is newer than entries in "
                 "libbladeRF's compatibility table. Please update libbladeRF "
                 "if problems arise.\n",
                 fpga_version->major, fpga_version->minor, fpga_version->patch);
        return newest_compat;
    }

    for (i = 0; i < fpga_compat_table->len; i++) {
        if (version_equal(fpga_version, &fpga_compat_table->table[i].ver)) {
            return &fpga_compat_table->table[i];
        }
    }

    return NULL;
}

int version_check_fw(const struct version_compat_table *fw_compat_table,
                     const struct bladerf_version *fw_version,
                     struct bladerf_version *required_fw_version)
{
    const struct bladerf_version *oldest_version = &fw_compat_table->table[fw_compat_table->len-1].ver;

    if (required_fw_version) {
        *required_fw_version = *oldest_version;
    }

    if (version_greater_or_equal(fw_version, oldest_version)) {
        return 0;
    }

    return BLADERF_ERR_UPDATE_FW;
}

int version_check(const struct version_compat_table *fw_compat_table,
                  const struct version_compat_table *fpga_compat_table,
                  const struct bladerf_version *fw_version,
                  const struct bladerf_version *fpga_version,
                  struct bladerf_version *required_fw_version,
                  struct bladerf_version *required_fpga_version)
{
    const struct compat *fw_compat = find_fw_compat(fw_compat_table, fw_version);
    const struct compat *fpga_compat = find_fpga_compat(fpga_compat_table, fpga_version);

    if (fw_compat == NULL) {
        log_debug("%s is missing FW version compat table entry?\n",
                  __FUNCTION__);
        assert(!"BUG!");
    } else if (fpga_compat == NULL) {
        log_debug("%s is missing FPGA version compat table entry?\n",
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

