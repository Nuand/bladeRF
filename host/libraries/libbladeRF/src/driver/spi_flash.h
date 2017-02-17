/**
 * @file flash.h
 *
 * @brief Flash conversion and alignment routines
 *
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
#include <assert.h>

#include <libbladeRF.h>

#ifndef SPI_FLASH_H_
#define SPI_FLASH_H_

/**
 * Erase regions of SPI flash
 *
 * @param   dev             Device handle
 * @param   erase_block     Erase block to start erasing at
 * @param   count           Number of blocks to erase.
 *
 * @return 0 on success, or BLADERF_ERR_INVAL on an invalid `erase_block` or
 *         `count` value, or a value from \ref RETCODES list on other failures
 */
int spi_flash_erase(struct bladerf *dev, uint32_t erase_block, uint32_t count);

/**
 * Read data from flash
 *
 * @param   dev     Device handle
 * @param   buf     Buffer to read data into. Must be
 *                  `count` * BLADERF_FLASH_PAGE_SIZE bytes or larger.
 *
 * @param   page    Page to begin reading from
 * @param   count   Number of pages to read
 *
 * @return 0 on success, or BLADERF_ERR_INVAL on an invalid `page` or
 *         `count` value, or a value from \ref RETCODES list on other failures.
 */
int spi_flash_read(struct bladerf *dev, uint8_t *buf,
                   uint32_t page, uint32_t count);

/**
 * Verify data in flash
 *
 * @param   dev             Device handle
 * @param   readback_buf    Buffer to read data into. Must be
 *                          `count` * BLADERF_FLASH_PAGE_SIZE bytes or larger.
 * @param   
 *
 * @param   page    Page to begin reading from
 * @param   count   Number of pages to read
 *
 * @return 0 on success, or BLADERF_ERR_INVAL on an invalid `page` or
 *         `count` value, or a value from \ref RETCODES list on other failures.
 */
int spi_flash_verify(struct bladerf *dev, uint8_t *readback_buf,
                     const uint8_t *expected_buf, uint32_t page,
                     uint32_t count);

/**
 * Write data to flash
 *
 * @param   dev   Device handle
 * @param   buf   Data to write to flash
 *
 * @param   page  Page to begin writing at
 * @param   count Number of pages to write
 *
 * @return 0 on success, or BLADERF_ERR_INVAL on an invalid `page` or
 *         `count` value, or a value from \ref RETCODES list on other failures.
 */
int spi_flash_write(struct bladerf *dev, const uint8_t *buf,
                    uint32_t page, uint32_t count);

#endif
