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
#include <limits.h>
#include <stdbool.h>
#include <string.h>

#include <libbladeRF.h>
#include "conversions.h"
#include "cmd.h"
#include "input.h"

#define MSG_NO_DEV "\n  No devices in bootloader mode were found.\n\n"

static int list_bootloader_devs(struct cli_state *s)
{
    int i, num_devs;
    struct bladerf_devinfo *list;

    num_devs = bladerf_get_bootloader_list(&list);
    if (num_devs == BLADERF_ERR_NODEV || num_devs == 0) {
        printf("%s", MSG_NO_DEV);
        return 0;
    } else if (num_devs < 0) {
        s->last_lib_error = num_devs;
        return CLI_RET_LIBBLADERF;
    }

    printf("\n  FX3 bootloader devices:\n");
    printf("  ---------------------------------------------------------\n");

    for (i = 0; i < num_devs; i++) {
        printf("    Backend:    %s\n", backend_description(list[i].backend));
        printf("    Bus:        %u\n", list[i].usb_bus);
        printf("    Address:    %u\n", list[i].usb_addr);
    }

    printf("\n  Use 'recover <bus> <addr> <firmware>' to download and boot\n"
           "  firmware to the specified device.\n\n");

    return 0;
}

/* Usage:
 *      recover <bus> <addr> <FX3 firmware>
 *      recover
 */
int cmd_recover(struct cli_state *state, int argc, char **argv)
{
    int status;
    const char *expanded_path;
    bool ok;
    uint8_t bus, addr;

    if (argc == 1) {
        return list_bootloader_devs(state);
    } else if (argc != 4) {
        return CLI_RET_NARGS;
    }

    bus = str2uint(argv[1], 0, UINT8_MAX, &ok);
    if (!ok) {
        cli_err(state, argv[0], "Invalid bus: %s\n", bus);
        return CLI_RET_INVPARAM;
    }

    addr = str2uint(argv[2], 0, UINT8_MAX, &ok);
    if (!ok) {
        cli_err(state, argv[0], "Invalid address: %s\n", addr);
        return CLI_RET_INVPARAM;
    }

    if ((expanded_path = input_expand_path(argv[3])) == NULL) {
        cli_err(state,
                "Unable to expand FX3 firmware file path: \"%s\"", argv[2]);
        return CLI_RET_INVPARAM;
    }

    status = bladerf_load_fw_from_bootloader(NULL,
                                             BLADERF_BACKEND_ANY,
                                             bus, addr,
                                             expanded_path);
    free((void*)expanded_path);

    if (status == 0) {
        putchar('\n');
        printf("  Success! Use \"open\" to switch to this device.\n");
        printf("  Note that a \"load fx3 <firmware>\" is required to "
               "write the firmware to flash.\n");
        putchar('\n');
        return CLI_RET_OK;
    } else if (status == CLI_RET_NODEV) {
        printf("%s", MSG_NO_DEV);
        return CLI_RET_OK;
    } else {
        state->last_lib_error  = status;
        return CLI_RET_LIBBLADERF;
    }
}
