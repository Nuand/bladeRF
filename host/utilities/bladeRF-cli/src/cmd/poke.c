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
#include <libbladeRF.h>

#include "cmd.h"
#include "peekpoke.h"
#include "conversions.h"

int cmd_poke(struct cli_state *state, int argc, char **argv)
{
    /* Valid commands:
        poke dac <address> <value>
        poke lms <address> <value>
        poke si  <address> <value>
    */
    int rv = CLI_RET_OK;
    int status;
    bool ok;
    int (*f)(struct bladerf *, uint8_t, uint8_t) = NULL;
    unsigned int address, value;

    if( argc == 4 ) {

        /* Parse the value */
        value = str2uint( argv[3], 0, MAX_VALUE, &ok );
        if( !ok ) {
            cli_err(state, argv[0],
                    "Invalid value provided (%s)", argv[3]);
            return CLI_RET_INVPARAM;
        }

        /* Are we reading from the DAC? */
        if( strcasecmp( argv[1], "dac" ) == 0 ) {
            /* Parse address */
            address = str2uint( argv[2], 0, DAC_MAX_ADDRESS, &ok );
            if( !ok ) {
                invalid_address(state, argv[0], argv[2]);
                rv = CLI_RET_INVPARAM;
            } else {
                /* TODO: Point function pointer */
                /* f = vctcxo_dac_write */
                f = NULL;
            }
        }

        /* Are we reading from the LMS6002D */
        else if( strcasecmp( argv[1], "lms" ) == 0 ) {
            /* Parse address */
            address = str2uint( argv[2], 0, LMS_MAX_ADDRESS, &ok );
            if( !ok ) {
                invalid_address(state, argv[0], argv[2]);
                rv = CLI_RET_INVPARAM;
            } else {
                f = bladerf_lms_write;
            }
        }

        /* Are we reading from the Si5338? */
        else if( strcasecmp( argv[1], "si" ) == 0 ) {
            /* Parse address */
            address = str2uint( argv[2], 0, SI_MAX_ADDRESS, &ok );
            if( !ok ) {
                invalid_address(state, argv[0], argv[2]);
                rv = CLI_RET_INVPARAM;
            } else {
                f = bladerf_si5338_write;
            }
        }

        /* I guess we aren't reading from anything :( */
        else {
            cli_err(state, argv[0], "%s is not a pokeable device\n", argv[1] );
            rv = CLI_RET_INVPARAM;
        }

        /* Write the value to the address */
        if( rv == CLI_RET_OK && f ) {
            status = f( state->dev, (uint8_t)address, (uint8_t)value );
            if (status < 0) {
                state->last_lib_error = status;
                rv = CLI_RET_LIBBLADERF;
            } else {
                printf( "\n  0x%2.2x: 0x%2.2x\n", address, value );
                if (f == bladerf_lms_write) {
                    uint8_t readback;
                    int status = bladerf_lms_read(state->dev,
                                                  (uint8_t)address,
                                                  &readback);
                    if (status == 0) {
                        lms_reg_info(address, readback);
                        putchar('\n'); /* To be consistent with peek output */
                    }
                }
            }
        }

    } else {
        cli_err(state, argv[0], "Invalid number of arguments (%d)\n", argc);
        rv = CLI_RET_INVPARAM;
    }
    return rv;
}

