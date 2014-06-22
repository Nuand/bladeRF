/**
 * @file cmd.h
 *
 * @brief Common command-related routines
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
#ifndef CMD_H__
#define CMD_H__

#include <stdbool.h>
#include "common.h"
#include "conversions.h"

#define NUM_FREQ_SUFFIXES 6
extern const struct numeric_suffix freq_suffixes[NUM_FREQ_SUFFIXES];

/**
 * Parse and execute the supplied command
 *
 * @return 0 on success, CLI_RET_ on failure/exceptional condition
 */
int cmd_handle(struct cli_state *s, const char *line);

/**
 * Print help information for all interactive commands
 */
void cmd_show_help_all();

#endif
