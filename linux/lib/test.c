#include <stdlib.h>
#include <stdio.h>
#include "include/libbladeRF.h"

int main(int argc, char *argv[])
{
    struct bladerf *dev;
    int status;

    if (argc != 2) {
        fprintf(stderr, "Usage: <device identifier>\n");
        return -1;
    }

    dev = bladerf_open(argv[1]);

    if (dev) {
        status = bladerf_load_fpga(dev, "hostedx40.rbf");
        if (status < 0) {
            fprintf(stderr, "Failed to load FPGA: %s\n", bladerf_strerror(status));
        }

        bladerf_close(dev);
     } else {
        fprintf(stderr, "Failed to open device!\n");
     }
}
