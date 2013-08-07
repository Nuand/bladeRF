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

static struct bladerf * open_with_any_backend(struct bladerf_devinfo *info)
{
    const struct bladerf_fn **fn_tbl;
    struct bladerf *ret = NULL;

    for (fn_tbl = &backend_fns[0]; *fn_tbl != NULL && ret == NULL; fn_tbl++) {
        assert((*fn_tbl)->open);
        ret = (*fn_tbl)->open(info);
    }

    return ret;
}

/* To keep this from getting messy, the fn table could be a tuple:
 *  <BACKEND_* , fn_tbl> and we could walk through it here...
 *
 *  (Then again, how many backend will we ever have? 4?) */
struct bladerf * backend_open(struct bladerf_devinfo *info) {
    switch (info->backend) {

#ifdef ENABLE_BACKEND_LIBUSB
        case BACKEND_LIBUSB:
            return bladerf_lusb_fn.open(info);
            break;
#endif

#ifdef ENABLE_BACKEND_LINUX_DRIVER
        case BACKEND_LINUX:
            return bladerf_linux_fn.open(info);
            break;
#endif

        case BACKEND_ANY:
            return open_with_any_backend(info);
            break;

        default:
            /* Bug - this shouldn't happend */
            dbg_printf("Invalid backend!\n");
            assert(0);
    }
    return NULL;
}

int backend_probe(struct bladerf_devinfo **devinfo_items, size_t *num_items)
{
    int status;
    struct bladerf_devinfo_list *list;
    const struct bladerf_fn **fn_tbl;

    status = bladerf_devinfo_list_alloc(&list);

    if (status == 0) {
        for (fn_tbl = &backend_fns[0]; *fn_tbl != NULL; fn_tbl++) {
            assert((*fn_tbl)->probe);
            status = (*fn_tbl)->probe(list);

            if (status < 0) {
                bladerf_devinfo_list_free(list);
            }
        }
    }

    return status;
}
