/*
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2013  Daniel Gröber <dxld AT darkboxed DOT org>
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
#include "cmd.h"
#include "interactive.h"
#include <minmax.h>

#include <libbladeRF.h>
#include <bladeRF.h>
#include <conversions.h>

#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>

int cmd_init_cal(struct cli_state *state, int argc, char **argv)
{
    int rv;
    bool ok;
    uint16_t dac;
    char buf[CAL_BUFFER_SIZE];

    if(argc != 3)
        return CMD_RET_NARGS;

    if(strcmp(argv[1], "40") != 0 && strcmp(argv[1], "115") != 0) {
        cli_err(state, argv[0], "Invalid FPGA size: \"%s\". Size must be either '40' or '115'.", argv[1]);
        return CMD_RET_INVPARAM;
    }

    dac = str2uint(argv[2], 0, 0xffff, &ok);
    if(!ok) {
        cli_err(state, argv[0], "VCTCXO trim value invalid");
        return CMD_RET_INVPARAM;
    }

    if (!cli_device_is_opened(state)) {
        return CMD_RET_NODEV;
    }

    rv = bladerf_make_cal_region(argv[1], dac, buf, sizeof(buf));
    if(rv < 0) {
        state->last_lib_error = rv;
        cli_err(state, argv[0], "Failed to create calibration data");
        return CMD_RET_LIBBLADERF;
    }

    rv = bladerf_program_flash_unaligned(
        state->dev,
        CAL_PAGE * 256,
        (uint8_t*)buf,
        CAL_BUFFER_SIZE
    );
    if(rv < 0) {
        state->last_lib_error = rv;
        cli_err(state, argv[0],
                "Failed to write calibration data.\n"
                "\n"
                "This may have resulted in a corrupted flash. If the device fails to\n"
                "boot at the next power cycle, re-flash the firmware.\n"
                "\n"
                "See the following page for more information:\n"
                "  https://github.com/Nuand/bladeRF/wiki/Upgrading-bladeRF-firmware\n"
        );
        return CMD_RET_LIBBLADERF;
    }

    return 0;
}
