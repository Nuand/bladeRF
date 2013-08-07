#include <assert.h>
#include <string.h>
#include <libbladeRF.h>

#include "bladerf_priv.h"
#include "debug.h"

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

/* TODO Check for truncation (e.g., odd # bytes)? */
size_t bytes_to_c16_samples(size_t n_bytes)
{
    return n_bytes / (2 * sizeof(int16_t));
}

/* TODO Overflow check? */
size_t c16_samples_to_bytes(size_t n_samples)
{
    return n_samples * 2 * sizeof(int16_t);
}

int bladerf_init_device(struct bladerf *dev)
{
    /* Set the GPIO pins to enable the LMS and select the low band */
    bladerf_gpio_write( dev, 0x51 );

    /* Set the internal LMS register to enable RX and TX */
    bladerf_lms_write( dev, 0x05, 0x3e );

    /* LMS FAQ: Improve TX spurious emission performance */
    bladerf_lms_write( dev, 0x47, 0x40 );

    /* LMS FAQ: Improve ADC performance */
    bladerf_lms_write( dev, 0x59, 0x29 );

    /* LMS FAQ: Common mode voltage for ADC */
    bladerf_lms_write( dev, 0x64, 0x36 );

    /* LMS FAQ: Higher LNA Gain */
    bladerf_lms_write( dev, 0x79, 0x37 );

    /* FPGA workaround: Set IQ polarity for RX */
    bladerf_lms_write( dev, 0x5a, 0xa0 );

    /* TODO: Read this return from the SPI calls */
    return 0;
}

void bladerf_init_devinfo(struct bladerf_devinfo *d)
{
    d->backend  = BACKEND_ANY;
    d->serial   = DEVINFO_SERIAL_ANY;
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
    return a->serial == DEVINFO_SERIAL_ANY ||
           b->serial == DEVINFO_SERIAL_ANY ||
           a->serial == b->serial;
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

int bladerf_devinfo_list_alloc(struct bladerf_devinfo_list **list_ret)
{
    int status = BLADERF_ERR_UNEXPECTED;
    struct bladerf_devinfo_list *list;

    list = malloc(sizeof(struct bladerf_devinfo_list));
    if (!list) {
        dbg_printf("Failed to allocated devinfo list!\n");
        status = BLADERF_ERR_MEM;
    } else {
        list->cookie = 0xdeadbeef;
        list->num_elt = 0;
        list->backing_size = 5;

        list->elt = malloc(list->backing_size * sizeof(struct bladerf_devinfo));

        if (!list->elt) {
            free(list);
            status = BLADERF_ERR_MEM;
        } else {
            *list_ret = list;
            status = 0;
        }
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
        /*list->elt[list->num_elt].backend = backend;
        list->elt[list->num_elt].serial = serial;
        list->elt[list->num_elt].usb_bus = usb_bus;
        list->elt[list->num_elt].usb_addr = usb_addr;
        list->elt[list->num_elt].instance = instance;
        list->num_elt++;*/
    }

    return status;
}

void bladerf_devinfo_list_free(struct bladerf_devinfo_list *list)
{
    assert(list && list->elt && list->cookie == 0xdeadbeef);
    free(list->elt);
    free(list);
}

/* In the spirit of container_of & offset_of
 *   http://www.kroah.com/log/linux/container_of.html)
 */
struct bladerf_devinfo_list *
bladerf_get_devinfo_list(struct bladerf_devinfo *devinfo)
{
    struct bladerf_devinfo_list *ret;
    size_t offset = (size_t)(((struct bladerf_devinfo_list*)0)->elt);

    ret = (struct bladerf_devinfo_list *)((char *)devinfo - offset);

    /* Assert for debug, error for release build... */
    assert(ret->cookie == 0xdeadbeef);
    if (ret->cookie != 0xdeadbeef) {
        ret = NULL;
    }

    return ret;
}


