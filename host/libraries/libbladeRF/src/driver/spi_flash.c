/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2013 Daniel Gröber <dxld AT darkboxed DOT org>
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

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <libbladeRF.h>

#include "rel_assert.h"
#include "log.h"

#include "spi_flash.h"
#include "board/board.h"

static inline int check_eb_access(struct bladerf *dev,
                                  uint32_t erase_block, uint32_t count)
{
    if (erase_block >= dev->flash_arch->num_ebs) {
        log_debug("Invalid erase block: %u\n", erase_block);
        return BLADERF_ERR_INVAL;
    } else if (count > dev->flash_arch->num_ebs) {
        log_debug("Invalid number of erase blocks: %u\n", count);
        return BLADERF_ERR_INVAL;
    } else if ((erase_block + count) > dev->flash_arch->num_ebs) {
        log_debug("Requested operation extends past end of flash: "
                  "eb=%u, count=%u\n", erase_block, count);
        return BLADERF_ERR_INVAL;
    } else {
        return 0;
    }
}

static inline int check_page_access(struct bladerf *dev,
                                    uint32_t page, uint32_t count)
{
    if (page >= dev->flash_arch->num_pages) {
        log_debug("Invalid page: %u\n", page);
        return BLADERF_ERR_INVAL;
    } else if (count > dev->flash_arch->num_pages) {
        log_debug("Invalid number of pages: %u\n", count);
        return BLADERF_ERR_INVAL;
    } else if ((page + count) > dev->flash_arch->num_pages) {
        log_debug("Requested operation extends past end of flash: "
                  "page=%u, count=%u\n", page, count);
        return BLADERF_ERR_INVAL;
    } else {
        return 0;
    }
}

int spi_flash_erase(struct bladerf *dev, uint32_t erase_block, uint32_t count)
{
    int status = check_eb_access(dev, erase_block, count);

    if (status == 0) {
        status = dev->backend->erase_flash_blocks(dev, erase_block, count);
    }

    return status;
}

int spi_flash_read(struct bladerf *dev, uint8_t *buf,
                   uint32_t page, uint32_t count)
{
    int status = check_page_access(dev, page, count);

    if (status == 0) {
        status = dev->backend->read_flash_pages(dev, buf, page, count);
    }

    return status;;
}

int spi_flash_write(struct bladerf *dev, const uint8_t *buf,
                    uint32_t page, uint32_t count)
{
    int status = check_page_access(dev, page, count);

    if (status == 0) {
        status = dev->backend->write_flash_pages(dev, buf, page, count);
    }

    return status;
}

int spi_flash_verify(struct bladerf *dev, uint8_t *readback_buf,
                     const uint8_t *expected_buf, uint32_t page,
                     uint32_t count)
{
    int status = 0;
    size_t i;
    const size_t len = count * dev->flash_arch->psize_bytes;

    log_info("Verifying %u pages, starting at page %u\n", count, page);
    status = spi_flash_read(dev, readback_buf, page, count);

    if (status < 0) {
        log_debug("Failed to read from flash: %s\n", bladerf_strerror(status));
        return status;
    }

    for (i = 0; i < len; i++) {
        if (expected_buf[i] != readback_buf[i]) {
            status = BLADERF_ERR_UNEXPECTED;
            log_info("Flash verification failed at byte %llu. "
                     "Read %02x, expected %02x\n",
                     i, readback_buf[i], expected_buf[i]);
            break;
        }
    }

    return status;
}

