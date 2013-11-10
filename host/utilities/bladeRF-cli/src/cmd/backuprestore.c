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

#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>


#define rv_error(rv, ...) do {                              \
        state->last_lib_error = (rv);                       \
        cli_err(state, argv[0], __VA_ARGS__);               \
        rv = CMD_RET_LIBBLADERF;                            \
    } while(0)

struct options {
    char *file;
    uint32_t address, len;
    int user_set_address;
};

static int parse_argv(struct cli_state *state, int argc, char **argv,
                      struct options *opt)
{
    int rv;

    if(argc < 2 || argc > 3)
        return CMD_RET_NARGS;

    opt->user_set_address = 0;

    if(argc >= 2) {
        if ((opt->file = interactive_expand_path(argv[1])) == NULL) {
            cli_err(state, argv[0], "Unable to expand file path: \"%s\"",
                    argv[1]);
            return CMD_RET_INVPARAM;
        }

        opt->address = CAL_PAGE;
        opt->len = CAL_BUFFER_SIZE;
    }

    if(argc == 3) {
        char *address, *len;

        address = strtok(argv[2], ",");
        if(!address)
            return CMD_RET_INVPARAM;

        len = strtok(NULL, ",");
        if(!len)
            return CMD_RET_INVPARAM;

        rv = sscanf(address, "0x%x", &opt->address);
        if(rv < 1)
            return CMD_RET_INVPARAM;

        rv = sscanf(len, "0x%x", &opt->len);
        if(rv < 1)
            return CMD_RET_INVPARAM;

        opt->user_set_address = 1;
    }

    if (!cli_device_is_opened(state)) {
        return CMD_RET_NODEV;
    }

    return 0;
}

int cmd_backup(struct cli_state *state, int argc, char **argv)
{
    int rv;
    char *buf = NULL;
    struct bladerf_devinfo info;
    struct bladerf_image img;
    struct options opt = { 0 };

    rv = parse_argv(state, argc, argv, &opt);
    if(rv < 0)
        return rv;

    if(!state->dev) {
        rv = CMD_RET_NODEV;
        goto out;
    }

    buf = malloc(opt.len);

    rv = bladerf_get_devinfo(state->dev, &info);
    if(rv < 0) {
        rv_error(rv, "Getting device info failed!");
        goto out;
    }

    rv = bladerf_read_flash_unaligned(state->dev,
                                      opt.address,
                                      (uint8_t*)buf,
                                      opt.len);
    if(rv < 0) {
        rv_error(rv, "Reading flash data failed!");
        goto out;
    }

    rv = bladerf_image_fill(&img,
                            BLADERF_IMAGE_TYPE_RAW,
                            buf,
                            opt.len);
    if(rv < 0) {
        rv_error(rv, "Initializing flash image failed!");
        goto out;
    }

    uint32_t address_be = HOST_TO_BE32(opt.address);
    rv = bladerf_image_meta_add(&img.meta,
                                BLADERF_IMAGE_META_ADDRESS,
                                (void*) &address_be,
                                sizeof(address_be));
    if(rv < 0) {
        rv_error(rv, "Adding metadata (address) falied!");
        goto out;
    }

    rv = bladerf_image_meta_add(&img.meta,
                                BLADERF_IMAGE_META_SERIAL,
                                (void*) info.serial,
                                sizeof(info.serial));
    if(rv < 0) {
        rv_error(rv, "Adding metadata (serial) falied!");
        goto out;
    }


    rv = bladerf_image_write(&img, opt.file);
    if(rv < 0) {
        rv_error(rv, "Writing flash image to file failed!");
        goto out;
    }

    rv = CMD_RET_OK;

out:
    if(buf)
        free(buf);

    return rv;
}

static ssize_t metadata_get(struct cli_state *state,
                            struct options *opt,
                            struct bladerf_image *img)
{
    int rv;

    if(!opt->user_set_address) {
        uint32_t address_be;
        rv = bladerf_image_meta_get(&img->meta,
                                    BLADERF_IMAGE_META_ADDRESS,
                                    (void*) &address_be,
                                    sizeof(address_be));
        if(rv < 0)
            return rv;

        opt->address = BE32_TO_HOST(address_be);
        opt->len = img->len;
    }

    return CMD_RET_OK;
}

int cmd_restore(struct cli_state *state, int argc, char **argv)
{
    int rv;
    char *buf = NULL;
    struct bladerf_devinfo info;
    struct bladerf_image img;
    struct options opt = { 0 };

    rv = parse_argv(state, argc, argv, &opt);
    if(rv < 0)
        return rv;

    if(!state->dev) {
        rv = CMD_RET_NODEV;
        goto out;
    }

    rv = bladerf_get_devinfo(state->dev, &info);
    if(rv < 0) {
        rv_error(rv, "Getting device info failed!");
        goto out;
    }

    rv = bladerf_image_read(&img, opt.file);
    if(rv < 0) {
        rv_error(rv, "Reading flash image from file failed!");
        goto out;
    }

    rv = metadata_get(state, &opt, &img);
    if(rv < 0) {
        rv_error(rv, "Parsing image metadata failed!");
        goto out;
    }

    rv = bladerf_program_flash_unaligned(state->dev,
                                       opt.address,
                                       (uint8_t*)img.data,
                                       uint_min(img.len, opt.len));
    if(rv < 0) {
        cli_err(state, argv[0],
"Restoring flash region failed!\n"
"\n"
"This is Pretty Bad^{TM} you should reflash the FX3 image just to be\n"
"sure it didn't get damaged.\n"
"\n"
"See: https://github.com/Nuand/bladeRF/wiki/Upgrading-bladeRF-firmware\n"
        );
        goto out;
    }

    rv = CMD_RET_OK;

out:
    if(buf)
        free(buf);

    return rv;
}
