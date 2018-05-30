/**
 * @file xb.h
 *
 * @brief XB-100 support
 *
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014 Nuand LLC
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

#ifndef EXPANSION_XB100_H_
#define EXPANSION_XB100_H_

#include <stdbool.h>
#include <stdint.h>

#include "board/board.h"

int xb100_attach(struct bladerf *dev);
void xb100_detach(struct bladerf *dev);
int xb100_enable(struct bladerf *dev, bool enable);
int xb100_init(struct bladerf *dev);
int xb100_gpio_read(struct bladerf *dev, uint32_t *val);
int xb100_gpio_write(struct bladerf *dev, uint32_t val);
int xb100_gpio_masked_write(struct bladerf *dev, uint32_t mask, uint32_t val);
int xb100_gpio_dir_read(struct bladerf *dev, uint32_t *val);
int xb100_gpio_dir_write(struct bladerf *dev, uint32_t val);
int xb100_gpio_dir_masked_write(struct bladerf *dev,
                                uint32_t mask,
                                uint32_t val);

#endif
