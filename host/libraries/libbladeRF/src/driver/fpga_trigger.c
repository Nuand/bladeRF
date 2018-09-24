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

#include <libbladeRF.h>

#include "log.h"

#include "fpga_trigger.h"

static bool is_valid_signal(bladerf_trigger_signal signal)
{
    switch (signal) {
        case BLADERF_TRIGGER_J71_4:
        case BLADERF_TRIGGER_J51_1:
        case BLADERF_TRIGGER_MINI_EXP_1:

        case BLADERF_TRIGGER_USER_0:
        case BLADERF_TRIGGER_USER_1:
        case BLADERF_TRIGGER_USER_2:
        case BLADERF_TRIGGER_USER_3:
        case BLADERF_TRIGGER_USER_4:
        case BLADERF_TRIGGER_USER_5:
        case BLADERF_TRIGGER_USER_6:
        case BLADERF_TRIGGER_USER_7:
            return true;

        default:
            log_debug("Invalid trigger signal: %d\n", signal);
            return false;
    }
}

int fpga_trigger_read(struct bladerf *dev, bladerf_channel ch,
                      bladerf_trigger_signal signal, uint8_t *regval)
{
    if (ch != BLADERF_CHANNEL_RX(0) && ch != BLADERF_CHANNEL_TX(0))
        return BLADERF_ERR_INVAL;

    if (!is_valid_signal(signal))
        return BLADERF_ERR_INVAL;

    return dev->backend->read_trigger(dev, ch, signal, regval);
}

int fpga_trigger_write(struct bladerf *dev, bladerf_channel ch,
                       bladerf_trigger_signal signal, uint8_t regval)
{
    if (ch != BLADERF_CHANNEL_RX(0) && ch != BLADERF_CHANNEL_TX(0))
        return BLADERF_ERR_INVAL;

    if (!is_valid_signal(signal))
        return BLADERF_ERR_INVAL;

    return dev->backend->write_trigger(dev, ch, signal, regval);
}

int fpga_trigger_init(struct bladerf *dev, bladerf_channel ch,
                      bladerf_trigger_signal signal,
                 struct bladerf_trigger *trigger)

{
    int status;
    uint8_t regval;

    trigger->options = 0;

    status = fpga_trigger_read(dev, ch, signal, &regval);
    if (status != 0) {
        trigger->channel = BLADERF_CHANNEL_INVALID;
        trigger->role   = BLADERF_TRIGGER_ROLE_INVALID;
        trigger->signal = BLADERF_TRIGGER_INVALID;
        return status;
    }

    if ((regval & BLADERF_TRIGGER_REG_MASTER) != 0) {
        trigger->role = BLADERF_TRIGGER_ROLE_MASTER;
    } else {
        trigger->role = BLADERF_TRIGGER_ROLE_SLAVE;
    }

    trigger->channel = ch;
    trigger->signal = signal;

    return 0;
}

int fpga_trigger_arm(struct bladerf *dev,
                     const struct bladerf_trigger *trigger, bool arm)
{
    int status;
    uint8_t regval;

    status = fpga_trigger_read(dev, trigger->channel, trigger->signal, &regval);
    if (status != 0) {
        return status;
    }

    /* Reset any previous fire request */
    regval &= ~BLADERF_TRIGGER_REG_FIRE;

    if (arm) {
        regval |= BLADERF_TRIGGER_REG_ARM;
    } else {
        regval &= ~BLADERF_TRIGGER_REG_ARM;
    }

    switch (trigger->role) {
        case BLADERF_TRIGGER_ROLE_MASTER:
            regval |= BLADERF_TRIGGER_REG_MASTER;
            break;

        case BLADERF_TRIGGER_ROLE_SLAVE:
            regval &= ~BLADERF_TRIGGER_REG_MASTER;
            break;

        case BLADERF_TRIGGER_ROLE_DISABLED:
            regval = 0;
            break;

        default:
            log_debug("Invalid trigger role: %d\n", trigger->role);
            return BLADERF_ERR_INVAL;
    }

    status = fpga_trigger_write(dev, trigger->channel, trigger->signal, regval);

    return status;
}

int fpga_trigger_fire(struct bladerf *dev,
                      const struct bladerf_trigger *trigger)
{
    int status;
    uint8_t regval;

    status = fpga_trigger_read(dev, trigger->channel, trigger->signal, &regval);
    if (status != 0) {
        return status;
    }

    regval |= BLADERF_TRIGGER_REG_FIRE;
    status = fpga_trigger_write(dev, trigger->channel, trigger->signal, regval);

    return status;
}

int fpga_trigger_state(struct bladerf *dev, const struct bladerf_trigger *trigger,
                       bool *is_armed, bool *fired, bool *fire_requested)
{
    int status;
    uint8_t regval;

    status = fpga_trigger_read(dev, trigger->channel, trigger->signal, &regval);
    if (status != 0) {
        *fired = false;
        return status;
    }

    if (is_armed != NULL) {
        *is_armed = (regval & BLADERF_TRIGGER_REG_ARM) != 0;
    }

    if (fired != NULL) {
        /* Signal is active-low */
        *fired = (regval & BLADERF_TRIGGER_REG_LINE) == 0;
    }

    if (fire_requested != NULL) {
        if (trigger->role == BLADERF_TRIGGER_ROLE_MASTER) {
            *fire_requested = (regval & BLADERF_TRIGGER_REG_FIRE) != 0;
        } else {
            *fire_requested = false;
        }
    }

    return status;
}

