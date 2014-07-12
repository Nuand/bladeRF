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

#include "libbladeRF.h"
#include "bladerf_priv.h"
#include "rel_assert.h"
#include "flash.h"
#include "flash_fields.h"
#include "log.h"

static inline int check_eb_access(uint32_t erase_block, uint32_t count)
{
    if (erase_block >= BLADERF_FLASH_NUM_EBS) {
        log_debug("Invalid erase block: %u\n", erase_block);
        return BLADERF_ERR_INVAL;
    } else if (count > BLADERF_FLASH_NUM_EBS) {
        log_debug("Invalid number of erase blocks: %u\n", count);
        return BLADERF_ERR_INVAL;
    } else if ((erase_block + count) > BLADERF_FLASH_NUM_EBS) {
        log_debug("Requested operation extends past end of flash: "
                  "eb=%u, count=%u\n", erase_block, count);
        return BLADERF_ERR_INVAL;
    } else {
        return 0;
    }
}

static inline int check_page_access(uint32_t page, uint32_t count)
{
    if (page >= BLADERF_FLASH_NUM_PAGES) {
        log_debug("Invalid page: %u\n", page);
        return BLADERF_ERR_INVAL;
    } else if (count > BLADERF_FLASH_NUM_PAGES) {
        log_debug("Invalid number of pages: %u\n", count);
        return BLADERF_ERR_INVAL;
    } else if ((page + count) > BLADERF_FLASH_NUM_PAGES) {
        log_debug("Requested operation extends past end of flash: "
                  "page=%u, count=%u\n", page, count);
        return BLADERF_ERR_INVAL;
    } else {
        return 0;
    }
}

int flash_erase(struct bladerf *dev, uint32_t erase_block, uint32_t count)
{
    int status = check_eb_access(erase_block, count);

    if (status == 0) {
        status = dev->fn->erase_flash_blocks(dev, erase_block, count);
    }

    return status;
}

int flash_read(struct bladerf *dev, uint8_t *buf,
               uint32_t page, uint32_t count)
{
    int status = check_page_access(page, count);

    if (status == 0) {
        status = dev->fn->read_flash_pages(dev, buf, page, count);
    }

    return status;;
}

int flash_write(struct bladerf *dev, const uint8_t *buf,
                uint32_t page, uint32_t count)
{
    int status = check_page_access(page, count);

    if (status == 0) {
        status = dev->fn->write_flash_pages(dev, buf, page, count);
    }

    return status;
}

static inline int verify_flash(struct bladerf *dev, uint8_t *readback_buf,
                               uint8_t *image, uint32_t page, uint32_t count)
{
    int status = 0;
    size_t i;
    const size_t len = count * BLADERF_FLASH_PAGE_SIZE;

    log_info("Verifying %u pages, starting at page %u\n", count, page);
    status = flash_read(dev, readback_buf, page, count);

    if (status < 0) {
        log_debug("Failed to read from flash: %s\n", bladerf_strerror(status));
        return status;
    }

    for (i = 0; i < len; i++) {
        if (image[i] != readback_buf[i]) {
            status = BLADERF_ERR_UNEXPECTED;
            log_info("Flash verification failed at byte %llu. "
                     "Read %02x, expected %02x\n",
                     i, readback_buf[i], image[i]);
            break;
        }
    }

    return status;
}

int flash_write_fx3_fw(struct bladerf *dev, uint8_t **image, size_t len)
{
    int status;
    uint8_t *readback_buf;
    uint8_t *padded_image;
    uint32_t padded_image_len;

    /* Pad firwmare data out to a page size */
    const uint32_t page_size = BLADERF_FLASH_PAGE_SIZE;
    const uint32_t padding_len =
        (len % page_size == 0) ? 0 : page_size - (len % page_size);

    if (len >= (UINT32_MAX - padding_len)) {
        return BLADERF_ERR_INVAL;
    }

    padded_image_len = (uint32_t) len + padding_len;

    readback_buf = (uint8_t*) malloc(padded_image_len);
    if (readback_buf == NULL) {
        return BLADERF_ERR_MEM;
    }

    padded_image = (uint8_t *) realloc(*image, padded_image_len);
    if (padded_image == NULL) {
        status = BLADERF_ERR_MEM;
        goto error;
    }

    *image = padded_image;

    /* Clear the padded region */
    memset(padded_image + len, 0xFF, padded_image_len - len);

    /* Erase the entire firmware region */
    status = flash_erase(dev, BLADERF_FLASH_EB_FIRMWARE,
                         BLADERF_FLASH_EB_LEN_FIRMWARE);

    if (status != 0) {
        log_debug("Failed to erase firmware region: %s\n",
                  bladerf_strerror(status));
        goto error;
    }

    /* Convert the image length to pages */
    padded_image_len /= BLADERF_FLASH_PAGE_SIZE;

    /* Write the firmware image to flash */
    status = flash_write(dev, padded_image,
                         BLADERF_FLASH_PAGE_FIRMWARE, padded_image_len);

    if (status < 0) {
        log_debug("Failed to write firmware: %s\n", bladerf_strerror(status));
        goto error;
    }

    /* Read back and double-check what we just wrote */
    status = verify_flash(dev, readback_buf, padded_image,
                          BLADERF_FLASH_PAGE_FIRMWARE, padded_image_len);

    if (status != 0) {
        log_debug("Flash verification failed: %s\n", bladerf_strerror(status));
    }

error:
    free(readback_buf);
    return status;
}

static inline void fill_fpga_metadata_page(uint8_t *metadata,
                                           size_t actual_bitstream_len)
{
    char len_str[10];
    int idx = 0;

    memset(len_str, 0, sizeof(len_str));
    memset(metadata, 0xff, BLADERF_FLASH_PAGE_SIZE);

    snprintf(len_str, sizeof(len_str), "%u",
             (unsigned int)actual_bitstream_len);

    encode_field((char *)metadata, BLADERF_FLASH_PAGE_SIZE,
                 &idx, "LEN", len_str);
}

int flash_write_fpga_bitstream(struct bladerf *dev,
                               uint8_t **bitstream, size_t len)
{
    int status;
    uint8_t *readback_buf;
    uint8_t *padded_bitstream;
    uint8_t metadata[BLADERF_FLASH_PAGE_SIZE];
    uint32_t padded_bitstream_len;

    /* Pad data to be page-aligned */
    const uint32_t page_size = BLADERF_FLASH_PAGE_SIZE;
    const uint32_t padding_len =
        (len % page_size == 0) ? 0 : page_size - (len % page_size);

    if (len >= (UINT32_MAX - padding_len)) {
        return BLADERF_ERR_INVAL;
    }

    padded_bitstream_len = (uint32_t) len + padding_len;

    /* Fill in metadata with the *actual* FPGA bitstream length */
    fill_fpga_metadata_page(metadata, len);

    readback_buf = (uint8_t*) malloc(padded_bitstream_len);
    if (readback_buf == NULL) {
        return BLADERF_ERR_MEM;
    }

    padded_bitstream = (uint8_t*) realloc(*bitstream, padded_bitstream_len);
    if (padded_bitstream == NULL) {
        status = BLADERF_ERR_MEM;
        goto error;
    }

    *bitstream = padded_bitstream;

    /* Clear the padded region */
    memset(padded_bitstream + len, 0xFF, padded_bitstream_len - len);

    /* Erase FPGA metadata and bitstream region */
    status = flash_erase(dev, BLADERF_FLASH_EB_FPGA, BLADERF_FLASH_EB_LEN_FPGA);
    if (status != 0) {
        log_debug("Failed to erase FPGA meta & bitstream regions: %s\n",
                  bladerf_strerror(status));
        goto error;
    }

    /* Write the metadata page */
    status = flash_write(dev, metadata, BLADERF_FLASH_PAGE_FPGA, 1);
    if (status != 0) {
        log_debug("Failed to write FPGA metadata page: %s\n",
                  bladerf_strerror(status));
        goto error;
    }

    /* Convert the padded bitstream length to pages */
    padded_bitstream_len /= BLADERF_FLASH_PAGE_SIZE;

    /* Write the padded bitstream */
    status = flash_write(dev, padded_bitstream, BLADERF_FLASH_PAGE_FPGA + 1,
                         padded_bitstream_len);
    if (status != 0) {
        log_debug("Failed to write bitstream: %s\n",
                  bladerf_strerror(status));
        goto error;
    }

    /* Read back and verify metadata */
    status = verify_flash(dev, readback_buf, metadata,
                          BLADERF_FLASH_PAGE_FPGA, 1);
    if (status != 0) {
        log_debug("Failed to verify metadata: %s\n", bladerf_strerror(status));
        goto error;
    }

    /* Read back and verify the bitstream data */
    status = verify_flash(dev, readback_buf, padded_bitstream,
                          BLADERF_FLASH_PAGE_FPGA + 1, padded_bitstream_len);

    if (status != 0) {
        log_debug("Failed to verify bitstream data: %s\n",
                  bladerf_strerror(status));
        goto error;
    }

error:
    free(readback_buf);
    return status;
}

int flash_erase_fpga(struct bladerf *dev)
{
    /* Erase the entire FPGA region, including both autoload metadata and the
     * actual bitstream data */
    return flash_erase(dev, BLADERF_FLASH_EB_FPGA, BLADERF_FLASH_EB_LEN_FPGA);
}
