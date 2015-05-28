/**
 * @file tuning.h
 *
 * @brief Logic pertaining to device frequency tuning
 *
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014 Nuand LLC
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
#ifndef BLADERF_TUNING_H_
#define BLADERF_TUNING_H_

#include "bladerf_priv.h"
#include "lms.h"
#include "nios_pkt_retune.h"

/**
 * Configure the device for operation in the high or low band, based
 * upon the provided frequency
 *
 * @param   dev         Device handle
 * @param   module      Module to configure
 * @param   low_band    Select the low band (true), or high band (false)
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int tuning_select_band(struct bladerf *dev, bladerf_module module,
                       bool low_band);

/**
 * Tune to the specified frequency
 *
 * @param   dev         Device handle
 * @param   module      Module to configure
 * @param   frequency   Desired frequency
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int tuning_set_freq(struct bladerf *dev, bladerf_module module,
                    unsigned int frequency);

/**
 * Schedule a frequency retune to occur at specified sample timestamp value
 *
 * @param       dev         Device handle
 * @param       module      Which module to retune
 * @param       timestamp   Module's sample timestamp to perform the retune at.
 *                          If this value is in the past, the retune will
 *                          occur immediately.
 * @param       hint        Tuning hint value. Set to BLADERF_NO_RETUNE_HINT to
 *                          specify that this hint value should not be used.
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
static inline int tuning_schedule(struct bladerf *dev,
                                  bladerf_module module,
                                  uint64_t timestamp,
                                  struct lms_freq *f)
{
    return dev->fn->retune(dev, module, timestamp,
                           f->nint, f->nfrac, f->freqsel, f->vcocap,
                           (f->flags & LMS_FREQ_FLAGS_LOW_BAND) != 0,
                           (f->flags & LMS_FREQ_FLAGS_FORCE_VCOCAP) != 0);
}

/**
 * Clear scheduled retunes for the specified module
 *
 * @param   dev         Device handle
 * @param   module      Module to clear.
 */
static inline int tuning_cancel_scheduled(struct bladerf *dev,
                                          bladerf_module module)
{
    return dev->fn->retune(dev, module, NIOS_PKT_RETUNE_CLEAR_QUEUE,
                           0, 0, 0, 0, false, false);

}

/**
 * Get the current frequency that the specified module is tuned to
 *
 * @param[in]   dev         Device handle
 * @param[in]   module      Module to configure
 * @param[out]  frequency   Desired frequency
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int tuning_get_freq(struct bladerf *dev, bladerf_module module,
                    unsigned int *frequency);

/**
 * Get default tuning mode, which depends upon the FPGA version or the
 * BLADERF_DEFAULT_TUNING_MODE environment variable
 *
 * @param   dev     Device handle
 *
 * @return  Default mode
 */
bladerf_tuning_mode tuning_get_default_mode(struct bladerf *dev);

/**
 * Set tuning mode
 *
 * @param       dev         Device handle
 * @param       mode        Desired tuning mode. Note that the available modes
 *                          depends on the FPGA version.
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int tuning_set_mode(struct bladerf *dev, bladerf_tuning_mode mode);


#endif
