/*
 * bladeRF command line interface
 *
 * This program requires libtecla for interactive mode.
 */
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <stdbool.h>
#include <libbladeRF.h>
#include <libtecla.h>

#include "version.h"

#define OPTSTR "d:bf:l:LVh"
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
                rc->fw_file = calloc(1, strlen(optarg) + 1);
                if (rc->fw_file)
                    strcpy(rc->fw_file, optarg);
                else {
                    perror("calloc");
                    return -1;
                }
                break;

            case 'b':
                rc->batch_mode = true;
                break;

            case 'p':
                rc->probe = true;
                break;

            case 'h':
                rc->show_help = true;
                break;

            case 'V':
                rc->show_version = true;
                break;

            case 'L':
                rc->show_lib_version = true;
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
    printf("  -f, --flash-firmware <file>      Flash specified firmware file. Requires -d.\n");
    printf("  -l, --load-fpga <file>           Load specified FPGA bitstream. Requires -d.\n");
    printf("  -p, --probe                      Probe for devices, print results, then exit.\n");
    printf("  -b, --batch                      Batch mode - do not enter interactive mode.\n");
    printf("  -L, --lib-version                Print libbladeRF version and exit.\n");
    printf("  -V, --version                    Print CLI version and exit.\n");
    printf("  -h, --help                       Show this help text.\n");
    printf("\n");
}

/* Todo move to cmd_probe.c */
int cmd_probe()
{
    struct bladerf_devinfo *devices = NULL;
    ssize_t n_devices, i;

    if ((n_devices = bladerf_get_device_list(&devices)) < 0) {
        fprintf(stderr, "Error: Failed to probe for devices.\n");
        return -1;
    }

    printf("     Attached bladeRF devices:\n");
    printf("---------------------------------------\n");
    for (i = 0; i < n_devices; i++) {
        printf("    Path: %s\n", devices[i].path);
        printf("    Serial: 0x%016lX\n", devices[i].serial);
        printf("    Firmware: v%d.%d\n", devices[i].fw_ver_maj,
               devices[i].fw_ver_min);

        if (devices[i].fpga_configured) {
            printf("    FPGA: v%d.%d\n",
                    devices[i].fpga_ver_maj, devices[i].fpga_ver_min);
        } else {
            printf("    FPGA: not configured\n");
        }

        printf("\n");
    }

    bladerf_free_device_list(devices, n_devices);

    return 0;
}


#ifndef CLI_MAX_LINE_LEN
#   define CLI_MAX_LINE_LEN 320
#endif

#ifndef CLI_MAX_HIST_LEN
#   define CLI_MAX_HIST_LEN 500
#endif

#ifndef CLI_PROMPT
#   define CLI_PROMPT   "bladeRF> "
#endif
static inline int interactive()
{
    char *line;
    bool quit = false;
    GetLine *gl = new_GetLine(CLI_MAX_LINE_LEN, CLI_MAX_HIST_LEN);

    if (!gl) {
        perror("new_GetLine");
        return 2;
    }

    while (!quit) {
        line = gl_get_line(gl, CLI_PROMPT, NULL, 0);
        if (!line)
            break;

        quit = !strcasecmp(line, "quit\n") || !strcasecmp(line, "exit\n");
    }

    del_GetLine(gl);
    return 0;
}

int main(int argc, char *argv[])
{
    int status;
    struct rc_config rc = DEFAULT_RC_CONFIG;

    if (get_rc_config(argc, argv, &rc))
        return 1;

    status = 0;

    if (rc.show_help)
        usage(argv[0]);
    else if (rc.show_version)
        printf(CLI_VERSION "\n");
    else if (rc.show_lib_version) {
        /* TODO implement libbladerf_version() */
        printf("TODO!\n");
    } else if (rc.probe)
        status = cmd_probe();


    free(rc.device);
    free(rc.fw_file);
    free(rc.fpga_file);

    /* Abort if anything went wrong */
    if (status)
        return 2;

    /* Exit cleanly if we did some 'X and exit' operations */
    if (rc.batch_mode || rc.show_help || rc.show_version || rc.show_lib_version)
        return 0;
    else
        return interactive();
}
