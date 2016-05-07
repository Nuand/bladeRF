/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2016 Nuand LLC
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

#ifndef BLADERF_TRIGGER_H_
#define BLADERF_TRIGGER_H_

#include "bladerf_priv.h"

/**
 * Read trigger control register
 *
 * @param   dev             Device handle
 * @param   signal          Trigger signal control register to read from
 * @param   val             Pointer to variable that register is read into
 *                          See the BLADERF_TRIGGER_REG_* macros for the meaning
 *                          of each bit.
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int trigger_read(struct bladerf *dev, bladerf_module module,
                 bladerf_trigger_signal trigger, uint8_t *val);

/**
 * Write trigger control register
 *
 * @param   dev             Device handle
 * @param   signal          Trigger signal to configure
 * @param   val             Data to write into the trigger control register.
 *                          See the BLADERF_TRIGGER_REG_* macros for options.
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int trigger_write(struct bladerf *dev, bladerf_module module,
                  bladerf_trigger_signal trigger, uint8_t val);


/**
 * Initialize a bladerf_trigger structure based upon the current state
 * of a module's trigger control register.
 *
 * @param[in]   dev         Device to query
 * @param[in]   module      Module to query
 * @param[in]   signal      Trigger signal to query
 * @param[out]  trigger     Updated to describe trigger
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */

int trigger_init(struct bladerf *dev, bladerf_module module,
                 bladerf_trigger_signal signal,
                 struct bladerf_trigger *trigger);

/**
 * Arm or re-arm the specified trigger.
 *
 * @param[in]   dev     Device handle
 * @param[in]   trigger Description of trigger to arm
 * @param[in]   arm     If true, the specified trigger will be armed. Setting
 *                      this to false will disarm the trigger specified in
 *                      `config`.
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int trigger_arm(struct bladerf *dev,
                const struct bladerf_trigger *trigger, bool arm);

/**
 * Fire a trigger event.
 *
 * Calling this functiona with a trigger whose role is anything other than
 * ::BLADERF_TRIGGER_REG_MASTER will yield a BLADERF_ERR_INVAL return value.
 *
 * @param[in]   dev         Device handle
 * @param[in]   trigger     Trigger to assert
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int trigger_fire(struct bladerf *dev,
                 const struct bladerf_trigger *trigger);

/**
 * Query the fire request status of a master trigger
 *
 * @param[in]   dev             Device handle
 *
 * @param[in]   trigger         Trigger to query
 *
 * @param[out]  is_armed        Set to true if the trigger is armed, and false
 *                              otherwise. May be NULL.
 *
 * @param[out]  has_fired       Set to true if the trigger has fired, and false
 *                              otherwise. May be NULL.
 *
 * @param[out]  fire_requested  Only applicable to a trigger master.
 *                              Set to true if a fire request has been
 *                              previously submitted. May be NULL.
 *
 * @param[out]  resv1           Reserved parameter. Set to NULL.
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int trigger_state(struct bladerf *dev, const struct bladerf_trigger *trigger,
                  bool *is_armed, bool *has_fired, bool *fire_requested);

#endif
