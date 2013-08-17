#include <assert.h>
#include <string.h>
#include <libbladeRF.h>
#include <stddef.h>

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
    unsigned int actual ;

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

    /* Set a default saplerate */
    bladerf_set_sample_rate( dev, TX, 1000000, &actual );
    bladerf_set_sample_rate( dev, RX, 1000000, &actual );

    /* Enable TX and RX */
    bladerf_enable_module( dev, TX, true );
    bladerf_enable_module( dev, RX, true );

    /* Set a default frequency of 1GHz */
    bladerf_set_frequency( dev, TX, 1000000000 );
    bladerf_set_frequency( dev, RX, 1000000000 );

    /* TODO: Read this return from the SPI calls */
    return 0;
}

void bladerf_init_devinfo(struct bladerf_devinfo *d)
{
    d->backend  = BACKEND_ANY;
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
