/*
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2014 Nuand LLC
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
#include "xb.h"
#include "xb100.h"
#include "xb200.h"

int cmd_xb(struct cli_state *state, int argc, char **argv)
{
    int status = 0;

    if (argc >= 3) {
        // xb <model> <subcommand> <args>
        int modelnum = atoi(argv[1]);

        switch (modelnum)
        {
            case MODEL_XB100:
                status = cmd_xb100(state, argc, argv);
                break;

            case MODEL_XB200:
                status = cmd_xb200(state, argc, argv);
                break;

            default:
                cli_err(state, argv[1],
                        "Invalid expansion board model number\n");
                return CLI_RET_INVPARAM;
                break;
        }

        if ((status != 0) && (status != CLI_RET_INVPARAM) &&
            (status != CLI_RET_NARGS)) {
          return CLI_RET_LIBBLADERF;
        }

        return status;
    }

    return CLI_RET_NARGS;
}
