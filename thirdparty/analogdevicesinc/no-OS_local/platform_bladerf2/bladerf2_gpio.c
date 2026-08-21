/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2026 Nuand LLC
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

/*
 * GPIO back end for the AD9361 driver, as a no-OS platform ops table.
 *
 * The AD9361 control lines the driver drives (resetb, sync, cal_sw1, cal_sw2)
 * are bits of the FPGA's RFFE control register on this board, not pins of a
 * GPIO controller. The register is read-modify-write, so a set is a read, a
 * bit change and a write.
 *
 * The older driver revision took a single struct gpio_device and a pin number;
 * the current one holds one struct no_os_gpio_desc per line and goes through
 * struct no_os_gpio_platform_ops. The bit number travels in desc->number and
 * the bladeRF device handle in desc->extra, so the mapping is unchanged - only
 * the shape of the call is.
 */

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include "board/board.h"

#include "no_os_gpio.h"

static int32_t bladerf2_gpio_get(struct no_os_gpio_desc **desc,
                                 const struct no_os_gpio_init_param *param)
{
    struct no_os_gpio_desc *d;

    if (NULL == desc || NULL == param) {
        return -EINVAL;
    }

    d = calloc(1, sizeof(*d));
    if (NULL == d) {
        return -ENOMEM;
    }

    d->port = param->port;
    d->number = param->number;
    d->pull = param->pull;
    d->platform_ops = param->platform_ops;
    d->extra = param->extra;

    *desc = d;

    return 0;
}

static int32_t bladerf2_gpio_get_optional(struct no_os_gpio_desc **desc,
                                          const struct no_os_gpio_init_param *param)
{
    /* An absent line is not an error: the driver treats a NULL descriptor as
     * "this board does not wire that signal". */
    if (NULL == param) {
        if (NULL != desc) {
            *desc = NULL;
        }
        return 0;
    }

    return bladerf2_gpio_get(desc, param);
}

static int32_t bladerf2_gpio_remove(struct no_os_gpio_desc *desc)
{
    free(desc);

    return 0;
}

static int32_t bladerf2_gpio_direction_output(struct no_os_gpio_desc *desc,
                                             uint8_t value)
{
    /* Every line reachable here is an output of the RFFE register; there is no
     * direction to program. The value still has to be applied. */
    if (NULL == desc) {
        return -EINVAL;
    }

    return no_os_gpio_set_value(desc, value);
}

static int32_t bladerf2_gpio_direction_input(struct no_os_gpio_desc *desc)
{
    (void)desc;

    /* No line the driver asks for is an input on this board. */
    return -ENOTSUP;
}

static int32_t bladerf2_gpio_get_direction(struct no_os_gpio_desc *desc,
                                           uint8_t *direction)
{
    if (NULL == desc || NULL == direction) {
        return -EINVAL;
    }

    *direction = NO_OS_GPIO_OUT;

    return 0;
}

static int32_t bladerf2_gpio_set_value(struct no_os_gpio_desc *desc,
                                       uint8_t value)
{
    struct bladerf *dev;
    uint32_t reg;
    int status;

    if (NULL == desc) {
        return -EINVAL;
    }

    dev = desc->extra;
    if (NULL == dev) {
        return -EINVAL;
    }

    status = dev->backend->rffe_control_read(dev, &reg);
    if (status < 0) {
        return -EIO;
    }

    if (value) {
        reg |= (1u << desc->number);
    } else {
        reg &= ~(1u << desc->number);
    }

    status = dev->backend->rffe_control_write(dev, reg);
    if (status < 0) {
        return -EIO;
    }

    return 0;
}

static int32_t bladerf2_gpio_get_value(struct no_os_gpio_desc *desc,
                                       uint8_t *value)
{
    struct bladerf *dev;
    uint32_t reg;
    int status;

    if (NULL == desc || NULL == value) {
        return -EINVAL;
    }

    dev = desc->extra;
    if (NULL == dev) {
        return -EINVAL;
    }

    status = dev->backend->rffe_control_read(dev, &reg);
    if (status < 0) {
        return -EIO;
    }

    *value = (reg & (1u << desc->number)) ? 1 : 0;

    return 0;
}

const struct no_os_gpio_platform_ops bladerf2_gpio_ops = {
    .gpio_ops_get = bladerf2_gpio_get,
    .gpio_ops_get_optional = bladerf2_gpio_get_optional,
    .gpio_ops_remove = bladerf2_gpio_remove,
    .gpio_ops_direction_input = bladerf2_gpio_direction_input,
    .gpio_ops_direction_output = bladerf2_gpio_direction_output,
    .gpio_ops_get_direction = bladerf2_gpio_get_direction,
    .gpio_ops_set_value = bladerf2_gpio_set_value,
    .gpio_ops_get_value = bladerf2_gpio_get_value,
};
