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
#include "rel_assert.h"
#include <limits.h>
#include <stdbool.h>
#include <conversions.h>
#include "cmd.h"

int cmd_erase(struct cli_state *state, int argc, char **argv)
{
    int status;
    int addr, len;
    int eb_offset, n_ebs;
    bool ok;

    if (argc != 3) {
        return CLI_RET_NARGS;
    }

    eb_offset = str2uint(argv[1], 0, INT_MAX, &ok);
    if(!ok) {
        cli_err(state, argv[0],
                "Invalid value for \"eb_offset\" (%s)\n", argv[1]);
        return CLI_RET_INVPARAM;
    }

    n_ebs = str2uint(argv[2], 0, INT_MAX, &ok);
    if(!ok) {
        cli_err(state, argv[0], "Invalid value for \"n_ebs\" (%s)\n", argv[2]);
        return CLI_RET_INVPARAM;
    }

    addr = eb_offset * BLADERF_FLASH_EB_SIZE;
    len  = n_ebs * BLADERF_FLASH_EB_SIZE;

    status = bladerf_erase_flash(state->dev, addr, len);

    if (status >= 0) {
        printf("\n  Erased %d bytes at 0x%02x\n\n", status, addr);
        return CLI_RET_OK;
    } else {
        state->last_lib_error = status;
        return CLI_RET_LIBBLADERF;
    }
}
