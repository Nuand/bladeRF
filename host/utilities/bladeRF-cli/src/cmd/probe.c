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
#include "cmd.h"
#include "rel_assert.h"
#include <string.h>

static inline const char *backend2str(bladerf_backend b)
{
    switch (b) {
        case BLADERF_BACKEND_LIBUSB:
            return "libusb";
        case BLADERF_BACKEND_CYPRESS:
            return "CyUSB driver";
        case BLADERF_BACKEND_LINUX:
            return "Linux kernel driver";
        default:
            return "Unknown";
    }
}

/* Todo move to cmd_probe.c */
int cmd_probe(struct cli_state *s, int argc, char *argv[])
{
    bool error_on_no_dev            = false;
    struct bladerf_devinfo *devices = NULL;
    struct bladerf_devinfo my_dev;
    int n_devices, i, status;

    n_devices = bladerf_get_device_list(&devices);

    if (argc > 1 && !strcasecmp(argv[1], "strict")) {
        error_on_no_dev = true;
    }

    if (n_devices < 0) {
        if (n_devices == BLADERF_ERR_NODEV) {
            cli_err(s, argv[0],
                    "No devices are available. If one is attached, ensure it\n"
                    "         "
                    "is not in use by another program and that the current\n"
                    "         "
                    "user has permission to access it.\n");

            if (error_on_no_dev) {
                return CLI_RET_CMD_HANDLED;
            } else {
                return 0;
            }
        } else {
            cli_err(s, argv[0], "Failed to probe for devices: %s\n",
                    bladerf_strerror(n_devices));
            s->last_lib_error = n_devices;
            return CLI_RET_LIBBLADERF;
        }
    }

    if (s->dev != NULL) {
        status = bladerf_get_devinfo(s->dev, &my_dev);
        if (status < 0) {
            cli_err(s, argv[0], "Failed to get open device info: %s\n",
                    bladerf_strerror(status));
            s->last_lib_error = status;
            return CLI_RET_LIBBLADERF;
        }
    }

    putchar('\n');
    for (i = 0; i < n_devices; i++) {
        struct bladerf_devinfo *this = &devices[i];

        bool already_open = (s->dev != NULL) &&
                            (this->usb_bus == my_dev.usb_bus) &&
                            (this->usb_addr == my_dev.usb_addr);

        if (already_open) {
            this = &my_dev;
        } else if (0 == strcmp("ANY", this->serial)) {
            printf("  Description:    inaccessible device (already in use?)\n");
            printf("  Backend:        %s\n", backend2str(this->backend));
            printf("  USB Bus:        %d\n", this->usb_bus);
            printf("  USB Address:    %d\n", this->usb_addr);
            putchar('\n');
            continue;
        }

        printf("  Description:    %s %s%s\n", this->manufacturer, this->product,
               already_open ? " (currently open)" : "");
        printf("  Backend:        %s\n", backend2str(this->backend));
        printf("  Serial:         %s\n", this->serial);
        printf("  USB Bus:        %d\n", this->usb_bus);
        printf("  USB Address:    %d\n", this->usb_addr);
        putchar('\n');
    }

    if (devices) {
        assert(n_devices != 0);
        bladerf_free_device_list(devices);
    }

    return 0;
}
