/**
 * @file xb.h
 *
 * @brief XB-300 support
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

#ifndef EXPANSION_XB300_H_
#define EXPANSION_XB300_H_

#include <stdbool.h>

#include <libbladeRF.h>

#include "board/board.h"

int xb300_attach(struct bladerf *dev);
void xb300_detach(struct bladerf *dev);
int xb300_enable(struct bladerf *dev, bool enable);
int xb300_init(struct bladerf *dev);

/**
 * Configure the XB-300 TRX path
 *
 * @param       dev     Device handle
 * @param[in]   trx     Desired XB-300 TRX setting
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int xb300_set_trx(struct bladerf *dev, bladerf_xb300_trx trx);

/**
 * Get the current XB-300 signal path
 *
 * @param       dev     Device handle
 * @param[out]  trx     XB300 TRX antenna setting
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int xb300_get_trx(struct bladerf *dev, bladerf_xb300_trx *trx);

/**
 * Enable or disable selected XB-300 amplifier
 *
 * @param       dev     Device handle
 * @param[in]   amp     XB-300 amplifier
 * @param[in]   enable  Set true to enable or false to disable
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int xb300_set_amplifier_enable(struct bladerf *dev,
                               bladerf_xb300_amplifier amp,
                               bool enable);
/**
 * Get state of selected XB-300 amplifier
 *
 * @param       dev     Device handle
 * @param[in]   amp     XB-300 amplifier
 * @param[out]  enable  Set true to enable or false to disable
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int xb300_get_amplifier_enable(struct bladerf *dev,
                               bladerf_xb300_amplifier amp,
                               bool *enable);
/**
 * Get current PA PDET output power in dBm
 *
 * @param       dev     Device handle
 * @param[out]  val     Output power in dBm
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
int xb300_get_output_power(struct bladerf *dev, float *val);

#endif
