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

#ifndef BLADERF_FLASH_H_
#define BLADERF_FLASH_H_

/**
 * Test if a flash access is valid, based upon the specified alignment
 * and whether the access extends past the end of available flash.
 *
 * Valid values for the alignment parameters are the FLASH_ALIGNMENT_* macros.
 *
 * @param   addr_align  Address aligment
 * @param   len_align   Length alignment
 * @param   addr        Flash byte address
 * @param   len         Length of the access
 *
 * @return true on a valid acesss, false otherwise
 */
bool flash_is_valid_access(uint32_t addr_align, uint32_t len_align,
                           uint32_t addr, size_t len);

/**
 * Convert a byte address to an erase block.
 *
 * @pre Bytes address is erase block-aligned.
 *
 * @param   byte_addr   Byte address
 *
 * @return  Erase block number
 */
uint16_t flash_bytes_to_eb(uint32_t byte_addr);

/**
 * Convert a byte address to a page.
 *
 * @pre Byte address is page-aligned.
 *
 * @param   byte_addr   Byte address
 *
 * @return Page number
 */
uint16_t flash_bytes_to_pages(uint32_t byte_addr);

/**
 * Convert a page number to a byte address
 *
 * @param   page    Page number
 * @return  byte address
 */
uint32_t flash_pages_to_bytes(uint16_t page);

/**
 * Convert an erase block number to a byte address
 *
 * @param   eb  Erase block number
 * @return  byte address
 */
uint32_t flash_eb_to_bytes(unsigned int eb);

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
int flash_erase(struct bladerf *dev, uint32_t erase_block, uint32_t count);

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
int flash_read(struct bladerf *dev, uint8_t *buf,
               uint32_t page, uint32_t count);
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
int flash_write(struct bladerf *dev, const uint8_t *buf,
                uint32_t page, uint32_t count);

/**
 * Write the provided data to the FX3 Firmware region to flash.
 *
 * This function does no validation of the data (i.e., that it's valid FW).
 *
 * @param   dev             bladeRF handle
 * @param   image           Firmware image data. Buffer will be
 *                          realloc'd as needed to pad the image data.
 * @param   len             Length of firmware to write, in bytes
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int flash_write_fx3_fw(struct bladerf *dev, uint8_t **image, size_t len);

/**
 * Write the provided FPGA bitstream to flash and enable autoloading via
 * writing the associated metadata.
 *
 * @param   dev             bladeRF handle
 * @param   bitstream       FPGA bitstream data. Buffer will be realloc'd as
 *                          needed to pad the image data.
 * @param   len             Length of the bitstream data
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int flash_write_fpga_bitstream(struct bladerf *dev,
                               uint8_t **bitstream, size_t len);

/**
 * Erase FPGA metadata and bitstream regions of flash
 *
 * @param   dev             bladeRF handle
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int flash_erase_fpga(struct bladerf *dev);

#endif
