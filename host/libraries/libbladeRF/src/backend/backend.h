/**
 * @file backend.h
 *
 * @brief This file defines the generic interface to bladeRF backends
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
#ifndef BACKEND_H__
#define BACKEND_H__

#include "bladerf_priv.h"

#define BACKEND_STR_ANY    "*"
#define BACKEND_STR_LIBUSB "libusb"
#define BACKEND_STR_LINUX  "linux"

/**
 * Backend-specific function table
 */
struct backend_fns {

    /* Returns true if a backend supports the specified backend type,
     * and false otherwise */
    bool (*matches)(bladerf_backend backend);

    /* Backends probe for devices and append entries to this list using
     * bladerf_devinfo_list_append() */
    int (*probe)(struct bladerf_devinfo_list *info_list);

    /* Opening device based upon specified device info */
    int (*open)(struct bladerf *device,  struct bladerf_devinfo *info);

    /* Closing of the device and freeing of the data */
    void (*close)(struct bladerf *dev);

    /* FPGA Loading and checking */
    int (*load_fpga)(struct bladerf *dev, uint8_t *image, size_t image_size);
    int (*is_fpga_configured)(struct bladerf *dev);


    /* Flash operations */

    /* Erase the specified number of erase blocks */
    int (*erase_flash_blocks)(struct bladerf *dev, uint32_t eb, uint16_t count);

    /* Read the specified number of pages */
    int (*read_flash_pages)(struct bladerf *dev,
                            uint8_t *buf, uint32_t page, uint32_t count);

    /* Write the specified number of pages */
    int (*write_flash_pages)(struct bladerf *dev, const uint8_t *buf,
                             uint32_t page, uint32_t count);

    /* Device startup and reset */
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

/**
 * Open the device using the backend specified in the provided
 * bladerf_devinfo structure.
 *
 * @param[in]   device  Device to fill in backend info for
 * @param[in]   info    Filled-in device info
 *
 * @return  0 on success, BLADERF_ERR_* code on failure
 */
int backend_open(struct bladerf *device, struct bladerf_devinfo *info);

/**
 * Probe for devices, filling in the provided devinfo list and size of
 * the list that gets populated
 *
 * @param[out]  devinfo_items
 * @param[out]  num_items
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int backend_probe(struct bladerf_devinfo **devinfo_items, size_t *num_items);

/**
 * Convert a backend enumeration value to a string
 *
 * @param   backend     Value to convert to a string
 *
 * @return  A backend string for the associated enumeration value. An invalid
 *          value will yield the "ANY" wildcard.
 */
const char * backend2str(bladerf_backend backend);

/**
 * Convert a string to a backend type value
 *
 * @param[in]   str         Backend type, as a string.
 * @param[out]  backend     Associated backend, on success
 *
 * @return 0 on success, BLADERF_ERR_INVAL on invalid type string
 */
int str2backend(const char *str, bladerf_backend *backend);


#endif

