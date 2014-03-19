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
#include <time.h>
#include <inttypes.h>
#include <libbladeRF.h>

#include "cmd.h"
#include "common.h"
#include "conversions.h"
#include "rel_assert.h"
#include "interactive.h"

struct params
{
    char *img_file;
    char *data_file;
    char serial[BLADERF_SERIAL_LENGTH];
    uint32_t address;
    uint32_t max_length;
    bladerf_image_type type;

    bool override_timestamp;
    bool override_address;
};

static int handle_param(const char *param, char *val,
                        struct params *p, struct cli_state *s,
                        const char *argv0)
{
    bool ok;
    int status = 0;

    if (!strcasecmp("data", param)) {
        free(p->data_file);
        p->data_file = interactive_expand_path(val);
    } else if (!strcasecmp("serial", param)) {
        size_t i;
        size_t len = strlen(val);

        if (len != BLADERF_SERIAL_LENGTH - 1) {
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
            cli_err(s, argv0, "Serial number must be %d hexadecimal digits.",
                    BLADERF_SERIAL_LENGTH - 1);
        }
    } else if (!strcasecmp("address", param)) {
        p->address = str2uint(param, 0, UINT_MAX, &ok);
        if (!ok) {
            cli_err(s, argv0, "Invalid address provided.");
            status = CMD_RET_INVPARAM;
        }
    } else if (!strcasecmp("type", param)) {
        if (!strcasecmp("raw", val)) {
            p->max_length = UINT_MAX;
            p->type = BLADERF_IMAGE_TYPE_RAW;
        } else if (!strcasecmp("cal", val)) {

            if (!p->override_address) {
                p->address = BLADERF_FLASH_ADDR_CALIBRATION;
            }

            p->max_length = BLADERF_FLASH_LEN_CALIBRATION;
            p->type = BLADERF_IMAGE_TYPE_CALIBRATION;

        } else if (!strcasecmp("fpga40", val) ||
                   !strcasecmp("fpga115", val)) {

            if (!p->override_address) {
                p->address = BLADERF_FLASH_ADDR_FPGA_META;
            }

            p->max_length = BLADERF_FLASH_LEN_FPGA_META +
                            BLADERF_FLASH_LEN_FPGA;

            if (!strcasecmp("fpga40", val)) {
                p->type = BLADERF_IMAGE_TYPE_FPGA_40KLE;
            } else {
                p->type = BLADERF_IMAGE_TYPE_FPGA_115KLE;
            }

        } else if (!strcasecmp("fw", val) || !strcasecmp("firmware", val)) {
            if (!p->override_address) {
                p->address = BLADERF_FLASH_ADDR_FIRMWARE;
            }

            p->max_length = BLADERF_FLASH_LEN_FIRMWARE;
            p->type = BLADERF_IMAGE_TYPE_FIRMWARE;

        } else {
            cli_err(s, argv0, "Invalid type provided.");
            status = CMD_RET_INVPARAM;
        }
    } else {
        cli_err(s, argv0, "Invalid parameter provided - \"%s\"", param);
        status = CMD_RET_INVPARAM;
    }

    return status;
}

int parse_cmdline(int argc, char **argv, struct params *p, struct cli_state *s)
{
    int i;
    int status = 0;
    char *sep;

    memset(p, 0, sizeof(*p));
    memset(p->serial, '0', BLADERF_SERIAL_LENGTH - 1);
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
                p->img_file = interactive_expand_path(argv[i]);
            }
        } else {
            *sep = '\0';
            sep++;
            status = handle_param(argv[i], sep, p, s, argv[0]);
        }
    }

    if (status == 0) {
        if (!p->img_file) {
            cli_err(s, argv[0], "An image file parameter is required.");
            status = CMD_RET_INVPARAM;
        } else if (p->type == BLADERF_IMAGE_TYPE_RAW &&
                !(p->override_address || p->max_length == 0)) {
            cli_err(s, argv[0],
                    "An address and a length are required for type=raw.");
            status = CMD_RET_INVPARAM;
        } else if (argc > 2 && !p->data_file) {
            cli_err(s, argv[0],
                    "A data input file is required when creating an image.");
            status = CMD_RET_INVPARAM;
        }
    }

    return status;
}


static int print_image_metadata(struct cli_state *s, struct params *p,
                                const char *argv0)
{
    int status = 0;
    struct bladerf_image *image;
    char datetime[64];
    struct tm *timeval;
    time_t time_tmp;
    int i;

    image = bladerf_alloc_image(BLADERF_IMAGE_TYPE_INVALID, 0, 0);
    if (!image) {
        return CMD_RET_MEM;
    }

    status = bladerf_image_read(image, p->img_file);

    if (status == 0) {
        printf("\n");
        printf("Checksum: ");

        for (i = 0; i < BLADERF_IMAGE_CHECKSUM_LEN; i++) {
            printf("%02x", image->checksum[i]);
        }

        printf("\nImage format version: %d.%d.%d\n", image->version.major,
               image->version.minor, image->version.patch);

        time_tmp = image->timestamp;
        timeval = localtime(&time_tmp);
        if (timeval) {
            memset(datetime, 0, sizeof(datetime));
            strftime(datetime, sizeof(datetime) - 1, "%Y-%m-%d %H:%M:%S", timeval);
        } else {
            strncpy(datetime, "Invalid value", sizeof(datetime));
        }

        printf("Timestamp: %s\n", datetime);
        printf("Serial #: %s\n", image->serial);

        switch (image->type) {
            case BLADERF_IMAGE_TYPE_RAW:
                printf("Image type: Raw\n");
                break;

            case BLADERF_IMAGE_TYPE_CALIBRATION:
                printf("Image type: Calibration data\n");
                break;

            case BLADERF_IMAGE_TYPE_FIRMWARE:
                printf("Image type: Firmware\n");
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

        printf("Address: 0x%08" PRIx32 "\n", image->address);
        printf("Length:  0x%08" PRIx32 "\n", image->length);

        printf("\n");
    } else {
        if (status == BLADERF_ERR_INVAL) {
            cli_err(s, argv0, "Image contains invalid fields or data.");
            status = CMD_RET_INVPARAM;
        }

        s->last_lib_error = status;
        status = CMD_RET_LIBBLADERF;
    }

    bladerf_free_image(image);
    return status;
}

static int write_image(struct cli_state *s, struct params *p, const char *argv0)
{
    int status;
    FILE *f;
    long data_size;
    struct bladerf_image *image = NULL;

    f = expand_and_open(p->data_file, "rb");
    if (!f) {
        return CMD_RET_FILEOP;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        status = CMD_RET_FILEOP;
        goto write_image_out;
    }

    data_size = ftell(f);
    if (data_size < 0) {
        status = CMD_RET_FILEOP;
        goto write_image_out;
    }

    if ((uint32_t)data_size > p->max_length) {
        status = CMD_RET_INVPARAM;
        cli_err(s, argv0, "The provided data file is too large for the specified flash region.");
        goto write_image_out;
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        status = CMD_RET_FILEOP;
        goto write_image_out;
    }

    image = bladerf_alloc_image(p->type, p->address, data_size);
    if (!image) {
        status = CMD_RET_MEM;
        goto write_image_out;
    }

    if (fread(image->data, 1, data_size, f) != (size_t)data_size) {
        status = CMD_RET_FILEOP;
        goto write_image_out;
    }

    memcpy(image->serial, p->serial, BLADERF_SERIAL_LENGTH - 1);

    status = bladerf_image_write(image, p->img_file);
    if (status != 0) {
        s->last_lib_error = status;
        status = CMD_RET_LIBBLADERF;
    }

write_image_out:
    fclose(f);
    bladerf_free_image(image);
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
        assert(p.img_file);
        if (argc == 2) {
            status = print_image_metadata(state, &p, argv[0]);
        } else {
            assert(p.data_file);
            status = write_image(state, &p, argv[0]);
        }
    }

    free(p.data_file);
    free(p.img_file);
    return status;
}
