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

#include "flash_common.h"
#include "cmd.h"
#include "interactive.h"
#include "minmax.h"
#include "conversions.h"

#define rv_error(rv, ...) do {                              \
        state->last_lib_error = (rv);                       \
        cli_err(state, argv[0], __VA_ARGS__);               \
        rv = CMD_RET_LIBBLADERF;                            \
    } while(0)

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
        return CMD_RET_NARGS;

    if (argc >= 2) {
        if ((opt->file = interactive_expand_path(argv[1])) == NULL) {
            cli_err(state, argv[0], "Unable to expand file path: \"%s\"",
                    argv[1]);
            return CMD_RET_INVPARAM;
        }

        opt->address = CAL_PAGE;
        opt->len = CAL_BUFFER_SIZE;
        opt->override_defaults = false;
    }

    if (argc == 4) {
        opt->address = str2uint(argv[2], 0, UINT_MAX, &ok);
        if (!ok) {
            cli_err(state, argv[0], "Invalid address provided");
            return CMD_RET_INVPARAM;
        }

        opt->len = str2uint(argv[3], 0, UINT_MAX, &ok);
        if (!ok) {
            cli_err(state, argv[0], "Invalid length provided");
            return CMD_RET_INVPARAM;
        }

        opt->override_defaults = true;
    }

    return 0;
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

    rv = flash_check_state(state, argv[0]);
    if (rv != 0) {
        goto cmd_flash_restore_out;
    }

    image = bladerf_alloc_image(BLADERF_IMAGE_TYPE_INVALID, 0, 0);
    if (!image) {
        rv = CMD_RET_MEM;
        goto cmd_flash_restore_out;
    }

    rv = bladerf_image_read(image, opt.file);
    if (rv < 0) {
        rv_error(rv, "Failed to read flash image from file.");
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

    rv = bladerf_program_flash_unaligned(state->dev, addr, image->data, len);
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

    rv = CMD_RET_OK;

cmd_flash_restore_out:
    free(opt.file);
    bladerf_free_image(image);
    return rv;
}
