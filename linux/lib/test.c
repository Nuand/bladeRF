#include <stdlib.h>
#include <stdio.h>
#include "include/libbladeRF.h"

int main(int argc, char *argv[])
{
    struct bladerf *dev;

    if (argc != 2) {
        fprintf(stderr, "Usage: <device identifier>\n");
        return -1;
    }

    dev = bladerf_open(argv[0]);

    if (!dev) {
        fprintf(stderr, "Failed to open device!\n");
    }

    bladerf_close(dev);
}
