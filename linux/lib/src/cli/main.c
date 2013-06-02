/*
 * Simple CLI program to support testing/debugging API calls
 */
#include <stdlib.h>
#include <stdio.h>
#include "bladerf.h"

void usage(const char *argv0)
{
    printf("Usage: %s [options]\n", argv0);
    printf("bladeRF command line interface and test utility\n");
}

int probe()
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
        printf("    Firmware: v%02d.%02d\n", devices[i].fw_ver_maj,
               devices[i].fw_ver_min);
        printf("    FPGA Configured: %s\n\n",
                devices[i].fpga_configured ? "yes" : "no");
    }

    bladerf_free_device_list(devices, n_devices);

    return 0;
}

int main(int argc, char *argv[])
{
    struct bladerf *dev;
    probe();

    dev = bladerf_open_any();
    if (dev) {
        printf("Opened device. Closing...\n");
        bladerf_close(dev);
    }

    return 0;
}
