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
#include "log.h"


static inline bool is_aligned(uint32_t alignment, uint32_t addr)
{
    return (addr & alignment) == addr;
}

static inline bool within_bounds(uint32_t addr, size_t len)
{
    if (len > BLADERF_FLASH_TOTAL_SIZE || addr >= BLADERF_FLASH_TOTAL_SIZE) {
        return false;
    } else {
        return ((size_t)addr + len) <= ((size_t)BLADERF_FLASH_TOTAL_SIZE);
    }
}

bool flash_is_valid_access(uint32_t addr_align, uint32_t len_align,
                           uint32_t addr, size_t len)
{
    return is_aligned(addr_align, addr) &&
           is_aligned(len_align, len) &&
           within_bounds(addr, len);
}

uint16_t flash_bytes_to_eb(uint32_t byte_addr)
{
    uint32_t ret;

    assert(byte_addr < BLADERF_FLASH_TOTAL_SIZE);
    assert(is_aligned(FLASH_ALIGNMENT_EB, byte_addr));

    ret = byte_addr / BLADERF_FLASH_EB_SIZE;
    assert(ret <= UINT16_MAX);

    return (uint16_t)ret;
}

uint16_t flash_bytes_to_pages(uint32_t byte_addr)
{
    uint32_t ret;

    assert(is_aligned(FLASH_ALIGNMENT_PAGE, byte_addr));

    ret = byte_addr / BLADERF_FLASH_PAGE_SIZE;
    assert(ret <= UINT16_MAX);

    return (uint16_t)ret;
}

uint32_t flash_pages_to_bytes(uint16_t page)
{
    assert(page < BLADERF_FLASH_NUM_PAGES);
    return page * BLADERF_FLASH_PAGE_SIZE;
}

unsigned int flash_eb_to_bytes(uint32_t eb)
{
    assert(eb < BLADERF_FLASH_NUM_EBS);
    return eb * BLADERF_FLASH_EB_SIZE;
}


static inline void align_and_check(uint32_t align,
                                   uint32_t addr,
                                   size_t   len,
                                   uint32_t *aligned_addr,
                                   size_t   *aligned_len)
{
    *aligned_addr = addr & align;

    assert(len <= UINT32_MAX);
    if (!is_aligned(align, (uint32_t)len)) {
        /* TODO Make this more readable */
        *aligned_len  = (~align) + 1 + (len & align);
    } else
        *aligned_len = len;

    assert(flash_is_valid_access(align, align, *aligned_addr, *aligned_len));
}

int flash_erase(struct bladerf *dev, uint32_t addr, size_t len)
{
    uint16_t erase_block, count;
    const uint32_t align = FLASH_ALIGNMENT_EB;

    if (!flash_is_valid_access(align, align, addr, len)) {
        log_debug("Invalid addr/len 0x%08x/%llu\n", addr,
                  (unsigned long long) len);
        return BLADERF_ERR_INVAL;
    }

    erase_block = flash_bytes_to_eb(addr);
    count = flash_bytes_to_eb(len);

    return dev->fn->erase_flash_blocks(dev, erase_block, count);
}

int flash_read(struct bladerf *dev, uint32_t addr, uint8_t *buf, size_t len)
{
    uint16_t page, count;
    const uint32_t align = FLASH_ALIGNMENT_PAGE;

    if (!flash_is_valid_access(align, align, addr, len)) {
        log_debug("Invalid addr/len 0x%08x/%llu\n", addr,
                  (unsigned long long) len);

        return BLADERF_ERR_INVAL;
    }

    page = flash_bytes_to_pages(addr);
    count = flash_bytes_to_pages(len);

    return dev->fn->read_flash_pages(dev, page, buf, count);
}

int flash_write(struct bladerf *dev, uint32_t addr,
                uint8_t *buf, size_t len)
{
    uint16_t page, count;
    const uint32_t align = FLASH_ALIGNMENT_PAGE;

    if (!flash_is_valid_access(align, align, addr, len)) {
        log_debug("Invalid addr/len 0x%08x/%llu\n", addr,
                  (unsigned long long) len);

        return BLADERF_ERR_INVAL;
    }

    page = flash_bytes_to_pages(addr);
    count = flash_bytes_to_pages(len);

    return dev->fn->write_flash_pages(dev, page, buf, count);
}


int flash_unaligned_read(struct bladerf *dev,
                         uint32_t addr, uint8_t *data, size_t len)
{
    int status;
    uint32_t page_aligned_addr;
    size_t page_aligned_len;
    intptr_t addr_diff;
    uint8_t *buf;

    align_and_check(FLASH_ALIGNMENT_PAGE, addr, len,
                    &page_aligned_addr, &page_aligned_len);

    /* FIXME What is this nonsense? ---v */
    /* MSVC complains about abs() for uint32_t params not being defined */
    addr_diff = abs((long long)(addr - page_aligned_addr));

    buf = (uint8_t*)malloc(page_aligned_len);
    if (!buf) {
        return BLADERF_ERR_MEM;
    }

    status = flash_read(dev, page_aligned_addr, buf, page_aligned_len);
    if (status < 0) {
        goto out;
    }

    memcpy(data, buf + addr_diff, len);

out:
    free(buf);
    return status;
}

int flash_unaligned_write(struct bladerf *dev,
                          uint32_t addr, uint8_t *data, size_t len)
{
    int status;
    uint32_t eb_aligned_addr;
    size_t eb_aligned_len;
    uint8_t *buf = NULL;
    intptr_t addr_diff;

    align_and_check(FLASH_ALIGNMENT_EB, addr, len,
                    &eb_aligned_addr, &eb_aligned_len);

    if (addr != eb_aligned_addr || len != eb_aligned_len) {
        /* FIXME What is this nonsense? ---v */
        /* MSVC complains about abs(uint32_t) not being valid */
        addr_diff = abs((long long)(addr - eb_aligned_addr));

        buf = (uint8_t*)malloc(eb_aligned_len);
        if (buf == NULL) {
            return BLADERF_ERR_MEM;
        }

        status = flash_read(dev, eb_aligned_addr, buf, eb_aligned_len);
        if (status < 0) {
            goto out;
        }

        memcpy(buf + addr_diff, data, len);
    }

    status = flash_erase(dev, eb_aligned_addr, eb_aligned_len);
    if (status < 0) {
        goto out;
    }

    status = flash_write(dev, eb_aligned_addr, buf ? buf : data,
                         eb_aligned_len);

out:
    free(buf);
    return status;
}

static inline int verify_flash(struct bladerf *dev, uint8_t *readback_buf,
                               uint32_t addr, uint8_t *image, size_t len)
{
    int status = 0;
    size_t i;

    log_info("Verifying 0x%08x bytes at address 0x%08x\n", len, addr);
    status = flash_read(dev, addr, readback_buf, len);

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
    size_t padded_image_len;

    /* Pad firmare data out to an rase block size */
    const size_t page_size = BLADERF_FLASH_PAGE_SIZE;
    const size_t padding_len =
        (len % page_size == 0) ? 0 : page_size - (len % page_size);

    padded_image_len = len + padding_len;

    readback_buf = malloc(padded_image_len);
    if (readback_buf == NULL) {
        return BLADERF_ERR_MEM;
    }

    padded_image = realloc(*image, padded_image_len);
    if (padded_image == NULL) {
        status = BLADERF_ERR_MEM;
        goto error;
    }

    *image = padded_image;

    /* Clear the padded region */
    memset(padded_image + len, 0xFF, padded_image_len - len);

    /* Erase the entire firmware region */
    status = flash_erase(dev, FLASH_FIRMWARE_ADDR, FLASH_FIRMWARE_SIZE);
    if (status != 0) {
        log_debug("Failed to erase firmware region: %s\n",
                  bladerf_strerror(status));
        goto error;
    }

    /* Write the firmware image to flash */
    status = flash_write(dev, FLASH_FIRMWARE_ADDR,
                         padded_image, padded_image_len);
    if (status < 0) {
        log_debug("Failed to write firmware: %s\n", bladerf_strerror(status));
        goto error;
    }

    /* Read back and double-check what we just wrote */
    status = verify_flash(dev, readback_buf, FLASH_FIRMWARE_ADDR,
                          padded_image, padded_image_len);
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
    int idx;

    assert(actual_bitstream_len < INT_MAX);
    snprintf(len_str, sizeof(len_str), "%u",
             (unsigned int) actual_bitstream_len);

    memset(metadata, 0xff, BLADERF_FLASH_PAGE_SIZE);
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
    size_t padded_bitstream_len;

    /* Pad data to be page-aligned */
    const size_t page_size = BLADERF_FLASH_PAGE_SIZE;
    const size_t padding_len =
        (len % page_size == 0) ? 0 : page_size - (len % page_size);

    padded_bitstream_len = len + padding_len;

    /* Fill in metadata with the *actual* FPGA bitstream length */
    fill_fpga_metadata_page(metadata, len);

    readback_buf = malloc(padded_bitstream_len);
    if (readback_buf == NULL) {
        return BLADERF_ERR_MEM;
    }

    padded_bitstream = realloc(*bitstream, padded_bitstream_len);
    if (padded_bitstream == NULL) {
        status = BLADERF_ERR_MEM;
        goto error;
    }

    *bitstream = padded_bitstream;

    /* Clear the padded region */
    memset(padded_bitstream + len, 0xFF, padded_bitstream_len - len);

    /* Erase FPGA metadata and bitstream region */
    status = flash_erase(dev, FLASH_FPGA_META_ADDR,
                         FLASH_FPGA_META_SIZE + FLASH_FPGA_BIT_SIZE);
    if (status != 0) {
        log_debug("Failed to erase FPGA meta & bitstream regions: %s\n",
                  bladerf_strerror(status));
        goto error;
    }

    /* Write the metadata page */
    status = flash_write(dev, FLASH_FPGA_META_ADDR, metadata,
                         BLADERF_FLASH_PAGE_SIZE);
    if (status != 0) {
        log_debug("Failed to write FPGA metadata page: %s\n",
                  bladerf_strerror(status));
        goto error;
    }

    /* Write the padded bitstream */
    status = flash_write(dev, FLASH_FPGA_BIT_ADDR, padded_bitstream,
                         padded_bitstream_len);
    if (status != 0) {
        log_debug("Failed to write bitstream: %s\n",
                  bladerf_strerror(status));
        goto error;
    }

    /* Read back and verify metadata */
    status = verify_flash(dev, readback_buf, FLASH_FPGA_META_ADDR,
                          metadata, sizeof(metadata));
    if (status != 0) {
        log_debug("Failed to verify metadata: %s\n", bladerf_strerror(status));
        goto error;
    }

    /* Read back and verify the bitstream data */
    status = verify_flash(dev, readback_buf, FLASH_FPGA_BIT_ADDR,
                          padded_bitstream, padded_bitstream_len);

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
    return flash_erase(dev, FLASH_FPGA_META_ADDR,
                       FLASH_FPGA_META_SIZE + FLASH_FPGA_BIT_SIZE);
}
