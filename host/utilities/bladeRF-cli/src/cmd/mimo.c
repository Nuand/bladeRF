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
#include "common.h"
#include <string.h>

int cmd_mimo(struct cli_state *state, int argc, char **argv)
{
    int status = 0;

    if (state->dev == NULL) {
        printf("  No device is currently opened\n");
        return 0;
    }

    if (argc >= 2) {
        if (argc >= 3 && !strcmp(argv[1], "clk")) {
            if (!strcmp(argv[2], "slave")) {
                status |= bladerf_si5338_write(state->dev, 6, 4);
                status |= bladerf_si5338_write(state->dev, 28, 0x2b);
                status |= bladerf_si5338_write(state->dev, 29, 0x28);
                status |= bladerf_si5338_write(state->dev, 30, 0xa8);
                if (status)
                    printf("Could not set device to slave MIMO mode\n");
                else
                    printf("Successfully set device to slave MIMO mode\n");
            } else if (!strcmp(argv[2], "master")) {
                status |= bladerf_si5338_write(state->dev, 39, 1);
                status |= bladerf_si5338_write(state->dev, 34, 0x22);
                if (status)
                    printf("Could not set device to master MIMO mode\n");
                else
                    printf("Successfully set device to master MIMO mode\n");
            }
        }
    }
    return status;
}
