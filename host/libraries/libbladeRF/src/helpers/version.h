/**
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

#ifndef VERSION_COMPAT_H_
#define VERSION_COMPAT_H_

#include <libbladeRF.h>

#define BLADERF_VERSION_STR_MAX 32

/**
 * Test if two versions are equal. The "describe" field is not checked.
 *
 * @return true if equal, false otherwise
 */
bool version_equal(const struct bladerf_version *v1,
                   const struct bladerf_version *v2);

/**
 * Check if version v1 is greater than or equal to version v2.
 *
 * @param       v1  First version
 * @param       v1  Second version
 *
 * @return true for greater or equal, false otherwise
 */
bool version_greater_or_equal(const struct bladerf_version *v1,
                              const struct bladerf_version *v2);

/**
 * Check if version v1 is less than version v2.
 *
 * @param       version     Version structure to check
 * @param       major       Minor version
 * @param       minor       Minor version
 * @param       patch       Patch version
 *
 * @return true for less than, false otherwise
 */
bool version_less_than(const struct bladerf_version *v1,
                       const struct bladerf_version *v2);

/**
 * Check if version in the provided structure is greater or equal to
 * the version specified by the provided major, minor, and patch values
 *
 * @param       version     Version structure to check
 * @param       major       Minor version
 * @param       minor       Minor version
 * @param       patch       Patch version
 *
 * @return true for greater or equal, false otherwise
 */
bool version_fields_greater_or_equal(const struct bladerf_version *version,
                                     unsigned int major, unsigned int minor,
                                     unsigned int patch);

/**
 * Check if version in the provided structure is less than
 * the version specied by the provided major, minor, and patch values
 *
 * @param       version     Version structure to check
 * @param       major       Minor version
 * @param       minor       Minor version
 * @param       patch       Patch version
 *
 * @return true for less than, false otherwise
 */
bool version_fields_less_than(const struct bladerf_version *version,
                              unsigned int major, unsigned int minor,
                              unsigned int patch);

#endif
