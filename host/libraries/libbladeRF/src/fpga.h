/*
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

#ifndef BLADERF_FPGA_H_
#define BLADERF_FPGA_H_

#include "bladerf_priv.h"

#define FPGA_IS_CONFIGURED(dev) dev->fn->is_fpga_configured(dev)

/**
 * Test if a sufficiently new enough FPGA is being used and log warnings
 * if not.
 *
 * @param   dev     Device handle
 *
 * @return 0 on success,
 *         BLADERF_ERR_UPDATE_FPGA if the device firmware requires a new FPGA,
 *         BLADERF_ERR_UPDATE_FW if the FPGA version being used requires,
 *              newer firmware,
 *         BLADERF_ERR_* values on other failures
 */
int fpga_check_version(struct bladerf *dev);

/**
 * Load an FPGA bitstream from the specified RBF
 *
 * @param   dev         Device handle
 * @param   fpga_file   Path to an RBF file
 *
 * @return 0 on success,
 *         BLADERF_ERR_TIMEOUT generally occurs when attempting to load
 *              the wrong size FPGA image.
 *         BLADERF_ERR_* values on other failures
 */
int fpga_load_from_file(struct bladerf *dev, const char *fpga_file);

/**
 * Write an FPGA bitstream to the device's SPI flash. This will cause the
 * FPGA to be autoloaded the next time the device is powered on.
 *
 * @param   dev         Device handle
 * @param   fpga_file   Path to an RBF file
 *
 * @return 0 on success,
 *         BLADERF_ERR_* values on other failure
 */
int fpga_write_to_flash(struct bladerf *dev, const char *fpga_file);

#endif
