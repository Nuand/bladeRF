/*
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2013-2014 Nuand LLC
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
#include "cmd.h"
#include "input.h"

int cmd_load(struct cli_state *state, int argc, char **argv)
{
    /* Valid commands:
        load fpga <filename>
        load fx3 <filename>
        load cal[ibration] <filename>
    */
    int rv = CLI_RET_OK;

    if ( argc == 3 ) {
        int lib_status = 0;
        struct bladerf *dev = state->dev;
        char *expanded_path = input_expand_path(argv[2]);

        if (expanded_path == NULL) {
            cli_err(state, NULL,
                    "Unable to expand file path: \"%s\"\n", argv[2]);
            return CLI_RET_INVPARAM;
        }

        if (!strcasecmp(argv[1], "fpga")) {

            printf("\n  Loading fpga from %s...\n", expanded_path);
            lib_status = bladerf_load_fpga(dev, expanded_path);
            if (lib_status == 0) {
                printf("  Done.\n\n");
            }

        } else if (!strcasecmp(argv[1], "fx3")) {

            printf("\n  Flashing firmware from %s...\n\n", expanded_path);
            lib_status = bladerf_flash_firmware(dev, expanded_path);
            if (lib_status == 0) {
                printf("  Done. Cycle power on the device.\n\n");
            }

        } else {
            cli_err(state, argv[0], "  Invalid type: %s\n", argv[1]);
            rv = CLI_RET_INVPARAM;
        }

        free(expanded_path);
        if (lib_status < 0) {
            state->last_lib_error = lib_status;
            rv = CLI_RET_LIBBLADERF;
        }

    } else {
        rv = CLI_RET_NARGS;
    }

    return rv;
}

