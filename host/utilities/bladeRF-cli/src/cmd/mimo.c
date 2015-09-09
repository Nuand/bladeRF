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
#include "common.h"
#include <string.h>
#include "conversions.h"
#include "cmd.h"

int cmd_mimo(struct cli_state *state, int argc, char **argv)
{
    int status = 0;

    if (!strcasecmp(argv[1], "slave")) {
        uint8_t val;
        if (argc != 2) {
            return CLI_RET_NARGS;
        }

        status = bladerf_si5338_write(state->dev, 6, 4);
        if (status != 0) {
            goto out;
        }

        status = bladerf_si5338_write(state->dev, 28, 0x2b);
        if (status != 0) {
            goto out;
        }

        status = bladerf_si5338_write(state->dev, 29, 0x28);
        if (status != 0) {
            goto out;
        }

        status = bladerf_si5338_write(state->dev, 30, 0xa8);
        if (status != 0) {
            goto out;
        }

        /* Turn off any SMB connector output */
        status = bladerf_si5338_read(state->dev, 39, &val);
        if (status != 0) {
            goto out;
        }

        val &= ~(1);
        status = bladerf_si5338_write(state->dev, 39, val);
        if (status != 0) {
            goto out;
        }

        printf("\n  Successfully set device to slave MIMO mode.\n\n");

    } else if (!strcmp(argv[1], "master")) {
        if (argc == 2) {
            /* Output the 38.4MHz clock on the SMB connector. */
            uint8_t val ;
            status = bladerf_si5338_read(state->dev, 39, &val);

            if (status != 0) {
                goto out;
            }

            val |= 1;
            status = bladerf_si5338_write(state->dev, 39, val);
            if (status != 0) {
                goto out;
            }

            status = bladerf_si5338_write(state->dev, 34, 0x22);
            if (status != 0) {
                goto out;
            }

            printf("\n  Successfully set device to master MIMO mode.\n\n");
        } else if (argc == 3) {
            /* Output a specific frequency on the SMB connector. */

            unsigned int freq, actual;
            bool ok;
            freq = str2uint_suffix(argv[2], BLADERF_SMB_FREQUENCY_MIN,
                                   BLADERF_SMB_FREQUENCY_MAX, freq_suffixes,
                                   NUM_FREQ_SUFFIXES, &ok);
            if (!ok) {
                cli_err(state, argv[0], "Invalid SMB frequency (%s)\n",
                        argv[2]);
                return CLI_RET_INVPARAM;
            } else {
                status = bladerf_set_smb_frequency(state->dev, freq, &actual);
                if(status >= 0) {
                    printf("\n  Set device to master MIMO, "
                           "req freq:%9uHz, actual:%9uHz\n\n", freq, actual);
                } else {
                    goto out;
                }
            }
        } else {
            return CLI_RET_NARGS;
        }

    } else {
        cli_err(state, argv[0], "Invalid mode: %s\n", argv[2]);
    }

out:
    if (status != 0) {
        state->last_lib_error = status;
        status = CLI_RET_LIBBLADERF;
    }

    return status;
}
