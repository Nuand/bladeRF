/**
 * @file tuning.h
 *
 * @brief Logic pertaining to device frequency tuning
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
#ifndef BLADERF_TUNING_H_
#define BLADERF_TUNING_H_

/**
 * Configure the device for operation in the high or low band, based
 * upon the provided frequency
 *
 * @param   dev         Device handle
 * @param   module      Module to configure
 * @param   frequency   Desired frequency
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int tuning_select_band(struct bladerf *dev, bladerf_module module,
                       unsigned int frequency);

/**
 * Tune to the specified frequency
 *
 * @param   dev         Device handle
 * @param   module      Module to configure
 * @param   frequency   Desired frequency
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int tuning_set_freq(struct bladerf *dev, bladerf_module module,
                    unsigned int frequency);
/**
 * Get the current frequency that the specified module is tuned to
 *
 * @param[in]   dev         Device handle
 * @param[in]   module      Module to configure
 * @param[out]  frequency   Desired frequency
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int tuning_get_freq(struct bladerf *dev, bladerf_module module,
                    unsigned int *frequency);


#endif
