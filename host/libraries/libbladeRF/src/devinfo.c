/*
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
#include "devinfo.h"

bool bladerf_instance_matches(const struct bladerf_devinfo *a,
                              const struct bladerf_devinfo *b)
{
    return a->instance == DEVINFO_INST_ANY ||
           b->instance == DEVINFO_INST_ANY ||
           a->instance == b->instance;
}

bool bladerf_serial_matches(const struct bladerf_devinfo *a,
                            const struct bladerf_devinfo *b)
{
    return !strcmp(a->serial, DEVINFO_SERIAL_ANY) ||
           !strcmp(b->serial, DEVINFO_SERIAL_ANY) ||
           !strcmp(a->serial, b->serial);
}

bool bladerf_bus_addr_matches(const struct bladerf_devinfo *a,
                              const struct bladerf_devinfo *b)
{
    bool bus_match, addr_match;

    bus_match = a->usb_bus == DEVINFO_BUS_ANY ||
                b->usb_bus == DEVINFO_BUS_ANY ||
                a->usb_bus == b->usb_bus;

    addr_match = a->usb_addr == DEVINFO_BUS_ANY ||
                 b->usb_addr == DEVINFO_BUS_ANY ||
                 a->usb_addr == b->usb_addr;

    return bus_match && addr_match;
}

int bladerf_devinfo_list_init(struct bladerf_devinfo_list *list)
{
    int status = 0;

    list->num_elt = 0;
    list->backing_size = 5;

    list->elt = malloc(list->backing_size * sizeof(struct bladerf_devinfo));

    if (!list->elt) {
        status = BLADERF_ERR_MEM;
    }

    return status;
}

int bladerf_devinfo_list_add(struct bladerf_devinfo_list *list,
                             struct bladerf_devinfo *info)
{
    int status = 0;
    struct bladerf_devinfo *info_tmp;

    if (list->num_elt >= list->backing_size) {
        info_tmp = realloc(list->elt, list->backing_size * 2 * sizeof(*list->elt));
        if (!info_tmp) {
            status = BLADERF_ERR_MEM;
        } else {
            list->elt = info_tmp;
            list->backing_size = list->backing_size * 2;
        }
    }

    if (status == 0) {
        memcpy(&list->elt[list->num_elt], info, sizeof(*info));
        list->num_elt++;
    }

    return status;
}
