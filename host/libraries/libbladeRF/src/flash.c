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
#include "flash.h"
#include "log.h"

#define FLASH_FIRMWARE_ADDR 0x0000000   /* Address of FW in SPI flash */
#define FLASH_FIRMWARE_SIZE 0x0030000   /* Length (bytes) of FW region */

const unsigned int BLADERF_FLASH_ALIGNMENT_BYTE = ~0;
const unsigned int BLADERF_FLASH_ALIGNMENT_PAGE = ~((1<<BLADERF_FLASH_PAGE_BITS) - 1);
const unsigned int BLADERF_FLASH_ALIGNMENT_SECTOR = ~((1<<BLADERF_FLASH_SECTOR_BITS) - 1);


int flash_aligned(unsigned int align, unsigned int addr)
{
    assert(align == BLADERF_FLASH_ALIGNMENT_BYTE
           || align == BLADERF_FLASH_ALIGNMENT_PAGE
           || align == BLADERF_FLASH_ALIGNMENT_SECTOR);

    return (addr & (unsigned int)align) == addr;
}

int flash_bounds(unsigned int addr, unsigned int len)
{
    assert(addr < BLADERF_FLASH_TOTAL_SIZE);
    assert(addr + len <= BLADERF_FLASH_TOTAL_SIZE);

    return addr + len <= BLADERF_FLASH_TOTAL_SIZE;
}

int flash_bounds_aligned(unsigned int align,
                         unsigned int addr, unsigned int len)
{
    unsigned int aligned = flash_aligned(align, addr)
        && flash_aligned(align, addr + len)
        && flash_bounds(addr, len);

    assert(aligned);

    return aligned;
}

unsigned int flash_to_sectors(unsigned int bytes)
{
    assert(flash_aligned(BLADERF_FLASH_ALIGNMENT_SECTOR, bytes));

    return (bytes>>BLADERF_FLASH_SECTOR_BITS);
}

unsigned int flash_to_pages(unsigned int bytes)
{
    assert(flash_aligned(BLADERF_FLASH_ALIGNMENT_PAGE, bytes));

    return (bytes>>BLADERF_FLASH_PAGE_BITS);
}

unsigned int flash_from_pages(unsigned int page)
{
    assert(page < BLADERF_FLASH_NUM_PAGES);

    return page * BLADERF_FLASH_PAGE_SIZE;
}

unsigned int flash_from_sectors(unsigned int sector)
{
    assert(sector < BLADERF_FLASH_NUM_SECTORS);

    return sector * BLADERF_FLASH_SECTOR_SIZE;
}


static void flash_align_bounds(unsigned int align,
                               unsigned int addr,
                               unsigned int len,
                               unsigned int *aligned_addr,
                               unsigned int *aligned_len)
{
    if(!flash_aligned(align, addr))
        *aligned_addr = addr & align;
    else
        *aligned_addr = addr;

    if(!flash_aligned(align, len)) {
        *aligned_len  = ~align + 1 + (len & align);
    } else
        *aligned_len = len;

    assert(flash_bounds_aligned(align, *aligned_addr, *aligned_len));
}

int bladerf_read_flash_unaligned(struct bladerf *dev,
                                 uint32_t addr, uint8_t *pbuf, uint32_t len)
{
    int rv;
    uint32_t page_aligned_addr, page_aligned_len;
    intptr_t addr_diff;
    uint8_t *buf;

    flash_align_bounds(BLADERF_FLASH_ALIGNMENT_PAGE, addr, len,
                       &page_aligned_addr, &page_aligned_len);

    /* MSVC complains about abs() for uint32_t params not being defined */
    addr_diff = abs((long long)(addr - page_aligned_addr));

    buf = (uint8_t*)malloc(page_aligned_len);
    if(!buf)
        return BLADERF_ERR_MEM;

    rv = bladerf_read_flash(dev, page_aligned_addr, buf, page_aligned_len);
    if(rv < 0)
        goto out;

    memcpy(pbuf, buf + addr_diff, len);

    rv = len;

out:
    if(buf)
        free(buf);

    return rv;
}

int bladerf_program_flash_unaligned(struct bladerf *dev,
                                    uint32_t addr, uint8_t *pbuf, uint32_t len)
{
    int rv;
    uint32_t sector_aligned_addr, sector_aligned_len;
    uint8_t *buf = NULL;
    intptr_t addr_diff;

    flash_align_bounds(BLADERF_FLASH_ALIGNMENT_SECTOR, addr, len,
                       &sector_aligned_addr, &sector_aligned_len);

    if(addr != sector_aligned_addr || len != sector_aligned_len) {
        /* MSVC complains about abs(uint32_t) not being valid */
        addr_diff = abs((long long)(addr - sector_aligned_addr));

        buf = (uint8_t*)malloc(sector_aligned_len);
        if(!buf)
            return BLADERF_ERR_MEM;

        rv = bladerf_read_flash(dev, sector_aligned_addr, buf, sector_aligned_len);
        if(rv < 0)
            goto out;

        memcpy(buf + addr_diff, pbuf, len);
    }

    rv = bladerf_erase_flash(dev, sector_aligned_addr, sector_aligned_len);
    if(rv < 0)
        goto out;

    rv = bladerf_write_flash(
            dev,
            sector_aligned_addr,
            buf ? buf : pbuf,
            sector_aligned_len
        );
    if(rv < 0)
        goto out;

    rv = len;

out:
    if(buf)
        free(buf);

    return rv;
}

static inline int verify_flash(struct bladerf *dev, uint8_t *readback_buf,
                               uint32_t addr, uint8_t *image, size_t len)
{
    int status = 0;
    size_t i;

    log_info("Verifying 0x%08x bytes at address 0x%08x\n", len, addr);
    status = dev->fn->read_flash(dev, addr, readback_buf, len);

    if (status < 0) {
        log_debug("Failed to read from flash: %s\n", bladerf_strerror(status));
        return status;
    } else if ((size_t)status != len) {
        log_warning("Flash read of unexpected size: expected=%llu, read=%llu\n",
                    (unsigned long long)len, (unsigned long long)status);
    } else {
        status = 0;
    }

    for (i = 0; i < len; i++) {
        if (image[i] != readback_buf[i]) {
            status = BLADERF_ERR_UNEXPECTED;
            log_info("Flash verification failed at byte %llu. "
                     "Read %02x, expected %02x\n", readback_buf[i], image[i]);
            break;
        }
    }

    return status;
}

int flash_write_fx3_fw(struct bladerf *dev, uint8_t *image, size_t image_size)
{
    int status;
    uint8_t *readback_buf;

    readback_buf = malloc(image_size);
    if (readback_buf == NULL) {
        return BLADERF_ERR_MEM;
    }

    /* Erase the entire firmware region */
    status = dev->fn->erase_flash(dev, FLASH_FIRMWARE_ADDR,
                                  FLASH_FIRMWARE_SIZE);
    if (status < 0) {
        log_debug("Failed to erase firmware region: %s\n",
                  bladerf_strerror(status));
        goto error;
    } else if (status != FLASH_FIRMWARE_SIZE) {
        log_warning("Erase reported size of %llu vs expected %llu\n",
                    (unsigned long long)status,
                    (unsigned long long)FLASH_FIRMWARE_SIZE);
    }

    /* Write the firmware image to flash */
    status = dev->fn->write_flash(dev, FLASH_FIRMWARE_ADDR, image, image_size);
    if (status < 0) {
        log_debug("Failed to write firmware: %s\n", bladerf_strerror(status));
        goto error;
    } else if ((size_t)status != image_size) {
        log_warning("Firmware write reported %llu of %llu bytes written\n",
                    (unsigned long long)status, (unsigned long long)image_size);
    }

    /* Read back and double-check what we just wrote */
    status = verify_flash(dev, readback_buf, FLASH_FIRMWARE_ADDR,
                          image, image_size);

    if (status != 0) {
        log_debug("Flash verification failed: %s\n", bladerf_strerror(status));
    }

error:
    free(readback_buf);
    return status;
}
