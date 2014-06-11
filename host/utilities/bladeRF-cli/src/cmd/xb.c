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

int cmd_xb(struct cli_state *state, int argc, char **argv)
{
    int status = 0;

    if (state->dev == NULL) {
        printf("  No device is currently opened\n");
        return 0;
    }

    if (argc >= 2) {
        if (argc >= 3 && !strcmp(argv[1], "enable")) {
            if (!strcmp(argv[2], "200")) {
                printf("  Enabling transverter board\n");
                status = bladerf_expansion_attach(state->dev, BLADERF_XB_200);
                if (status) {
                    return status;
                }
                printf("  Transverter board successfully enabled\n");
            }
        }
    }
    return status;
}
