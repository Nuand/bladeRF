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
#include "rel_assert.h"

#include "backend.h"
#include "backend_config.h"
#include "log.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

struct backend_table_entry {
    const bladerf_backend type;
    const struct bladerf_fn *fns;
};

static const struct backend_table_entry backend_list[] = BLADERF_BACKEND_LIST;

int open_with_any_backend(struct bladerf **device,
                          struct bladerf_devinfo *info)
{
    size_t i;
    int status = BLADERF_ERR_NODEV;
    const size_t n_backends = ARRAY_SIZE(backend_list);

    for (i = 0; i < n_backends && status != 0; i++) {
        status = backend_list[i].fns->open(device, info);
    }

    return status;
}

const struct bladerf_fn * backend_getfns(bladerf_backend type) {
    size_t i;
    const size_t n_backends = ARRAY_SIZE(backend_list);

    for (i = 0; i < n_backends; i++) {
        if (backend_list[i].type == type) {
            return backend_list[i].fns;
        }
    }

    return NULL;
}

int backend_open(struct bladerf **device, struct bladerf_devinfo *info) {

    size_t i;
    int status = BLADERF_ERR_NODEV;
    const size_t n_backends = ARRAY_SIZE(backend_list);

    if (info->backend == BLADERF_BACKEND_ANY) {
        status = open_with_any_backend(device, info);
    } else {
        for (i = 0; i < n_backends; i++) {
            if (backend_list[i].type == info->backend) {
                status = backend_list[i].fns->open(device, info);
                break;
            }
        }
    }

    return status;
}

int backend_probe(struct bladerf_devinfo **devinfo_items, size_t *num_items)
{
    int status;
    int first_backend_error = 0;
    struct bladerf_devinfo_list list;
    size_t i;
    const size_t n_backends = ARRAY_SIZE(backend_list);

    *devinfo_items = NULL;
    *num_items = 0;

    status = bladerf_devinfo_list_init(&list);
    if (status != 0) {
        log_debug("Failed to initialize devinfo list: %s\n",
                  bladerf_strerror(status));
        return status;
    }

    for (i = 0; i < n_backends; i++) {
        status = backend_list[i].fns->probe(&list);

        if (status < 0 && status != BLADERF_ERR_NODEV) {
            log_debug("Probe failed on backend %d: %s\n",
                      i, bladerf_strerror(status));

            if (!first_backend_error) {
                first_backend_error = status;
            }
        }
    }

    *num_items = list.num_elt;

    if (*num_items != 0) {
        *devinfo_items = list.elt;
    } else {
        /* For no items, we end up passing back a NULL list to the
         * API caller, so we'll just free this up now */
        free(list.elt);

        /* Report the first error that occurred if we couldn't find anything */
        status =
            first_backend_error == 0 ? BLADERF_ERR_NODEV : first_backend_error;
    }

    return status;
}
