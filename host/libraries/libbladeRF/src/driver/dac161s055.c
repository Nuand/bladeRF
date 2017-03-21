/*
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

#include <libbladeRF.h>

#include "rel_assert.h"
#include "host_config.h"
#include "log.h"

#include "dac161s055.h"

int dac161s055_write(struct bladerf *dev, uint16_t value)
{
    int status;

    /* Ensure device is in write-through mode */
    status = dev->backend->vctcxo_dac_write(dev, 0x28, 0x0000);
    if (status < 0) {
        return status;
    }

    /* Write DAC value to channel 0 */
    status = dev->backend->vctcxo_dac_write(dev, 0x08, value);
    if (status < 0) {
        return status;
    }

    log_verbose("%s: Wrote 0x%04x\n", __FUNCTION__, value);

    return 0;
}

int dac161s055_read(struct bladerf *dev, uint16_t *value)
{
    int status;

    /* Read DAC value for channel 0 */
    status = dev->backend->vctcxo_dac_read(dev, 0x98, value);
    if (status < 0) {
        *value = 0;
        return status;
    }

    log_verbose("%s: Read 0x%04x\n", __FUNCTION__, *value);

    return 0;
}
