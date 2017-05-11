/**
 * @file ina219.h
 *
 * @brief INA219 Support
 *
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2017 Nuand LLC
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

#ifndef DRIVER_INA219_H_
#define DRIVER_INA219_H_

#include <libbladeRF.h>

#include "board/board.h"

/**
 * Initialize the INA219 voltage/current/power monitor.
 *
 * @param       dev         Device handle
 * @param[in]   r_shunt     Shunt resistor in ohms
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int ina219_init(struct bladerf *dev, float r_shunt);

/**
 * Read the shunt voltage.
 *
 * @param       dev         Device handle
 * @param[out]  voltage     Shunt voltage in volts
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int ina219_read_shunt_voltage(struct bladerf *dev, float *voltage);

/**
 * Read the bus voltage.
 *
 * @param       dev         Device handle
 * @param[out]  voltage     Bus voltage in volts
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int ina219_read_bus_voltage(struct bladerf *dev, float *voltage);

/**
 * Read the load current.
 *
 * @param       dev         Device handle
 * @param[out]  current     Load current in amps
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int ina219_read_current(struct bladerf *dev, float *current);

/**
 * Read the load power.
 *
 * @param       dev         Device handle
 * @param[out]  power       Load power in watts
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int ina219_read_power(struct bladerf *dev, float *power);

#endif
