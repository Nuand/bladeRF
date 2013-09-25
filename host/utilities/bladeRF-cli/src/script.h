/*
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
#ifndef SCRIPT_H__
#define SCRIPT_H__

#include <stdio.h>
#include <stdbool.h>

struct script;

/**
 * Open an switch to the specified script. The state of the currently executing
 * script is backed up so that it may be restored upon close_script;
 *
 * @param   s           Handle to currently executing script. If no scripts
 *                      are currently opened, ensure *s = NULL.
 *
 *                      On success, this pointer will be updated to the newly
 *                      opened script.
 *
 * @param   filename    File name
 *
 * @return 0 on success, 1 if a recursive loop is detected, or a
 *        negative errno value on failure
 */
int cli_open_script(struct script **s, const char *filename);

/**
 * Close a script, and switch to the previous script. Does nothing
 * if *s == NULL.
 *
 * @return 0 on success, negative errno value on failure
 */
int cli_close_script(struct script **s);

/**
 * Close all open scripts
 *
 * @param   s      rcript handle
 *
 * @return 0 on success, -1 on failure
 */
int cli_close_all_scripts(struct script **s);

/**
 * Get the count of currently opened scripts
 *
 * @param   s       Script handle
 *
 * @return number of opened scripts
 */
unsigned int cli_num_scripts(struct script *s);

/**
 * Determine if a script is currently loaded
 *
 * @param   s       Script handle
 *
 * @return true if a script is currently loaded, false otherwise
 */
bool cli_script_loaded(struct script *s);

/**
 * Get FILE* for current script
 *
 * @param   s       Script handle
 *
 * @note be sure to either preceed with a call to cli_script_loaded() or
 *       check this return value for NULL
 *
 * @return file handle or NULL if no script is loaded
 */
FILE * cli_script_file(struct script *s);

/**
 * Increment the line number of the current script, if one is loaded
 *
 * Does nothing if no script is loaded
 */
void cli_script_bump_line_count(struct script *s);

/**
 * Get the current script line number
 *
 * @return line number or 0 if no script is loaded
 */
unsigned int cli_script_line(struct script *s);

/**
 * Get the currently executing file name
 */
const char * cli_script_file_name(struct script *s);

#endif
