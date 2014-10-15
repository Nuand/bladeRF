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
#include <stdbool.h>
#include <libbladeRF.h>

#include "common.h"
#include "cmd.h"
#include "peekpoke.h"
#include "conversions.h"

static inline bool matches_lms6002d(const char *str)
{
    return strcasecmp("lms", str) == 0 || strcasecmp("lms6002d", str) == 0;
}

static inline bool matches_si5338(const char *str)
{
    return strcasecmp("si", str) == 0 || strcasecmp("si5338", str) == 0;
}

int cmd_peek(struct cli_state *state, int argc, char **argv)
{
    /* Valid commands:
        peek dac <address> [num addresses]
        peek lms <address> [num addresses]
        peek si  <address> [num addresses]
    */
    int rv = CLI_RET_OK;
    bool ok;
    int (*f)(struct bladerf *, uint8_t, uint8_t *);
    unsigned int count, address, max_address;

    if( argc == 3 || argc == 4 ) {
        count = 1;

        /* Parse the number of addresses */
        if( argc == 4 ) {
            count = str2uint( argv[3], 0, MAX_NUM_ADDRESSES, &ok );
            if( !ok ) {
                cli_err(state, argv[0],
                        "Invalid number of addresses provided (%s)\n", argv[3]);
                return CLI_RET_INVPARAM;
            }
        }

        /* Are we reading from the LMS6002D */
        if (matches_lms6002d(argv[1])) {
            /* Parse address */
            address = str2uint( argv[2], 0, LMS_MAX_ADDRESS, &ok );
            if( !ok ) {
                invalid_address(state, argv[0], argv[2]);
                rv = CLI_RET_INVPARAM;
            } else {
                f = bladerf_lms_read;
                max_address = LMS_MAX_ADDRESS;
            }
        }

        /* Are we reading from the Si5338? */
        else if (matches_si5338(argv[1])) {
            /* Parse address */
            address = str2uint( argv[2], 0, SI_MAX_ADDRESS, &ok );
            if( !ok ) {
                invalid_address(state, argv[0], argv[2]);
                rv = CLI_RET_INVPARAM;
            } else {
                f = bladerf_si5338_read;
                max_address = SI_MAX_ADDRESS;
            }
        }

        /* I guess we aren't reading from anything :( */
        else {
            cli_err(state, argv[0], "%s is not a peekable device\n", argv[1]);
            rv = CLI_RET_INVPARAM;
        }

        /* Loop over the addresses and output the values */
        if( rv == CLI_RET_OK && f ) {
            int status;
            uint8_t val;

            putchar('\n');

            for(; count > 0 && address < max_address; count-- ) {
                status = f( state->dev, (uint8_t)address, &val );
                if (status < 0) {
                    state->last_lib_error = status;
                    rv = CLI_RET_LIBBLADERF;
                } else {
                    printf( "  0x%2.2x: 0x%2.2x\n", address, val );

                    if (matches_lms6002d(argv[1])) {
                        lms_reg_info(address, val);
                    }

                    address++;
                    putchar('\n');
                }
            }
        }

    } else {
        cli_err(state, argv[0], "Invalid number of arguments (%d)\n",  argc);
        rv = CLI_RET_INVPARAM;
    }
    return rv;
}

