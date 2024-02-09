#include <libbladeRF.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FREQ_HOP_SPACING (bladerf_frequency)40e6
#define ITERATIONS 10000

#define CHECK_STATUS(_fn) do {                                             \
        status = _fn;                                                      \
        if (status != 0) {                                                 \
            fprintf(stderr, "[Error] %s:%d: %s\n", __FUNCTION__, __LINE__, \
                    bladerf_strerror(status));                             \
            goto error;                                                    \
        }                                                                  \
    } while (0)

#define CHECK_NULL(_ptr) do {                                                 \
        if (_ptr == NULL) {                                                   \
            fprintf(stderr, "Error: failed to alloc memory for %s\n", #_ptr); \
            status = BLADERF_ERR_MEM;                                         \
            goto error;                                                       \
        }                                                                     \
    } while (0)

struct {
    bladerf_channel ch;
    bladerf_frequency frequency;
} state;

int main(int argc, char *argv[])
{
    int status = -1;
    unsigned int i, j;

    struct bladerf *dev = NULL;
    struct bladerf_devinfo dev_info;

    const struct bladerf_range *freq_range = NULL;
    bladerf_frequency freq_min, freq_max;
    bladerf_timestamp timestamp;
    bladerf_frequency *frequencies = NULL;
    const bladerf_channel ch = BLADERF_CHANNEL_RX(0);
    struct bladerf_quick_tune *quick_tunes = NULL;

    bladerf_init_devinfo(&dev_info);

    /* Request a device with the provided serial number.
     * Invalid strings should simply fail to match a device. */
    if (argc >= 2) {
        strncpy(dev_info.serial, argv[1], sizeof(dev_info.serial) - 1);
    }

    CHECK_STATUS(bladerf_open_with_devinfo(&dev, &dev_info));
    CHECK_STATUS(bladerf_set_sample_rate(dev, BLADERF_RX, FREQ_HOP_SPACING, NULL));

    CHECK_STATUS(bladerf_get_frequency_range(dev, ch, &freq_range));
    freq_min = freq_range->min * freq_range->scale;
    freq_max = freq_range->max * freq_range->scale;

    size_t num_frequencies = (freq_max - freq_min) / FREQ_HOP_SPACING + 2;
    frequencies = malloc(num_frequencies * sizeof(bladerf_frequency));
    CHECK_NULL(frequencies);

    for(i = 0; i < num_frequencies; i++) {
        bladerf_frequency frequency = freq_min + i * FREQ_HOP_SPACING;
        frequencies[i] = frequency;

        if (frequency > freq_max) {
            frequencies[i] = freq_max;
        }
    }

    /* Get our quick tune parameters for each frequency we'll be using */
    quick_tunes = malloc(num_frequencies * sizeof(struct bladerf_quick_tune));
    CHECK_NULL(quick_tunes);

    for (i = 0; i < num_frequencies; i++) {
        status = bladerf_set_frequency(dev, ch, frequencies[i]);
        if (status != 0) {
            fprintf(stderr, "Failed to set frequency to %lu Hz: %s\n",
                    frequencies[i], bladerf_strerror(status));
            return status;
        }

        bladerf_log_set_verbosity(BLADERF_LOG_LEVEL_VERBOSE);
        printf("Frequency: %lu Hz\n", frequencies[i]);
        status = bladerf_get_quick_tune(dev, ch, &quick_tunes[i]);
        if (status != 0) {
            fprintf(stderr, "Failed to get quick tune for %lu Hz: %s\n",
                    frequencies[i], bladerf_strerror(status));
            return status;
        }
        bladerf_log_set_verbosity(BLADERF_LOG_LEVEL_INFO);

    }

    CHECK_STATUS(bladerf_sync_config(dev, BLADERF_RX_X1, BLADERF_FORMAT_SC16_Q11_META, 1024, 8192, 16, 1000));

    printf("Starting frequency hopping...\n");
    CHECK_STATUS(bladerf_enable_module(dev, BLADERF_RX, true));
    bladerf_get_timestamp(dev, BLADERF_RX, &timestamp);
    timestamp += 1000;
    for (i = j = 0; i < ITERATIONS; i++) {
        CHECK_STATUS(bladerf_schedule_retune(dev, ch, timestamp, frequencies[i % num_frequencies], &quick_tunes[j]));

        j = (j + 1) % num_frequencies;
        timestamp += 256;
    }

error:
    free(frequencies);
    free(quick_tunes);
    bladerf_enable_module(dev, BLADERF_RX, false);
    bladerf_close(dev);
    return status;
}
