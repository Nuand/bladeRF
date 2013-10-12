/**
 * @file backend.h
 *
 * @brief This file defines the generic interface to bladeRF backends
 *
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2013 Nuand LLC
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
#ifndef BACKEND_H__
#define BACKEND_H__

#include "bladerf_priv.h"

/**
 * Open the device using the backend specified the provided info
 *
 * @param[out]  device  On success, updated with device handle
 * @param[in]   info    Filled-in device info
 *
 * @return  0 on success, BLADERF_ERR_* code on failure
 */
int backend_open(struct bladerf **device,  struct bladerf_devinfo *info);

/**
 * Probe for devices, filling in the provided devinfo list and size of
 * the list that gets populated
 *
 * @param[out]  devinfo_items
 * @param[out]  num_items
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int backend_probe(struct bladerf_devinfo **devinfo_items, size_t *num_items);

/**
 * Get functions for a backend
 *
 * @param       type        Type of backend
 *
 * @return NULL on unknown backend
 */
const struct bladerf_fn * backend_getfns(bladerf_backend type);

#endif

