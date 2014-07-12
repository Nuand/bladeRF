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
#include "bladeRF.h"
#include <pthread.h>
#include "host_config.h"

#if BLADERF_OS_WINDOWS || BLADERF_OS_OSX
#include "clock_gettime.h"
#else
#include <time.h>
#endif

#include "thread.h"
#include "minmax.h"
#include "conversions.h"
#include "devinfo.h"
#include "flash.h"
#include "backend/backend.h"

/* 1 TX, 1 RX */
#define NUM_MODULES 2

/* For >= 1.5 GHz uses the high band should be used. Otherwise, the low
 * band should be selected */
#define BLADERF_BAND_HIGH (1500000000)

#define CONFIG_GPIO_WRITE(dev, val) dev->fn->config_gpio_write(dev, val)
#define CONFIG_GPIO_READ(dev, val)  dev->fn->config_gpio_read(dev, val)

#define CONFIG_GPIO_WRITE(dev, val) dev->fn->config_gpio_write(dev, val)
#define CONFIG_GPIO_READ(dev, val)  dev->fn->config_gpio_read(dev, val)

#define DAC_WRITE(dev, val) dev->fn->dac_write(dev, val)

/* Forward declaration for the function table */
struct bladerf;

#define FW_LEGACY_ALT_SETTING_MAJOR 1
#define FW_LEGACY_ALT_SETTING_MINOR 1
#define LEGACY_ALT_SETTING  1

#define FW_LEGACY_CONFIG_IF_MAJOR   1
#define FW_LEGACY_CONFIG_IF_MINOR   4
#define LEGACY_CONFIG_IF    2

#define BLADERF_VERSION_STR_MAX 32

struct calibrations {
    struct dc_cal_tbl *dc_rx;
    struct dc_cal_tbl *dc_tx;
};

struct bladerf {

    /* Control lock - use this to ensure atomic access to control and
     * configuration operations */
    MUTEX ctrl_lock;

    /* Ensure sync transfers occur atomically. If this is to be held in
     * conjunction with ctrl_lock, ctrl_lock should be acquired BEFORE
     * the relevant sync_lock[] */
    MUTEX sync_lock[NUM_MODULES];

    struct bladerf_devinfo ident;  /* Identifying information */

    uint16_t dac_trim;
    bladerf_fpga_size fpga_size;

    struct bladerf_version fpga_version;
    struct bladerf_version fw_version;
    int legacy;

    bladerf_dev_speed usb_speed;

    /* Backend's private data  */
    void *backend;

    /* Driver-sppecific implementations */
    const struct backend_fns *fn;

    /* Stream transfer timeouts for RX and TX */
    int transfer_timeout[NUM_MODULES];

    /* Synchronous interface handles */
    struct bladerf_sync *sync[NUM_MODULES];

    /* Calibration data */
    struct calibrations cal;

    /* Track filterbank selection for RX autoselection */
    bladerf_xb200_filter rx_filter;

    /* Track filterbank selection for TX autoselection */
    bladerf_xb200_filter tx_filter;
};

/**
 * Initialize device registers - required after power-up, but safe
 * to call multiple times after power-up (e.g., multiple close and reopens)
 */
int init_device(struct bladerf *dev);

/**
 * Configure the device for operation in the high or low band, based
 * upon the provided frequency
 *
 * @param   dev         Device handle
 * @param   module      Module to configure
 * @param   frequency   Desired frequency
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int select_band(struct bladerf *dev, bladerf_module module,
                unsigned int frequency);

/**
 * Tune to the specified frequency
 *
 * @param   dev         Device handle
 * @param   module      Module to configure
 * @param   frequency   Desired frequency
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int set_frequency(struct bladerf *dev, bladerf_module module,
                  unsigned int frequency);
/**
 * Get the current frequency that the specified module is tuned to
 *
 * @param[in]   dev         Device handle
 * @param[in]   module      Module to configure
 * @param[out]  frequency   Desired frequency
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int get_frequency(struct bladerf *dev, bladerf_module module,
                  unsigned int *frequency);

/*
 * Convert bytes to SC16Q11 samples
 */
size_t bytes_to_c16_samples(size_t n_bytes);

/*
 * Convert SC16A11 samples to bytes
 */
size_t c16_samples_to_bytes(size_t n_samples);

/**
 * Populate the provided timeval structure for the specified timeout
 *
 * @param[out]  t_abs       Absolute timeout structure to populate
 * @param[in]   timeout_ms  Desired timeout in ms.
 *
 * 0 on success, BLADERF_ERR_UNEXPECTED on failure
 */
int populate_abs_timeout(struct timespec *t_abs, unsigned int timeout_ms);

/**
 * Load a calibration table and apply its settings
 *
 * @param       dev         Device handle
 * @param       filename    Name (and path) of file to load
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int load_calibration_table(struct bladerf *dev, const char *filename);

/**
 * Set system gain for the specified module
 *
 * @param   dev     Device handle
 * @param   module  Module to configure
 * @param   gain    Desired gain
 */
int set_gain(struct bladerf *dev, bladerf_module module, int gain);

#endif
