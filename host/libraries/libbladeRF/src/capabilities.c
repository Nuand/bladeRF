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
#include "bladerf_priv.h"
#include "version_compat.h"
#include "capabilities.h"
#include "log.h"

void capabilities_init_pre_fpga_load(struct bladerf *dev)
{
    dev->capabilities = 0;

    if (version_greater_or_equal(&dev->fw_version, 1, 7, 1)) {
        dev->capabilities |= BLADERF_CAP_FW_LOOPBACK;
    }

    if (version_greater_or_equal(&dev->fw_version, 1, 8, 0)) {
        dev->capabilities |= BLADERF_CAP_QUERY_DEVICE_READY;
    }

    if (version_greater_or_equal(&dev->fw_version, 1, 9, 0)) {
        dev->capabilities |= BLADERF_CAP_READ_FW_LOG_ENTRY;
    }

    log_verbose("Capability mask before FPGA load: 0x%016"PRIx64"\n",
                 dev->capabilities);
}

void capabilities_init_post_fpga_load(struct bladerf *dev)
{
    if (version_greater_or_equal(&dev->fpga_version, 0, 0, 4)) {
        dev->capabilities |= BLADERF_CAP_UPDATED_DAC_ADDR;
    }

    if (version_greater_or_equal(&dev->fpga_version, 0, 0, 5)) {
        dev->capabilities |= BLADERF_CAP_XB200;
    }

    if (version_greater_or_equal(&dev->fpga_version, 0, 1, 0)) {
        dev->capabilities |= BLADERF_CAP_TIMESTAMPS;
    }

    if (version_greater_or_equal(&dev->fpga_version, 0, 2, 0)) {
        dev->capabilities |= BLADERF_CAP_FPGA_TUNING;
        dev->capabilities |= BLADERF_CAP_SCHEDULED_RETUNE;
    }

    if (version_greater_or_equal(&dev->fpga_version, 0, 3, 0)) {
        dev->capabilities |= BLADERF_CAP_PKT_HANDLER_FMT;
    }

    if (version_greater_or_equal(&dev->fpga_version, 0, 3, 2)) {
        dev->capabilities |= BLADERF_CAP_VCTCXO_TRIMDAC_READ;
    }

    if (version_greater_or_equal(&dev->fpga_version, 0, 4, 0)) {
        dev->capabilities |= BLADERF_CAP_ATOMIC_NINT_NFRAC_WRITE;
    }

    if (version_greater_or_equal(&dev->fpga_version, 0, 4, 1)) {
        dev->capabilities |= BLADERF_CAP_MASKED_XBIO_WRITE;
    }

    if (version_greater_or_equal(&dev->fpga_version, 0, 5, 0)) {
        dev->capabilities |= BLADERF_CAP_VCTCXO_TAMING_MODE;
    }

    if (version_greater_or_equal(&dev->fpga_version, 0, 6, 0)) {
        dev->capabilities |= BLADERF_CAP_TRX_SYNC_TRIG;
    }

    if (version_greater_or_equal(&dev->fpga_version, 0, 7, 0)) {
        dev->capabilities |= BLADERF_CAP_AGC_DC_LUT;
    }
    if (version_greater_or_equal(&dev->fpga_version, 0, 8, 0)) {
        dev->capabilities |= BLADERF_CAP_FABRIC_REGISTER_ACCESS;
    }

    log_verbose("Capability mask after FPGA load: 0x%016"PRIx64"\n",
                 dev->capabilities);
}
