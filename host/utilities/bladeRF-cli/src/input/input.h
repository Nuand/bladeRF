/**
 * @file input.h
 *
 * @brief Functions to be provided by input mode implementatoins
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
#ifndef INPUT_H_
#define INPUT_H_

#include <libbladeRF.h>
#include "common.h"
#include "str_queue.h"

/**
 * Interactive mode or script execution 'main loop'
 *
 * @param   s           CLI state
 * @param   interactive Enter interactive mode after completing script or
 *                      commands provided via the command line
 * @param   exec_list   Commands to executue prior to scripts or interactive
 *                      mode
 *
 * @return  0 on success, CLI_RET_* on failure
 */
int input_loop(struct cli_state *s, bool interactive);

/**
 * Expand a file path using the input mode support backend
 *
 * @post Heap-allocated memory is used to return the expanded path. The caller
 *       is responsible for calling free().
 *
 * @return Expanded path on success, NULL on failure.
 */
char * input_expand_path(const char *path);

/**
 * Notify input support that we caught CTRL-C. This is neccessary if
 * the underlying support doesn't catch signals, such as the simple fgets-based
 * implementation.
 */
void input_ctrlc(void);

#endif  /* INTERACTICE_H__ */
