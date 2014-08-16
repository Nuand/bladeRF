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
#include "common.h"
#include <string.h>
#include <xb.h>
#include <xb100.h>

int cmd_xb100(struct cli_state *state, int argc, char **argv)
{
    int status = 0;

    if (NULL == state->dev) {
        printf("  No device is currently opened\n");
        return 0;
    }

    if (argc < 3)
    {
        // incorrect number of arguments
        return CLI_RET_NARGS;
    }

    // xb 100 <subcommand> <args>
    unsigned int modelnum = atoi(argv[1]);
    const char *subcommand = argv[2];

    if (modelnum != MODEL_XB100) {
        return CLI_RET_INVPARAM;
    }

    // xb 100 enable
    if (!strcmp(subcommand, "enable")) {
        printf(" Enabling XB-100 GPIO expansion board\n");
        status = bladerf_expansion_attach(state->dev, BLADERF_XB_100);
        if (status != 0) {
            state->last_lib_error = status;
            return CLI_RET_LIBBLADERF;
        }

        printf("  XB-100 GPIO expansion board successfully enabled\n");
    }

    // unknown subcommand
    else {
        return CLI_RET_INVPARAM;
    }

    return status;
}

