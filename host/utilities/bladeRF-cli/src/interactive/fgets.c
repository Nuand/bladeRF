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
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include "interactive_impl.h"
#include "host_config.h"

static FILE *input = NULL;
static char *line_buf = NULL;
static const int line_buf_size = CLI_MAX_LINE_LEN + 1;
static bool caught_signal = false;

/* Makes the assumption that the caller hasn't done all sorts of closing
 * and dup'ing... */
static inline bool is_stdin(FILE *file)
{
    return file && fileno(file) == 0;
}

int interactive_init()
{

    if (input || line_buf) {
        interactive_deinit();
    }

    input = stdin;

    line_buf = calloc(line_buf_size, 1);
    if (!line_buf) {
        interactive_deinit();
        return CMD_RET_MEM;
    }

    caught_signal = false;

    return 0;
}

void interactive_deinit()
{
    if (input) {
        input = NULL;
    }

    if (line_buf) {
        free(line_buf);
        line_buf = NULL;
    }
}

int interactive_set_input(FILE *new_input)
{
    input = new_input;
    return 0;
}

char * interactive_get_line(const char *prompt)
{
    char *eol;
    char *ret = NULL;

    if (input) {

        if (is_stdin(input)) {
            fputs(prompt, stdout);
        }

        memset(line_buf, 0, line_buf_size);
        ret = fgets(line_buf, line_buf_size - 1, input);

        if (ret) {

            /* Scrape out newline */
            if ((eol = strchr(line_buf, '\r')) ||
                (eol = strchr(line_buf, '\n'))) {

                *eol = '\0';
            }

            /* Somewhat unnecessary, since fgets would have returned NULL if it
             * was interrupted. Just here as a sanity check and nice place to
             * plop a breakpoint */
            if (caught_signal) {
                ret = NULL;
            }
        }
    }

    return ret;
}


void interactive_clear_terminal()
{
    /* If our input is a script, we should be nice and not clear the user's
     * terminal - only do this if our input's from stdin */
    if (input && is_stdin(input)) {
        /* FIXME Ugly, disgusting hack alert! Sound the alarms! */
#       if BLADERF_OS_WINDOWS
            system("cls");
#       else
            fputs("\033[2J\033[1;3f", stdout);
#endif

    }
}

/* TODO: Linux/OSX: Expand ~/ to home directory, $<variables>
 *       Windows: %<varibles%
 */
char * interactive_expand_path(const char *path)
{
    return strdup(path);
}

void interactive_ctrlc(void)
{
    caught_signal = true;
}
