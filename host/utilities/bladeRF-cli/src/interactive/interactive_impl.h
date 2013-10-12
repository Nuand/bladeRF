/**
 * @file interactive_impl.h
 *
 * @brief This file defines the routines to be implemented by the
 *        interactive/script support code. These routines should only be called
 *        from interactive.c. They are not thread safe or re-entrant.
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
#ifndef INTERACTIVE_IMPL_H__
#define INTERACTIVE_IMPL_H__

#include "interactive.h"

/** Maximum length of a user-input line */
#ifndef CLI_MAX_LINE_LEN
#   define CLI_MAX_LINE_LEN 320
#endif

/** Maximum number of lines of history, if supported */
#ifndef CLI_MAX_HIST_LEN
#   define CLI_MAX_HIST_LEN 500
#endif

/** Default prompt */
#ifndef CLI_PROMPT
#   define CLI_DEFAULT_PROMPT   "bladeRF> "
#endif

#define SIGHANLDER_FAILED "Warning: Failed to set up signal handlers.\n"

/**
 * Perform any required initialization. SIGINT, SIGTERM and SIGQUIT are
 * trappper to allow for a clean exit.
 *
 * @return 0 on success, CMD_RET_* value on failure
 */
int interactive_init();

/**
 * Deinitialize and deallocate resources for interactive/script support
 */
void interactive_deinit();

/**
 * Get a line from the user or script. This should only be used within
 * the interactive() implementation.
 *
 * The returned value is a pointer to an internal buffer and should
 * not be passed to free(). This buffer may be modified, but not beyond the
 * number of characters indicated by strlen()
 *
 * @param   prompt          Prompt to present the user with
 * @param   max_line_len    Maximum lenth a line
 *
 * @return  Line from user on success, NULL on failure or EOF.
 */
char * interactive_get_line(const char *prompt);

/**
 * Set the input source
 *
 * @param   input   Input file stream
 *
 * @return 0 on success CMD_RET_* value on failure
 */
int interactive_set_input(FILE *input);

/**
 * Clear the terminal
 */
void interactive_clear_terminal();

/**
 *
 */

#endif
