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
#include "fx3_fw.h"
#include "logger_entry.h"

#define BACKEND_STR_ANY    "*"
#define BACKEND_STR_LIBUSB "libusb"
#define BACKEND_STR_LINUX  "linux"
#define BACKEND_STR_CYPRESS "cypress"

/**
 * Specifies what to probe for
 */
typedef enum {
    BACKEND_PROBE_BLADERF,
    BACKEND_PROBE_FX3_BOOTLOADER,
} backend_probe_target;

/**
 * Backend-specific function table
 *
 * The backend is responsible for making calls to
 * capabilities_init_pre_fpga_load() and capabilities_init_post_fpga_load() when
 * opening a device and after loading an FPGA, or detecting an FPGA is already
 * loaded.
 *
 */
struct backend_fns {

    /* Returns true if a backend supports the specified backend type,
     * and false otherwise */
    bool (*matches)(bladerf_backend backend);

    /* Backends probe for devices and append entries to this list using
     * bladerf_devinfo_list_append() */
    int (*probe)(backend_probe_target probe_target,
                 struct bladerf_devinfo_list *info_list);

    /* Opening device based upon specified device info.
     *
     * The backend open implementation must call
     * capabilities_init_pre_fpga_load(), as it may need this information while
     * opening the device, and will definitely be needed by core code after
     * the handle is put to use.
     */
    int (*open)(struct bladerf *device, struct bladerf_devinfo *info);

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

    /* Configuration GPIO (NIOS II <-> logic) accessors */
    int (*config_gpio_write)(struct bladerf *dev, uint32_t val);
    int (*config_gpio_read)(struct bladerf *dev, uint32_t *val);

    /* Expansion GPIO accessors */
    int (*expansion_gpio_write)(struct bladerf *dev,
                                uint32_t mask, uint32_t val);

    int (*expansion_gpio_read)(struct bladerf *dev, uint32_t *val);

    int (*expansion_gpio_dir_write)(struct bladerf *dev,
                                    uint32_t mask, uint32_t outputs);

    int (*expansion_gpio_dir_read)(struct bladerf *dev, uint32_t *outputs);

    /* IQ Correction Settings */
    int (*set_iq_gain_correction)(struct bladerf *dev, bladerf_module module,
                                  int16_t value);

    int (*set_iq_phase_correction)(struct bladerf *dev, bladerf_module module,
                                   int16_t value);

    int (*get_iq_gain_correction)(struct bladerf *dev, bladerf_module module,
                                  int16_t *value);

    int (*get_iq_phase_correction)(struct bladerf *dev, bladerf_module module,
                                   int16_t *value);

    /* Get current timestamp counter values */
    int (*get_timestamp)(struct bladerf *dev, bladerf_module mod, uint64_t *value);

    /* Si5338 accessors */
    int (*si5338_write)(struct bladerf *dev, uint8_t addr, uint8_t data);
    int (*si5338_read)(struct bladerf *dev, uint8_t addr, uint8_t *data);

    /* LMS6002D accessors */
    int (*lms_write)(struct bladerf *dev, uint8_t addr, uint8_t data);
    int (*lms_read)(struct bladerf *dev, uint8_t addr, uint8_t *data);

    /* VCTCXO accessors */
    int (*vctcxo_dac_write)(struct bladerf *dev, uint16_t value);
    int (*vctcxo_dac_read)(struct bladerf *dev, uint16_t *value);

    int (*set_vctcxo_tamer_mode)(struct bladerf *dev,
                                 bladerf_vctcxo_tamer_mode mode);

    int (*get_vctcxo_tamer_mode)(struct bladerf *dev,
                                 bladerf_vctcxo_tamer_mode *mode);

    /* Expansion board SPI */
    int (*xb_spi)(struct bladerf *dev, uint32_t value);

    /* Configure firmware loopback */
    int (*set_firmware_loopback)(struct bladerf *dev, bool enable);
    int (*get_firmware_loopback)(struct bladerf *dev, bool *is_enabled);

    /* Sample stream */
    int (*enable_module)(struct bladerf *dev, bladerf_module m, bool enable);

    int (*init_stream)(struct bladerf_stream *stream, size_t num_transfers);
    int (*stream)(struct bladerf_stream *stream, bladerf_module module);
    int (*submit_stream_buffer)(struct bladerf_stream *stream, void *buffer,
                                unsigned int timeout_ms, bool nonblock);
    void (*deinit_stream)(struct bladerf_stream *stream);

    /* Schedule a frequency retune operation */
    int (*retune)(struct bladerf *dev, bladerf_module module,
                  uint64_t timestamp, uint16_t nint, uint32_t nfrac,
                  uint8_t freqsel, uint8_t vcocap, bool low_band,
                  bool quick_tune);

    /* Load firmware from FX3 bootloader */
    int (*load_fw_from_bootloader)(bladerf_backend backend,
                                   uint8_t bus, uint8_t addr,
                                   struct fx3_firmware *fw);

    /* Read a log entry from the FX3 firmware */
    int (*read_fw_log)(struct bladerf *dev, logger_entry *e);
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
 * @param[in]   probe_target    Device type to probe for
 * @param[out]  devinfo_items   Device info for identified devices
 * @param[out]  num_items       Number of items in the devinfo list.
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int backend_probe(backend_probe_target probe_target,
                  struct bladerf_devinfo **devinfo_items, size_t *num_items);

/**
 * Search for bootloader via provided specification, download firmware,
 * and boot it.
 *
 * @param   backend     Backend to use for this operation
 * @param   bus         USB bus the device is connected to
 * @param   addr        USB addr associated with the device
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int backend_load_fw_from_bootloader(bladerf_backend backend,
                                    uint8_t bus, uint8_t addr,
                                    struct fx3_firmware *fw);


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

