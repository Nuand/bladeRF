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

/* This file defines functionality related to libbladeRF configuration
 * directories and files */

#ifndef BLADERF_CONFIG_H_
#define BLADERF_CONFIG_H_

/**
 * Load DC calibration tables from their associated files, if available.
 *
 * Note that successful return value doesn't imply that tables were found and
 * loaded. Use the BLADERF_HAS_RX_DC_CAL() and BLADERF_HAS_TX_DC_CAL() macros to
 * ensure the device has calibration tables before attempting to use thea
 *
 * @return  0 on success, BLADERF_ERR_MEM on memory allocation error.  *
 */
int config_load_dc_cals(struct bladerf *dev);

/**
 * Load the FPGA from the associated image, by name.
 *
 * @pre     dev->fpga_size is populated
 *
 * @param   dev     Handle to device to load FPGA on
 *
 * @return  0 on success,
 *          BLADERF_ERR_TIMEOUT generally occurs when attempting to load
 *                              the wrong size FPGA image.
 *          BLADERF_ERR_* values on other failures
 */
int config_load_fpga(struct bladerf *dev);

#endif
