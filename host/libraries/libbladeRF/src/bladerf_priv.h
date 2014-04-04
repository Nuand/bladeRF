/**
 * @file bladerf_priv.h
 *
 * @brief "Private" defintions and functions.
 *
 * This file is not part of the API and may be changed at any time.
 * If you're interfacing with libbladeRF, DO NOT use this file.
 *
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2013 Nuand LLC
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef BLADERF_PRIV_H_
#define BLADERF_PRIV_H_

#include <limits.h>
#include <libbladeRF.h>
#include "host_config.h"

#if BLADERF_OS_WINDOWS || BLADERF_OS_OSX
#include "clock_gettime.h"
#else
#include <time.h>
#endif

#include "minmax.h"
#include "conversions.h"
#include "devinfo.h"
#include "flash.h"

/* 1 TX, 1 RX */
#define NUM_MODULES 2

/* For >= 1.5 GHz uses the high band should be used. Otherwise, the low
 * band should be selected */
#define BLADERF_BAND_HIGH (1500000000)

typedef enum {
    ETYPE_ERRNO,
    ETYPE_LIBBLADERF,
    ETYPE_BACKEND,
    ETYPE_OTHER = INT_MAX - 1
} bladerf_error_type;

struct bladerf_error {
    bladerf_error_type type;
    int value;
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
    int (*flash_firmware)(struct bladerf *dev, uint8_t *image, size_t image_size);

    /* Flash operations */
    int (*erase_flash)(struct bladerf *dev, uint32_t addr, uint32_t len);
    int (*read_flash) (struct bladerf *dev, uint32_t addr, uint8_t *buf,
                       uint32_t len);
    int (*write_flash)(struct bladerf *dev, uint32_t addr, uint8_t *buf,
                       uint32_t len);
    int (*device_reset)(struct bladerf *dev);

    int (*jump_to_bootloader)(struct bladerf *dev);

    /* Platform information */
    int (*get_cal)(struct bladerf *dev, char *cal);
    int (*get_otp)(struct bladerf *dev, char *otp);
    int (*get_device_speed)(struct bladerf *dev, bladerf_dev_speed *speed);

    /* Configuration GPIO accessors */
    int (*config_gpio_write)(struct bladerf *dev, uint32_t val);
    int (*config_gpio_read)(struct bladerf *dev, uint32_t *val);

    /* Expansion GPIO accessors */
    int (*expansion_gpio_write)(struct bladerf *dev, uint32_t val);
    int (*expansion_gpio_read)(struct bladerf *dev, uint32_t *val);
    int (*expansion_gpio_dir_write)(struct bladerf *dev, uint32_t val);
    int (*expansion_gpio_dir_read)(struct bladerf *dev, uint32_t *val);

    /* IQ Calibration Settings */
    int (*set_correction)(struct bladerf *dev, bladerf_module,
                          bladerf_correction corr, int16_t value);
    int (*get_correction)(struct bladerf *dev, bladerf_module module,
                          bladerf_correction corr, int16_t *value);

    /* Get current timestamp counter values */
    int (*get_timestamp)(struct bladerf *dev, bladerf_module mod, uint64_t *value);

    /* Si5338 accessors */
    int (*si5338_write)(struct bladerf *dev, uint8_t addr, uint8_t data);
    int (*si5338_read)(struct bladerf *dev, uint8_t addr, uint8_t *data);

    /* LMS6002D accessors */
    int (*lms_write)(struct bladerf *dev, uint8_t addr, uint8_t data);
    int (*lms_read)(struct bladerf *dev, uint8_t addr, uint8_t *data);

    /* VCTCXO accessor */
    int (*dac_write)(struct bladerf *dev, uint16_t value);

    /* Expansion board SPI */
    int (*xb_spi)(struct bladerf *dev, uint32_t value);

    /* Configure firmware loopback */
    int (*set_firmware_loopback)(struct bladerf *dev, bool enable);

    /* Sample stream */
    int (*enable_module)(struct bladerf *dev, bladerf_module m, bool enable);

    int (*init_stream)(struct bladerf_stream *stream, size_t num_transfers);
    int (*stream)(struct bladerf_stream *stream, bladerf_module module);
    int (*submit_stream_buffer)(struct bladerf_stream *stream, void *buffer,
                                unsigned int timeout_ms);
    void (*deinit_stream)(struct bladerf_stream *stream);
};

#define FW_LEGACY_ALT_SETTING_MAJOR 1
#define FW_LEGACY_ALT_SETTING_MINOR 1
#define LEGACY_ALT_SETTING  1

#define FW_LEGACY_CONFIG_IF_MAJOR   1
#define FW_LEGACY_CONFIG_IF_MINOR   4
#define LEGACY_CONFIG_IF    2

#define BLADERF_VERSION_STR_MAX 32

struct bladerf {

    struct bladerf_devinfo ident;  /* Identifying information */

    uint16_t dac_trim;
    bladerf_fpga_size fpga_size;

    struct bladerf_version fpga_version;
    struct bladerf_version fw_version;
    int legacy;

    bladerf_dev_speed usb_speed;

    /* Last error encountered */
    struct bladerf_error error;

    /* Backend's private data  */
    void *backend;

    /* Driver-sppecific implementations */
    const struct bladerf_fn *fn;

    /* Stream transfer timeouts for RX and TX */
    int transfer_timeout[NUM_MODULES];

    /* Synchronous interface handles */
    struct bladerf_sync *sync[NUM_MODULES];
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
                        bladerf_error_type type, int val);

/**
 * Fetch an error and type
 */
void bladerf_get_error(struct bladerf_error *error,
                        bladerf_error_type *type, int *val);

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

/**
 * Create data that can be read by extract_field()
 *
 * @param[in]       ptr     Pointer to data buffer that will contain encoded data
 * @param[in]       len     Length of data buffer that will contain encoded data
 * @param[inout]    idx     Pointer indicating next free byte inside of data
 *                          buffer that will contain encoded data
 * @param[in]       field   Key of value to be stored in encoded data buffer
 * @param[in]       val     Value to be stored in encoded data buffer
 *
 * 0 on success, BLADERF_ERR_* on failure
 */
int encode_field(char *ptr, int len, int *idx, const char *field,
                 const char *val);

/**
 * Add kv association to data region readable by extract_field()
 *
 * @param[in]    buf    Buffer to add field to
 * @param[in]    len    Length of `buf' in bytes
 * @param[in]    field  Key of value to be stored in encoded data buffer
 * @param[in]    val    Value associated with key `field'
 *
 * 0 on success, BLADERF_ERR_* on failure
 */
int add_field(char *buf, int len, const char *field, const char *val);

/**
 * Populate the provided timeval structure for the specified timeout
 *
 * @param[out]  t_abs       Absolute timeout structure to populate
 * @param[in]   timeout_ms  Desired timeout in ms.
 *
 * 0 on success, BLADERF_ERR_UNEXPECTED on failure
 */
int populate_abs_timeout(struct timespec *t_abs, unsigned int timeout_ms);

#endif
