/* This program is intended to exercise a situation where sync interfaces
 * are initialized, but not used.
 *
 * Prior to the fix to issue #244, this yielded lockups.
 */

#include <stdlib.h>
#include <stdio.h>
#include <libbladeRF.h>

int main(int argc, char *argv[])
{
    struct bladerf *dev;
    int status;

    status = bladerf_open(&dev, NULL);
    if (status != 0) {
        fprintf(stderr, "Failed to open device: %s\n",
                bladerf_strerror(status));
        return EXIT_FAILURE;
    }

    status = bladerf_sync_config(dev, BLADERF_MODULE_RX,
                                 BLADERF_FORMAT_SC16_Q11,
                                 64, 16384, 16, 3500);

    if (status != 0) {
        fprintf(stderr, "Failed to init RX: %s", bladerf_strerror(status));
    }

    status = bladerf_sync_config(dev, BLADERF_MODULE_TX,
                                 BLADERF_FORMAT_SC16_Q11,
                                 64, 16384, 16, 3500);

    if (status != 0) {
        fprintf(stderr, "Failed to init TX: %s\n", bladerf_strerror(status));
    }

    bladerf_close(dev);
    return status;
}
