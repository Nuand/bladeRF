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
#include "input.h"
#include "input_impl.h"
#include "cmd.h"
#include "script.h"

static void exit_script(struct cli_state *s)
{
    int status;

    if (cli_script_loaded(s->scripts)) {

        status = cli_close_script(&s->scripts);
        if (status < 0) {
            cli_err(s, "Error", "Failed to close script");
        }

        if (!cli_script_loaded(s->scripts)) {
            status = input_set_input(stdin);

            /* At least attempt to report something... */
            if (status < 0) {
                cli_err(s, "Error",
                        "Failed to reset terminal when exiting script");
            }
        }
    }
}

int input_loop(struct cli_state *s, bool script_only)
{
    char *line;
    int status;
    const char *error;

    status = input_init();
    if (status < 0) {
        return status;
    }

    status = 0;

    if (cli_script_loaded(s->scripts)) {
        status = input_set_input(cli_script_file(s->scripts));
        if (status < 0) {
            fprintf(stderr, "Failed to run script. Aborting!\n");
            status = CLI_RET_QUIT;
        }
    }

    while (!cli_fatal(status) && status != CLI_RET_QUIT) {

        /* TODO: Change the prompt based on which device is open */
        line = input_get_line(CLI_DEFAULT_PROMPT);

        if (!line) {
            if (cli_script_loaded(s->scripts)) {
                exit_script(s);

                /* Exit if we were run with a script, but not asked
                 * to drop into interactive mode */
                if (script_only)
                    status = CLI_RET_QUIT;

            } else {
                /* Leaving interactivce mode */
                break;
            }
        } else {
            status = cmd_handle(s, line);

            if (status < 0) {
                error = cli_strerror(status, s->last_lib_error);
                if (error) {
                    cli_err(s, "Error", "%s", error);
                }

                /* Stop executing script if we're in one */
                exit_script(s);

            } else if (status > 0){
                switch (status) {
                    case CLI_RET_CLEAR_TERM:
                        input_clear_terminal();
                        break;
                    case CLI_RET_RUN_SCRIPT:
                        status = input_set_input(
                                    cli_script_file(s->scripts));

                        if (status < 0) {
                            cli_err(s, "Error",
                                    "Failed to begin executing script");
                        }
                        break;
                    default:
                        printf("Unknown return code: %d\n", status);
                }
            }
        }
        cli_script_bump_line_count(s->scripts);
    }

    input_deinit();

    return 0;
}
