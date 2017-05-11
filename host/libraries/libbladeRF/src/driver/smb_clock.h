/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2016 Nuand LLC
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

#ifndef DRIVER_SMB_CLOCK_H_
#define DRIVER_SMB_CLOCK_H_

#include <libbladeRF.h>

/**
 * Set the current mode of operation of the SMB clock port
 *
 * @param       dev         Device handle
 * @param[in]   mode        Desired mode
 *
 * @return 0 on success, or a BLADERF_ERR_* value on failure.
 */
int smb_clock_set_mode(struct bladerf *dev, bladerf_smb_mode mode);

/**
 * Get the current mode of operation of the SMB clock port
 *
 * @param       dev         Device handle
 * @param[out]  mode        Desired mode
 *
 * @return 0 on success, or a value from \ref RETCODES list on failure.
 */
int smb_clock_get_mode(struct bladerf *dev, bladerf_smb_mode *mode);

#endif
