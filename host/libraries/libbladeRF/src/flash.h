/* flash.h
 * Licensed under GPLv2+ or LGPLv2+
 * Copyright (C) 2013  Daniel Gröber <dxld AT darkboxed DOT org>
 */

#include <stdint.h>
#include <assert.h>

#include <libbladeRF.h>

#ifndef BLADERF_FLASH_H_
#define BLADERF_FLASH_H_

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
