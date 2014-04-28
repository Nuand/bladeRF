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

#define FLASH_ALIGNMENT_BYTE (~0)
#define FLASH_ALIGNMENT_PAGE (~((1 << BLADERF_FLASH_PAGE_BITS) - 1))
#define FLASH_ALIGNMENT_EB   (~((1 << BLADERF_FLASH_EB_BITS) - 1))


/* Addresses and lengths of flash regions */

#define FLASH_FIRMWARE_ADDR     0x0000000   /* Firmware region */
#define FLASH_FIRMWARE_SIZE     0x0030000

#define FLASH_CAL_ADDR          0x0030000   /* Calibration data region */
#define FLASH_CAL_SIZE          0x0010000

#define FLASH_FPGA_META_ADDR    0x0040000   /* FPGA autoload metadata region */
#define FLASH_FPGA_META_SIZE    0x0000100

#define FLASH_FPGA_BIT_ADDR     0x0040100   /* FPGA bitstream data region */
#define FLASH_FPGA_BIT_SIZE     0x036FF00

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
 * Erase flash, aligned on erase blocks
 *
 * @param   dev             Handle to bladeRF to perform erase on
 * @param   addr            Byte address to begin erase. Must be aligned
 *                          on an erase block boundary.
 * @param   len             Number of bytes to erase. Must be a multiple
 *                          of erase blocks
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int flash_erase(struct bladerf *dev, uint32_t addr, size_t len);

/**
 * Read flash, aligned on pages
 *
 * @param[in]   dev         Handle to device to read from
 * @param[in]   addr        Byte address to start read. Must be page-aligned
 * @param[out]  buf         Buffer to store read data in
 * @param[in]   len         Number of bytes to read. Must be a multiple of pages
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int flash_read(struct bladerf *dev, uint32_t addr, uint8_t *buf, size_t len);

/**
 * Write flash, aligned on pages
 *
 * @param[in]   dev         Handle to device to write to
 * @param[in]   addr        Byte address to start write. Must be page-aligned
 * @param[in]   buf         Buffer containing data to write
 * @param[in]   len         Number of bytes to write. Must be a
 *                          multiple of pages
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int flash_write(struct bladerf *dev, uint32_t addr,
                uint8_t *buf, size_t len);

/**
 * Perform an unaligned flash read
 *
 * @param[in]   dev         Handle to bladeRF to read flash data from
 * @param[in]   addr        Flash byte address
 * @param[out]  data        Buffer to store read data in
 * @param[in]   len         Length of read, in bytes
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int flash_unaligned_read(struct bladerf *dev,
                         uint32_t addr, uint8_t *data, size_t len);

/**
 * Perform an unaligned flash write via a read-modify-write on an erase block
 *
 * @param[in]   dev         Handle to bladeRF to read flash data from
 * @param[in]   addr        Flash byte address
 * @param[out]  data        Buffer to store read data in
 * @param[in]   len         Length of read, in bytes
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int flash_unaligned_write(struct bladerf *dev,
                          uint32_t addr, uint8_t *data, size_t len);

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
 * Write the provided FPGA bitstream and enable autoloading.
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
 * Erase FPGA metadata and bitstream region
 *
 * @param   dev             bladeRF handle
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int flash_erase_fpga(struct bladerf *dev);

#endif
