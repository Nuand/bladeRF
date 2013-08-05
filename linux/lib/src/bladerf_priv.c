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

    /* TODO: Read this return from the SPI calls */
    return 0;
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

