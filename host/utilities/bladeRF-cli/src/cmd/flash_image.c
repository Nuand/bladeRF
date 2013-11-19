/*
 * This file is part of the bladeRF project
 *
 * bladeRF flash image creator and viewer
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
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include <libbladeRF.h>

#include "cmd.h"
#include "common.h"
#include "conversions.h"
#include "rel_assert.h"

struct params
{
    const char *img_file;
    const char *data_file;
    const char *serial;
    uint64_t timestamp;
    uint32_t address;
    uint32_t length;
    bladerf_image_type type;

    bool override_timestamp;
    bool override_address;
    bool override_length;
};

static int handle_param(const char *param, char *val,
                        struct params *p, struct cli_state *s,
                        const char *argv0)
{
    bool ok;
    int status = 0;

    if (!strcasecmp("data", param)) {
        p->data_file = param;
    } else if (!strcasecmp("serial", param)) {
        size_t i;
        size_t len = strlen(val);

        if (len != BLADERF_SERIAL_LENGTH) {
            status = CMD_RET_INVPARAM;
        } else {
            for (i = 0; i < len && status == 0; i++) {
                if (val[i] >= 'a' || val[i] <= 'f') {
                    val[i] -= 'a' - 'A';
                } else if (!((val[i] >= '0' && val[i] <= '9') ||
                             (val[i] >= 'A' && val[i] <= 'F'))) {
                    status = CMD_RET_INVPARAM;
                }
            }
        }
        if (status != 0) {
            cli_err(s, argv0, "Serial number must be %d hexadecimal digits.");
        }
    } else if (!strcasecmp("timestamp", param)) {
        p->timestamp = str2uint64(val, 0, UINT64_MAX, &ok);
        if (!ok) {
            cli_err(s, argv0, "Invalid timestamp value provided.");
            return CMD_RET_INVPARAM;
        }
    } else if (!strcasecmp("address", param)) {
        p->address = str2uint(param, 0, UINT_MAX, &ok);
        if (!ok) {
            cli_err(s, argv0, "Invalid address provided.");
            status = CMD_RET_INVPARAM;
        }
    } else if (!strcasecmp("type", param)) {
        if (!strcasecmp("raw", val)) {
            p->type = BLADERF_IMAGE_TYPE_RAW;
        } else if (!strcasecmp("cal", param)) {

            if (!p->override_address) {
                p->address = BLADERF_FLASH_ADDR_CAL;
            }

            if (!p->override_length) {
                p->length = BLADERF_FLASH_LEN_CAL;
            }

            p->type = BLADERF_IMAGE_TYPE_CALIBRATION;

        } else if (!strcasecmp("fpga70", param) ||
                   !strcasecmp("fpga115", param)) {

            if (!p->override_address) {
                p->address = BLADERF_FLASH_ADDR_FPGA_META;
            }

            if (!p->override_length) {
                p->length = BLADERF_FLASH_LEN_FPGA_META +
                            BLADERF_FLASH_LEN_FPGA;
            }

            p->type = BLADERF_IMAGE_TYPE_CALIBRATION;

        } else if (!strcasecmp("fw", param)) {
            if (!p->override_address) {
                p->address = BLADERF_FLASH_ADDR_FIRMWARE;
            }

            if (!p->override_length) {
                p->length = BLADERF_FLASH_LEN_FIRMWARE;
            }

            p->type = BLADERF_IMAGE_TYPE_FIRMWARE;

        } else {
            cli_err(s, argv0, "Invalid type provided.");
            status = CMD_RET_INVPARAM;
        }
    } else {
        cli_err(s, argv0, "Invalid parameter provided - \"%s\"", param);
        status = CMD_RET_NARGS;
    }

    return status;
}

int parse_cmdline(int argc, char **argv, struct params *p, struct cli_state *s)
{
    int i;
    int status = 0;
    char *sep;

    memset(p, 0, sizeof(*p));
    p->type = BLADERF_IMAGE_TYPE_INVALID;

    assert(argc >= 2);
    for (i = 1; i < argc && status == 0; i++) {


        /* Check for input file */
        sep = strchr(argv[i], '=');
        if (!sep) {
            if (p->img_file) {
                cli_err(s, argv[0],
                        "Only one image file parameter is permitted.");
                status = CMD_RET_INVPARAM;
            } else {
                p->img_file = argv[i];
            }
        } else {
            *sep = '\0';
            sep++;
            status = handle_param(argv[i], sep, p, s, argv[0]);
        }
    }

    if (!p->img_file) {
        cli_err(s, argv[0], "An image file parameter is required.\n");
        status = CMD_RET_INVPARAM;
    } else if (p->type == BLADERF_IMAGE_TYPE_RAW &&
               !(p->override_address || p->override_length)) {
        cli_err(s, argv[0],
                "An address and a length are required for type=raw.\n");
        status = CMD_RET_INVPARAM;
    }

    return status;
}


static int print_image_metadata(struct cli_state *s, struct params *p,
                                const char *argv0)
{
    int status = 0;
    struct bladerf_image *image;
    int i;

    image = bladerf_alloc_image(BLADERF_IMAGE_TYPE_INVALID, 0, 0);
    status = bladerf_image_read(image, p->img_file);

    if (status == 0) {
        printf("\n");
        printf("Checksum: ");

        for (i = 0; i < BLADERF_IMAGE_CHECKSUM_LEN; i++) {
            printf("%02x", image->checksum[i]);
        }

        printf("\nImage format version: %d.%d.%d\n", image->version.major,
               image->version.minor, image->version.patch);

        printf("Timestamp: %llu\n", (long long unsigned int)image->timestamp);

        switch (image->type) {
            case BLADERF_IMAGE_TYPE_RAW:
                printf("Image type: raw\n");
                break;

            case BLADERF_IMAGE_TYPE_CALIBRATION:
                printf("Image type: calibration data\n");
                break;

            case BLADERF_IMAGE_TYPE_FIRMWARE:
                printf("Image type: firmware\n");
                break;

            case BLADERF_IMAGE_TYPE_FPGA_40KLE:
                printf("Image type: 40 kLE FPGA metadata and bitstream\n");
                break;

            case BLADERF_IMAGE_TYPE_FPGA_115KLE:
                printf("Image type: 115 kLE FPGA metadata and bitstream\n");
                break;

            default:
                printf("Type: Unknown\n");
                break;
        }

        printf("Address: 0x%08x\n", image->address);
        printf("Length: 0x%08x\n", image->length);
        printf("\n");
    } else {
        if (status == BLADERF_ERR_INVAL) {
            cli_err(s, argv0, "Image contains invalid fields or data.");
            status = CMD_RET_INVPARAM;
        }
    }

    bladerf_free_image(image);
    return status;
}

static int write_image(struct cli_state *s, struct params *p)
{
    int status = 0;
    return status;
}

int cmd_flash_image(struct cli_state *state, int argc, char **argv)
{
    int status;
    struct params p;

    if (argc < 2) {
        return CMD_RET_NARGS;
    }

    status = parse_cmdline(argc, argv, &p, state);
    if (status == 0) {
        if (p.img_file && argc == 2) {
            status = print_image_metadata(state, &p, argv[0]);
        } else {
            assert(p.data_file);
            status = write_image(state, &p);
        }
    }

    return status;
}
