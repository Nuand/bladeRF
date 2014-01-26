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
#include "common.h"
#include "cmd.h"

/* Usage:
 *  open [device identifier items]
 *
 * It's a bit silly, but we just merge all the argv items into the
 * device identifier string here.
 */
int cmd_open(struct cli_state *state, int argc, char **argv)
{
    char *dev_ident = NULL;
    size_t dev_ident_len;
    int i;
    int status;
	int ret;

    /* Disallow opening of a diffrent device if the current one is doing work */
    if (cli_device_is_streaming(state)) {
        return CMD_RET_BUSY;
    }

    if (state->dev) {
        bladerf_close(state->dev);
    }

    /* We have some device indentifer items */
    if (argc > 1) {
        dev_ident_len = argc - 2; /* # spaces to place between args */

        for (i = 1; i < argc; i++) {
            dev_ident_len += strlen(argv[i]);
        }

        /* Room for '\0' terminator */
        dev_ident_len++;

        dev_ident = calloc(dev_ident_len, 1);
        if (!dev_ident) {
            return CMD_RET_MEM;
        }

        for (i = 1; i < argc; i++) {
            strncat(dev_ident, argv[i], (dev_ident_len - 1 - strlen(dev_ident)));

            if (i != (argc - 1)) {
                dev_ident[strlen(dev_ident)] = ' ';
            }
        }

        printf("Using device string: %s\n", dev_ident);
    }

    status = bladerf_open(&state->dev, dev_ident);
    if (status) {
        state->last_lib_error = status;
        ret = CMD_RET_LIBBLADERF;
    } else {
        ret = 0;
    }

    free(dev_ident);
    return ret;
}

