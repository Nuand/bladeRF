/**
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2014 Nuand LLC
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
#ifndef FLASH_COMMON_H_
#define FLASH_COMMON_H_

#include "common.h"

/**
 * Check that the CLI and device are in a state where we can perform a flash
 * operation. Prints appropriate output for any issues.
 *
 * @param   s           CLI state
 * @param   cmd_name    Name of command
 *
 * @return  CLI error state
 */
int flash_check_state(struct cli_state *s, const char *cmd_name);

#endif
