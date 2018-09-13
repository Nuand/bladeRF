/*
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2013-2014 Nuand LLC
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
#include "conversions.h"
#include <stdio.h>

int cmd_info(struct cli_state *state, int argc, char **argv)
{
    int status;
    bladerf_fpga_size fpga_size;
    uint32_t flash_size;
    uint16_t dac_trim;
    bool fpga_loaded;
    bool flash_size_guessed;
    struct bladerf_devinfo info;
    bladerf_dev_speed usb_speed;
    const char *backend_str;

    status = bladerf_get_devinfo(state->dev, &info);
    if (status < 0) {
        state->last_lib_error = status;
        return CLI_RET_LIBBLADERF;
    }

    backend_str = backend_description(info.backend);

    status = bladerf_is_fpga_configured(state->dev);
    if (status < 0) {
        state->last_lib_error = status;
        return CLI_RET_LIBBLADERF;
    }
    fpga_loaded = status != 0;

    status = bladerf_get_fpga_size(state->dev, &fpga_size);
    if (status < 0) {
        state->last_lib_error = status;
        return CLI_RET_LIBBLADERF;
    }

    status = bladerf_get_flash_size(state->dev, &flash_size,
                                    &flash_size_guessed);
    if (status < 0) {
        state->last_lib_error = status;
        return CLI_RET_LIBBLADERF;
    }

    status = bladerf_get_vctcxo_trim(state->dev, &dac_trim);
    if (status < 0) {
        state->last_lib_error = status;
        return CLI_RET_LIBBLADERF;
    }

    usb_speed = bladerf_device_speed(state->dev);

    printf("\n");
    printf("  Board:                    %s %s (%s)\n", info.manufacturer,
           info.product, bladerf_get_board_name(state->dev));
    printf("  Serial #:                 %s\n", info.serial);
    printf("  VCTCXO DAC calibration:   0x%.4x\n", dac_trim);
    if (fpga_size != 0) {
        printf("  FPGA size:                %d KLE\n", fpga_size);
    } else {
        printf("  FPGA size:                Unknown\n");
    }
    printf("  FPGA loaded:              %s\n", fpga_loaded ? "yes" : "no");
    if (flash_size != 0) {
        printf("  Flash size:               %u Mbit", (flash_size >> 17));
        printf( flash_size_guessed ? " (assumed)\n" : "\n");
    } else {
        printf("  Flash size:               Unknown\n");
    }
    printf("  USB bus:                  %d\n", info.usb_bus);
    printf("  USB address:              %d\n", info.usb_addr);
    printf("  USB speed:                %s\n", devspeed2str(usb_speed));
    printf("  Backend:                  %s\n", backend_str);
    printf("  Instance:                 %d\n", info.instance);

    printf("\n");
    return 0;
}
