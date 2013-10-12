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

extern const unsigned int BLADERF_FLASH_ALIGNMENT_BYTE;
extern const unsigned int BLADERF_FLASH_ALIGNMENT_PAGE;
extern const unsigned int BLADERF_FLASH_ALIGNMENT_SECTOR;

/* Pass one of the BLADERF_FLASH_ALIGNMENT_* constants as `align' */
int flash_aligned(unsigned int align, unsigned int addr);
int flash_bounds(unsigned int addr, unsigned int len);
int flash_bounds_aligned(unsigned int align,
                         unsigned int addr, unsigned int len);

unsigned int flash_to_sectors(unsigned int bytes);
unsigned int flash_to_pages(unsigned int bytes);

unsigned int flash_from_pages(unsigned int page);
unsigned int flash_from_sectors(unsigned int sector);

#endif
