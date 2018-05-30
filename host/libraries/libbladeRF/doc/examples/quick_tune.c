#include <libbladeRF.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_FREQUENCIES 6
#define ITERATIONS 10000

int example(struct bladerf *dev, bladerf_channel ch)
{
    /** [example] */
    int status;
    unsigned int i, j;

    // clang-format off
    const unsigned int frequencies[NUM_FREQUENCIES] = {
        902000000,
        903000000,
        904000000,
        925000000,
        926000000,
        927000000,
    };
    // clang-format on

    struct bladerf_quick_tune quick_tunes[NUM_FREQUENCIES];

    /* Get our quick tune parameters for each frequency we'll be using */
    for (i = 0; i < NUM_FREQUENCIES; i++) {
        status = bladerf_set_frequency(dev, ch, frequencies[i]);
        if (status != 0) {
            fprintf(stderr, "Failed to set frequency to %u Hz: %s\n",
                    frequencies[i], bladerf_strerror(status));
            return status;
        }

        status = bladerf_get_quick_tune(dev, ch, &quick_tunes[i]);
        if (status != 0) {
            fprintf(stderr, "Failed to get quick tune for %u Hz: %s\n",
                    frequencies[i], bladerf_strerror(status));
            return status;
        }
    }

    for (i = j = 0; i < ITERATIONS; i++) {
        /* Tune to the specified frequency immediately via BLADERF_RETUNE_NOW.
         *
         * Alternatively, this re-tune could be scheduled by providing a
         * timestamp counter value */
        status = bladerf_schedule_retune(dev, ch, BLADERF_RETUNE_NOW, 0,
                                         &quick_tunes[j]);

        if (status != 0) {
            fprintf(stderr, "Failed to apply quick tune: %s\n",
                    bladerf_strerror(status));
            return status;
        }

        j = (j + 1) % NUM_FREQUENCIES;

        /* ... Handle signals at current frequency ... */
    }

    /** [example] */

    return status;
}

int main(int argc, char *argv[])
{
    int status = 0;

    struct bladerf *dev = NULL;
    struct bladerf_devinfo dev_info;

    /* Initialize the information used to identify the desired device
     * to all wildcard (i.e., "any device") values */
    bladerf_init_devinfo(&dev_info);

    /* Request a device with the provided serial number.
     * Invalid strings should simply fail to match a device. */
    if (argc >= 2) {
        strncpy(dev_info.serial, argv[1], sizeof(dev_info.serial) - 1);
    }

    status = bladerf_open_with_devinfo(&dev, &dev_info);
    if (status != 0) {
        fprintf(stderr, "Unable to open device: %s\n",
                bladerf_strerror(status));

        return 1;
    }

    /* A quick check that this works is to watch LO leakage on a VSA */

    status = bladerf_enable_module(dev, BLADERF_TX, true);
    if (status != 0) {
        fprintf(stderr, "Failed to enable TX: %s\n", bladerf_strerror(status));
        return status;
    }

    status = example(dev, BLADERF_CHANNEL_TX(0));

    bladerf_enable_module(dev, BLADERF_TX, false);
    bladerf_close(dev);
    return status;
}
