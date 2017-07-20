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

/* This file defines device capabilities added across libbladeRF, FX3, and FPGA
 * versions that we can check for */

#ifndef BLADERF_CAPABILITIES_H_
#define BLADERF_CAPABILITIES_H_

#include <stdint.h>
#include "bladerf_priv.h"

/* Device capabilities are stored in a 64-bit mask.
 *
 * FPGA-oriented capabilities start at bit 0.
 * FX3-oriented capabilities start at bit 24.
 * Other/mixed capabilities start at bit 48.
 */

/**
 * Prior to FPGA 0.0.4, the DAC register were located at a different address
 */
#define BLADERF_CAP_UPDATED_DAC_ADDR    (1 << 0)

/**
 * FPGA version 0.0.5 introduced XB-200 support
 */
#define BLADERF_CAP_XB200               (1 << 1)

/**
 * FPGA version 0.1.0 introduced timestamp support
 */
#define BLADERF_CAP_TIMESTAMPS          (1 << 2)

/**
 * FPGA version 0.2.0 introduced NIOS-based tuning support.
 */
#define BLADERF_CAP_FPGA_TUNING         (1 << 3)

/**
 * FPGA version 0.2.0 also introduced scheduled retune support. The
 * re-use of this capability bit is intentional.
 */
#define BLADERF_CAP_SCHEDULED_RETUNE    (1 << 3)

/**
 * FPGA version 0.3.0 introduced new packet handler formats that pack
 * operations into a single requests.
 */
#define BLADERF_CAP_PKT_HANDLER_FMT     (1 << 4)

/**
 * A bug fix in FPGA version 0.3.2 allowed support for reading back
 * the current VCTCXO trim dac value.
 */
#define BLADERF_CAP_VCTCXO_TRIMDAC_READ (1 << 5)

/**
 * FPGA v0.4.0 introduced the ability to write LMS6002D TX/RX PLL
 * NINT & NFRAC regiters atomically.
 */
#define BLADERF_CAP_ATOMIC_NINT_NFRAC_WRITE (1 << 6)

/**
 * FPGA v0.4.1 fixed an issue with masked writes to the expansion GPIO
 * and expansion GPIO direction registers.
 *
 * To help users avoid getting bitten by this bug, we'll mark this
 * as a capability and disallow masked writes unless an FPGA with the
 * fix is being used.
 */
#define BLADERF_CAP_MASKED_XBIO_WRITE   (1 << 7)

/**
 * FPGA v0.5.0 introduces the ability to tame the VCTCXO via a 1 pps or 10 MHz
 * input source on the mini expansion header.
 */
#define BLADERF_CAP_VCTCXO_TAMING_MODE  (1 << 8)

/**
 * FPGA v0.6.0 introduced trx synchronization trigger via J71-4
 */
#define BLADERF_CAP_TRX_SYNC_TRIG (1 << 9)

/**
 * FPGA v0.7.0 introduced AGC DC correction Look-Up-Table
 */
#define BLADERF_CAP_AGC_DC_LUT          (1 << 10)

/**
 * FPGA v0.8.0 introduces the ability to access registers in the FPGA fabric
 * from libbladeRF
 */
#define BLADERF_CAP_FABRIC_REGISTER_ACCESS (1 << 10)

/**
 * Firmware 1.7.1 introduced firmware-based loopback
 */
#define BLADERF_CAP_FW_LOOPBACK         (((uint64_t) 1) << 32)

/**
 * FX3 firmware version 1.8.0 introduced the ability to query when the
 * device has become ready for use by the host.  This was done to avoid
 * opening and attempting to use the device while flash-based FPGA autoloading
 * was occurring.
 */
#define BLADERF_CAP_QUERY_DEVICE_READY  (((uint64_t) 1) << 33)

/**
 * FX3 firmware v1.9.0 introduced a vendor request by which firmware log
 * events could be retrieved.
 */
#define BLADERF_CAP_READ_FW_LOG_ENTRY   (((uint64_t) 1) << 34)

/**
 * Convenience wrapper for testing capabilities mask
 */
static inline bool have_cap(struct bladerf *dev, uint64_t capability)
{
    return (dev->capabilities & capability) != 0;
}

/**
 * Determine device's initial capabilities, prior to FPGA load.
 * This will clear any existing flags in the device's capabilities mask.
 *
 * @param   dev     Device handle
 *
 * @pre     Device handle already has FX3 version set
 */
void capabilities_init_pre_fpga_load(struct bladerf *dev);

/**
 * Add capability bits based upon FPGA version stored in the device handle
 *
 * @param   dev     Device handle
 *
 * @pre     FPGA has been loaded and its version is in the device handle
 */
void capabilities_init_post_fpga_load(struct bladerf *dev);


#endif
