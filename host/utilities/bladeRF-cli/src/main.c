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
#include <pthread.h>
#include <string.h>
#include <libbladeRF.h>
#include "interactive.h"
#include "common.h"
#include "cmd.h"
#include "rxtx.h"
#include "version.h"


#define OPTSTR "d:f:l:s:ipLv:Vh"

static const struct option longopts[] = {
    { "device",         required_argument,  0, 'd' },
    { "flash-firmware", required_argument,  0, 'f' },
    { "load-fpga",      required_argument,  0, 'l' },
    { "script",         required_argument,  0, 's' },
    { "interactive",    no_argument,        0, 'i' },
    { "probe",          no_argument,        0, 'p' },
    { "lib-version",    no_argument,        0, 'L' },
    { "verbosity",      required_argument,  0, 'v' },
    { "version",        no_argument,        0, 'V' },
    { "help",           no_argument,        0, 'h' },
    { 0,                0,                  0,  0  },
};

/* Runtime configuration items */
struct rc_config {
    bool interactive_mode;
    bool flash_fw;
    bool load_fpga;
    bool probe;
    bool show_help;
    bool show_version;
    bool show_lib_version;

    bladerf_log_level verbosity;

    char *device;
    char *fw_file;
    char *fpga_file;
    char *script_file;
};

static void init_rc_config(struct rc_config *rc)
{
    rc->interactive_mode = false;
    rc->flash_fw = false;
    rc->load_fpga = false;
    rc->probe = false;
    rc->show_help = false;
    rc->show_version = false;
    rc->show_lib_version = false;

    rc->verbosity = BLADERF_LOG_LEVEL_INFO;

    rc->device = NULL;
    rc->fw_file = NULL;
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

            case 'V':
                rc->show_version = true;
                break;

            case 'L':
                rc->show_lib_version = true;
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
    printf("  -f, --flash-firmware <file>      Flash specified firmware file.\n");
    printf("  -l, --load-fpga <file>           Load specified FPGA bitstream.\n");
    printf("  -p, --probe                      Probe for devices, print results, then exit.\n");
    printf("  -s, --script <file>              Run provided script.\n");
    printf("  -i, --interactive                Enter interactive mode.\n");
    printf("  -L, --lib-version                Print libbladeRF version and exit.\n");
    printf("  -v, --verbosity <level>          Set the libbladeRF verbosity level.\n");
    printf("                                   Levels, listed in increasing verbosity, are:\n");
    printf("                                    critical, error, warning,\n");
    printf("                                    info, debug, verbose\n");
    printf("  -V, --version                    Print CLI version and exit.\n");
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
            fprintf(stderr, "Failed to open device (%s): %s\n",
                    rc->device ? rc->device : "NULL",
                    bladerf_strerror(status));
            status = -1;
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
        }
    }

    /* TODO Do we have to fire off some sort of reset after flashing
     *      the firmware, and before loading the FPGA? */

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

static int open_script(struct rc_config *rc, struct cli_state *state, int status)
{
    if (!status && rc->script_file) {
        state->script = fopen(rc->script_file, "r");
        if (!state->script) {
            status = -1;
        }
    }

    return status;
}

static inline int start_threads(struct cli_state *s)
{
    return rxtx_start_tasks(s);
}

static inline void stop_threads(struct cli_state *s)
{
    rxtx_stop_tasks(s->rxtx_data);
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
        printf("%s\n", bladerf_version(NULL, NULL, NULL));
        exit_immediately = true;
    } else if (rc.probe) {
        status = cmd_handle(state, "probe");
        exit_immediately = true;
    }

    if (!exit_immediately) {
        /* Conditionally performed items, depending on runtime config */
        status = open_device(&rc, state, status);
        if (status) {
            fprintf(stderr, "Could not open device\n");
            goto main__issues ;
        }

        status = flash_fw(&rc, state, status);
        if (status) {
            fprintf(stderr, "Could not flash firmware\n");
            goto main__issues ;
        }

        status = load_fpga(&rc, state, status);
        if (status) {
            fprintf(stderr, "Could not load fpga\n");
            goto main__issues ;
        }

        status = open_script(&rc, state, status);
        if (status) {
            fprintf(stderr, "Could not load scripts\n");
            goto main__issues ;
        }

main__issues:
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
        if (rc.interactive_mode || state->script != NULL) {
            status = start_threads(state);

            if (status < 0) {
                fprintf(stderr, "Failed to kick off threads\n");
            } else {
                status = interactive(state, !rc.interactive_mode);
                stop_threads(state);
            }

        }

        /* Ensure we exit with RX & TX disabled.
         * Can't do much about an error at this point anyway... */
        if (state->dev && bladerf_is_fpga_configured(state->dev)) {
            bladerf_enable_module(state->dev, BLADERF_MODULE_TX, false);
            bladerf_enable_module(state->dev, BLADERF_MODULE_RX, false);
        }
    }

    cli_state_destroy(state);
    return status;
}
