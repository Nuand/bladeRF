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
#include "interactive.h"
#include "bladeRF.h"

#ifdef CLI_LIBUSB_ENABLED
#include <libusb.h>
#include "ezusb.h"

static bool is_bootloader_device(struct cli_state *s, libusb_device *dev)
{
    int status;
    bool ret = false;
    struct libusb_device_descriptor desc;

    status = libusb_get_device_descriptor(dev, &desc);
    if (status < 0) {
        cli_err(s, "Warning", "Failed to open libusb device: %s\n",
                libusb_error_name(status));
    } else {
        if((desc.idVendor == USB_CYPRESS_VENDOR_ID &&
            desc.idProduct == USB_FX3_PRODUCT_ID) ||
           (desc.idVendor == USB_NUAND_VENDOR_ID &&
            desc.idProduct == USB_NUAND_BLADERF_BOOT_PRODUCT_ID)) {
            ret = true;
        }
    }
    return ret;
}

static int find_device(struct cli_state *s,
                       libusb_context * context,
                       uint8_t bus, uint8_t addr,
                       libusb_device_handle **handle,
                       bool just_print)
{
    int status, i;
    libusb_device *dev, **devs;
    libusb_device *found_dev = NULL;
    ssize_t status_sz;
    size_t num_found = 0;

    status_sz = libusb_get_device_list(context, &devs);
    if (status_sz < 0) {
        cli_err(s, "Error", "libusb_get_device_list() failed: %d %s\n",
                status_sz, libusb_error_name((int)status_sz));

        return CMD_RET_UNKNOWN;
    }

    for (i = 0; (dev = devs[i]) != NULL && found_dev == NULL; i++) {

        if (just_print && is_bootloader_device(s, dev)) {
            if (num_found++ == 0) {
                printf("\n");
            }

            printf("  Bootloader @ bus=%d, addr=%d\n",
                   libusb_get_bus_number(dev), libusb_get_device_address(dev));

        } else if (is_bootloader_device(s, dev) &&
                   bus == libusb_get_bus_number(dev) &&
                   addr == libusb_get_device_address(dev)) {
            found_dev = dev;
        }
    }

    if (found_dev == NULL || (just_print && num_found == 0)) {
        s->last_lib_error =  BLADERF_ERR_NODEV;
        return CMD_RET_LIBBLADERF;
    } else if (!just_print) {
        status = libusb_open(found_dev, handle);

        if (status != 0) {
            s->last_lib_error = BLADERF_ERR_IO;
            return CMD_RET_LIBBLADERF;
        }
    }

    libusb_free_device_list(devs, 1);
    return 0;
}


static int perform_recovery(struct cli_state *state,
                            uint8_t bus, uint8_t addr,
                            const char *firmware_file)
{
    int status;
    libusb_context *ctx;
    libusb_device_handle *handle = NULL;

    status = libusb_init(&ctx);
    if (status != 0) {
        return CMD_RET_UNKNOWN;
    }

    status = find_device(state, ctx, bus, addr, &handle, false);
    if (status == 0) {
        status = ezusb_load_ram(handle, firmware_file,
                                FX_TYPE_FX3, IMG_TYPE_IMG, 0);
    }

    libusb_close(handle);
    libusb_exit(ctx);
    return status;
}

static int list_bootloader_devs(struct cli_state *state)
{
    int status;
    libusb_context *ctx;

    status = libusb_init(&ctx);
    if (status != 0) {
        return CMD_RET_UNKNOWN;
    }

    status = find_device(state, ctx, 0, 0, NULL, true);
    if (status == 0) {
        printf("\n");
    }

    libusb_exit(ctx);
    return status;
}

#else

static int perform_recovery(struct cli_state *state,
                            struct bladerf_devinfo *devinfo,
                            const char *firmware_file)
{
    state->last_lib_error = BLADERF_ERR_UNSUPPORTED;
    return CMD_RET_LIBBLADERF;
}

struct int list_bootloader_devs(struct cli_state *state)
{
    state->last_lib_error = BLADERF_ERR_UNSUPPORTED;
    return CMD_RET_LIBBLADERF;
}

#endif


/* Usage:
 *      recover <devinfo> <FX3 firmware>
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
        return CMD_RET_NARGS;
    }

    bus = str2uint(argv[1], 0, UINT8_MAX, &ok);
    if (!ok) {
        cli_err(state, argv[0], "Invalid bus: %s\n", bus);
        return CMD_RET_INVPARAM;
    }

    addr = str2uint(argv[2], 0, UINT8_MAX, &ok);
    if (!ok) {
        cli_err(state, argv[0], "Invalid address: %s\n", addr);
        return CMD_RET_INVPARAM;
    }

    if ((expanded_path = interactive_expand_path(argv[3])) == NULL) {
        cli_err(state,
                "Unable to expand FX3 firmware file path: \"%s\"", argv[2]);
        return CMD_RET_INVPARAM;
    }

    status = perform_recovery(state, bus, addr, expanded_path);

    if (status == 0) {
        printf("\n");
        printf("Success! Use \"open\" to switch to this device.\n");
        printf("Note that a \"load fx3 <firmware>\" is required to "
               "write the firmware to flash.\n");
        printf("\n");
        return CMD_RET_OK;
    } else if (status == CMD_RET_NODEV) {
        printf("No devices in bootloader mode were found.\n");
        return CMD_RET_OK;
    } else {
        return status;
    }
}
