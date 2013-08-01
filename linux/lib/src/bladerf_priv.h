/* This file is not part of the API and may be changed on a whim.
 * If you're interfacing with libbladerf, DO NOT use this file! */

#ifndef BLADERF_PRIV_H_
#define BLADERF_PRIV_H_

#include <libbladeRF.h>

typedef enum {
    INVALID = -1,
    KERNEL,
    LIBUSB,
} bladerf_driver_t;

typedef enum {
    ETYPE_ERRNO,
    ETYPE_LIBBLADERF,
    ETYPE_DRIVER,
    ETYPE_OTHER = INT_MAX - 1
} bladerf_error_t;

struct bladerf_error {
    bladerf_error_t type;
    int value;
};


/* TODO Should there be a "big-lock" wrapped around accesses to a device */
struct bladerf {
    int speed;      /* The device's USB speed, 0 is HS, 1 is SS */
    struct bladerf_stats stats;

    /* FIXME temporary workaround for not being able to read back sample rate */
    unsigned int last_tx_sample_rate;
    unsigned int last_rx_sample_rate;

    /* Last error encountered */
    struct bladerf_error error;

    /* Type of the underlying driver and its private data  */
    bladerf_driver_t driver_type;
    void *driver;

    /* Driver-sppecific implementations */
    struct bladerf_fn *fn;
};

struct bladerf_fn {
    int (*load_fpga)(struct bladerf *dev, uint8_t *image, size_t image_size);

    int (*close)(struct bladerf *dev);

    int (*gpio_write)(struct bladerf *dev, uint32_t val);
    int (*gpio_read)(struct bladerf *dev, uint32_t *val);

    int (*si5338_write)(struct bladerf *dev, uint8_t addr, uint8_t data);
    int (*si5338_read)(struct bladerf *dev, uint8_t addr, uint8_t *data);

    int (*lms_write)(struct bladerf *dev, uint8_t addr, uint8_t data);
    int (*lms_read)(struct bladerf *dev, uint8_t addr, uint8_t *data);

    int (*dac_write)(struct bladerf *dev, uint16_t value);

    ssize_t (*read_samples)(struct bladerf *dev, int16_t *samples, size_t n);
    ssize_t (*write_samples)(struct bladerf *dev, int16_t *samples, size_t n);

};

/**
 * Set an error and type
 */
void bladerf_set_error(struct bladerf_error *error,
                        bladerf_error_t type, int val);

void bladerf_get_error(struct bladerf_error *error,
                        bladerf_error_t *type, int *val);

/* TODO Check for truncation (e.g., odd # bytes)? */
static inline size_t bytes_to_c16_samples(size_t n_bytes)
{
    return n_bytes / (2 * sizeof(int16_t));
}

/* TODO Overflow check? */
static inline size_t c16_samples_to_bytes(size_t n_samples)
{
    return n_samples * 2 * sizeof(int16_t);
}

static inline size_t min_sz(size_t x, size_t y)
{
    return x < y ? x : y;
}

#endif
