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

#include <string.h>
#include <ctype.h>
#include <limits.h>

#include <libbladeRF.h>

#include "rel_assert.h"

#include "devinfo.h"
#include "conversions.h"
#include "log.h"

/******************************************************************************/
/* Device List Probe */
/******************************************************************************/

int probe(backend_probe_target target_device, struct bladerf_devinfo **devices)
{
    int ret;
    size_t num_devices;
    struct bladerf_devinfo *devices_local;
    int status;

    status = backend_probe(target_device, &devices_local, &num_devices);

    if (status < 0) {
        ret = status;
    } else {
        assert(num_devices <= INT_MAX);
        ret = (int)num_devices;
        *devices = devices_local;
    }

    return ret;
}

int bladerf_get_device_list(struct bladerf_devinfo **devices)
{
    return probe(BACKEND_PROBE_BLADERF, devices);
}

void bladerf_free_device_list(struct bladerf_devinfo *devices)
{
    /* Admittedly, we could just have the user call free() directly,
     * but this creates a 1:1 pair of calls, and this gives us a spot
     * to do any additional cleanup here, if ever needed in the future */
    free(devices);
}

/******************************************************************************/
/* Device Information Helpers */
/******************************************************************************/

bool bladerf_instance_matches(const struct bladerf_devinfo *a,
                              const struct bladerf_devinfo *b)
{
    return a->instance == DEVINFO_INST_ANY ||
           b->instance == DEVINFO_INST_ANY ||
           a->instance == b->instance;
}

bool bladerf_serial_matches(const struct bladerf_devinfo *a,
                            const struct bladerf_devinfo *b)
{
    /* User specified a "Any serial number" so just report a match */
    const bool wildcard_match = !strcmp(a->serial, DEVINFO_SERIAL_ANY) ||
                                !strcmp(b->serial, DEVINFO_SERIAL_ANY);

    if (wildcard_match) {
        return true;
    } else {
        /* The user-supplied serial number matches the a subset of the
         * entire serial number, starting at the beginning.
         *
         * i.e., "abc01234" can be used to match "abc0123456789def..."
         */
        bool subset_match = (strstr(a->serial, b->serial) == a->serial) ||
                            (strstr(b->serial, a->serial) == b->serial);

        return subset_match;
    }
}

bool bladerf_bus_addr_matches(const struct bladerf_devinfo *a,
                              const struct bladerf_devinfo *b)
{
    bool bus_match, addr_match;

    bus_match = a->usb_bus == DEVINFO_BUS_ANY ||
                b->usb_bus == DEVINFO_BUS_ANY ||
                a->usb_bus == b->usb_bus;

    addr_match = a->usb_addr == DEVINFO_BUS_ANY ||
                 b->usb_addr == DEVINFO_BUS_ANY ||
                 a->usb_addr == b->usb_addr;

    return bus_match && addr_match;
}

int bladerf_devinfo_list_init(struct bladerf_devinfo_list *list)
{
    int status = 0;

    list->num_elt = 0;
    list->backing_size = 5;

    list->elt = malloc(list->backing_size * sizeof(struct bladerf_devinfo));

    if (!list->elt) {
        status = BLADERF_ERR_MEM;
    }

    return status;
}

int bladerf_devinfo_list_add(struct bladerf_devinfo_list *list,
                             struct bladerf_devinfo *info)
{
    int status = 0;
    struct bladerf_devinfo *info_tmp;

    if (list->num_elt >= list->backing_size) {
        info_tmp = realloc(list->elt, list->backing_size * 2 * sizeof(*list->elt));
        if (!info_tmp) {
            status = BLADERF_ERR_MEM;
        } else {
            list->elt = info_tmp;
            list->backing_size = list->backing_size * 2;
        }
    }

    if (status == 0) {
        memcpy(&list->elt[list->num_elt], info, sizeof(*info));
        list->num_elt++;
    }

    return status;
}

void bladerf_init_devinfo(struct bladerf_devinfo *info)
{
    info->backend = BLADERF_BACKEND_ANY;

    memset(info->serial, 0, BLADERF_SERIAL_LENGTH);
    strncpy(info->serial, DEVINFO_SERIAL_ANY, BLADERF_SERIAL_LENGTH - 1);

    info->usb_bus  = DEVINFO_BUS_ANY;
    info->usb_addr = DEVINFO_ADDR_ANY;
    info->instance = DEVINFO_INST_ANY;

    memset(info->manufacturer, 0, BLADERF_DESCRIPTION_LENGTH);
    strncpy(info->manufacturer, "<unknown>", BLADERF_DESCRIPTION_LENGTH - 1);

    memset(info->product, 0, BLADERF_DESCRIPTION_LENGTH);
    strncpy(info->product, "<unknown>", BLADERF_DESCRIPTION_LENGTH - 1);
}

bool bladerf_devinfo_matches(const struct bladerf_devinfo *a,
                             const struct bladerf_devinfo *b)
{
    return bladerf_instance_matches(a, b) &&
           bladerf_serial_matches(a, b)   &&
           bladerf_bus_addr_matches(a ,b);
}

bool bladerf_devstr_matches(const char *dev_str,
                            struct bladerf_devinfo *info)
{
    int status;
    bool ret;
    struct bladerf_devinfo from_str;

    status = str2devinfo(dev_str, &from_str);
    if (status < 0) {
        ret = false;
        log_debug("Failed to parse device string: %s\n",
                  bladerf_strerror(status));
    } else {
        ret = bladerf_devinfo_matches(&from_str, info);
    }

    return ret;
}

int bladerf_get_devinfo_from_str(const char *devstr,
                                 struct bladerf_devinfo *info)
{
    return str2devinfo(devstr, info);
}

/******************************************************************************/
/* str2devinfo */
/******************************************************************************/

#define DELIM_SPACE         " \t\r\n\v\f"

static int handle_backend(char *str, struct bladerf_devinfo *d)
{
    char *str_end;

    if (!str || strlen(str) == 0) {
        return BLADERF_ERR_INVAL;
    }

    /* Gobble up any leading whitespace */
    while (*str && isspace((unsigned char) *str)) {
        str++;
    };

    /* Likewise for trailing whitespace */
    str_end = str + strlen(str) - 1;
    while (str_end > str && isspace((unsigned char) *str_end)) { str_end--; };
    str_end[1] = '\0';

    return str2backend(str, &d->backend);
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

    if (value == NULL) {
        return BLADERF_ERR_INVAL;
    }

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
    size_t i;
    size_t len;

    if (value == NULL) {
        return BLADERF_ERR_INVAL;
    }

    len = strlen(value);
    if (len > (BLADERF_SERIAL_LENGTH - 1)) {
        log_debug("Provided serial # string too long: %"PRIu64"\n",
                  (uint64_t) len);

        return BLADERF_ERR_INVAL;
    }

    for (i = 0; i < len; i++) {
        c = value[i];
        if (c >= 'A' && c <='F') {
            value[i] = tolower((unsigned char) c);
        }

        if ((c < 'a' || c > 'f') && (c < '0' || c > '9')) {
            log_debug("Bad serial: %s\n", value);
            return BLADERF_ERR_INVAL;
        }
    }

    strncpy(d->serial, value, sizeof(d->serial));
    d->serial[sizeof(d->serial) - 1] = '\0';

    if (len == (BLADERF_SERIAL_LENGTH - 1)) {
        log_verbose("Requested serial number: %s\n", d->serial);
    } else {
        log_verbose("Requested serial number subset: %s\n", d->serial);
    }
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
        *value = NULL;
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
    char *dev_id    = NULL;
    char *token     = NULL;
    char *arg       = NULL;
    char *val       = NULL;
    char *saveptr   = NULL;
    int status      = BLADERF_ERR_UNEXPECTED;
    int arg_status  = BLADERF_ERR_UNEXPECTED;

    assert(d);

    /* Prep our device info before we begin manpulating it, defaulting to
     * a "wildcard" device indentification */
    bladerf_init_devinfo(d);

    /* No device indentifier -- pick anything we can find */
    if (dev_id_const == NULL || strlen(dev_id_const) == 0) {
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
