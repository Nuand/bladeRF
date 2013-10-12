/**
 * @file device_identifier.h
 *
 * @brief Routines for parsing and handling device identifier strings
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
#ifndef DEVICE_IDENTIFIER_H__
#define DEVICE_IDENTIFIER_H__


/**
 * Fill out a device info structure based upon the provided device indentifer
 * string. If a failure occurrs, the contents of d are undefined.
 *
 * For device identifier format, see the documentation for bladerf_open
 * (in include/libbladeRF.h)
 *
 * @param[in]  device_identifier   Device identifier string
 * @param[out] d                   Device info to fill in
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int str2devinfo(const char *device_identifier, struct bladerf_devinfo *d);

#endif
