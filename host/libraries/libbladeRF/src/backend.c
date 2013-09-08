#include <assert.h>

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
    int probe_status, backend_status;
    struct bladerf_devinfo_list list;
    size_t i;
    const size_t n_backends = ARRAY_SIZE(backend_list);

    *devinfo_items = NULL;
    *num_items = 0;

    probe_status = bladerf_devinfo_list_init(&list);

    if (probe_status == 0) {
        for (i = 0; i < n_backends && probe_status == 0; i++) {
            backend_status = backend_list[i].fns->probe(&list);

            /* Error out if a backend hit any concerning error */
            if (backend_status  < 0 && backend_status != BLADERF_ERR_NODEV) {
                probe_status = backend_status;
            }
        }
    }

    if (probe_status == 0) {
        *num_items = list.num_elt;

        if (*num_items != 0) {
            *devinfo_items = list.elt;
        } else {
            /* For no items, we end up passing back a NULL list to the
             * API caller, so we'll just free this up now */
            free(list.elt);
            probe_status = BLADERF_ERR_NODEV;
        }

    } else {
        free(list.elt);
    }

    return probe_status;
}
