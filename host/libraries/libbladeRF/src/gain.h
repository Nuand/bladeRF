/**
 * @file gain.h
 *
 * @brief Higher level system gain functions
 *
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
#ifndef BLADERF_GAIN_H_
#define BLADERF_GAIN_H_

#include "libbladeRF.h"

/**
 * Set system gain for the specified module
 *
 * @param   dev     Device handle
 * @param   module  Module to configure
 * @param   gain    Desired gain
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int gain_set(struct bladerf *dev, bladerf_module module, int gain);

/* TODO gain_get() */

#endif
