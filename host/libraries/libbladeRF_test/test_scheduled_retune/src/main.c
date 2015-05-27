/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2015 Nuand LLC
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

/*
 * This program exercises the scheduled retune functionality added in
 * FPGA v0.2.0.
 *
 * It transmits a tone for 1ms every 10 ms.  During the
 * "off time" the device is scheduled to retune to a different frequency,
 * per the list of frequencies provided to this program.
 *
 * The "hop set" file should be a newline-delimited list of frequencies.
 * The M or G suffixes may be used, as well as scientific notation.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include <libbladeRF.h>

#include "test_common.h"
#include "hop_set.h"

#define TIMEOUT_MS  2500
#define ITERATIONS  40
#define SAMPLE_RATE 2000000

#define NUM_SAMPLES 2000
#define TS_INC      20000
#define RETUNE_INC  (NUM_SAMPLES + (TS_INC - NUM_SAMPLES) / 10)

int run_test(struct bladerf *dev, struct hop_set *hops, bool quick_tune)
{
    int status = -1;
    int16_t *samples = NULL;
    struct bladerf_metadata meta;
    unsigned int i;
    uint64_t retune_ts;

    memset(&meta, 0, sizeof(meta));

    samples = malloc(2 * NUM_SAMPLES * sizeof(samples[0]));
    if (samples == NULL) {
        perror("malloc");
        return -1;
    }

    /* Set IQ values to ~ sqrt(2)/2 */
    for (i = 0; i < (2 * NUM_SAMPLES); i += 2) {
        samples[i] = samples[i+1] = 1448;
    }

    status = bladerf_get_timestamp(dev, BLADERF_MODULE_TX, &meta.timestamp);
    if (status != 0) {
        fprintf(stderr, "Failed to get initial TX timestamp: %s\n",
                bladerf_strerror(status));
        goto out;
    }

    /* Add some initial startup delay */
    meta.timestamp += (SAMPLE_RATE / 50);
    meta.flags = BLADERF_META_FLAG_TX_BURST_START |
                 BLADERF_META_FLAG_TX_BURST_END;

    retune_ts = meta.timestamp + RETUNE_INC;

    /* Set initial frequency */
    status = bladerf_set_frequency(dev, BLADERF_MODULE_TX,
                                   hop_set_next(hops, NULL));
    if (status != 0) {
        fprintf(stderr, "Failed to set initial frequency: %s\n",
                bladerf_strerror(status));
        goto out;
    }

    for (i = 0; i < ITERATIONS; i++) {
        status = bladerf_sync_tx(dev, samples, NUM_SAMPLES, &meta, TIMEOUT_MS);
        if (status != 0) {
            fprintf(stderr, "Failed to TX @ iteration %u: %s\n",
                    i + 1, bladerf_strerror(status));
            goto out;
        } else {
            /* Update timestamp for next transmission */
            meta.timestamp += TS_INC;
        }

        if (quick_tune) {
            struct hop_params p;
            hop_set_next(hops, &p);

            status = bladerf_schedule_retune(dev, BLADERF_MODULE_TX,
                                             meta.timestamp + RETUNE_INC,
                                             0, &p.qt);
        } else {
            unsigned int freq = hop_set_next(hops, NULL);

            status = bladerf_schedule_retune(dev, BLADERF_MODULE_TX,
                                             meta.timestamp + RETUNE_INC,
                                             freq, NULL);
        }

        if (status == 0) {
            /* Update the retune timestamp for the next off-time */
            retune_ts += TS_INC;
        } else if (status == BLADERF_ERR_QUEUE_FULL) {
            printf("Retune queue full @ iteration %u. Trying again next "
                   "iteration.\n", i);
        } else {
            fprintf(stderr, "Failed to schedule retune @ %"PRIu64": %s\n",
                    meta.timestamp + RETUNE_INC, bladerf_strerror(status));
            goto out;
        }
    }

out:
    free(samples);
    return status;
}

int main(int argc, char *argv[])
{
    int status, disable_status;
    struct bladerf *dev = NULL;
    const char *dev_str = NULL;
    const char *hop_file = NULL;
    const char *mode = NULL;
    struct hop_set *hops;
    struct device_config config;
    bool quick_tune;
    size_t i;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s [device str] <hop file> <quick/normal>\n",
                argv[0]);

        return 1;
    } else if (argc >= 4) {
        dev_str = argv[1];
        hop_file = argv[2];
        mode = argv[3];
    } else {
        hop_file = argv[1];
        mode = argv[2];
    }

    if (!strcasecmp(mode, "quick")) {
        quick_tune = true;
    } else if (!strcasecmp(mode, "normal")) {
        quick_tune = false;
    } else {
        fprintf(stderr, "Invalid tuning mode: %s\n", mode);
        return 1;
    }

    hops = hop_set_load(hop_file);
    if (hops == NULL) {
        return 1;
    }

    status = bladerf_open(&dev, dev_str);
    if (status != 0) {
        fprintf(stderr, "Unable to open device: %s\n",
                bladerf_strerror(status));

        goto out;
    }

    test_init_device_config(&config);

    config.tx_samplerate = SAMPLE_RATE;
    config.tx_bandwidth  = 1500000;

    config.samples_per_buffer = 4096;
    config.num_buffers = 16;
    config.num_transfers = 8;

    status = test_apply_device_config(dev, &config);
    if (status != 0) {
        fprintf(stderr, "Failed to configure device.\n");
        goto out;
    }

    if (quick_tune) {
        status = hop_set_load_quick_tunes(dev, BLADERF_MODULE_TX, hops);
        if (status != 0) {
            goto out;
        }
    }

    printf("Hop set:\n");
    for (i = 0; i < hops->count; i++) {
        if (quick_tune) {
            struct hop_params p;
            hop_set_next(hops, &p);

            printf("  f=%-10u nint=%-6u nfrac=%-8u flags=0x%02x vcocap=%u\n",
                    p.f, p.qt.nint, p.qt.nfrac, p.qt.flags, p.qt.vcocap);
        } else {
            printf("  f=%u\n", hop_set_next(hops, NULL));
        }
    }

    hops->idx = 0;
    printf("\n");

    status = test_perform_sync_config(dev, BLADERF_MODULE_TX,
                                      BLADERF_FORMAT_SC16_Q11_META,
                                      &config, true);
    if (status != 0) {
        goto out;
    }

    printf("Running...\n");
    fflush(stdout);

    status = run_test(dev, hops, quick_tune);

    printf("Done.\n");
    fflush(stdout);

    disable_status = bladerf_enable_module(dev, BLADERF_MODULE_TX, false);
    if (disable_status != 0) {
        fprintf(stderr, "Failed to disable_status TX module: %s\n",
                bladerf_strerror(status));
        goto out;
    }

    status = (status == 0) ? disable_status : status;

out:
    bladerf_close(dev);
    hop_set_free(hops);
    return status;
}
