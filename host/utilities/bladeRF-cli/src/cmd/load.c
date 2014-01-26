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
#include "cmd.h"

/* FIXME Including interactive routines here is an indicator that this
         design needs some refactoring? */
#include "interactive.h"

static int load_fpga(struct cli_state *s, char *file)
{
    char *expanded_path;
    int cmd_status = 0;
    int lib_status;

    if ((expanded_path = interactive_expand_path(file)) == NULL) {
        cli_err(s, "Unable to expand FPGA file path: \"%s\"", file);
        cmd_status = CMD_RET_INVPARAM;
    } else {
        printf("Loading fpga from %s...\n", expanded_path);
        lib_status = bladerf_load_fpga(s->dev, expanded_path);

        if (lib_status < 0) {
            s->last_lib_error = lib_status;
            cmd_status = CMD_RET_LIBBLADERF;
        } else {
            printf("Done.\n");
        }

        free(expanded_path);
    }

    return cmd_status;
}

static int load_fx3(struct cli_state *s, char *file)
{
    char *expanded_path;
    int cmd_status = 0;
    int lib_status;

    if ((expanded_path = interactive_expand_path(file)) == NULL) {
        cli_err(s, "Unable to expand firmware file path: \"%s\"", file);
        cmd_status = CMD_RET_INVPARAM;
    } else {
        printf("Flashing firmware from %s...\n", expanded_path);
        lib_status = bladerf_flash_firmware(s->dev, expanded_path);

        if (lib_status < 0) {
            s->last_lib_error = lib_status;
            cmd_status = CMD_RET_LIBBLADERF;
        } else {
            printf("Done. Cycle power on the device.\n");
        }

        free(expanded_path);
    }

    return cmd_status;
}

int cmd_load(struct cli_state *state, int argc, char **argv)
{
    /* Valid commands:
        load fpga <filename>
        load fx3 <filename>
    */
    int rv = CMD_RET_OK ;

    if (!cli_device_is_opened(state)) {
        return CMD_RET_NODEV;
    } else if (cli_device_is_streaming(state)) {
        return CMD_RET_BUSY;
    }

    if ( argc == 3 ) {
        if( strcasecmp( argv[1], "fpga" ) == 0 ) {
            rv = load_fpga(state, argv[2]);
        } else if( strcasecmp( argv[1], "fx3" ) == 0 ) {
            rv = load_fx3(state, argv[2]);
        } else {
            cli_err(state, argv[0],
                    "\"%s\" is not a valid programming target\n", argv[1]) ;
            rv = CMD_RET_INVPARAM;
        }
    } else {
        rv = CMD_RET_NARGS;
    }

    return rv;
}

