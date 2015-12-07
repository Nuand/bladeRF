/*
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2015 Nuand LLC
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

#include "cmd.h"

int cmd_fw_log(struct cli_state *state, int argc, char **argv)
{
    int status;
    const char *filename;

    switch (argc) {
        case 1:
            filename = NULL;
            break;

        case 2:
            filename = argv[1];
            break;

        default:
            return CLI_RET_NARGS;
    }

    status = bladerf_get_fw_log(state->dev, filename);
    if (status != 0) {
        state->last_lib_error = status;
        status = CLI_RET_LIBBLADERF;
    }

    return status;
}
