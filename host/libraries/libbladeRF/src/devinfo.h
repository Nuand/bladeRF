/**
 * @file devinfo.h
 *
 * @brief Routines for parsing and handling device identifier
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

#ifndef DEVINFO_H_
#define DEVINFO_H_

#include <stddef.h>

/* Reserved values for bladerf_devinfo fields to indicate "undefined" */
#define DEVINFO_SERIAL_ANY "ANY"
#define DEVINFO_BUS_ANY UINT8_MAX
#define DEVINFO_ADDR_ANY UINT8_MAX
#define DEVINFO_INST_ANY UINT_MAX

struct bladerf_devinfo_list {
    struct bladerf_devinfo *elt;
    size_t num_elt;      /* Number of elements in the list */
    size_t backing_size; /* Size of backing array */
};

#include "backend/backend.h"

int probe(backend_probe_target target_device, struct bladerf_devinfo **devices);

int bladerf_get_device_list(struct bladerf_devinfo **devices);

void bladerf_free_device_list(struct bladerf_devinfo *devices);

/**
 * Do the device instances for the two provided device info structures match
 * (taking wildcards into account)?
 *
 * @param[in]   a   Device information to compare
 * @param[in]   b   Device information to compare
 *
 * @return true on match, false otherwise
 */
bool bladerf_instance_matches(const struct bladerf_devinfo *a,
                              const struct bladerf_devinfo *b);

/**
 * Do the serials for the two provided device info structures match
 * (taking wildcards into account)?
 *
 * @param[in]   a   Device information to compare
 * @param[in]   b   Device information to compare
 *
 * @return true on match, false otherwise
 */
bool bladerf_serial_matches(const struct bladerf_devinfo *a,
                            const struct bladerf_devinfo *b);

/**
 * Do the bus and addr for the two provided device info structures match
 * (taking wildcards into account)?
 *
 * @param[in]   a   Device information to compare
 * @param[in]   b   Device information to compare
 *
 * @return true on match, false otherwise
 */
bool bladerf_bus_addr_matches(const struct bladerf_devinfo *a,
                              const struct bladerf_devinfo *b);

/**
 * Create list of devinfos
 *
 * @param[in]   list    List of devinfos
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int bladerf_devinfo_list_init(struct bladerf_devinfo_list *list);

/**
 * Get a pointer to the parent devinfo_list container of a devinfo
 *
 * @param[in]   devinfo Device info
 *
 * @return pointer to container on success, NULL on error
 */
struct bladerf_devinfo_list *bladerf_get_devinfo_list(
    struct bladerf_devinfo *devinfo);

/**
 * Add an item to our internal devinfo list
 *
 * @param[inout]    list    List to append to
 * @param[in]       info    Info to copy into the list
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int bladerf_devinfo_list_add(struct bladerf_devinfo_list *list,
                             struct bladerf_devinfo *info);

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
