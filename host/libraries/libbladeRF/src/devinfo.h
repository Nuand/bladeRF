/**
 * @file devinfo.h
 *
 * @brief Routines for handling and comparing device information
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
#ifndef _BLADERF_DEVINFO_H_
#define _BLADERF_DEVINFO_H_

#include "libbladeRF.h"
#include <string.h>
#include <limits.h>

/* Reserved values for bladerf_devinfo fields to indicate "undefined" */
#define DEVINFO_SERIAL_ANY    "ANY"
#define DEVINFO_BUS_ANY       UINT8_MAX
#define DEVINFO_ADDR_ANY      UINT8_MAX
#define DEVINFO_INST_ANY      UINT_MAX

struct bladerf_devinfo_list {
    struct bladerf_devinfo *elt;
    size_t num_elt;      /* Number of elements in the list */
    size_t backing_size; /* Size of backing array */
};

/**
 * Do the device instances for the two provided device info structures match
 * (taking wildcards into account)?
 *
 * @param   a   Device information to compare
 * @param   b   Device information to compare
 *
 * @return true on match, false otherwise
 */
bool bladerf_instance_matches(const struct bladerf_devinfo *a,
                              const struct bladerf_devinfo *b);

/**
 * Do the serials match for the two provided device info structures match
 * (taking wildcards into account)?
 *
 * @param   a   Device information to compare
 * @param   b   Device information to compare
 *
 * @return true on match, false otherwise
 */
bool bladerf_serial_matches(const struct bladerf_devinfo *a,
                            const struct bladerf_devinfo *b);

/**
 * Do the bus and addr match for the two provided device info structures match
 * (taking wildcards into account)?
 *
 * @param   a   Device information to compare
 * @param   b   Device information to compare
 *
 * @return true on match, false otherwise
 */
bool bladerf_bus_addr_matches(const struct bladerf_devinfo *a,
                              const struct bladerf_devinfo *b);

/**
 * Create list of deinfos
 */
int bladerf_devinfo_list_init(struct bladerf_devinfo_list *list);

/**
 * Get a pointer to the parent devinfo_list container of a devinfo
 *
 * @return pointer to container on success, NULL on error
 */
struct bladerf_devinfo_list *
bladerf_get_devinfo_list(struct bladerf_devinfo *devinfo);

/**
 * Add and item to our internal devinfo list
 *
 * @param   list    List to append to
 * @param   info    Info to copy into the list
 *
 * 0 on success, BLADERF_ERR_* on failure
 */
int bladerf_devinfo_list_add(struct bladerf_devinfo_list *list,
                             struct bladerf_devinfo *info);

#endif /* _BLADERF_DEVINFO_H_ */
