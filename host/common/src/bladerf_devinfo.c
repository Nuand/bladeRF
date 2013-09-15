#include "bladerf_devinfo.h"

void bladerf_init_devinfo(struct bladerf_devinfo *d)
{
    d->backend  = BLADERF_BACKEND_ANY;
    strcpy(d->serial, DEVINFO_SERIAL_ANY);
    d->usb_bus  = DEVINFO_BUS_ANY;
    d->usb_addr = DEVINFO_ADDR_ANY;
    d->instance = DEVINFO_INST_ANY;
}

bool bladerf_devinfo_matches(struct bladerf_devinfo *a,
                             struct bladerf_devinfo *b)
{
    return
      bladerf_instance_matches(a,b) &&
      bladerf_serial_matches(a,b) &&
      bladerf_bus_addr_matches(a,b);
}

bool bladerf_instance_matches(struct bladerf_devinfo *a,
                              struct bladerf_devinfo *b)
{
    return a->instance == DEVINFO_INST_ANY ||
           b->instance == DEVINFO_INST_ANY ||
           a->instance == b->instance;
}

bool bladerf_serial_matches(struct bladerf_devinfo *a,
                            struct bladerf_devinfo *b)
{
    return !strcmp(a->serial, DEVINFO_SERIAL_ANY) ||
           !strcmp(b->serial, DEVINFO_SERIAL_ANY) ||
           !strcmp(a->serial, b->serial);
}

bool bladerf_bus_addr_matches(struct bladerf_devinfo *a,
                              struct bladerf_devinfo *b)
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
        free(list);
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
        info_tmp = realloc(list->elt, list->backing_size * 2);
        if (!info_tmp) {
            status = BLADERF_ERR_MEM;
        } else {
            list->elt = info_tmp;
        }
    }

    if (status == 0) {
        memcpy(&list->elt[list->num_elt], info, sizeof(*info));
        list->num_elt++;
    }

    return status;
}


