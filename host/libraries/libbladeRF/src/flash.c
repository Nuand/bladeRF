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
#include "flash.h"
#include <libbladeRF.h>

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
