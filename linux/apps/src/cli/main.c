/*
 * bladeRF command line interface
 *
 * This program requires libtecla for interactive mode.
 */
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <stdbool.h>
#include <errno.h>
#include <libbladeRF.h>
#include "interactive.h"
#include "common.h"
#include "cmd.h"
#include "version.h"


#define OPTSTR "pd:bf:l:LVh"
static const struct option longopts[] = {
    { "device",         required_argument,  0, 'd' },
    { "batch",          no_argument,        0, 'b' },
    { "flash-firmware", required_argument,  0, 'f' },
    { "probe",          no_argument,        0, 'p' },
    { "load-fpga",      required_argument,  0, 'l' },
    { "lib-version",    no_argument,        0, 'L' },
    { "version",        no_argument,        0, 'V' },
    { "help",           no_argument,        0, 'h' },
    { 0,                0,                  0,  0  },
};

/* Runtime configuration items */
struct rc_config {
    bool batch_mode;
    bool flash_fw;
    bool load_fpga;
    bool probe;
    bool show_help;
    bool show_version;
    bool show_lib_version;

    char *device;
    char *fw_file;
    char *fpga_file;
};

#define DEFAULT_RC_CONFIG {\
    .device = NULL, \
    .batch_mode = false, \
    .flash_fw = false, \
    .load_fpga = false, \
    .probe = false, \
    .show_version = false, \
    .show_lib_version = false, \
    .fw_file = NULL, \
    .fpga_file = NULL, \
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

            case 'b':
                rc->batch_mode = true;
                break;

            case 'p':
                rc->probe = true;
                rc->batch_mode = true;
                break;

            case 'h':
                rc->show_help = true;
                rc->batch_mode = true;
                break;

            case 'V':
                rc->show_version = true;
                rc->batch_mode = true;
                break;

            case 'L':
                rc->show_lib_version = true;
                rc->batch_mode = true;
                break;
        }

        c = getopt_long(argc, argv, OPTSTR, longopts, &optidx);
    } while (c != -1);

    return 0;
}

void usage(const char *argv0)
{
    printf("Usage: %s [options]\n", argv0);
    printf("bladeRF command line interface and test utility (" CLI_VERSION ")\n\n");
    printf("Options:\n");
    printf("  -d, --device <device>            Use the specified bladeRF device.\n");
    printf("  -f, --flash-firmware <file>      Flash specified firmware file.\n");
    printf("  -l, --load-fpga <file>           Load specified FPGA bitstream.\n");
    printf("  -p, --probe                      Probe for devices, print results, then exit.\n");
    printf("  -b, --batch                      Batch mode - do not enter interactive mode.\n");
    printf("  -L, --lib-version                Print libbladeRF version and exit.\n");
    printf("  -V, --version                    Print CLI version and exit.\n");
    printf("  -h, --help                       Show this help text.\n");
    printf("\n");
    printf("Notes:\n");
    printf("  If the -d parameter is not provided, the first available device\n");
    printf("  will be used for the provided command, or will be opened prior\n");
    printf("  to entering interactive mode.\n");
    printf("\n");
    printf("  Batch mode is implicit for the following options:\n");
    printf("     -p, --probe               -h, --help\n");
    printf("     -L, --lib-version         -V, --version\n");
    printf("\n");

#ifndef INTERACTIVE
    printf("  This tool has been build with INTERACTIVE=n. the interactive\n"
           "  console is disabled.\n");
#endif
}

static void print_error_need_devarg()
{
    printf("\nError: Either no devices or multiple devices are present,\n"
           "       but -d was not specified. Aborting.\n\n");
}

int main(int argc, char *argv[])
{
    int status;
    struct rc_config rc = DEFAULT_RC_CONFIG;
    struct cli_state state = CLI_STATE_INITIALIZER;

    if (get_rc_config(argc, argv, &rc)) {
        return 1;
    }

    status = 0;

    if (rc.show_help) {
        usage(argv[0]);
    } else if (rc.show_version) {
        printf(CLI_VERSION "\n");
    } else if (rc.show_lib_version) {
        /* TODO implement libbladerf_version() */
        printf("TODO!\n");
    } else if (rc.probe) {
        status = cmd_handle(&state, "probe");
    }

    if (rc.device) {
        state.curr_device = bladerf_open(rc.device);
        if (!state.curr_device) {
            /* TODO use upcoming bladerf_strerror() here */
            fprintf(stderr, "Failed to open device (%s): %s\n",
                    rc.device, strerror(errno));
            status = -1;
        }
    } else {
        /* TODO re-enable once the composite device situation is resolved */
        //state.curr_device = bladerf_open_any();
    }

    if (!status && rc.fw_file) {
        if (!state.curr_device) {
            print_error_need_devarg();
            status = -1;
        } else {
            printf("Flashing firmware...\n");
            status = bladerf_flash_firmware(state.curr_device, rc.fw_file);
            if (status) {
                fprintf(stderr, "Error: failed to flash firmware: %s\n",
                        bladerf_strerror(status));
            } else {
                printf("Done.\n");
            }

        }
    }

    /* TODO Do we have to fire off some sort of reset after flashing
     *      the firmware, and before loading the FPGA */

    if (rc.fpga_file) {
        if (!state.curr_device) {
            print_error_need_devarg();
            status = -1;
        } else {
            printf("Loading fpga...\n");
            status = bladerf_load_fpga(state.curr_device, rc.fpga_file);
            if (status) {
                fprintf(stderr, "Error: failed to flash firmware: %s\n",
                        bladerf_strerror(status));
            } else {
                printf("Done.\n");
            }
        }
    }

    free(rc.device);
    free(rc.fw_file);
    free(rc.fpga_file);

    /* Abort if anything went wrong */
    if (status)
        return 2;

    /* Exit cleanly when configured for batch mode. Remember that this is
     * implicit with some commands */
    if (rc.batch_mode) {
        if (state.curr_device)
            bladerf_close(state.curr_device);
        return 0;
    } else {
        return interactive(&state);
    }
}
