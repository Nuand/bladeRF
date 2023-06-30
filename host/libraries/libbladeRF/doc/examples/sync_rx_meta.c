/*
 * Example of RX synchronous interface usage with metadata
 *
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014 Nuand LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <inttypes.h>
#include <libbladeRF.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "example_common.h"
#include "conversions.h"
#include "log.h"

/* Initialize sync interface for metadata and allocate our "working"
 * buffer that we'd use to process our RX'd samples.
 *
 * Return sample buffer on success, or NULL on failure.
 */

int16_t *init(struct bladerf *dev, int16_t num_samples, bladerf_format format,
              bladerf_channel_layout channel_layout, unsigned int buffer_size)
{
    int status = -1;

    /* "User" buffer that we read samples into and do work on, and its
     * associated size, in units of samples. Recall that for the
     * SC16Q11 format (native to the ADCs), one sample = two int16_t values.
     *
     * When using the bladerf_sync_* functions, the buffer size isn't
     * restricted to multiples of any particular size.
     *
     * The value for `num_samples` has no major restrictions here, while the
     * `buffer_size` below must be a multiple of 1024.
     */
    int16_t *samples;

    /* These items configure the underlying asynch stream used by the the sync
     * interface. The "buffer" here refers to those used internally by worker
     * threads, not the `samples` buffer above. */
    const unsigned int num_buffers   = 16;
    const unsigned int num_transfers = 8;
    const unsigned int timeout_ms    = 1000;

    samples = malloc(num_samples * 2 * sizeof(int16_t));
    if (samples == NULL) {
        perror("malloc");
        goto error;
    }

    /** [sync_config] */

    /* Configure the device's RX for use with the sync interface.
     * SC16 Q11 samples *with* metadata are used. */
    status = bladerf_sync_config(dev, channel_layout,
                                 format, num_buffers,
                                 buffer_size, num_transfers, timeout_ms);
    if (status != 0) {
        fprintf(stderr, "Failed to configure RX sync interface: %s\n",
                bladerf_strerror(status));

        goto error;
    }

    /** [sync_config] */

    /* We must always enable the RX front end *after* calling
     * bladerf_sync_config(), and *before* attempting to RX samples via
     * bladerf_sync_rx(). */
    status = bladerf_enable_module(dev, BLADERF_CHANNEL_RX(0), true);
    if (status != 0) {
        fprintf(stderr, "Failed to enable RX(0): %s\n", bladerf_strerror(status));
        goto error;
    }

    if (channel_layout == BLADERF_RX_X2) {
        status = bladerf_enable_module(dev, BLADERF_CHANNEL_RX(1), true);
        if (status != 0) {
            fprintf(stderr, "Failed to enable RX(1): %s\n", bladerf_strerror(status));
            goto error;
        }
    }

    status = 0;

error:
    if (status != 0) {
        free(samples);
        samples = NULL;
    }

    return samples;
}

void deinit(struct bladerf *dev, int16_t *samples)
{
    printf("\nDeinitalizing device.\n");

    /* Disable RX, shutting down our underlying RX stream */
    int status = bladerf_enable_module(dev, BLADERF_RX, false);
    if (status != 0) {
        fprintf(stderr, "Failed to disable RX: %s\n", bladerf_strerror(status));
    }

    /* Deinitialize and free resources */
    free(samples);
    bladerf_close(dev);
}

/** [rx_meta_now_example] */
int sync_rx_meta_now_example(struct bladerf *dev,
                             int16_t *samples,
                             unsigned int samples_len,
                             unsigned int rx_count,
                             unsigned int timeout_ms)
{
    int status = 0;
    struct bladerf_metadata meta;
    unsigned int i;


    /* Perform a read immediately, and have the bladerf_sync_rx function
     * provide the timestamp of the read samples */
    memset(&meta, 0, sizeof(meta));
    meta.flags = BLADERF_META_FLAG_RX_NOW;

    /* Receive samples and do work on them */
    for (i = 0; i < rx_count && status == 0; i++) {
        status = bladerf_sync_rx(dev, samples, samples_len, &meta, timeout_ms);
        if (status != 0) {
            fprintf(stderr, "RX \"now\" failed: %s\n\n",
                    bladerf_strerror(status));
        } else if (meta.status & BLADERF_META_STATUS_OVERRUN) {
            fprintf(stderr, "Overrun detected. %u valid samples were read.\n",
                    meta.actual_count);
        } else {
            printf("RX'd %u samples at t=0x%016" PRIx64 "\n", meta.actual_count,
                   meta.timestamp);

            fflush(stdout);

            /* ... Do work on samples here...
             *
             * status = process_samples(samples, samples_len);
             */
        }
    }

    return status;
}
/** [rx_meta_now_example] */

/** [rx_meta_sched_example] */
int sync_rx_meta_sched_example(struct bladerf *dev,
                               int16_t *samples,
                               unsigned int samples_len,
                               unsigned int rx_count,
                               unsigned int samplerate,
                               unsigned int timeout_ms)
{
    int status;
    struct bladerf_metadata meta;
    unsigned int i;

    /* 150 ms timestamp increment */
    const uint64_t ts_inc_150ms = ((uint64_t)samplerate) * 150 / 1000;

    /* 1 ms timestamp increment */
    const uint64_t ts_inc_1ms = samplerate / 1000;

    memset(&meta, 0, sizeof(meta));

    /* Perform scheduled RXs by having meta.timestamp set appropriately
     * and ensuring the BLADERF_META_FLAG_RX_NOW flag is cleared. */
    meta.flags = 0;

    /* Retrieve the current timestamp */
    status = bladerf_get_timestamp(dev, BLADERF_RX, &meta.timestamp);
    if (status != 0) {
        fprintf(stderr, "Failed to get current RX timestamp: %s\n",
                bladerf_strerror(status));
    } else {
        printf("Current RX timestamp: 0x%016" PRIx64 "\n", meta.timestamp);
    }

    /* Schedule first RX to be 150 ms in the future */
    meta.timestamp += ts_inc_150ms;

    /* Receive samples and do work on them */
    for (i = 0; i < rx_count && status == 0; i++) {
        status =
            bladerf_sync_rx(dev, samples, samples_len, &meta, 2 * timeout_ms);


        if (status != 0) {
            fprintf(stderr, "Scheduled RX failed: %s\n\n",
                    bladerf_strerror(status));
        } else if (meta.status & BLADERF_META_STATUS_OVERRUN) {
            fprintf(stderr, "Overrun detected in scheduled RX. "
                            "%u valid samples were read.\n\n",
                    meta.actual_count);
        } else {
            printf("RX'd %u samples at t=0x%016" PRIx64 "\n", meta.actual_count,
                   meta.timestamp);

            fflush(stdout);

            /* ... Process samples here ... */

            /* Schedule the next RX to be 1 ms after the end of the samples we
             * just processed */
            meta.timestamp += samples_len + ts_inc_1ms;
        }
    }

    return status;
}

/** [rx_meta_sched_example] */

static struct option const long_options[] = {
    { "device", required_argument, NULL, 'd' },
    { "bitmode", required_argument, NULL, 'b' },
    { "buflen", required_argument, NULL, 'l' },
    { "mimo", no_argument, NULL, 'm' },
    { "numsamples", required_argument, NULL, 'n' },
    { "rxcount", required_argument, NULL, 'c' },
    { "verbosity", required_argument, 0, 'v' },
    { "help", no_argument, NULL, 'h' },
    { NULL, 0, NULL, 0 },
};

static void usage(const char *argv0)
{
    printf("Usage: %s [options]\n", argv0);
    printf("  -d, --device <str>        Specify device to open.\n");
    printf("  -b, --bitmode <mode>      Specify 16bit or 8bit mode\n");
    printf("                              <16bit|16> (default)\n");
    printf("                              <8bit|8>\n");
    printf("  -l, --buflen              Buffer Size\n");
    printf("  -m, --mimo                Enable MIMO\n");
    printf("  -n, --numsamples          Specify Number of samples\n");
    printf("  -c, --rxcount <int>       Specify RX sync iterations\n");
    printf("  -v, --verbosity <level>   Set test verbosity\n");
    printf("  -h, --help                Show this text.\n");
}

int main(int argc, char *argv[])
{
    int status          = -1;
    struct bladerf *dev = NULL;
    const char *devstr  = NULL;
    int16_t *samples    = NULL;
    bladerf_channel_layout channel_layout = BLADERF_RX_X1;
    bladerf_format fmt  = BLADERF_FORMAT_SC16_Q11_META;

    unsigned int num_samples = 4096;
    unsigned int rx_count    = 15;
    unsigned int buffer_size = 4096;
    const unsigned int timeout_ms  = 2500;

    bladerf_log_level lvl = BLADERF_LOG_LEVEL_SILENT;
    bladerf_log_set_verbosity(lvl);
    bool ok;

    int opt = 0;
    int opt_ind = 0;
    while (opt != -1) {
        opt = getopt_long(argc, argv, "d:b:l:mn:c:v:h", long_options, &opt_ind);

        switch (opt) {
            case 'd':
                devstr = optarg;
                break;

            case 'b':
                if (strcmp(optarg, "16bit") == 0 || strcmp(optarg, "16") == 0) {
                    fmt = BLADERF_FORMAT_SC16_Q11_META;
                } else if (strcmp(optarg, "8bit") == 0 || strcmp(optarg, "8") == 0) {
                    fmt = BLADERF_FORMAT_SC8_Q7_META;
                } else {
                    printf("Unknown bitmode: %s\n", optarg);
                    return -1;
                }
                break;

            case 'l':
                buffer_size = str2int(optarg, 1, INT_MAX, &ok);
                if (!ok) {
                    printf("Buffer size invalid: %s\n", optarg);
                    return -1;
                }
                break;

            case 'm':
                channel_layout = BLADERF_RX_X2;
                break;

            case 'n':
                num_samples = str2int(optarg, 1, INT_MAX, &ok);
                if (!ok) {
                    printf("Number of samples not valid: %s\n", optarg);
                    return -1;
                }
                break;

            case 'c':
                rx_count = str2int(optarg, 1, INT_MAX, &ok);
                if (!ok) {
                    printf("RX count not valid: %s\n", optarg);
                    return -1;
                }
                break;

            case 'v':
                lvl = str2loglevel(optarg, &ok);
                if (!ok) {
                    log_error("Invalid log level provided: %s\n", optarg);
                    return -1;
                } else {
                    bladerf_log_set_verbosity(lvl);
                }
                break;

            case 'h':
                usage(argv[0]);
                return 0;

            default:
                break;
        }
    }

    dev = example_init(devstr);
    printf("Format: ");
    if (fmt == BLADERF_FORMAT_SC16_Q11_META)
        printf("SC16_Q11_META\n");
    else
        printf("SC8_Q7_META\n");
    printf("RX Count: %i\n", rx_count);
    printf("Mimo: %s\n", (channel_layout == BLADERF_RX_X2) ? "Enabled" : "Disabled");
    printf("Buffer Size: %i\n", buffer_size);

    if (dev) {
        samples = init(dev, num_samples, fmt, channel_layout, buffer_size);
        if (samples != NULL) {
            printf("\nRunning RX meta \"now\" example...\n");
            status = sync_rx_meta_now_example(dev, samples, num_samples,
                                              rx_count, timeout_ms);

            if (status == 0) {
                printf("\nRunning RX meta \"scheduled\" example...\n");
                status = sync_rx_meta_sched_example(
                    dev, samples, num_samples, rx_count, EXAMPLE_SAMPLERATE,
                    timeout_ms);
            }
        }

        deinit(dev, samples);
    }

    return status;
}
