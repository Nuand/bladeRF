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
#include "rel_assert.h"
#include "cmd.h"

static inline const char *backend2str(bladerf_backend b)
{
    switch (b) {
        case BLADERF_BACKEND_LIBUSB:
            return "libusb";
        case BLADERF_BACKEND_LINUX:
            return "Linux kernel driver";
        default:
            return "Unknown";
    }
}

/* Todo move to cmd_probe.c */
int cmd_probe(struct cli_state *s, int argc, char *argv[])
{
    struct bladerf_devinfo *devices = NULL;
    int n_devices, i;

    n_devices = bladerf_get_device_list(&devices);

    if (n_devices < 0) {
        if (n_devices == BLADERF_ERR_NODEV) {
            cli_err(s, argv[0], "No devices found.");
        } else {
            cli_err(s, argv[0], "Failed to probe for devices: %s",
                    bladerf_strerror(n_devices));
        }

        s->last_lib_error = n_devices;
        return CMD_RET_LIBBLADERF;
    }

    printf("\n");
    for (i = 0; i < n_devices; i++) {
        printf("    Backend:        %s\n", backend2str(devices[i].backend));
        /* printf("    Serial: 0x%016lX\n", devices[i].serial); */
        /* TODO: Fix OTP support for serial readback! */
        printf("    Serial:         %s\n", devices[i].serial);
        printf("    USB Bus:        %d\n", devices[i].usb_bus);
        printf("    USB Address:    %d\n", devices[i].usb_addr);
        /*printf("    Firmware: v%d.%d\n", devices[i].fw_ver_maj,
               devices[i].fw_ver_min);

        if (devices[i].fpga_configured) {
            printf("    FPGA: v%d.%d\n",
                    devices[i].fpga_ver_maj, devices[i].fpga_ver_min);
        } else {
            printf("    FPGA: not configured\n");
        }*/
        printf("\n");
    }

    if (devices) {
        assert(n_devices != 0);
        bladerf_free_device_list(devices);
    }

    return 0;
}
