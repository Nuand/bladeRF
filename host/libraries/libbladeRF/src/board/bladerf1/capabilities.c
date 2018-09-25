/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2015 Nuand LLC
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

#include <inttypes.h>

#include "log.h"
#include "helpers/version.h"

#include "capabilities.h"

uint64_t bladerf1_get_fw_capabilities(const struct bladerf_version *fw_version)
{
    uint64_t capabilities = 0;

    if (version_fields_greater_or_equal(fw_version, 1, 7, 1)) {
        capabilities |= BLADERF_CAP_FW_LOOPBACK;
    }

    if (version_fields_greater_or_equal(fw_version, 1, 8, 0)) {
        capabilities |= BLADERF_CAP_QUERY_DEVICE_READY;
    }

    if (version_fields_greater_or_equal(fw_version, 1, 9, 0)) {
        capabilities |= BLADERF_CAP_READ_FW_LOG_ENTRY;
    }

    if (version_fields_greater_or_equal(fw_version, 2, 3, 0)) {
        capabilities |= BLADERF_CAP_FW_FLASH_ID;
    }

    if (version_fields_greater_or_equal(fw_version, 2, 3, 1)) {
        capabilities |= BLADERF_CAP_FW_FPGA_SOURCE;
    }

    return capabilities;
}

uint64_t bladerf1_get_fpga_capabilities(const struct bladerf_version *fpga_version)
{
    uint64_t capabilities = 0;

    if (version_fields_greater_or_equal(fpga_version, 0, 0, 4)) {
        capabilities |= BLADERF_CAP_UPDATED_DAC_ADDR;
    }

    if (version_fields_greater_or_equal(fpga_version, 0, 0, 5)) {
        capabilities |= BLADERF_CAP_XB200;
    }

    if (version_fields_greater_or_equal(fpga_version, 0, 1, 0)) {
        capabilities |= BLADERF_CAP_TIMESTAMPS;
    }

    if (version_fields_greater_or_equal(fpga_version, 0, 2, 0)) {
        capabilities |= BLADERF_CAP_FPGA_TUNING;
        capabilities |= BLADERF_CAP_SCHEDULED_RETUNE;
    }

    if (version_fields_greater_or_equal(fpga_version, 0, 3, 0)) {
        capabilities |= BLADERF_CAP_PKT_HANDLER_FMT;
    }

    if (version_fields_greater_or_equal(fpga_version, 0, 3, 2)) {
        capabilities |= BLADERF_CAP_VCTCXO_TRIMDAC_READ;
    }

    if (version_fields_greater_or_equal(fpga_version, 0, 4, 0)) {
        capabilities |= BLADERF_CAP_ATOMIC_NINT_NFRAC_WRITE;
    }

    if (version_fields_greater_or_equal(fpga_version, 0, 4, 1)) {
        capabilities |= BLADERF_CAP_MASKED_XBIO_WRITE;
    }

    if (version_fields_greater_or_equal(fpga_version, 0, 5, 0)) {
        capabilities |= BLADERF_CAP_VCTCXO_TAMING_MODE;
    }

    if (version_fields_greater_or_equal(fpga_version, 0, 6, 0)) {
        capabilities |= BLADERF_CAP_TRX_SYNC_TRIG;
    }

    if (version_fields_greater_or_equal(fpga_version, 0, 7, 0)) {
        capabilities |= BLADERF_CAP_AGC_DC_LUT;
    }

    return capabilities;
}
