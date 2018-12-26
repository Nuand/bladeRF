/*
 * This file is part of the bladeRF project
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
#include "input.h"
#include "minmax.h"
#include "conversions.h"

/* Previously BLADERF_FLASH_BYTE_LEN_FPGA. TODO: don't hardcode this */
static uint32_t const LEGACY_FLASH_BYTE_LEN_FPGA = 0x00370000;

struct options {
    char *file;
    uint32_t address, len;
    bool override_defaults;
};

static int parse_argv(struct cli_state *state, int argc, char **argv,
                      struct options *opt)
{
    bool ok;

    if (argc != 2 && argc != 4)
        return CLI_RET_NARGS;

    if (argc >= 2) {
        if ((opt->file = input_expand_path(argv[1])) == NULL) {
            cli_err(state, argv[0], "Unable to expand file path: \"%s\"\n",
                    argv[1]);
            return CLI_RET_INVPARAM;
        }

        opt->address = CAL_PAGE;
        opt->len = CAL_BUFFER_SIZE;
        opt->override_defaults = false;
    }

    if (argc == 4) {
        opt->address = str2uint(argv[2], 0, UINT_MAX, &ok);
        if (!ok) {
            cli_err(state, argv[0], "Invalid address provided\n");
            return CLI_RET_INVPARAM;
        }

        opt->len = str2uint(argv[3], 0, UINT_MAX, &ok);
        if (!ok) {
            cli_err(state, argv[0], "Invalid length provided\n");
            return CLI_RET_INVPARAM;
        }

        opt->override_defaults = true;
    }

    return 0;
}

static inline int erase_region(struct bladerf *dev, struct bladerf_image *img,
                               uint32_t addr, uint32_t len)
{
    switch (img->type) {
        case BLADERF_IMAGE_TYPE_FIRMWARE:
            return bladerf_erase_flash_bytes(dev, BLADERF_FLASH_ADDR_FIRMWARE,
                                             BLADERF_FLASH_BYTE_LEN_FIRMWARE);

        case BLADERF_IMAGE_TYPE_FPGA_40KLE:
        case BLADERF_IMAGE_TYPE_FPGA_115KLE:
            return bladerf_erase_flash_bytes(dev, BLADERF_FLASH_ADDR_FPGA,
                                             LEGACY_FLASH_BYTE_LEN_FPGA);

        case BLADERF_IMAGE_TYPE_FPGA_A4:
            return bladerf_erase_flash_bytes(dev, BLADERF_FLASH_ADDR_FPGA,
                                             0x290000);

        case BLADERF_IMAGE_TYPE_FPGA_A9:
            return bladerf_erase_flash_bytes(dev, BLADERF_FLASH_ADDR_FPGA,
                                             0xC50000);

        case BLADERF_IMAGE_TYPE_CALIBRATION:
            return bladerf_erase_flash_bytes(dev, BLADERF_FLASH_ADDR_CAL,
                                             BLADERF_FLASH_BYTE_LEN_CAL);

        case BLADERF_IMAGE_TYPE_RAW:
            return bladerf_erase_flash_bytes(dev, addr, len);

        default:
            return BLADERF_ERR_INVAL;
    }
}

int cmd_flash_restore(struct cli_state *state, int argc, char **argv)
{
    int rv;
    struct bladerf_image *image = NULL;
    struct options opt;
    uint32_t addr, len;

    memset(&opt, 0, sizeof(opt));

    rv = parse_argv(state, argc, argv, &opt);
    if (rv < 0)
        return rv;

    image = bladerf_alloc_image(state->dev, BLADERF_IMAGE_TYPE_INVALID, 0, 0);
    if (!image) {
        rv = CLI_RET_MEM;
        goto cmd_flash_restore_out;
    }

    rv = bladerf_image_read(image, opt.file);
    if (rv < 0) {
        state->last_lib_error = rv;
        rv = CLI_RET_LIBBLADERF;
        goto cmd_flash_restore_out;
    }

    if (opt.override_defaults) {
        addr = opt.address;
        len = u32_min(opt.len, image->length);

        if (len < opt.len) {
            printf("  Warning: Reduced length because only %u bytes are in "
                   "the image.\n", opt.len);
        }

    } else {
        addr = image->address;
        len = image->length;
    }

    rv = erase_region(state->dev, image, addr, len);
    if (rv < 0) {
        state->last_lib_error = rv;
        rv = CLI_RET_LIBBLADERF;
        goto cmd_flash_restore_out;
    }

    rv = bladerf_write_flash_bytes(state->dev, image->data, addr, len);
    if (rv < 0) {
        cli_err(state, argv[0],
        "Failed to restore flash region.\n"
        "\n"
        "Flash contents may be corrupted. If the device fails to boot at successive\n"
        "power-ons, see the following wiki page for recovery instructions:"
        "  https://github.com/Nuand/bladeRF/wiki/Upgrading-bladeRF-firmware"
        );
        goto cmd_flash_restore_out;
    }

    rv = CLI_RET_OK;

cmd_flash_restore_out:
    free(opt.file);
    bladerf_free_image(image);
    return rv;
}
