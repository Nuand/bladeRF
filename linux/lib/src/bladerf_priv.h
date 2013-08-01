/* This file is not part of the API and may be changed on a whim.
 * If you're interfacing with libbladerf, DO NOT use this file! */

#ifndef BLADERF_PRIV_H_
#define BLADERF_PRIV_H_

#include <limits.h>
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

/* XXX: Not sure where to put these */
int bladerf_init_device(struct bladerf *dev);
size_t bytes_to_c16_samples(size_t n_bytes);
size_t c16_samples_to_bytes(size_t n_samples);

/* Forward declaration for the function table */
struct bladerf;

/* Driver specific function table.  These functions are required for each
   unique platform to operate the device. */
struct bladerf_fn {
    /* FPGA Loading and checking */
    int (*load_fpga)(struct bladerf *dev, uint8_t *image, size_t image_size);
    int (*is_fpga_configured)(struct bladerf *dev);

    /* Platform information */
    int (*get_serial)(struct bladerf *dev, uint64_t *serial);
    int (*get_fw_version)(struct bladerf *dev, unsigned int *maj, unsigned int *min);
    int (*get_fpga_version)(struct bladerf *dev, unsigned int *maj, unsigned int *min);

    /* Closing of the device and freeing of the data */
    int (*close)(struct bladerf *dev);

    /* GPIO accessors */
    int (*gpio_write)(struct bladerf *dev, uint32_t val);
    int (*gpio_read)(struct bladerf *dev, uint32_t *val);

    /* Si5338 accessors */
    int (*si5338_write)(struct bladerf *dev, uint8_t addr, uint8_t data);
    int (*si5338_read)(struct bladerf *dev, uint8_t addr, uint8_t *data);

    /* LMS6002D accessors */
    int (*lms_write)(struct bladerf *dev, uint8_t addr, uint8_t data);
    int (*lms_read)(struct bladerf *dev, uint8_t addr, uint8_t *data);

    /* VCTCXO accessor */
    int (*dac_write)(struct bladerf *dev, uint16_t value);

    /* Sample stream */
    /* XXX: Add metadata struct? */
    ssize_t (*read_samples)(struct bladerf *dev, int16_t *samples, size_t n);
    ssize_t (*write_samples)(struct bladerf *dev, int16_t *samples, size_t n);
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

