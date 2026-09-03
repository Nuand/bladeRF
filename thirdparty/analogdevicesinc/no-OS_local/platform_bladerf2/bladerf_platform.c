/* This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (c) 2026 Nuand LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdbool.h>
#include <stdint.h>
#ifdef _WIN32
#include "host_config.h"
#else
#include <unistd.h>
#endif

#include "backend/backend.h"
#include "board/board.h"
#include "no_os_alloc.h"
#include "no_os_error.h"
#include "no_os_gpio.h"
#include "no_os_spi.h"
#include "platform.h"

static int32_t bladerf_spi_init(struct no_os_spi_desc **desc,
                                const struct no_os_spi_init_param *param)
{
    struct no_os_spi_desc *spi_desc;

    spi_desc = no_os_calloc(1, sizeof(*spi_desc));
    if (spi_desc == NULL) {
        return -ENOMEM;
    }

    spi_desc->device_id = param->device_id;
    spi_desc->max_speed_hz = param->max_speed_hz;
    spi_desc->mode = param->mode;
    spi_desc->chip_select = param->chip_select;
    spi_desc->platform_ops = param->platform_ops;
    spi_desc->extra = param->extra;

    *desc = spi_desc;
    return 0;
}

static int32_t bladerf_spi_write_and_read(struct no_os_spi_desc *desc,
                                          uint8_t *data,
                                          uint16_t bytes_number)
{
    struct bladerf *dev;
    uint64_t spi_data = 0;
    uint16_t command;
    uint8_t count;
    bool write;
    int status;
    size_t i;

    if (desc == NULL || desc->extra == NULL || data == NULL ||
        bytes_number < 3) {
        return -EINVAL;
    }

    dev = desc->extra;
    command = ((uint16_t)data[0] << 8) | data[1];
    count = ((command >> 12) & 0x7) + 1;
    write = (command & 0x8000) != 0;

    if (bytes_number != count + 2) {
        return -EINVAL;
    }

    if (write) {
        for (i = 0; i < count; ++i) {
            spi_data |= (uint64_t)data[i + 2] << (56 - 8 * i);
        }

        status = dev->backend->ad9361_spi_write(dev, command, spi_data);
    } else {
        status = dev->backend->ad9361_spi_read(dev, command, &spi_data);
        if (status == 0) {
            for (i = 0; i < count; ++i) {
                data[i + 2] = spi_data >> (56 - 8 * i);
            }
        }
    }

    if (status < 0) {
        return -EIO;
    }

    return 0;
}

static int32_t bladerf_spi_remove(struct no_os_spi_desc *desc)
{
    no_os_free(desc);

    return 0;
}

const struct no_os_spi_platform_ops bladerf_spi_ops = {
    .init = bladerf_spi_init,
    .write_and_read = bladerf_spi_write_and_read,
    .remove = bladerf_spi_remove,
};

static int32_t bladerf_gpio_get(struct no_os_gpio_desc **desc,
                                const struct no_os_gpio_init_param *param)
{
    struct no_os_gpio_desc *gpio_desc;

    gpio_desc = no_os_calloc(1, sizeof(*gpio_desc));
    if (gpio_desc == NULL) {
        return -ENOMEM;
    }

    gpio_desc->number = param->number;
    gpio_desc->port = param->port;
    gpio_desc->pull = param->pull;
    gpio_desc->platform_ops = param->platform_ops;
    gpio_desc->extra = param->extra;

    *desc = gpio_desc;
    return 0;
}

static int32_t bladerf_gpio_remove(struct no_os_gpio_desc *desc)
{
    no_os_free(desc);

    return 0;
}

static int32_t bladerf_gpio_set_value(struct no_os_gpio_desc *desc,
                                      uint8_t value)
{
    struct bladerf *dev;
    uint32_t rffe_control;
    int status;

    if (desc == NULL || desc->extra == NULL) {
        return -EINVAL;
    }

    if (desc->number != RFFE_CONTROL_RESET_N) {
        return 0;
    }

    dev = desc->extra;
    status = dev->backend->rffe_control_read(dev, &rffe_control);
    if (status < 0) {
        return -EIO;
    }

    if (value != 0) {
        rffe_control |= 1 << RFFE_CONTROL_RESET_N;
    } else {
        rffe_control &= ~(1 << RFFE_CONTROL_RESET_N);
    }

    status = dev->backend->rffe_control_write(dev, rffe_control);
    if (status < 0) {
        return -EIO;
    }

    return 0;
}

static int32_t bladerf_gpio_direction_output(struct no_os_gpio_desc *desc,
                                             uint8_t value)
{
    return bladerf_gpio_set_value(desc, value);
}

const struct no_os_gpio_platform_ops bladerf_gpio_ops = {
    .gpio_ops_get = bladerf_gpio_get,
    .gpio_ops_get_optional = NULL,
    .gpio_ops_remove = bladerf_gpio_remove,
    .gpio_ops_direction_input = NULL,
    .gpio_ops_direction_output = bladerf_gpio_direction_output,
    .gpio_ops_get_direction = NULL,
    .gpio_ops_set_value = bladerf_gpio_set_value,
    .gpio_ops_get_value = NULL,
};

void no_os_udelay(uint32_t usecs)
{
    usleep(usecs);
}

void no_os_mdelay(uint32_t msecs)
{
    usleep(msecs * 1000);
}

int32_t no_os_axi_io_read(void *ctx,
                          uint32_t base,
                          uint32_t offset,
                          uint32_t *data)
{
    struct bladerf *dev = ctx;
    int status;

    if (dev == NULL || data == NULL) {
        return -EINVAL;
    }

    status = dev->backend->adi_axi_read(dev, base + offset, data);
    if (status < 0) {
        return -EIO;
    }

    return 0;
}

int32_t no_os_axi_io_write(void *ctx,
                           uint32_t base,
                           uint32_t offset,
                           uint32_t data)
{
    struct bladerf *dev = ctx;
    int status;

    if (dev == NULL) {
        return -EINVAL;
    }

    status = dev->backend->adi_axi_write(dev, base + offset, data);
    if (status < 0) {
        return -EIO;
    }

    return 0;
}
