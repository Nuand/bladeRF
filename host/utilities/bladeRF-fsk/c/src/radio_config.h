/**
 * @file
 * @brief   BladeRF radio configuration - setting of frequencies, gains, etc.
 *
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2016 Nuand LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef RADIO_CONFIG_H
#define RADIO_CONFIG_H

#include <libbladeRF.h>
#include <stdio.h>

#include "common.h"

#define SYNC_BUFFER_SIZE 16384
//1.5MHz
#define BLADERF_BANDWIDTH 1500000
//2Msps
#define BLADERF_SAMPLE_RATE 2000000

/**
 * Configure bladeRF device
 *
 * @param[in]   dev     pointer to bladeRF device handle
 * @param[in]   params  pointer to radio_params struct specifying frequencies/gains
 * 
 * @return      0 on success, <0 on error
 */
int radio_init_and_configure(struct bladerf *dev, struct radio_params *params);

/**
 * Stop transmitting/receiving with bladerf. Device must be closed elsewhere.
 *
 * @param[in]   dev     pointer to bladeRF device handle to stop
 */
void radio_stop(struct bladerf *dev);

#endif
