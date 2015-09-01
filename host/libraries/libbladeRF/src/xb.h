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
#include "bladerf_priv.h"

#define XB_SPI_WRITE(dev, val) \
    dev->fn->xb_spi(dev, val)

#define XB_GPIO_READ(dev, val) \
    dev->fn->expansion_gpio_read(dev, val)

#define XB_GPIO_WRITE(dev, mask, val) \
    dev->fn->expansion_gpio_write(dev, mask, val)

#define XB_GPIO_DIR_READ(dev, val) \
    dev->fn->expansion_gpio_dir_read(dev, val)

#define XB_GPIO_DIR_WRITE(dev, mask, val) \
    dev->fn->expansion_gpio_dir_write(dev, mask, val)

/**
 * Attach and enable an expansion board's features
 *
 * @param       dev         Device handle
 * @param       xb          Expansion board
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int xb_attach(struct bladerf *dev, bladerf_xb xb);

/**
 * Determine which expansion board is attached
 *
 * @param       dev         Device handle
 * @param       xb          Expansion board
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int xb_get_attached(struct bladerf *dev, bladerf_xb *xb);

/**
 * Select an XB-200 filterbank
 *
 * @param       dev         Device handle
 * @param       mod         Module
 * @param       filter      XB200 filterbank
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int xb200_set_filterbank(struct bladerf *dev,
                         bladerf_module mod,
                         bladerf_xb200_filter filter);

/**
 * Select an appropriate filterbank, based upon the specified frequency
 *
 * @param[in]   dev        Device handle
 * @param[in]   mod        Module to change
 * @param[in]   frequency  Frequency
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int xb200_auto_filter_selection(struct bladerf *dev, bladerf_module mod,
                                unsigned int frequency);

/**
 * Get the current selected XB-200 filterbank
 *
 * @param[in]    dev        Device handle
 * @param[in]    module     Module to query
 * @param[out]   filter     Pointer to filterbank, only updated if return
 *                          value is 0.
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int xb200_get_filterbank(struct bladerf *dev,
                         bladerf_module module,
                         bladerf_xb200_filter *filter);
/**
 * Configure the XB-200 signal path
 *
 * @param       dev         Device handle
 * @param       module      Module to configure
 * @param       path        Desired XB-200 signal path
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int xb200_set_path(struct bladerf *dev,
                   bladerf_module module,
                   bladerf_xb200_path path);

/**
 * Get the current XB-200 signal path
 *
 * @param       dev         Device handle
 * @param       module      Module to query
 * @param       path        Pointer to XB200 signal path
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
int xb200_get_path(struct bladerf *dev,
                   bladerf_module module,
                   bladerf_xb200_path *path);

#endif /* XB_H_ */
