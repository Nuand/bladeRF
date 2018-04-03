/*
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

#include "xb100.h"

#include "log.h"

int xb100_attach(struct bladerf *dev)
{
    return 0;
}

void xb100_detach(struct bladerf *dev)
{
}

int xb100_enable(struct bladerf *dev, bool enable)
{
    int status = 0;

    const uint32_t mask =
        BLADERF_XB100_LED_D1        |
        BLADERF_XB100_LED_D2        |
        BLADERF_XB100_LED_D3        |
        BLADERF_XB100_LED_D4        |
        BLADERF_XB100_LED_D5        |
        BLADERF_XB100_LED_D6        |
        BLADERF_XB100_LED_D7        |
        BLADERF_XB100_LED_D8        |
        BLADERF_XB100_TLED_RED      |
        BLADERF_XB100_TLED_GREEN    |
        BLADERF_XB100_TLED_BLUE;

    const uint32_t outputs = mask;
    const uint32_t default_values = mask;

    if (enable) {
        status = dev->backend->expansion_gpio_dir_write(dev, mask, outputs);
        if (status == 0) {
            status = dev->backend->expansion_gpio_write(dev, mask, default_values);
        }
    }

    return status;
}

int xb100_init(struct bladerf *dev)
{
    return 0;
}

int xb100_gpio_read(struct bladerf *dev, uint32_t *val)
{
    return dev->backend->expansion_gpio_read(dev, val);
}

int xb100_gpio_write(struct bladerf *dev, uint32_t val)
{
    return dev->backend->expansion_gpio_write(dev, 0xffffffff, val);
}

int xb100_gpio_masked_write(struct bladerf *dev, uint32_t mask, uint32_t val)
{
    return dev->backend->expansion_gpio_write(dev, mask, val);
}

int xb100_gpio_dir_read(struct bladerf *dev, uint32_t *val)
{
    return dev->backend->expansion_gpio_dir_read(dev, val);
}

int xb100_gpio_dir_write(struct bladerf *dev, uint32_t val)
{
    return xb100_gpio_dir_masked_write(dev, 0xffffffff, val);
}

int xb100_gpio_dir_masked_write(struct bladerf *dev, uint32_t mask, uint32_t val)
{
    return dev->backend->expansion_gpio_dir_write(dev, mask, val);
}
