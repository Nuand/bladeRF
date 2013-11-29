/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2013 Nuand LLC
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <libbladeRF.h>
#include <limits.h>
#include "rel_assert.h"
#include <stdbool.h>
#include <stdint.h>

#include "device_identifier.h"
#include "devinfo.h"
#include "conversions.h"
#include "log.h"

#define DELIM_SPACE         " \t\r\n\v\f"


static int handle_backend(char *str, struct bladerf_devinfo *d)
{
    int status = 0;
    char *str_end;

    if (!str || strlen(str) == 0) {
        return BLADERF_ERR_INVAL;
    }


    /* Gobble up any leading whitespace */
    while (*str && isspace(*str)) {
        str++;
    };

    /* Likewise for trailing whitespace */
    str_end = str + strlen(str) - 1;
    while (str_end > str && isspace(*str_end)) { str_end--; };
    str_end[1] = '\0';

    if (!strcasecmp("libusb", str)) {
        d->backend = BLADERF_BACKEND_LIBUSB;
    } else if (!strcasecmp("linux", str)) {
        d->backend = BLADERF_BACKEND_LINUX;
    } else {
        log_debug("Invalid backend: %s\n", str);
        status = BLADERF_ERR_INVAL;
    }

    return status;
}

static int handle_device(struct bladerf_devinfo *d, char *value)
{
    int status = BLADERF_ERR_INVAL;
    bool bus_ok, addr_ok;
    char *bus = value;
    char *addr = strchr(value, ':');

    if (addr && addr[1] != '\0') {

        /* Null-terminate bus and set addr to start of addr text */
        *addr = '\0';
        addr++;

        d->usb_bus = str2uint(bus, 0, DEVINFO_BUS_ANY - 1, &bus_ok);
        d->usb_addr = str2uint(addr, 0, DEVINFO_ADDR_ANY - 1, &addr_ok);

        if (bus_ok && addr_ok) {
            status = 0;
            log_debug("Device: %d:%d\n", d->usb_bus, d->usb_addr);
        } else {
            log_debug("Bad bus (%s) or address (%s)\n", bus, addr);
        }
    }

    return status;
}

static int handle_instance(struct bladerf_devinfo *d, char *value)
{
    bool ok;

    d->instance = str2uint(value, 0, DEVINFO_INST_ANY - 1, &ok);
    if (!ok) {
        log_debug("Bad instance: %s\n", value);
        return BLADERF_ERR_INVAL;
    } else {
        log_debug("Instance: %u\n", d->instance);
        return 0;
    }
}

static int handle_serial(struct bladerf_devinfo *d, char *value)
{
    char c;
    int i;

    if (strlen(value) != 32) {
        return BLADERF_ERR_INVAL;
    }

    for (i = 0; i < 32; i++) {
        c = value[i];
        if (c >= 'A' && c <='F') {
            value[i] = tolower(c);
        }
        if ((c < 'a' || c > 'f') && (c < '0' || c > '9')) {
            log_debug("Bad serial: %s\n", value);
            return BLADERF_ERR_INVAL;
        }
    }

    strncpy(d->serial, value, 32);
    d->serial[32] = 0;

    log_debug("Serial 0x%s\n", d->serial);
    return 0;
}

/* Returns: 1 on arg and value populated
 *          0 on no args left
 *          BLADERF_ERR_INVAL on bad format
 */

static int next_arg(char **saveptr, char **arg, char **value)
{
    char *saveptr_local;
    char *token = strtok_r(NULL, DELIM_SPACE, saveptr);

    /* No arguments left */
    if (!token) {
        return 0;
    }

    /* Argument name */
    *arg = strtok_r(token, "=", &saveptr_local);

    if (!*arg) {
        return BLADERF_ERR_INVAL;
    }

    /* Argument value - gobble up the rest of the line*/
    *value = strtok_r(NULL, "", &saveptr_local);

    if (!*value) {
        return BLADERF_ERR_INVAL;
    }

    return 1;
}

int str2devinfo(const char *dev_id_const, struct bladerf_devinfo *d)
{
    char *dev_id, *token, *arg, *val, *saveptr;
    int status, arg_status;

    assert(d);

    /* Prep our device info before we begin manpulating it, defaulting to
     * a "wildcard" device indentification */
    bladerf_init_devinfo(d);

    /* No device indentifier -- pick anything we can find */
    if ( dev_id_const == NULL || strlen(dev_id_const) == 0) {
        return 0;
    }

    /* Copy the string so we can butcher it a bit while parsing */
    dev_id = strdup(dev_id_const);
    if (!dev_id) {
        return BLADERF_ERR_MEM;
    }

    /* Extract backend */
    token = strtok_r(dev_id, ":", &saveptr);

    /* We require a valid backend -- args only is not supported */
    if (token) {
        status = handle_backend(token, d);

        /* Loop over remainder of string, gathering up args */
        arg_status = 1;
        while (arg_status == 1 && status == 0) {
            arg_status = next_arg(&saveptr, &arg, &val);
            if (arg_status == 1) {

                /* Handle argument if we can */
                if (!strcasecmp("device", arg)) {
                    status = handle_device(d, val);
                } else if (!strcasecmp("instance", arg)) {
                    status = handle_instance(d, val);
                } else if (!strcasecmp("serial", arg)) {
                    status = handle_serial(d, val);
                } else {
                    arg_status = BLADERF_ERR_INVAL;
                }
            }
        };

        if (arg_status < 0) {
            status = arg_status;
        }

    } else {
        status = BLADERF_ERR_INVAL;
    }

    free(dev_id);
    return status;
}
