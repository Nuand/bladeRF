/**
 * @file xb.h
 *
 * @brief Expansion Board support
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
#ifndef XB_H_
#define XB_H_

#include <libbladeRF.h>

/**
 * Filterbank autoselection helper function
 *
 * @param[in]   dev        Device handle
 * @param[in]   mod        Module to change
 * @param[in]   frequency  Frequency
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int bladerf_xb200_auto_filter_selection(struct bladerf *dev, bladerf_module mod, unsigned int frequency);

#endif /* XB_H_ */
