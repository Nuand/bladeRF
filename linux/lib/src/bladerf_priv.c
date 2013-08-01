#include <libbladeRF.h>
#include "bladerf_priv.h"

void bladerf_set_error(struct bladerf_error *error,
                        bladerf_error_t type, int val)
{
    error->type = type;
    error->value = val;
}

void bladerf_get_error(struct bladerf_error *error,
                        bladerf_error_t *type, int *val)
{
    if (type) {
        *type = error->type;
    }

    if (val) {
        *val = error->value;
    }
}

