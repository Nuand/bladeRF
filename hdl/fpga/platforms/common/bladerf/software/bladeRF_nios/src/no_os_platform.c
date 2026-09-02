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
#include <stdlib.h>
#include <unistd.h>

#include "devices.h"
#include "no_os_error.h"
#include "no_os_gpio.h"
#include "no_os_spi.h"

void *no_os_malloc(size_t size)
{
    return malloc(size);
}

void *no_os_calloc(size_t nmemb, size_t size)
{
    return calloc(nmemb, size);
}

void no_os_free(void *ptr)
{
    free(ptr);
}

void no_os_udelay(uint32_t usecs)
{
    usleep(usecs);
}

void no_os_mdelay(uint32_t msecs)
{
    usleep(msecs * 1000);
}

int32_t no_os_spi_init(struct no_os_spi_desc **desc,
                       const struct no_os_spi_init_param *param)
{
    struct no_os_spi_desc *spi;

    if (desc == NULL || param == NULL) {
        return -EINVAL;
    }

    spi = no_os_calloc(1, sizeof(*spi));
    if (spi == NULL) {
        return -ENOMEM;
    }

    spi->device_id = param->device_id;
    spi->max_speed_hz = param->max_speed_hz;
    spi->chip_select = param->chip_select;
    spi->mode = param->mode;
    spi->bit_order = param->bit_order;
    spi->platform_ops = param->platform_ops;
    spi->extra = param->extra;

    *desc = spi;
    return 0;
}

int32_t no_os_spi_remove(struct no_os_spi_desc *desc)
{
    no_os_free(desc);

    return 0;
}

int32_t no_os_spi_write_and_read(struct no_os_spi_desc *desc,
                                 uint8_t *data,
                                 uint16_t bytes_number)
{
    uint64_t spi_data = 0;
    uint16_t command;
    uint8_t count;
    bool write;
    size_t i;

    if (desc == NULL || data == NULL || bytes_number < 3) {
        return -EINVAL;
    }

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

        adi_spi_write(command, spi_data);
    } else {
        spi_data = adi_spi_read(command);
        for (i = 0; i < count; ++i) {
            data[i + 2] = spi_data >> (56 - 8 * i);
        }
    }

    return 0;
}

int32_t no_os_gpio_get(struct no_os_gpio_desc **desc,
                       const struct no_os_gpio_init_param *param)
{
    struct no_os_gpio_desc *gpio;

    if (desc == NULL || param == NULL) {
        return -EINVAL;
    }

    gpio = no_os_calloc(1, sizeof(*gpio));
    if (gpio == NULL) {
        return -ENOMEM;
    }

    gpio->port = param->port;
    gpio->number = param->number;
    gpio->pull = param->pull;
    gpio->platform_ops = param->platform_ops;
    gpio->extra = param->extra;

    *desc = gpio;
    return 0;
}

int32_t no_os_gpio_get_optional(struct no_os_gpio_desc **desc,
                                const struct no_os_gpio_init_param *param)
{
    if (desc == NULL) {
        return -EINVAL;
    }

    if (param == NULL || param->number < 0) {
        *desc = NULL;
        return 0;
    }

    return no_os_gpio_get(desc, param);
}

int32_t no_os_gpio_remove(struct no_os_gpio_desc *desc)
{
    no_os_free(desc);

    return 0;
}

int32_t no_os_gpio_set_value(struct no_os_gpio_desc *desc, uint8_t value)
{
    uint32_t rffe_control;

    if (desc == NULL) {
        return -EINVAL;
    }

    rffe_control = rffe_csr_read();
    if (value != 0) {
        rffe_control |= 1 << desc->number;
    } else {
        rffe_control &= ~(1 << desc->number);
    }
    rffe_csr_write(rffe_control);

    return 0;
}

int32_t no_os_gpio_direction_output(struct no_os_gpio_desc *desc,
                                    uint8_t value)
{
    return no_os_gpio_set_value(desc, value);
}

uint64_t no_os_do_div(uint64_t *n, uint64_t base)
{
    uint64_t remainder = *n % base;

    *n /= base;

    return remainder;
}

uint64_t no_os_div64_u64_rem(uint64_t dividend,
                             uint64_t divisor,
                             uint64_t *remainder)
{
    *remainder = dividend % divisor;

    return dividend / divisor;
}

uint64_t no_os_div_u64_rem(uint64_t dividend,
                           uint32_t divisor,
                           uint32_t *remainder)
{
    *remainder = dividend % divisor;

    return dividend / divisor;
}

int64_t no_os_div_s64_rem(int64_t dividend,
                          int32_t divisor,
                          int32_t *remainder)
{
    *remainder = dividend % divisor;

    return dividend / divisor;
}

int32_t no_os_axi_io_read(uint32_t base, uint32_t offset, uint32_t *data)
{
    if (data == NULL) {
        return -EINVAL;
    }

    *data = adi_axi_read((uint16_t)(base + offset));
    return 0;
}

int32_t no_os_axi_io_write(uint32_t base, uint32_t offset, uint32_t data)
{
    adi_axi_write((uint16_t)(base + offset), data);

    return 0;
}
