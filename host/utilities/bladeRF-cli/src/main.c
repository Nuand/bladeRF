/*
 * This file is part of the bladeRF project
 *
 * bladeRF command line interface
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
#include <pthread.h>
#include <string.h>
#include <libbladeRF.h>
#include "interactive.h"
#include "script.h"
#include "common.h"
#include "cmd.h"
#include "version.h"


#define OPTSTR "L:d:f:l:s:ipv:h"

static const struct option longopts[] = {
    { "flash-fpga",     required_argument,  0, 'L' },
    { "device",         required_argument,  0, 'd' },
    { "flash-firmware", required_argument,  0, 'f' },
    { "load-fpga",      required_argument,  0, 'l' },
    { "script",         required_argument,  0, 's' },
    { "interactive",    no_argument,        0, 'i' },
    { "probe",          no_argument,        0, 'p' },
    { "lib-version",    no_argument,        0,  1  },
    { "verbosity",      required_argument,  0, 'v' },
    { "version",        no_argument,        0,  2  },
    { "help",           no_argument,        0, 'h' },
    { 0,                0,                  0,  0  },
};

/* Runtime configuration items */
struct rc_config {
    bool interactive_mode;
    bool flash_fw;
    bool flash_fpga;
    bool load_fpga;
    bool probe;
    bool show_help;
    bool show_version;
    bool show_lib_version;

    bladerf_log_level verbosity;

    char *device;
    char *fw_file;
    char *flash_fpga_file;
    char *fpga_file;
    char *script_file;
};

static void init_rc_config(struct rc_config *rc)
{
    rc->interactive_mode = false;
    rc->flash_fw = false;
    rc->flash_fpga = false;
    rc->load_fpga = false;
    rc->probe = false;
    rc->show_help = false;
    rc->show_version = false;
    rc->show_lib_version = false;

    rc->verbosity = BLADERF_LOG_LEVEL_INFO;

    rc->device = NULL;
    rc->fw_file = NULL;
    rc->flash_fpga_file = NULL;
    rc->fpga_file = NULL;
    rc->script_file = NULL;
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
            case 'f':
                rc->fw_file = strdup(optarg);
                if (!rc->fw_file) {
                    perror("strdup");
                    return -1;
                }
                break;

            case 'L':
                rc->flash_fpga_file = strdup(optarg);
                if (!rc->flash_fpga_file) {
                    perror("strdup");
                    return -1;
                }
                break;

            case 'l':
                rc->fpga_file = strdup(optarg);
                if (!rc->fpga_file) {
                    perror("strdup");
                    return -1;
                }
                break;

            case 'd':
                rc->device = strdup(optarg);
                if (!rc->device) {
                    perror("strdup");
                    return -1;
                }
                break;

            case 's':
                rc->script_file = strdup(optarg);
                if (!rc->script_file) {
                    perror("strdup");
                    return -1;
                }
                break;

            case 'i':
                rc->interactive_mode = true;
                break;

            case 'p':
                rc->probe = true;
                break;

            case 'h':
                rc->show_help = true;
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

            case 1:
                rc->show_lib_version = true;
                break;

            case 2:
                rc->show_version = true;
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
    printf("bladeRF command line interface and test utility (" BLADERF_CLI_VERSION ")\n\n");
    printf("Options:\n");
    printf("  -d, --device <device>            Use the specified bladeRF device.\n");
    printf("  -f, --flash-firmware <file>      Write the provided FX3 firmware file to flash.\n");
    printf("  -l, --load-fpga <file>           Load the provided FPGA bitstream.\n");
    printf("  -L, --flash-fpga <file>          Write the provided FPGA image to flash for\n");
    printf("                                   autoloading. Use -L X or --flash-fpga X to\n");
    printf("                                   disable FPGA autoloading.\n");
    printf("  -p, --probe                      Probe for devices, print results, then exit.\n");
    printf("  -s, --script <file>              Run provided script.\n");
    printf("  -i, --interactive                Enter interactive mode.\n");
    printf("      --lib-version                Print libbladeRF version and exit.\n");
    printf("  -v, --verbosity <level>          Set the libbladeRF verbosity level.\n");
    printf("                                   Levels, listed in increasing verbosity, are:\n");
    printf("                                    critical, error, warning,\n");
    printf("                                    info, debug, verbose\n");
    printf("      --version                    Print CLI version and exit.\n");
    printf("  -h, --help                       Show this help text.\n");
    printf("\n");
    printf("Notes:\n");
    printf("  The -d option takes a device specifier string. See the bladerf_open()\n");
    printf("  documentation for more information about the format of this string.\n");
    printf("\n");
    printf("  If the -d parameter is not provided, the first available device\n");
    printf("  will be used for the provided command, or will be opened prior\n");
    printf("  to entering interactive mode.\n");
    printf("\n");
}

static void print_error_need_devarg()
{
    printf("\nError: Either no devices are present, or multiple devices are\n"
            "        present and -d was not specified. Aborting.\n\n");
}

static int open_device(struct rc_config *rc, struct cli_state *state, int status)
{
    if (!status) {
        status = bladerf_open(&state->dev, rc->device);
        if (status) {

            /* Just warn if no device is attached; don't error out */
            if (!rc->device && status == BLADERF_ERR_NODEV) {
                fprintf(stderr, "No device(s) attached.\n");
                status = 0;
            } else {
                fprintf(stderr, "Failed to open device (%s): %s\n",
                        rc->device ? rc->device : "first available",
                        bladerf_strerror(status));
                status = -1;
            }
        }
    }

    return status;
}

static int flash_fw(struct rc_config *rc, struct cli_state *state, int status)
{
    if (!status && rc->fw_file) {
        if (!state->dev) {
            print_error_need_devarg();
            status = -1;
        } else {
            printf("Flashing firmware...\n");
            status = bladerf_flash_firmware(state->dev, rc->fw_file);
            if (status) {
                fprintf(stderr, "Error: failed to flash firmware: %s\n",
                        bladerf_strerror(status));
            } else {
                printf("Done.\n");
            }
            bladerf_device_reset(state->dev);
#if 0 // Need a workaround for this just in case
            bladerf_close(state->dev);
#endif
            state->dev = NULL;
        }
    }

    /* TODO Do we have to fire off some sort of reset after flashing
     *      the firmware, and before loading the FPGA? */

    return status;
}

static int flash_fpga(struct rc_config *rc, struct cli_state *state, int status)
{
    if (!status && rc->flash_fpga_file) {
        if (!state->dev) {
            print_error_need_devarg();
            status = -1;
        } else {
            printf("Flashing fpga...\n");
            status = bladerf_flash_fpga(state->dev, rc->flash_fpga_file);
            if (status) {
                fprintf(stderr, "Error: failed to flash FPGA: %s\n",
                        bladerf_strerror(status));
            } else {
                printf("Done.\n");
            }
        }
    }

    return status;
}

static int load_fpga(struct rc_config *rc, struct cli_state *state, int status)
{
    if (!status && rc->fpga_file) {
        if (!state->dev) {
            print_error_need_devarg();
            status = -1;
        } else {
            printf("Loading fpga...\n");
            status = bladerf_load_fpga(state->dev, rc->fpga_file);
            if (status) {
                fprintf(stderr, "Error: failed to load FPGA: %s\n",
                        bladerf_strerror(status));
            } else {
                printf("Done.\n");
            }
        }
    }

    return status;
}

int main(int argc, char *argv[])
{
    int status = 0;
    struct rc_config rc;
    struct cli_state *state;
    bool exit_immediately = false;

    /* If no actions are specified, just show the usage text and exit */
    if (argc == 1) {
        usage(argv[0]);
        return 0;
    }

    init_rc_config(&rc);

    if (get_rc_config(argc, argv, &rc)) {
        return 1;
    }

    state = cli_state_create();

    if (!state) {
        fprintf(stderr, "Failed to create state object\n");
        return 1;
    }

    bladerf_log_set_verbosity(rc.verbosity);

    if (rc.show_help) {
        usage(argv[0]);
        exit_immediately = true;
    } else if (rc.show_version) {
        printf(BLADERF_CLI_VERSION "\n");
        exit_immediately = true;
    } else if (rc.show_lib_version) {
        struct bladerf_version version;
        bladerf_version(&version);
        printf("%s\n", version.describe);
        exit_immediately = true;
    } else if (rc.probe) {
        status = cmd_handle(state, "probe");
        exit_immediately = true;
    }

    if (!exit_immediately) {
        /* Conditionally performed items, depending on runtime config */
        status = open_device(&rc, state, status);
        if (status) {
            goto main_issues;
        }

        status = flash_fw(&rc, state, status);
        if (status) {
            goto main_issues;
        }

        status = flash_fpga(&rc, state, status);
        if (status) {
            goto main_issues;
        }

        status = load_fpga(&rc, state, status);
        if (status) {
            goto main_issues;
        }

        if (rc.script_file) {
            status = cli_open_script(&state->scripts, rc.script_file);
            if (status != 0) {
                goto main_issues;
            }
        }

main_issues:
        /* These items are no longer needed */
        free(rc.device);
        rc.device = NULL;

        free(rc.fw_file);
        rc.fw_file = NULL;

        free(rc.fpga_file);
        rc.fpga_file = NULL;

        free(rc.script_file);
        rc.script_file = NULL;

        /* Drop into interactive mode or begin executing commands
         * from a script. If we're not requested to do either, exit cleanly */
        if (rc.interactive_mode || cli_script_loaded(state->scripts)) {
            status = interactive(state, !rc.interactive_mode);
        }
    }

    cli_state_destroy(state);
    return status;
}
