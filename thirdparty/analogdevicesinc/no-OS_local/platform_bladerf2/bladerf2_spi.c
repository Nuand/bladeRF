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
 * SPI back end for the AD9361 driver, as a no-OS platform ops table.
 *
 * The older driver revision this replaces had no such table: it called
 * spi_write_then_read() directly, and the bladeRF tree carried a patch that
 * substituted its own spi_read()/spi_write() inside the driver body behind
 * #ifdef NUAND_MODIFICATIONS. The current driver takes the transfer through
 * struct no_os_spi_platform_ops, so the substitution belongs here instead and
 * the driver source stays unmodified.
 *
 * Buffer layout produced by the driver (see ad9361_spi_read(),
 * ad9361_spi_write() and ad9361_spi_writem()):
 *
 *   buf[0] = cmd >> 8      cmd = AD_READ or AD_WRITE, | AD_CNT(n) | AD_ADDR(reg)
 *   buf[1] = cmd & 0xFF
 *   buf[2..] = payload, n bytes, read into or written from
 *
 * The bladeRF back end takes the 16-bit command and up to 8 payload bytes
 * packed into a 64-bit word, most significant byte first, so this file only
 * has to split the buffer and pack or unpack the payload.
 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "board/board.h"

#include "no_os_spi.h"

/* Payload bytes one transaction can carry. Mirrors MAX_MBYTE_SPI, which the
 * driver defines as 8 in ad9361.h; kept local so this file does not need the
 * driver's private header. */
#define BLADERF2_SPI_MAX_PAYLOAD 8

/* Command word bit that marks a write. ad9361.h defines AD_READ as (0 << 15)
 * and AD_WRITE as (1 << 15), so bit 15 is the direction. */
#define BLADERF2_SPI_CMD_WRITE (1u << 15)

static int32_t bladerf2_spi_init(struct no_os_spi_desc **desc,
                                 const struct no_os_spi_init_param *param)
{
    struct no_os_spi_desc *d;

    if (NULL == desc || NULL == param) {
        return -EINVAL;
    }

    d = calloc(1, sizeof(*d));
    if (NULL == d) {
        return -ENOMEM;
    }

    /* The bladeRF device handle travels in extra, set by the caller that
     * builds the init parameters. Everything else in the descriptor is
     * unused: the transfer goes through the device back end, not a bus
     * peripheral this file owns. */
    d->extra = param->extra;
    d->platform_ops = param->platform_ops;

    *desc = d;

    return 0;
}

static int32_t bladerf2_spi_remove(struct no_os_spi_desc *desc)
{
    free(desc);

    return 0;
}

static int32_t bladerf2_spi_write_and_read(struct no_os_spi_desc *desc,
                                           uint8_t *data,
                                           uint16_t bytes_number)
{
    struct bladerf *dev;
    uint16_t cmd;
    uint16_t payload;
    uint64_t word;
    uint16_t i;
    int status;

    if (NULL == desc || NULL == data) {
        return -EINVAL;
    }

    /* Two command bytes are mandatory; anything shorter is not a transfer
     * this driver produces. */
    if (bytes_number < 2) {
        return -EINVAL;
    }

    payload = (uint16_t)(bytes_number - 2);
    if (payload > BLADERF2_SPI_MAX_PAYLOAD) {
        return -EINVAL;
    }

    dev = desc->extra;
    if (NULL == dev) {
        return -EINVAL;
    }

    cmd = (uint16_t)((((uint16_t)data[0]) << 8) | data[1]);

    if (cmd & BLADERF2_SPI_CMD_WRITE) {
        /* Pack the payload most significant byte first, matching what the
         * back end expects to shift onto the bus. */
        word = 0;
        for (i = 0; i < payload; i++) {
            word |= ((uint64_t)data[2 + i]) << (8 * (7 - i));
        }

        status = dev->backend->ad9361_spi_write(dev, cmd, word);
        if (status < 0) {
            return -EIO;
        }

        return 0;
    }

    word = 0;
    status = dev->backend->ad9361_spi_read(dev, cmd, &word);
    if (status < 0) {
        return -EIO;
    }

    /* The read replaces the payload in place; the command bytes are left
     * alone, as the driver only looks past them. */
    for (i = 0; i < payload; i++) {
        data[2 + i] = (uint8_t)((word >> (8 * (7 - i))) & 0xff);
    }

    return 0;
}

const struct no_os_spi_platform_ops bladerf2_spi_ops = {
    .init = bladerf2_spi_init,
    .write_and_read = bladerf2_spi_write_and_read,
    .remove = bladerf2_spi_remove,
};
