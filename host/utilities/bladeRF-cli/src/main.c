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
#include "input/input.h"
#include "str_queue.h"
#include "script.h"
#include "common.h"
#include "cmd.h"
#include "version.h"


#define OPTSTR "e:L:d:f:l:s:ipv:h"

static const struct option longopts[] = {
    { "exec",               required_argument,  0, 'e' },
    { "flash-fpga",         required_argument,  0, 'L' },
    { "device",             required_argument,  0, 'd' },
    { "flash-firmware",     required_argument,  0, 'f' },
    { "load-fpga",          required_argument,  0, 'l' },
    { "script",             required_argument,  0, 's' },
    { "interactive",        no_argument,        0, 'i' },
    { "probe",              no_argument,        0, 'p' },
    { "lib-version",        no_argument,        0,  1  },
    { "verbosity",          required_argument,  0, 'v' },
    { "version",            no_argument,        0,  2  },
    { "help",               no_argument,        0, 'h' },
    { "help-interactive",   no_argument,        0,  3  },
    { 0,                    0,                  0,  0  },
};

/* Runtime configuration items */
struct rc_config {
    bool interactive_mode;
    bool flash_fw;
    bool flash_fpga;
    bool load_fpga;
    bool probe;
    bool show_help;
    bool show_help_interactive;
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
    rc->show_help_interactive = false;

    rc->verbosity = BLADERF_LOG_LEVEL_INFO;

    rc->device = NULL;
    rc->fw_file = NULL;
    rc->flash_fpga_file = NULL;
    rc->fpga_file = NULL;
    rc->script_file = NULL;
}

static void deinit_rc_config(struct rc_config *rc)
{
    free(rc->device);
    free(rc->fw_file);
    free(rc->flash_fpga_file);
    free(rc->fpga_file);
    free(rc->script_file);
}

/* Fetch runtime-configuration info
 *
 * Returns 0 on success, -1 on fatal error (and prints error msg to stderr)
 */
int get_rc_config(int argc, char *argv[], struct rc_config *rc,
                  struct str_queue *exec_list)
{
    int optidx;
    int c = getopt_long(argc, argv, OPTSTR, longopts, &optidx);

    do {
        switch(c) {
            case 'e':
                if (str_queue_enq(exec_list, optarg) != 0) {
                    return -1;
                }
                break;

            case 'f':
                if (rc->fw_file != NULL) {
                    fprintf(stderr, "Error: Firmware file specified more "
                            "than once.\n");
                    return -1;
                }

                rc->fw_file = strdup(optarg);
                if (!rc->fw_file) {
                    perror("strdup");
                    return -1;
                }
                break;

            case 'L':
                if (rc->flash_fpga_file != NULL) {
                    fprintf(stderr, "Error: FPGA file specified more "
                            "than once.\n");
                    return -1;
                }

                rc->flash_fpga_file = strdup(optarg);
                if (!rc->flash_fpga_file) {
                    perror("strdup");
                    return -1;
                }
                break;

            case 'l':
                if (rc->fpga_file != NULL) {
                    fprintf(stderr, "Error: FPGA file specified more "
                            "than once.\n");
                    return -1;
                }

                rc->fpga_file = strdup(optarg);
                if (!rc->fpga_file) {
                    perror("strdup");
                    return -1;
                }
                break;

            case 'd':
                if (rc->device != NULL) {
                    fprintf(stderr, "Error: FPGA file specified more "
                            "than once.\n");
                    return -1;
                }

                rc->device = strdup(optarg);
                if (!rc->device) {
                    perror("strdup");
                    return -1;
                }
                break;

            case 's':
                if (rc->script_file != NULL) {
                    fprintf(stderr, "Error: Script file already specified.\n");
                    return -1;
                }

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

            case 3:
                rc->show_help_interactive = true;
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
    printf("                                    A non-zero return status will be returned if no\n");
    printf("                                    devices are available.\n");
    printf("  -e, --exec <command>             Execute the specified interactive mode command.\n");
    printf("                                   Multiple -e flags may be specified. The commands\n");
    printf("                                   will be executed in the provided order.\n");
    printf("  -s, --script <file>              Run provided script.\n");
    printf("  -i, --interactive                Enter interactive mode.\n");
    printf("      --lib-version                Print libbladeRF version and exit.\n");
    printf("  -v, --verbosity <level>          Set the libbladeRF verbosity level.\n");
    printf("                                   Levels, listed in increasing verbosity, are:\n");
    printf("                                    critical, error, warning,\n");
    printf("                                    info, debug, verbose\n");
    printf("      --version                    Print CLI version and exit.\n");
    printf("  -h, --help                       Show this help text.\n");
    printf("      --help-interactive           Print help information for all interactive\n");
    printf("                                   commands.\n");
    printf("\n");
    printf("Notes:\n");
    printf("  The -d option takes a device specifier string. See the bladerf_open()\n");
    printf("  documentation for more information about the format of this string.\n");
    printf("\n");
    printf("  If the -d parameter is not provided, the first available device\n");
    printf("  will be used for the provided command, or will be opened prior\n");
    printf("  to entering interactive mode.\n");
    printf("\n");
    printf("  Commands are executed in the following order:\n");
    printf("    Command line options, -e <command>, script commands, interactive mode commands.\n");
    printf("\n");
    printf("  When running 'rx/tx start' from a script or via -e, ensure these commands\n");
    printf("  are later followed by 'rx/tx wait [timeout]' to ensure the program will\n");
    printf("  not attempt to exit before reception/transmission is complete.\n");
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
                fprintf(stderr, "\n");
                fprintf(stderr, "No bladeRF device(s) available.\n");
                fprintf(stderr, "\n");
                fprintf(stderr, "If one is attached, ensure it is not in use by another program\n");
                fprintf(stderr, "and that the current user has permission to access it.\n");
                fprintf(stderr, "\n");
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
                printf("Done. "
                       "A power cycle is required for this to take effect.\n");
            }
            bladerf_close(state->dev);
            state->dev = NULL;
        }
    }

    return status;
}

static int flash_fpga(struct rc_config *rc, struct cli_state *state, int status)
{
    if (!status && rc->flash_fpga_file) {
        if (!state->dev) {
            print_error_need_devarg();
            status = -1;
        } else {
            if (!strcmp("X", rc->flash_fpga_file)) {
                printf("Erasing stored FPGA to disable autoloading...\n");
                status = bladerf_erase_stored_fpga(state->dev);
            } else {
                printf("Writing FPGA to flash for autoloading...\n");
                status = bladerf_flash_fpga(state->dev, rc->flash_fpga_file);
            }

            if (status) {
                fprintf(stderr, "Error: %s\n", bladerf_strerror(status));
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

void check_for_bootloader_devs()
{
    int num_devs;
    struct bladerf_devinfo *list;

    num_devs = bladerf_get_bootloader_list(&list);

    if (num_devs <= 0) {
        if (num_devs != BLADERF_ERR_NODEV) {
            fprintf(stderr, "Error: failed to check for bootloader devices.\n");
        }

        return;
    }

    bladerf_free_device_list(list);
    printf("NOTE: One or more FX3-based devices operating in bootloader mode\n"
           "      were detected. Run 'help recover' to view information about\n"
           "      downloading firmware to the device(s).\n\n");
}

int main(int argc, char *argv[])
{
    int status = 0;
    struct rc_config rc;
    struct cli_state *state;
    bool exit_immediately = false;
    struct str_queue exec_list;

    /* If no actions are specified, just show the usage text and exit */
    if (argc == 1) {
        usage(argv[0]);
        return 0;
    }

    str_queue_init(&exec_list);
    init_rc_config(&rc);

    if (get_rc_config(argc, argv, &rc, &exec_list)) {
        return 1;
    }

    state = cli_state_create();

    if (!state) {
        fprintf(stderr, "Failed to create state object\n");
        return 1;
    }

    state->exec_list = &exec_list;
    bladerf_log_set_verbosity(rc.verbosity);

    if (rc.show_help) {
        usage(argv[0]);
        exit_immediately = true;
    } else if (rc.show_help_interactive) {
        printf("Interactive Commands:\n\n");
        cmd_show_help_all();
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
        status = cmd_handle(state, "probe strict");
        exit_immediately = true;
    }

    if (!exit_immediately) {
        check_for_bootloader_devs();

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
                fprintf(stderr, "Failed to open script file \"%s\": %s\n",
                        rc.script_file, strerror(-status));
                goto main_issues;
            }
        }

        /* Drop into interactive mode or begin executing commands from a a
         * command-line list or a script. If we're not requested to do either,
         * exit cleanly */
        if (!str_queue_empty(&exec_list) || rc.interactive_mode ||
            cli_script_loaded(state->scripts)) {

            status = cli_start_tasks(state);
            if (status == 0) {
                status = input_loop(state, rc.interactive_mode);
            }
        }
    }

main_issues:
    cli_state_destroy(state);
    str_queue_deinit(&exec_list);
    deinit_rc_config(&rc);
    return status;
}
