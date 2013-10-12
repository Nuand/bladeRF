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
#include "rel_assert.h"
#include <limits.h>
#include <stdbool.h>
#include <conversions.h>
#include "cmd.h"

int cmd_erase(struct cli_state *state, int argc, char **argv)
{
    int status;
    int addr, len;
    int sector_offset, n_sectors;
    bool ok;

    if (!cli_device_is_opened(state)) {
        return CMD_RET_NODEV;
    }

    if (argc != 3) {
        return CMD_RET_NARGS;
    }

    sector_offset = str2uint(argv[1], 0, INT_MAX, &ok);
    if(!ok) {
        cli_err(state, argv[0], "Invalid value for \"sector_offset\" (%s)", argv[1]);
        return CMD_RET_INVPARAM;
    }

    n_sectors = str2uint(argv[2], 0, INT_MAX, &ok);
    if(!ok) {
        cli_err(state, argv[0], "Invalid value for \"n_sectors\" (%s)", argv[2]);
        return CMD_RET_INVPARAM;
    }

    addr = sector_offset * BLADERF_FLASH_SECTOR_SIZE;
    len  = n_sectors * BLADERF_FLASH_SECTOR_SIZE;

    status = bladerf_erase_flash(state->dev, addr, len);

    if (status >= 0) {
        printf("Erased %d bytes at 0x%02x\n", status, addr);
        return CMD_RET_OK;
    } else {
        state->last_lib_error = status;
        return CMD_RET_LIBBLADERF;
    }
}
