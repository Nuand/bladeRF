#include <stdio.h>
#include <inttypes.h>
#include <libbladeRF.h>
#include <unistd.h>
#include "log.h"

#define CHECK(func_call) do { \
    status = (func_call); \
    if (status != 0) { \
        fprintf(stderr, "Failed at %s: %s\n", #func_call, bladerf_strerror(status)); \
        status = EXIT_FAILURE; \
        goto out; \
    } \
} while (0)

static int init_sync(struct bladerf *dev, const size_t NUM_CHANNELS, bladerf_sample_rate samp_rate)
{
    int status;
    const unsigned int num_buffers   = 10*512;
    const unsigned int buffer_size   = 2*8192; /* Must be a multiple of 1024 */
    const unsigned int num_transfers = 64;
    const unsigned int timeout_ms    = 3500;
    const bladerf_frequency freq     = 915e6;

    CHECK(bladerf_sync_config(dev, BLADERF_RX_X2, BLADERF_FORMAT_SC16_Q11,
                              num_buffers, buffer_size, num_transfers,
                              timeout_ms));

    for (size_t i = 0; i < NUM_CHANNELS; i++) {
        CHECK(bladerf_set_frequency(dev, BLADERF_CHANNEL_RX(i), (bladerf_frequency)freq));
        CHECK(bladerf_set_sample_rate(dev, BLADERF_CHANNEL_RX(i), samp_rate, NULL));
        CHECK(bladerf_set_gain_mode(dev, BLADERF_CHANNEL_RX(i), BLADERF_GAIN_MGC));
        CHECK(bladerf_set_gain(dev, BLADERF_CHANNEL_RX(i), 60));
    }

out:
    return status;
}

int main(int argc, char *argv[]) {
    int status;
    struct bladerf *dev = NULL;
    char *devstr = NULL;
    bladerf_sample_rate samp_rate = 2e6;

    const size_t NUM_CHANNELS = 2;
    const int NUM_SAMPLES = 10e6;
    int16_t *samples = NULL;
    FILE *fp = NULL;

    bladerf_log_set_verbosity(BLADERF_LOG_LEVEL_DEBUG);

    samples = (int16_t *)malloc(2 * NUM_CHANNELS * NUM_SAMPLES * sizeof(int16_t));
    if (samples == NULL) {
        fprintf(stderr, "Failed to allocate memory for samples\n");
        status = EXIT_FAILURE;
        goto out;
    }

    CHECK(bladerf_open(&dev, devstr));
    CHECK(init_sync(dev, NUM_CHANNELS, samp_rate));

    CHECK(bladerf_enable_module(dev, BLADERF_CHANNEL_RX(0), true));
    CHECK(bladerf_enable_module(dev, BLADERF_CHANNEL_RX(1), true));
    CHECK(bladerf_sync_rx(dev, samples, NUM_CHANNELS * NUM_SAMPLES, NULL, 5000));

    fp = fopen("dual_test.bin", "wb");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open file for writing\n");
        status = EXIT_FAILURE;
        goto out;
    }

    usleep(1e6 * NUM_SAMPLES / samp_rate);

    printf("Sample Preview:\n");
    for (size_t i = 0; i < 10; i++) {
        printf("[ch1]%" PRIi16 ", %" PRIi16 "\n", samples[4*i], samples[4*i+1]);
        printf("[ch2]%" PRIi16 ", %" PRIi16 "\n", samples[4*i+2], samples[4*i+3]);
    }

    if (fwrite(samples, sizeof(int16_t), 2 * NUM_CHANNELS * NUM_SAMPLES, fp) != 2 * NUM_CHANNELS * NUM_SAMPLES) {
        perror("Failed to write to file");
        status = EXIT_FAILURE;
    }

out:
    if (fp != NULL)
        fclose(fp);

    CHECK(bladerf_enable_module(dev, BLADERF_CHANNEL_RX(0), false));
    CHECK(bladerf_enable_module(dev, BLADERF_CHANNEL_RX(1), false));
    bladerf_close(dev);

    free(samples);
    return status;
}
