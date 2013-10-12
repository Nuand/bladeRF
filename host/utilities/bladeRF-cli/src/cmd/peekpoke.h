/*
 * @file peekpoke.h
 *
 * @brief Items common to the peek and poke commands
 *
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2013 Nuand LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef PEEKPOKE_H__
#define PEEKPOKE_H__

#include "common.h"

#define DAC_MAX_ADDRESS 127
#define LMS_MAX_ADDRESS 127
#define SI_MAX_ADDRESS  255

#define MAX_NUM_ADDRESSES 256
#define MAX_VALUE 255


/* Convenience shared function */
void invalid_address(struct cli_state *s, char *fn, char *addr);

#endif /* PEEKPOKE_H__ */

