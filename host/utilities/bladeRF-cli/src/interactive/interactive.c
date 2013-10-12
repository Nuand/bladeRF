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
#include <errno.h>
#include <string.h>
#include "interactive.h"
#include "interactive_impl.h"
#include "cmd.h"

static void exit_script(struct cli_state *s, bool *in_script)
{
    int status;

    if (*in_script) {
        *in_script = false;

        /* TODO pop from a script stack here, restore line count */
        status = interactive_set_input(stdin);

        /* At least attempt to report something... */
        if (status < 0) {
            cli_err(s, "Error", "Failed to reset terminal when exiting script");
        }

        if (s->script) {
            fclose(s->script);
            s->script = NULL;
        }
    }
}

int interactive(struct cli_state *s, bool script_only)
{
    char *line;
    int status;
    const char *error;
    bool in_script;

    status = interactive_init();
    if (status < 0) {
        return status;
    }

    status = 0;
    in_script = s->script != NULL;
    s->lineno = 1;

    if (in_script) {
        status = interactive_set_input(s->script);
        if (status < 0) {
            fprintf(stderr, "Failed to run script. Aborting!\n");
            status = CMD_RET_QUIT;
        }
    }

    while (!cmd_fatal(status) && status != CMD_RET_QUIT) {

        /* TODO: Change the prompt based on which device is open */
        line = interactive_get_line(CLI_DEFAULT_PROMPT);

        if (!line) {
            if (in_script) {
                exit_script(s, &in_script);

                /* Exit if we were run with a script, but not asked
                 * to drop into interactive mode */
                if (script_only)
                    status = CMD_RET_QUIT;

            } else {
                /* Leaving interactivce mode */
                break;
            }
        } else {
            status = cmd_handle(s, line);

            if (status < 0) {
                error = cmd_strerror(status, s->last_lib_error);
                if (error) {
                    cli_err(s, "Error", "%s", error);
                }

                /* Stop executing script if we're in one */
                exit_script(s, &in_script);

            } else if (status > 0){
                switch (status) {
                    case CMD_RET_CLEAR_TERM:
                        interactive_clear_terminal();
                        break;
                    case CMD_RET_RUN_SCRIPT:
                        break;
                    default:
                        printf("Unknown return code: %d\n", status);
                }
            }
        }

        if (in_script) {
            s->lineno++;
        }
    }

    interactive_deinit();

    return 0;
}
