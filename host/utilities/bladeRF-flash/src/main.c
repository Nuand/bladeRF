/*
 * This file is part of the bladeRF project
 *
 * bladeRF flash and firmware recovery utility
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
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <libbladeRF.h>
#include <libusb.h>
#include <bladeRF.h>
#include "version.h"
#include "log.h"
#include "ezusb.h"
#include "host_config.h"
#include <time.h>


#define RECONNECT_TIME 10 /* Time for USB reconnect after jump */
#define OPTSTR "d:f:rlLv:Vh"

static const struct option longopts[] = {
    { "device",         required_argument,  0, 'd' },
    { "flash-firmware", required_argument,  0, 'f' },
    { "reset",          no_argument,        0, 'r' },
    { "load-ram",       no_argument,        0, 'l' },
    { "lib-version",    no_argument,        0, 'L' },
    { "verbosity",      required_argument,  0, 'v' },
    { "version",        no_argument,        0, 'V' },
    { "help",           no_argument,        0, 'h' },
    { 0,                0,                  0,  0  },
};

/* Runtime configuration items */
struct rc_config {
    bool reset;
    bool load_ram_only;
    bool show_help;
    bool show_version;
    bool show_lib_version;

    bladerf_log_level verbosity;

    char *device;
    char *fw_file;
};

static void init_rc_config(struct rc_config *rc)
{
    rc->reset = false;
    rc->load_ram_only = false;
    rc->show_help = false;
    rc->show_version = false;
    rc->show_lib_version = false;

    rc->verbosity = BLADERF_LOG_LEVEL_INFO;

    rc->device = NULL;
    rc->fw_file = NULL;
}


/* Fetch runtime-configuration info
 *
 * Returns 0 on success, -1 on fatal error (and prints error msg to stderr)
 */
int get_rc_config(int argc, char *argv[], struct rc_config *rc)
{
    int optidx;
    int c = getopt_long(argc, argv, OPTSTR, longopts, &optidx);

    do {
        switch(c) {
            case 'd':
                rc->device = strdup(optarg);
                if (!rc->device) {
                    perror("strdup");
                    return -1;
                }
                break;

            case 'f':
                rc->fw_file = strdup(optarg);
                if (!rc->fw_file) {
                    perror("strdup");
                    return -1;
                }
                break;

            case 'r':
                rc->reset = true;
                break;

            case 'l':
                rc->load_ram_only = true;
                break;

            case 'L':
                rc->show_lib_version = true;
                break;

            case 'v':
                if (!strcasecmp(optarg, "critical")) {
                    rc->verbosity = BLADERF_LOG_LEVEL_CRITICAL;
                } else if (!strcasecmp(optarg, "error")) {
                    rc->verbosity = BLADERF_LOG_LEVEL_ERROR;
                } else if (!strcasecmp(optarg, "warning")) {
                    rc->verbosity = BLADERF_LOG_LEVEL_WARNING;
                } else if (!strcasecmp(optarg, "info")) {
                    rc->verbosity = BLADERF_LOG_LEVEL_INFO;
                } else if (!strcasecmp(optarg, "debug")) {
                    rc->verbosity = BLADERF_LOG_LEVEL_DEBUG;
                } else if (!strcasecmp(optarg, "verbose")) {
                    rc->verbosity = BLADERF_LOG_LEVEL_VERBOSE;
                } else {
                    fprintf(stderr, "Unknown verbosity level: %s\n", optarg);
                    return -1;
                }
                break;

            case 'V':
                rc->show_version = true;
                break;

            case 'h':
                rc->show_help = true;
                break;

            default:
                return -1;
        }

        c = getopt_long(argc, argv, OPTSTR, longopts, &optidx);
    } while (c != -1);

    return 0;
}

void usage(const char *argv0)
{
    printf("Usage: %s <options>\n", argv0);
    printf("bladeRF flashing utility (" BLADERF_FLASH_VERSION ")\n\n");
    printf("Options:\n");
    printf("  -d, --device <device>            Use the specified bladeRF device.\n");
    printf("  -f, --flash-firmware <file>      Flash specified firmware file.\n");
    printf("  -r, --reset                      Start with RESET instead of JUMP_TO_BOOTLOADER.\n");
    printf("  -l, --load-ram                   Only load FX3 RAM instead of also flashing.\n");
    printf("  -L, --lib-version                Print libbladeRF version and exit.\n");
    printf("  -v, --verbosity <level>          Set the libbladeRF verbosity level.\n");
    printf("                                   Levels, listed in increasing verbosity, are:\n");
    printf("                                    critical, error, warning,\n");
    printf("                                    info, debug, verbose\n");
    printf("  -V, --version                    Print flasher version and exit.\n");
    printf("  -h, --help                       Show this help text.\n");
    printf("\n");
    printf("Notes:\n");
    printf("  The -d option takes a device specifier string. See the bladerf_open()\n");
    printf("  documentation for more information about the format of this string.\n");
    printf("\n");
}

typedef struct {
    int found;
    struct bladerf_devinfo devinfo;
    libusb_device *dev;
} event_count_t;

static int event_count = 0;
static event_count_t fx3_bootloader;
static event_count_t bladerf_bootloader;
static event_count_t bladerf;

static int count_events(
        libusb_context *ctx, libusb_device *dev,
        libusb_hotplug_event event, void *user_data)
{
    struct libusb_device_descriptor desc;
    int rc;
    event_count_t * s = (event_count_t *)user_data;

    if(event != LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
        return 0;
    }

    rc = libusb_get_device_descriptor(dev, &desc);
    if (LIBUSB_SUCCESS != rc) {
        log_error ("Error getting device descriptor for hotplug\n");
        return 0;
    }

    event_count += 1;
    s->found += 1;
    bladerf_init_devinfo(&s->devinfo);
    s->devinfo.backend  = BLADERF_BACKEND_LIBUSB;
    s->devinfo.usb_bus  = libusb_get_bus_number(dev);
    s->devinfo.usb_addr = libusb_get_device_address(dev);

    if(s->dev != NULL) {
         libusb_unref_device(s->dev);
    }
    s->dev = dev;
    libusb_ref_device(s->dev);

    return 0;
}

static int init_event_counts(libusb_context *ctx,
        int vender_id, int product_id, event_count_t * data)
{
    int status;

    memset(data, 0, sizeof(event_count_t));

    status = libusb_hotplug_register_callback(
            ctx,
            LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED,
            0,
            vender_id, product_id,
            LIBUSB_HOTPLUG_MATCH_ANY,
            count_events,
            data,
            NULL);

    return status;
}

static int get_event_counts(
        event_count_t * data,
        struct bladerf_devinfo * devinfo, libusb_device **device)
{
    int found;

    found = data->found;
    *devinfo = data->devinfo;
    *device = data->dev;
    data->found = 0;

    return found;
}

static int register_hotplug_notifications(libusb_context *context)
{
    int status = 0;

    status = init_event_counts(context,
            USB_CYPRESS_VENDOR_ID, USB_FX3_PRODUCT_ID,
            &fx3_bootloader);
    if(status != 0) {
        log_error("Failed to init fx3_bootloader monitor, status = %s\n", libusb_error_name(status));
        return status;
    }

    status = init_event_counts(context,
            USB_NUAND_VENDOR_ID, USB_NUAND_BLADERF_BOOT_PRODUCT_ID,
            &bladerf_bootloader);
    if(status != 0) {
        log_error("Failed to init bladeRF bladerf_bootloader monitor, status = %s\n", libusb_error_name(status));
        return status;
    }

    status = init_event_counts(context,
            USB_NUAND_VENDOR_ID, USB_NUAND_BLADERF_PRODUCT_ID,
            &bladerf);
    if(status != 0) {
        log_error("Failed to init bladerf monitor, status = %s\n", libusb_error_name(status));
        return status;
    }

    return status;
}

static int device_is_fx3(libusb_device *dev)
{
    int err;
    int rv = 0;
    struct libusb_device_descriptor desc;

    err = libusb_get_device_descriptor(dev, &desc);
    if( err ) {
        log_error( "Couldn't open libusb device - %s\n", libusb_error_name(err) );
    } else {
        if(
            (desc.idVendor == USB_CYPRESS_VENDOR_ID && desc.idProduct == USB_FX3_PRODUCT_ID) ||
            (desc.idVendor == USB_NUAND_VENDOR_ID && desc.idProduct == USB_NUAND_BLADERF_BOOT_PRODUCT_ID)
            ) {
            rv = 1;
        }
    }
    return rv;
}

static int get_devinfo(libusb_device *dev, struct bladerf_devinfo *info)
{
    int status = 0;
    libusb_device_handle *handle;
    struct libusb_device_descriptor desc;

    status = libusb_open( dev, &handle );
    if( status ) {
        log_error( "Couldn't populate devinfo - %s\n", libusb_error_name(status) );
    } else {
        /* Populate device info */
        info->backend = BLADERF_BACKEND_LIBUSB;
        info->usb_bus = libusb_get_bus_number(dev);
        info->usb_addr = libusb_get_device_address(dev);

        status = libusb_get_device_descriptor(dev, &desc);
        if (status) {
            memset(info->serial, 0, BLADERF_SERIAL_LENGTH);
        } else {
            status = libusb_get_string_descriptor_ascii(handle,
                                                        desc.iSerialNumber,
                                                        (unsigned char *)&info->serial,
                                                        BLADERF_SERIAL_LENGTH);

            /* Consider this to be non-fatal, otherwise firmware <= 1.1
             * wouldn't be able to get far enough to upgrade */
            if (status < 0) {
                log_error("Failed to retrieve serial number\n");
                memset(info->serial, 0, BLADERF_SERIAL_LENGTH);
            }

            /* Additinally, adjust for > 0 return code */
            status = 0;
        }

        libusb_close( handle );
    }


    return status;
}

static int find_fx3_via_info(
        libusb_context * context,
        struct bladerf_devinfo *info,
        libusb_device_handle **handle) {
    int status, i, count;
    struct bladerf_devinfo thisinfo;
    libusb_device *dev, **devs;
    libusb_device *found_dev = NULL;
    ssize_t status_sz;

    count = 0;

    status_sz = libusb_get_device_list(context, &devs);
    if (status_sz < 0) {
        log_error("libusb_get_device_list() failed: %d %s\n", status_sz, libusb_error_name((int)status_sz));
        return (int)status_sz;
    }

    for (i=0; (dev=devs[i]) != NULL; i++) {
        if (!device_is_fx3(dev)) {
            continue;
        }

        status = get_devinfo(dev, &thisinfo);
        if (status < 0) {
            log_error( "Could not open bladeRF device: %s\n", libusb_error_name(status) );
            break;
        }

        if (bladerf_devinfo_matches(&thisinfo, info)) {
            log_verbose("Found bladeRF bootloader, libusb:device=%d:%d\n",
                    thisinfo.usb_bus, thisinfo.usb_addr);
            count += 1;
            found_dev = dev;
        }
    }

    if(count > 1) {
        log_error("Multiple bootloaders found, select one:\n");
        for (i=0; (dev=devs[i]) != NULL; i++) {
            if (!device_is_fx3(dev)) {
                continue;
            }

            status = get_devinfo(dev, &thisinfo);
            if (status < 0) {
                log_error( "Could not open bladeRF device: %s\n", libusb_error_name(status) );
                break;
            }

            if (bladerf_devinfo_matches(&thisinfo, info)) {
                log_error( "    libusb:device=%d:%d\n",
                    thisinfo.usb_bus, thisinfo.usb_addr);
            }
        }
        return BLADERF_ERR_UNEXPECTED;
    }

    if (found_dev == NULL) {
        libusb_free_device_list(devs, 1);
        log_error("could not find a known device - try specifing bus, dev\n");
        return BLADERF_ERR_NODEV;
    }

    status = libusb_open(found_dev, handle);
    libusb_free_device_list(devs, 1);
    if (status != 0) {
        log_error("Error opening device: %s\n", libusb_error_name(status));
        return status;
    }

    return 0;
}

static int poll_for_events(libusb_context * ctx) {
    time_t end_t = time(NULL) + RECONNECT_TIME;
    struct timeval timeout;
    int status;

    timeout.tv_sec = RECONNECT_TIME;
    timeout.tv_usec = 0;

    event_count = 0;

    while(!event_count) {
        status = libusb_handle_events_timeout_completed(ctx, &timeout, NULL);
        if(status != 0) {
            log_error("Error waiting for events: %s\n", libusb_error_name(status));
            return status;
        }

        if(time(NULL) > end_t) {
            break;
        }
    }

    return 0;
}

static int look_for_bootloader_connect(libusb_context * ctx, libusb_device **device_out) {
    struct bladerf_devinfo devinfo;
    int count, status;

    status = poll_for_events(ctx);
    if(status != 0) {
        return status;
    }

    count = get_event_counts(&fx3_bootloader, &devinfo, device_out);
    if(count == 1) {
        return 0;
    } else if(count > 1) {
        log_error("Just saw %d FX3 bootloader connect events, thats pretty weird, bailing\n",
                count);
        return BLADERF_ERR_UNEXPECTED;
    }

    count = get_event_counts(&bladerf_bootloader, &devinfo, device_out);
    if(count == 1) {
        return 0;
    } else if(count > 1) {
        log_error("Just saw %d bladeRF bootloader connect events, thats pretty weird, bailing\n",
                count);
        return BLADERF_ERR_UNEXPECTED;
    }

    return BLADERF_ERR_UNEXPECTED;
}

static int reach_bootloader(bool reset, struct bladerf *dev, libusb_context *ctx, libusb_device **device_out) {
    int status;
    if(reset) {
        status = bladerf_device_reset(dev);
        if(status != 0) {
            log_error("Error resetting device: %s\n", bladerf_strerror(status));
            return status;
        }

        status = look_for_bootloader_connect(ctx, device_out);
        if(status == 0) {
            return status;
        }
    }

    status = bladerf_jump_to_bootloader(dev);
    if(status != 0) {
        log_info("Older bladeRF's don't support JUMP_TO_BOOTLOADER, "
                "so ignore LIBUSB timeout errors until you update the FX3 firmware\n");
    }

    status = look_for_bootloader_connect(ctx, device_out);
    if(status == 0) {
        return status;
    }

    log_info("Falling back to manually erasing first sector of FX3 firmware\n");

    status = bladerf_erase_flash(dev, 0, BLADERF_FLASH_SECTOR_SIZE);
    if(status != 0) {
        log_warning("Maybe failed to erase first sector."
                "A manual reset of the bladeRF may place it in the FX3 "
                "bootloader.  After the manual reset, try and re-run bladeRF-flash.\n");
        return BLADERF_ERR_UNEXPECTED;
    }

    status = bladerf_device_reset(dev);
    if(status != 0) {
        log_error("Failed to reset device after erasing first page."
                "A manual reset of the bladeRF should place it in the FX3 "
                "bootloader.  After the manual reset, re-run bladeRF-flash.\n");
        return BLADERF_ERR_UNEXPECTED;

    }

    status = look_for_bootloader_connect(ctx, device_out);
    if(status != 0) {
        log_error("Failed to reach bootloader.  Flashing will likely "
                "require manual force to FX3 bootloader. See "
                "http://nuand.com/forums/viewtopic.php?f=6&t=3072\n");
        return BLADERF_ERR_NODEV;
    }

    return 0;
}

static int get_to_bootloader(bool reset, const char *device_identifier,
        libusb_context *context, libusb_device_handle **device_out)
{
    struct bladerf *dev;
    int status;
    libusb_device *device = NULL;
    struct bladerf_devinfo devinfo;

    log_verbose("First try to open bladerf with devid provided\n");

    status = bladerf_open(&dev, device_identifier);
    if(status == 0) {
        /* Reset bladeRF count */
        log_verbose("bladeRF found, trying to reach the bootloader\n");
        status = reach_bootloader(reset, dev, context, &device);
        if(status != 0) {
            return status;
        }

        status = libusb_open(device, device_out);
        if(status != 0) {
            log_error("Error opening bootloader: %s\n",
                    libusb_error_name(status));
        }
        return status;
    } else {
        log_verbose("No bladeRF found, search for bootloader\n");
        status = bladerf_get_devinfo_from_str(device_identifier, &devinfo);
        if(status != 0) {
            log_error("Failed to parse dev string %s, %s\n",
                device_identifier, bladerf_strerror(status));
            return status;
        }

        if(devinfo.backend != BLADERF_BACKEND_LIBUSB && devinfo.backend != BLADERF_BACKEND_ANY) {
            status = BLADERF_ERR_UNSUPPORTED;
            log_error("Only libusb supported, %s\n",
                bladerf_strerror(status));
            return status;
        }

        return find_fx3_via_info(context, &devinfo, device_out);
    }
}

static int get_bladerf(libusb_context *ctx, struct bladerf_devinfo *devinfo)
{
    libusb_device *device;
    int count, status;

    status = poll_for_events(ctx);
    if(status != 0) {
        return status;
    }

    count = get_event_counts(&bladerf, devinfo, &device);
    if(count > 1) {
        log_error("Just saw %d bladeRF connect events, thats pretty weird, bailing\n",
                count);
        return BLADERF_ERR_UNEXPECTED;
    } else if (count == 0) {
        return BLADERF_ERR_NODEV;
    }

    return 0;
}

int main(int argc, char *argv[])

{
    /* Arguments:
     * - Firmware image
     * - Device string (optional)
     * - Load RAM only (option)
     * - Issue RESET before JUMP_TO_BOOTLOADER (option)
     */
    int status;
    struct rc_config rc;
    struct bladerf *dev;
    struct bladerf_devinfo devinfo;
    libusb_context *context;
    libusb_device_handle *device;

    /* If no actions are specified, just show the usage text and exit */
    if (argc == 1) {
        usage(argv[0]);
        return 0;
    }

    init_rc_config(&rc);

    if (get_rc_config(argc, argv, &rc)) {
        return 1;
    }

    log_set_verbosity(rc.verbosity);
    bladerf_log_set_verbosity(rc.verbosity);

    if (rc.show_help) {
        usage(argv[0]);
        return 0;
    } else if (rc.show_version) {
        printf(BLADERF_FLASH_VERSION "\n");
        return 0;
    } else if (rc.show_lib_version) {
        struct bladerf_version version;
        bladerf_version(&version);
        printf("%s\n", version.describe);
        return 0;
    }

    if (rc.fw_file == NULL) {
        fprintf(stderr, "Must provide FX3 firmware to flash\n");
        return -1;
    }

    /* Get initial device or scan */
    /* If normal bladeRF, perform protocol to get to FX3 bootloader */
    /* Perform RAM bootloader */
    /* Optionally flash new image to SPI flash */
    status = libusb_init(&context);
    if (status != 0) {
        log_error( "Could not initialize libusb: %s\n", libusb_error_name(status) );
        return status;
    }

    status = register_hotplug_notifications(context);
    if (status != 0) {
        return status;
    }

    log_verbose("Hotplug notifications installed\n");

    device = NULL;
    status = get_to_bootloader(rc.reset, rc.device, context, &device);
    if (status != 0) {
        log_error("Failed to find bladeRF bootloader after jumping, %d\n", status);
        return status;
    }

    log_info("Attempting load with file %s\n", rc.fw_file);
    status = ezusb_load_ram(device, rc.fw_file, FX_TYPE_FX3, IMG_TYPE_IMG, 0);
    libusb_close(device);

    if (status != 0) {
        log_error("Failed to load FX3 RAM %d\n", status);
        return status;
    }

    if (rc.load_ram_only) {
        log_info("All done\n");
        return 0;
    }

    status = get_bladerf(context, &devinfo);
    if (status != 0) {
        log_error("Failed to find bladeRF after loading RAM, %d\n", status);
        return status;
    }

    status = bladerf_open_with_devinfo(&dev, &devinfo);
    if (status != 0) {
        log_error("Error opening device again, %s\n", bladerf_strerror(status));
        return status;
    }

    status = bladerf_flash_firmware(dev, rc.fw_file);
    if (status != 0) {
        log_error("Error flashing firmware, %s\n", bladerf_strerror(status));
        return status;
    }

    status = bladerf_device_reset(dev);
    if (status != 0) {
        log_error("Error resetting bladeRF, %s\n", bladerf_strerror(status));
        return status;
    }

    status = get_bladerf(context, &devinfo);
    if (status == 0) {
        log_info("Successfully flashed bladeRF\n");
    } else {
        log_error("Failed to find bladeRF after flashing FX3 firmware, %d\n", status);
    }

    return 0;
}
