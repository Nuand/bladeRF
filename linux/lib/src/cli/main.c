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
    ssize_t n_devices;

    if ((n_devices = bladerf_get_device_list(&devices)) < 0) {
        fprintf(stderr, "Error: Failed to probe for devices.\n");
        return -1;
    }


    bladerf_free_device_list(devices, n_devices);

    return 0;
}

int main(int argc, char *argv[])
{
    probe();
    return 0;
}
