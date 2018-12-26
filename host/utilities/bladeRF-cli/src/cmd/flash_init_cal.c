/*
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2013  Daniel Gröber <dxld AT darkboxed DOT org>
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
#include <limits.h>
#include <string.h>
#include <stdlib.h>

#include <libbladeRF.h>
#include <bladeRF.h>

#include "conversions.h"
#include "cmd.h"
#include "input.h"
#include "minmax.h"
#include "rel_assert.h"

int str2fpga(const char *str, bladerf_fpga_size *fpga_size)
{
    if (!strcmp(str, "40")) {
        *fpga_size = BLADERF_FPGA_40KLE;
        return 0;
    } else if (!strcmp(str, "115")) {
        *fpga_size = BLADERF_FPGA_115KLE;
        return 0;
    } else if (!strcmp(str, "49") || !strcmp(str, "A4")) {
        *fpga_size = BLADERF_FPGA_A4;
        return 0;
    } else if (!strcmp(str, "301") || !strcmp(str, "A9")) {
        *fpga_size = BLADERF_FPGA_A9;
        return 0;
    } else {
        return CLI_RET_INVPARAM;
    }
}

int cmd_flash_init_cal(struct cli_state *state, int argc, char **argv)
{
    int rv;
    bool ok;
    uint16_t dac;
    bladerf_fpga_size fpga_size;
    struct bladerf_image *image = NULL;

    if(argc != 3 && argc != 4) {
        return CLI_RET_NARGS;
    }

    rv = str2fpga(argv[1], &fpga_size);
    if (rv != 0) {
        cli_err(state, argv[0], "Invalid FPGA provided.\n");
        return rv;
    }

    dac = str2uint(argv[2], 0, 0xffff, &ok);
    if(!ok) {
        cli_err(state, argv[0], "Invalid VCTCXO trim value provided.\n");
        return CLI_RET_INVPARAM;
    }

    image = bladerf_alloc_cal_image(state->dev, fpga_size, dac);
    if (!image) {
        return CLI_RET_MEM;
    }

    if (argc == 3) {
        rv = bladerf_erase_flash_bytes(state->dev, BLADERF_FLASH_ADDR_CAL,
                                       BLADERF_FLASH_BYTE_LEN_CAL);
        if (rv != 0) {
            goto cmd_flash_init_cal_out;
        }

        rv = bladerf_write_flash_bytes(state->dev, image->data,
                                       image->address, image->length);
        if(rv < 0) {
            cli_err(state, argv[0],
            "Failed to write calibration data.\n"
            "\n"
            "    This may have resulted in a corrupted flash. If the device fails to\n"
            "    boot at the next power cycle, re-flash the firmware.\n"
            "\n"
            "    See the following page for more information:\n"
            "      https://github.com/Nuand/bladeRF/wiki/Upgrading-bladeRF-firmware\n"
            );

            state->last_lib_error = rv;
            rv = CLI_RET_LIBBLADERF;
        } else {
            rv = 0;
        }
    } else {
        char *filename;
        assert(argc == 4);

        filename = input_expand_path(argv[3]);
        rv = bladerf_image_write(state->dev, image, filename);
        free(filename);
    }

cmd_flash_init_cal_out:
    bladerf_free_image(image);
    return rv;
}
