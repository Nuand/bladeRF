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
 * When configured for TX, it transmits a tone for 1ms every 10 ms.  During the
 * "off time" the device is scheduled to retune to a different frequency,
 * per the list of frequencies provided to this program.
 *
 * When configured for RX, this program reads samples to a samples.bin file.
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
#include "devcfg.h"
#include "hop_set.h"

#define TIMEOUT_MS  2500
#define ITERATIONS  500
#define SAMPLE_RATE 2000000

#define NUM_SAMPLES 2000
#define TS_INC      20000
#define RETUNE_INC  (NUM_SAMPLES + (TS_INC - NUM_SAMPLES) / 10)

#define SCHEDULE_AHEAD 4

int schedule_retune(struct bladerf *dev, bladerf_module m, struct hop_set *hops,
                    bool quick_tune, uint64_t *hop_ts)
{
    int status;

    if (quick_tune) {
        struct hop_params p;
        hop_set_next(hops, &p);
        status = bladerf_schedule_retune(dev, m, *hop_ts, 0, &p.qt);
    } else {
        unsigned int freq = hop_set_next(hops, NULL);
        status = bladerf_schedule_retune(dev, m, *hop_ts, freq, NULL);
    }

    if (status == 0) {
        *hop_ts += TS_INC;
    }

    return status;
}

int run_test(struct bladerf *dev, bladerf_module module,
             struct hop_set *hops, bool quick_tune)

{
    int status = -1;
    int16_t *samples = NULL;
    struct bladerf_metadata meta;
    unsigned int i;
    uint64_t retune_ts;
    FILE *out = NULL;

    memset(&meta, 0, sizeof(meta));

    samples = calloc(2 * NUM_SAMPLES, sizeof(samples[0]));
    if (samples == NULL) {
        perror("malloc");
        return -1;
    }

    if (module == BLADERF_MODULE_TX) {
        /* Set IQ values to ~ sqrt(2)/2 */
        for (i = 0; i < (2 * NUM_SAMPLES); i += 2) {
            samples[i] = samples[i+1] = 1448;
        }
    } else {
        out = fopen("samples.bin", "wb");
        if (out == NULL) {
            perror("fopen");
            status = -1;
            goto out;
        }
    }

    status = bladerf_get_timestamp(dev, module, &meta.timestamp);
    if (status != 0) {
        fprintf(stderr, "Failed to get initial timestamp: %s\n",
                bladerf_strerror(status));
        goto out;
    }

    /* Add some initial startup delay */
    meta.timestamp += (SAMPLE_RATE / 50);

    if (module == BLADERF_MODULE_TX) {
        meta.flags = BLADERF_META_FLAG_TX_BURST_START |
                     BLADERF_META_FLAG_TX_BURST_END;
    } else {
        meta.flags = 0;
    }

    retune_ts = meta.timestamp + RETUNE_INC;

    /* Set initial frequency */
    status = bladerf_set_frequency(dev, module, hop_set_next(hops, NULL));
    if (status != 0) {
        fprintf(stderr, "Failed to set initial frequency: %s\n",
                bladerf_strerror(status));
        goto out;
    }

    /* Schedule first N hops */
    for (i = 0; i < SCHEDULE_AHEAD; i++) {
        status = schedule_retune(dev, module, hops, quick_tune, &retune_ts);
        if (status != 0) {
            fprintf(stderr, "Failed to schedule initial retune #%u: %s\n",
                    i, bladerf_strerror(status));
            goto out;
        }
    }

    for (i = 0; i < ITERATIONS; i++) {
        if (module == BLADERF_MODULE_TX) {
            status = bladerf_sync_tx(dev, samples, NUM_SAMPLES, &meta, TIMEOUT_MS);
            if (status != 0) {
                fprintf(stderr, "Failed to TX @ iteration %u: %s\n",
                        i + 1, bladerf_strerror(status));
                goto out;
            }
        } else {
            size_t n_written;

            status = bladerf_sync_rx(dev, samples, NUM_SAMPLES, &meta, TIMEOUT_MS);
            if (status != 0) {
                fprintf(stderr, "Failed to RX @ iteration %u: %s\n",
                        i + 1, bladerf_strerror(status));
                goto out;
            }

            n_written = fwrite(samples, 2 * sizeof(int16_t), NUM_SAMPLES, out);
            if (n_written != NUM_SAMPLES) {
                fprintf(stderr, "File write of samples failed.\n");
                status = -1;
                goto out;
            }
        }

        /* Update timestamp for next transmission */
        meta.timestamp += TS_INC;

        /* Schedule an additional retune */
        status = schedule_retune(dev, module, hops, quick_tune, &retune_ts);

        if (status == BLADERF_ERR_QUEUE_FULL) {
            printf("Retune queue full @ iteration %u. Trying again next "
                   "iteration.\n", i);
        } else if (status != 0) {
            fprintf(stderr, "Failed to schedule retune @ %"PRIu64": %s\n",
                    meta.timestamp + RETUNE_INC, bladerf_strerror(status));
            goto out;
        }
    }

    /* If transmitting, wait until we read back a timestamp that is in the
     * future past our scheduled transmissions*/
    if (module == BLADERF_MODULE_TX) {
        fprintf(stderr, "Waiting for transmission to complete...\n");
        status = wait_for_timestamp(dev, BLADERF_MODULE_TX,
                                    meta.timestamp, 2500);

        if (status == BLADERF_ERR_TIMEOUT) {
            fprintf(stderr, "Wait timed out.\n");
        } else if (status != 0) {
            fprintf(stderr, "Wait failed: %s\n", bladerf_strerror(status));
        } else {
            fprintf(stderr, "Done waiting.\n");
        }

    }

out:
    if (out != NULL) {
        fclose(out);
    }

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
    const char *module_str;
    struct hop_set *hops;
    struct devcfg config;
    bladerf_module module;
    bool quick_tune;
    size_t i;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s [device str] <rx/tx> <hop file> <quick/normal>\n",
                argv[0]);

        return 1;
    } else if (argc >= 5) {
        dev_str = argv[1];
        module_str = argv[2];
        hop_file = argv[3];
        mode = argv[4];
    } else {
        module_str = argv[1];
        hop_file = argv[2];
        mode = argv[3];
    }

    if (!strcasecmp(module_str, "rx")) {
        module = BLADERF_MODULE_RX;
    } else if (!strcasecmp(module_str, "tx")) {
        module = BLADERF_MODULE_TX;
    } else {
        fprintf(stderr, "Invalid module: %s\n", module_str);
        return 1;
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

    devcfg_init(&config);

    config.tx_samplerate = SAMPLE_RATE;
    config.tx_bandwidth  = 1500000;

    config.samples_per_buffer = 4096;
    config.num_buffers = 16;
    config.num_transfers = 8;

    status = devcfg_apply(dev, &config);
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

    status = devcfg_perform_sync_config(dev, module,
                                        BLADERF_FORMAT_SC16_Q11_META,
                                        &config, true);
    if (status != 0) {
        goto out;
    }

    printf("Running test with %s module...\n",
            module == BLADERF_MODULE_RX ? "RX" : "TX");
    fflush(stdout);

    status = run_test(dev, module, hops, quick_tune);

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
