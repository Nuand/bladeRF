/* This file is part of the bladeRF project
 *
 * Copyright (C) 2013  Daniel Gröber <dxld AT darkboxed DOT org>
 * Copyright (C) 2013  Nuand, LLC.
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

#include "cmd.h"
#include "flash_common.h"
#include "interactive.h"
#include "minmax.h"
#include "conversions.h"
#include "rel_assert.h"

#define lib_error(status, ...) do { \
    state->last_lib_error = (status); \
    cli_err(state, argv[0], __VA_ARGS__); \
    status = CMD_RET_LIBBLADERF; \
} while (0)

int cmd_flash_backup(struct cli_state *state, int argc, char **argv)
{
    int status = 0;
    struct bladerf_devinfo info;
    struct bladerf_image *image = NULL;
    bladerf_image_type image_type;
    unsigned int address, length;
    char *filename = NULL;
    bool ok;

    if (argc != 3 && argc != 4) {
        return CMD_RET_NARGS;
    }

    status = flash_check_state(state, argv[0]);
    if (status != 0) {
        return status;
    }

    filename = interactive_expand_path(argv[1]);
    if (!filename) {
        return CMD_RET_MEM;
    }

    if (argc == 3) {
        if (!strcasecmp(argv[2], "cal")) {
            image_type = BLADERF_IMAGE_TYPE_CALIBRATION;
            address = BLADERF_FLASH_ADDR_CALIBRATION;
            length = BLADERF_FLASH_LEN_CALIBRATION;
        } else if (!strcasecmp(argv[2], "fw")) {
            image_type = BLADERF_IMAGE_TYPE_FIRMWARE;
            address = BLADERF_FLASH_ADDR_FIRMWARE;
            length = BLADERF_FLASH_LEN_FIRMWARE;
        } else if (!strcasecmp(argv[2], "fpga40")) {
            image_type = BLADERF_IMAGE_TYPE_FPGA_40KLE;
            address = BLADERF_FLASH_ADDR_FPGA_META;
            length = BLADERF_FLASH_LEN_FPGA_META + BLADERF_FLASH_LEN_FPGA;
        } else if (!strcasecmp(argv[2], "fpga115")) {
            image_type = BLADERF_IMAGE_TYPE_FPGA_115KLE;
            address = BLADERF_FLASH_ADDR_FPGA_META;
            length = BLADERF_FLASH_LEN_FPGA_META + BLADERF_FLASH_LEN_FPGA;
        } else {
            cli_err(state, argv[0], "Invalid image type provided.");
            status = CMD_RET_INVPARAM;
            goto cmd_flash_backup_out;
        }
    } else {
        assert(argc == 4);
        address = str2uint(argv[2], 0, UINT_MAX, &ok);
        if (!ok) {
            cli_err(state, argv[0], "Invalid address provided.");
            goto cmd_flash_backup_out;
        }

        length = str2uint(argv[3], 0, UINT_MAX, &ok);
        if (!ok) {
            cli_err(state, argv[0], "Invalid length provided.");
            goto cmd_flash_backup_out;
        }

        image_type = BLADERF_IMAGE_TYPE_RAW;
    }

    image = bladerf_alloc_image(image_type, address, length);
    if (!image) {
        return CMD_RET_MEM;
    }

    status = bladerf_get_devinfo(state->dev, &info);
    if (status < 0) {
        lib_error(status, "Failed to get serial number");
        goto cmd_flash_backup_out;
    }

    strncpy(image->serial, info.serial, BLADERF_SERIAL_LENGTH);

    status = bladerf_read_flash_unaligned(state->dev, address,
                                          image->data, length);

    if (status < 0) {
        lib_error(status, "Failed to read flash region");
        goto cmd_flash_backup_out;
    }

    status = bladerf_image_write(image, filename);
    if (status < 0) {
        lib_error(status, "Failed to write image file.");
        goto cmd_flash_backup_out;
    }

cmd_flash_backup_out:
    if (image) {
        bladerf_free_image(image);
    }

    if (filename) {
        free(filename);
    }

    return status;
}
