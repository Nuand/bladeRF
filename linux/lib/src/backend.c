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
    const struct bladerf_fn *fn_tbl;
    struct bladerf *ret = NULL;

    for (fn_tbl = backend_fns[0]; fn_tbl != NULL && ret == NULL; fn_tbl++) {
        assert(fn_tbl->open);
        ret = fn_tbl->open(info);
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

/* XXX for the probe routine, we can do something similar to
 *     open_with_any_backend, where we loop over the backend funtion tables
 */
