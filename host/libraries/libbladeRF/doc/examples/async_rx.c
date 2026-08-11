/** [Full listing] */
/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2026 Nuand LLC
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/* Receive IQ samples with the asynchronous interface. */

#include <libbladeRF.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_BUFFERS 16
#define NUM_TRANSFERS 8
#define SAMPLES_PER_BUFFER 8192
#define NUM_SAMPLES (1000 * SAMPLES_PER_BUFFER)
#define OUTPUT_FILENAME_SC8 "async_rx.cs8"
#define OUTPUT_FILENAME_SC16 "async_rx.cs16"

#define CHECK_STATUS(fn) \
    do { \
        status = (fn); \
        if (status != 0) { \
            fprintf(stderr, "%s:%d: %s failed: %s\n", __FILE__, __LINE__, \
                    #fn, bladerf_strerror(status)); \
            goto out; \
        } \
    } while (0)

struct rx_state {
    void *samples;
    size_t bytes_per_sample;
    size_t samples_received;
};

/** [receive callback] */
static void *rx_callback(struct bladerf *dev,
                         struct bladerf_stream *stream,
                         struct bladerf_metadata *meta,
                         void *samples,
                         size_t num_samples,
                         void *user_data)
{
    struct rx_state *state = user_data;

    (void)dev;
    (void)stream;
    (void)meta;

    if (samples == NULL) {
        return BLADERF_STREAM_SHUTDOWN;
    }
    if (num_samples > NUM_SAMPLES - state->samples_received) {
        return BLADERF_STREAM_SHUTDOWN;
    }

    memcpy((uint8_t *)state->samples +
               state->bytes_per_sample * state->samples_received,
           samples, state->bytes_per_sample * num_samples);
    state->samples_received += num_samples;

    if (state->samples_received == NUM_SAMPLES) {
        return BLADERF_STREAM_SHUTDOWN;
    }

    return samples;
}
/** [receive callback] */

int main(int argc, char *argv[])
{
    struct bladerf_stream *stream = NULL;
    struct bladerf *dev           = NULL;
    struct rx_state state;
    FILE *output;
    const char *output_filename = OUTPUT_FILENAME_SC16;
    bladerf_channel channel = BLADERF_CHANNEL_RX(0);
    bladerf_format format   = BLADERF_FORMAT_SC16_Q11_PACKED;
    int module_enabled      = 0;
    int result              = EXIT_FAILURE;
    int status;

    state.samples          = NULL;
    state.bytes_per_sample = 2 * sizeof(int16_t);
    state.samples_received = 0;
    output                 = NULL;

    if (argc == 2) {
        if (strcmp(argv[1], "sc8") == 0) {
            format = BLADERF_FORMAT_SC8_Q7;
            state.bytes_per_sample = 2 * sizeof(int8_t);
            output_filename = OUTPUT_FILENAME_SC8;
        } else if (strcmp(argv[1], "sc16") == 0) {
            format = BLADERF_FORMAT_SC16_Q11;
        } else if (strcmp(argv[1], "packed") != 0) {
            fprintf(stderr, "Usage: %s [sc8|sc16|packed]\n", argv[0]);
            goto out;
        }
    } else if (argc != 1) {
        fprintf(stderr, "Usage: %s [sc8|sc16|packed]\n", argv[0]);
        goto out;
    }

    CHECK_STATUS(bladerf_open(&dev, NULL));
    CHECK_STATUS(bladerf_set_frequency(dev, channel, 915000000));
    CHECK_STATUS(bladerf_set_sample_rate(dev, channel, 20000000, NULL));
    CHECK_STATUS(bladerf_set_bandwidth(dev, channel, 56000000, NULL));
    CHECK_STATUS(bladerf_set_gain_mode(dev, channel, BLADERF_GAIN_DEFAULT));
    state.samples = malloc(NUM_SAMPLES * state.bytes_per_sample);
    if (state.samples == NULL) {
        perror("malloc");
        goto out;
    }
    /** [start stream] */
    CHECK_STATUS(bladerf_init_stream(
        &stream, dev, rx_callback, NULL, NUM_BUFFERS, format,
        SAMPLES_PER_BUFFER, NUM_TRANSFERS, &state));
    CHECK_STATUS(bladerf_enable_module(dev, channel, true));
    module_enabled = 1;

    printf("Receiving with %s through the asynchronous interface...\n",
           bladerf_format_to_string(format));
    CHECK_STATUS(bladerf_stream(stream, BLADERF_RX_X1));
    /** [start stream] */
    if (state.samples_received != NUM_SAMPLES) {
        fprintf(stderr, "Expected %zu samples, received %zu\n",
                (size_t)NUM_SAMPLES, state.samples_received);
        goto out;
    }
    CHECK_STATUS(bladerf_enable_module(dev, channel, false));
    module_enabled = 0;

    /** [write capture] */
    output = fopen(output_filename, "wb");
    if (output == NULL) {
        perror(output_filename);
        goto out;
    }
    if (fwrite(state.samples, state.bytes_per_sample, NUM_SAMPLES, output) !=
        NUM_SAMPLES) {
        fprintf(stderr, "Failed to write %s\n", output_filename);
        goto out;
    }

    printf("Saved %zu samples to %s\n", (size_t)NUM_SAMPLES, output_filename);
    /** [write capture] */
    result = EXIT_SUCCESS;

out:
    if (module_enabled != 0) {
        (void)bladerf_enable_module(dev, channel, false);
    }
    if (stream != NULL) {
        bladerf_deinit_stream(stream);
    }
    if (dev != NULL) {
        bladerf_close(dev);
    }
    if (output != NULL && fclose(output) != 0) {
        perror(output_filename);
        result = EXIT_FAILURE;
    }
    free(state.samples);

    return result;
}
/** [Full listing] */
