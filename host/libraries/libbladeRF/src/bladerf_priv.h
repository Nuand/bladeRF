/* This file is not part of the API and may be changed on a whim.
 * If you're interfacing with libbladerf, DO NOT use this file! */

#ifndef BLADERF_PRIV_H_
#define BLADERF_PRIV_H_

#include <limits.h>
#include <libbladeRF.h>
#include "host_config.h"
#include "minmax.h"
#include "conversions.h"
#include "devinfo.h"

/* MX25U3235 - 32Mbit flash w/ 4KiB sectors */
#define FLASH_SECTOR_SIZE   0x10000
#define FLASH_PAGE_SIZE     256
#define FLASH_BYTES_TO_SECTORS(x) ((x + (FLASH_SECTOR_SIZE - 1)) / FLASH_SECTOR_SIZE)
#define FLASH_BYTES_TO_PAGES(x) ((x + (FLASH_PAGE_SIZE - 1)) / FLASH_PAGE_SIZE)
#define FLASH_NUM_SECTORS   4096
#define FLASH_NUM_PAGES     (FLASH_NUM_SECTORS * (FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE))

typedef enum {
    ETYPE_ERRNO,
    ETYPE_LIBBLADERF,
    ETYPE_BACKEND,
    ETYPE_OTHER = INT_MAX - 1
} bladerf_error;

struct bladerf_error {
    bladerf_error type;
    int value;
};

typedef enum {
    STREAM_IDLE,            /* Idle and initialized */
    STREAM_RUNNING,         /* Currently running */
    STREAM_SHUTTING_DOWN,   /* Currently tearing down.
                             * See bladerf_stream->error_code to determine
                             * whether or not the shutdown was a clean exit
                             * or due to an error. */
    STREAM_DONE             /* Done and deallocated */
} bladerf_stream_state;

struct bladerf_stream {
    struct bladerf *dev;
    bladerf_module module;
    int error_code;
    bladerf_stream_state state;

    size_t samples_per_buffer;
    size_t num_buffers;
    size_t num_transfers;
    bladerf_format format;

    void **buffers;
    void *backend_data;

    bladerf_stream_cb cb;
    void *user_data;
};

/* Forward declaration for the function table */
struct bladerf;

/* Driver specific function table.  These functions are required for each
   unique platform to operate the device. */
struct bladerf_fn {

    /* Backends probe for devices and append entries to this list using
     * bladerf_devinfo_list_append() */
    int (*probe)(struct bladerf_devinfo_list *info_list);

    /* Opening device based upon specified device info
     * devinfo structure. The implementation of this function is responsible
     * for ensuring (*device)->ident is populated */
    int (*open)(struct bladerf **device,  struct bladerf_devinfo *info);

    /* Closing of the device and freeing of the data */
    int (*close)(struct bladerf *dev);

    /* FPGA Loading and checking */
    int (*load_fpga)(struct bladerf *dev, uint8_t *image, size_t image_size);
    int (*is_fpga_configured)(struct bladerf *dev);

    /* Flash FX3 firmware */
    int (*recover)(struct bladerf_devinfo *info,
                    const char *fname);
    int (*flash_firmware)(struct bladerf *dev, uint8_t *image, size_t image_size);
    int (*erase_flash)(struct bladerf *dev, int page_offset,
                            int n_bytes);
    int (*read_flash)(struct bladerf *dev, int page_offset,
                            uint8_t *ptr, size_t n_bytes);
    int (*write_flash)(struct bladerf *dev, int page_offset,
                            uint8_t *data, size_t data_size);
    int (*device_reset)(struct bladerf *dev);
    int (*jump_to_bootloader)(struct bladerf *dev);

    /* Platform information */
    int (*get_cal)(struct bladerf *dev, char *cal);
    int (*get_otp)(struct bladerf *dev, char *otp);
    int (*get_fw_version)(struct bladerf *dev, unsigned int *maj, unsigned int *min);
    int (*get_fpga_version)(struct bladerf *dev, unsigned int *maj, unsigned int *min);
    int (*get_device_speed)(struct bladerf *dev, int *speed);

    /* Configuration GPIO accessors */
    int (*config_gpio_write)(struct bladerf *dev, uint32_t val);
    int (*config_gpio_read)(struct bladerf *dev, uint32_t *val);

    /* Si5338 accessors */
    int (*si5338_write)(struct bladerf *dev, uint8_t addr, uint8_t data);
    int (*si5338_read)(struct bladerf *dev, uint8_t addr, uint8_t *data);

    /* LMS6002D accessors */
    int (*lms_write)(struct bladerf *dev, uint8_t addr, uint8_t data);
    int (*lms_read)(struct bladerf *dev, uint8_t addr, uint8_t *data);

    /* VCTCXO accessor */
    int (*dac_write)(struct bladerf *dev, uint16_t value);

    /* Sample stream */
    int (*rx)(struct bladerf *dev, bladerf_format format, void *samples, int n, struct bladerf_metadata *metadata);
    int (*tx)(struct bladerf *dev, bladerf_format format, void *samples, int n, struct bladerf_metadata *metadata);

    int (*init_stream)(struct bladerf_stream *stream);
    int (*stream)(struct bladerf_stream *stream, bladerf_module module);
    void (*deinit_stream)(struct bladerf_stream *stream);

    /* Gather statistics */
    int (*stats)(struct bladerf *dev, struct bladerf_stats *stats);
};

#define FW_LEGACY_ALT_SETTING_MAJOR 1
#define FW_LEGACY_ALT_SETTING_MINOR 1
#define LEGACY_ALT_SETTING  1

struct bladerf {

    struct bladerf_devinfo ident;  /* Identifying information */

    uint16_t dac_trim;
    bladerf_fpga_size fpga_size;

    unsigned int fw_major, fw_minor;
    int legacy;

    int speed;      /* The device's USB speed, 0 is HS, 1 is SS */
    struct bladerf_stats stats;

    /* Last error encountered */
    struct bladerf_error error;

    /* Backend's private data  */
    void *backend;

    /* Driver-sppecific implementations */
    const struct bladerf_fn *fn;
};

/**
 * Initialize device registers - required after power-up, but safe
 * to call multiple times after power-up (e.g., multiple close and reopens)
 */
int bladerf_init_device(struct bladerf *dev);

/**
 *
 */
size_t bytes_to_c16_samples(size_t n_bytes);
size_t c16_samples_to_bytes(size_t n_samples);


/**
 * Set an error and type
 */
void bladerf_set_error(struct bladerf_error *error,
                        bladerf_error type, int val);

/**
 * Fetch an error and type
 */
void bladerf_get_error(struct bladerf_error *error,
                        bladerf_error *type, int *val);

/**
 * Read data from one-time-programmabe (OTP) section of flash
 *
 * @param[in]   dev         Device handle
 * @param[in]   field       OTP field
 * @param[out]  data        Populated with retrieved data
 * @param[in]   data_size   Size of the data to read
 *
 * 0 on success, BLADERF_ERR_* on failure
 */
int bladerf_get_otp_field(struct bladerf *device, char *field,
                            char *data, size_t data_size);

/**
 * Read data from calibration ("cal") section of flash
 *
 * @param[in]   dev         Device handle
 * @param[in]   field       Cal field
 * @param[out]  data        Populated with retrieved data
 * @param[in]   data_size   Size of the data to read
 *
 * 0 on success, BLADERF_ERR_* on failure
 */
int bladerf_get_otp_field(struct bladerf *device, char *field,
                            char *data, size_t data_size);

/**
 * Retrieve the device serial from flash and store it in the provided buffer.
 *
 * @pre The provided buffer is BLADERF_SERIAL_LENGTH in size
 *
 * @param[inout]   dev      Device handle. On success, serial field is updated
 *
 * 0 on success, BLADERF_ERR_* on failure
 */
int bladerf_read_serial(struct bladerf *device, char *serial_buf);

/**
 * Retrieve VCTCXO calibration value from flash and cache it in the
 * provided device structure
 *
 * @param[inout]   dev      Device handle. On success, trim field is updated
 *
 * 0 on success, BLADERF_ERR_* on failure
 */
int bladerf_get_and_cache_vctcxo_trim(struct bladerf *device);

/**
 * Retrieve FPGA size variant from flash and cache it in the provided
 * device structure
 *
 * @param[inout]   dev      Device handle.
 *                          On success, fpga_size field  is updated
 *
 * 0 on success, BLADERF_ERR_* on failure
 */
int bladerf_get_and_cache_fpga_size(struct bladerf *device);

#endif

