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
 * Copyright (C) 2013-2015 Nuand LLC
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
#include "rel_assert.h"

/* 1 TX, 1 RX */
#define NUM_MODULES 2

/* For >= 1.5 GHz uses the high band should be used. Otherwise, the low
 * band should be selected */
#define BLADERF_BAND_HIGH (1500000000)

#define CONFIG_GPIO_WRITE(dev, val) config_gpio_write(dev, val)
#define CONFIG_GPIO_READ(dev, val)  dev->fn->config_gpio_read(dev, val)

#define VCTCXO_DAC_WRITE(dev, val) dev->fn->vctcxo_dac_write(dev, val)
#define VCTCXO_DAC_READ(dev, val_ptr) dev->fn->vctcxo_dac_read(dev, val_ptr)

/* Forward declaration for the function table */
struct bladerf;

#define FW_LEGACY_ALT_SETTING_MAJOR 1
#define FW_LEGACY_ALT_SETTING_MINOR 1
#define LEGACY_ALT_SETTING  1

#define FW_LEGACY_CONFIG_IF_MAJOR   1
#define FW_LEGACY_CONFIG_IF_MINOR   4
#define LEGACY_CONFIG_IF    2

#define BLADERF_VERSION_STR_MAX 32

#define BLADERF_HAS_CAL_(dev, name)  (dev->cal.name != NULL)
#define BLADERF_HAS_RX_DC_CAL(dev)   (BLADERF_HAS_CAL_(dev, dc_rx))
#define BLADERF_HAS_TX_DC_CAL(dev)   (BLADERF_HAS_CAL_(dev, dc_tx))

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

    /* Bitmask of capabilities determined by version numbers */
    uint64_t capabilities;

    /* Which, if any XB do we have attached? */
    bladerf_xb xb;

    bladerf_dev_speed usb_speed;
    size_t msg_size; /* Fundamental "chunk" size of the data the FPGA sends to
                      * the host, in BYTES */

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

    /* Track filterbank selection for RX and TX auto-selection */
    bladerf_xb200_filter auto_filter[NUM_MODULES];

    /* Format currently being used with a module, or -1 if module is not used */
    bladerf_format module_format[NUM_MODULES];

    /* Which mode of operation we use for tuning */
    bladerf_tuning_mode tuning_mode;
};

/*
 * Convert bytes to SC16Q11 samples
 */
static inline size_t bytes_to_sc16q11(size_t n_bytes)
{
    const size_t sample_size = 2 * sizeof(int16_t);
    assert((n_bytes % sample_size) == 0);
    return n_bytes / sample_size;
}

/*
 * Convert SC16Q11 samples to bytes
 */
static inline size_t sc16q11_to_bytes(size_t n_samples)
{
    const size_t sample_size = 2 * sizeof(int16_t);
    assert(n_samples <= (SIZE_MAX / sample_size));
    return n_samples * sample_size;
}

/* Covert samples to bytes based upon the provided format */
static inline size_t samples_to_bytes(bladerf_format format, size_t n)
{
    switch (format) {
        case BLADERF_FORMAT_SC16_Q11:
        case BLADERF_FORMAT_SC16_Q11_META:
            return sc16q11_to_bytes(n);

        default:
            assert(!"Invalid format");
            return 0;
    }
}

/* Convert bytes to samples based upon the provided format */
static inline size_t bytes_to_samples(bladerf_format format, size_t n)
{
    switch (format) {
        case BLADERF_FORMAT_SC16_Q11:
        case BLADERF_FORMAT_SC16_Q11_META:
            return bytes_to_sc16q11(n);

        default:
            assert(!"Invalid format");
            return 0;
    }
}

/**
 * Initialize device registers - required after power-up, but safe
 * to call multiple times after power-up (e.g., multiple close and reopens)
 */
int init_device(struct bladerf *dev);

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
 * Write a to the FPGA configuration GPIOs.
 *
 * @param   dev     Device handle
 * @param   val     Value to write
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int config_gpio_write(struct bladerf *dev, uint32_t val);

/**
 * Perform the neccessary device configuration for the specified format
 * (e.g., enabling/disabling timestamp support), first checking that the
 * requested format would not conflict with the other module.
 *
 * @param   dev     Device handle
 * @param   module  Module that is currently being configured
 * @param   format  Format the module is being configured for
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int perform_format_config(struct bladerf *dev, bladerf_module module,
                          bladerf_format format);

/**
 * Deconfigure and update any state pertaining what a format that a module is no
 * longer using
 *
 * @param   dev     Device handle
 * @param   module  Module that is currently being deconfigured
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int perform_format_deconfig(struct bladerf *dev, bladerf_module module);

/**
 * Validate module selection
 *
 * @param   m       Module supplied by API user.
 *
 * @return 0 for valid enumeration value, BLADERF_ERR_INVAL otherwise.
 */
int check_module(bladerf_module m);

/**
 * Validate XB-200 filter selection
 *
 * @param   f       Filter supplied by API user.
 *
 * @return 0 for a valid enumeration value, BLADERF_ERR_INVAL otherwise.
 */
int check_xb200_filter(bladerf_xb200_filter f);

/**
 * Validate XB-200 path selection
 *
 * @param   p       Path supplied by API user.
 *
 * @return 0 for a valid enumeration value, BLADERF_ERR_INVAL otherwise.
 */
int check_xb200_path(bladerf_xb200_path p);

#endif
