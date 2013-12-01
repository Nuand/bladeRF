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
#include <libbladeRF.h>

#include "cmd.h"
#include "conversions.h"

#define MAX_GAIN_SETTING ((1<<16) -1)

int cmd_dc_gain(struct cli_state *state, int argc, char **argv)
{
    int rv = CMD_RET_OK;
    int status;
    bool ok = true;
    int (*f)(struct bladerf *, int16_t, int16_t);
    int dc_gain_i = 0,dc_gain_q = 0;

    if (!cli_device_is_opened(state)) {
        return CMD_RET_NODEV;
    }

    if( argc == 3 ) {

        /* Parse the value */
        if( argc == 3 ) {
            dc_gain_i = str2int( argv[1], -MAX_GAIN_SETTING ,MAX_GAIN_SETTING, &ok );
            if( !ok ) {
                cli_err(state, argv[0],
                        "Invalid I gain setting provided (%s)", argv[1]);
                return CMD_RET_INVPARAM;
            }
            dc_gain_q = str2int( argv[2], (-MAX_GAIN_SETTING) ,MAX_GAIN_SETTING, &ok );
            if( !ok ) {
                cli_err(state, argv[0],
                        "Invalid Q gain setting provided (%s)", argv[2]);
                return CMD_RET_INVPARAM;
            }
            else
            {
                f = bladerf_config_dc_gain_write;
            }
        }

        /* Write the value to the address */
        if( rv == CMD_RET_OK && f ) {
            status = f( state->dev, dc_gain_i, dc_gain_q );
            if (status < 0) {
                state->last_lib_error = status;
                rv = CMD_RET_LIBBLADERF;
            } else {
                //printf( "  0x%2.2x: 0x%2.2x\n", address, value );
            }
        }

    } 
    else if(argc == 1) {
        /* If the command is issued with no additional arguements read the current value*/

    }
    else {
        cli_err(state, argv[0], "Invalid number of arguments (%d)\n", argc);
        rv = CMD_RET_INVPARAM;
    }
    return rv;
}

