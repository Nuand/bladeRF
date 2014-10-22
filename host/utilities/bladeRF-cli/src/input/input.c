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
#include "rel_assert.h"

static void exit_script(struct cli_state *s)
{
    int status;

    if (cli_script_loaded(s->scripts)) {

        status = cli_close_script(&s->scripts);
        if (status < 0) {
            cli_err(s, "Error", "Failed to close script.\n");
        }

        if (!cli_script_loaded(s->scripts)) {
            status = input_set_input(stdin);

            /* At least attempt to report something... */
            if (status < 0) {
                cli_err(s, "Error",
                        "Failed to reset terminal when exiting script.\n");
            }
        }
    }
}

int input_loop(struct cli_state *s, bool interactive)
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
            cli_err(s, "Error", "Failed to load script. Aborting!\n");
            status = CLI_RET_QUIT;
        }
    }

    s->exec_from_cmdline = !str_queue_empty(s->exec_list);

    while (!cli_fatal(status) && status != CLI_RET_QUIT) {

        if (s->exec_from_cmdline) {
            line = str_queue_deq(s->exec_list);
            assert(line != NULL);
        } else {
            line = input_get_line(CLI_DEFAULT_PROMPT);
        }

        if (!line && !s->exec_from_cmdline) {
            if (cli_script_loaded(s->scripts)) {

                if (!s->exec_from_cmdline) {
                    exit_script(s);
                }

                /* Exit if we were run with a script, but not asked
                 * to drop into interactive mode */
                if (!interactive) {
                    status = CLI_RET_QUIT;
                }
            } else {
                /* Leaving interactive mode */
                break;
            }
        } else {
            status = cmd_handle(s, line);

            if (status < 0) {
                error = cli_strerror(status, s->last_lib_error);
                if (error) {
                    cli_err(s, "Error", "%s\n", error);
                }

                /* Stop executing script or command list */
                if (s->exec_from_cmdline) {
                    str_queue_clear(s->exec_list);
                } else {
                    exit_script(s);
                }

                /* Quit if we're not supposed to drop to a prompt */
                if (!interactive) {
                    status = CLI_RET_QUIT;
                }

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
                                    "Failed to begin executing script\n");
                        }
                        break;
                    default:
                        cli_err(s, "Error", "Unknown return code: %d\n", status);
                }
            }
        }

        if (s->exec_from_cmdline) {
            free(line);
            line = NULL;
            s->exec_from_cmdline = !str_queue_empty(s->exec_list);

            /* Nothing left to do here */
            if (!interactive &&
                !s->exec_from_cmdline && !cli_script_loaded(s->scripts)) {
                status = CLI_RET_QUIT;
            }

        } else {
            cli_script_bump_line_count(s->scripts);
        }
    }

    input_deinit();

    return 0;
}
