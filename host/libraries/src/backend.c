#include <assert.h>

#include "backend.h"
#include "debug.h"

#ifdef ENABLE_BACKEND_LIBUSB
#include "backend/libusb.h"
#endif

#ifdef ENABLE_BACKEND_LINUX_DRIVER
#include "backend/linux.h"
#endif

/* TODO complain if native drivers for multiple OSs are enabled */

/* This table should be in order of preferred precedence (first = highest) */
static const struct bladerf_fn * backend_fns[] = {

#ifdef ENABLE_BACKEND_LIBUSB
    &bladerf_lusb_fn,
#endif

#ifdef ENABLE_BACKEND_LINUX_DRIVER
    &bladerf_linux_fn,
#endif

    /* This table must be NULL terminated */
    NULL
};

int open_with_any_backend(struct bladerf **device,
                          struct bladerf_devinfo *info)
{
    int status = BLADERF_ERR_NODEV;
    const struct bladerf_fn **fn_tbl;

    for (fn_tbl = &backend_fns[0]; *fn_tbl != NULL && status != 0; fn_tbl++) {
        assert((*fn_tbl)->open);
        status = (*fn_tbl)->open(device, info);
    }

    return status;
}

/* To keep this from getting messy, the fn table could be a tuple:
 *  <BACKEND_* , fn_tbl> and we could walk through it here...
 *
 *  (Then again, how many backend will we ever have? 4?) */
int backend_open(struct bladerf **device, struct bladerf_devinfo *info) {
    int status = BLADERF_ERR_INVAL;

    switch (info->backend) {

#ifdef ENABLE_BACKEND_LIBUSB
        case BACKEND_LIBUSB:
            status = bladerf_lusb_fn.open(device, info);
            break;
#endif

#ifdef ENABLE_BACKEND_LINUX_DRIVER
        case BACKEND_LINUX:
            status = bladerf_linux_fn.open(device, info);
            break;
#endif

        case BACKEND_ANY:
            status = open_with_any_backend(device, info);
            break;

        default:
            /* Bug - this shouldn't happend */
            dbg_printf("Invalid backend!\n");
            assert(0);
    }

    return status;
}

int backend_probe(struct bladerf_devinfo **devinfo_items, size_t *num_items)
{
    int probe_status, backend_status;
    struct bladerf_devinfo_list list;
    const struct bladerf_fn **fn_tbl;

    *devinfo_items = NULL;
    *num_items = 0;

    probe_status = bladerf_devinfo_list_init(&list);

    if (probe_status == 0) {
        for (fn_tbl = &backend_fns[0]; *fn_tbl != NULL && !probe_status; fn_tbl++) {
            assert((*fn_tbl)->probe);
            backend_status = (*fn_tbl)->probe(&list);

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
