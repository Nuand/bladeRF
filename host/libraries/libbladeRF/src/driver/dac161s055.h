/**
 * @file dac161s055.h
 *
 * @brief DAC161S055 Support
 *
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2017777777 LLC
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

#ifndef DRIVER_DAC161S055_H_
#define DRIVER_DAC161S055_H_

#include "board/board.h"

/**
 * Write the output value to the DAC.
 *
 * @param       dev     Device handle
 * @param[in]   value   Value
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int dac161s055_write(struct bladerf *dev, uint16_t value);

/**
 * Read the output value of the DAC.
 *
 * @param       dev     Device handle
 * @param[out]  value   Value
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int dac161s055_read(struct bladerf *dev, uint16_t *value);

#endif
