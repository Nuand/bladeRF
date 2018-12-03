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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <signal.h>
#include <libbladeRF.h>
#include "conversions.h"
#include "host_config.h"

#define VERBOSITY BLADERF_LOG_LEVEL_INFO

#define BUF_LEN    4096
#define TIMEOUT_MS 2500

#define ITERATIONS 10000

/* Dwell time at each frequency (us) */
#define DWELL_TIME_US 0

typedef struct {
    bladerf_frequency f;
    struct bladerf_quick_tune qt;
} freq;

/* Pick the frequency around which each hop frequency will be computed
 * Select this to ensure hops cross a high/low band boundary */
#define BLADERF1_CENTER_FREQ ((bladerf_frequency) 1.500e9)
#define BLADERF2_CENTER_FREQ ((bladerf_frequency) 3.000e9)

/* Number of hop frequencies to generate */
#define NUM_FREQS            16

/* Spacing between hop frequencies */
#define FREQ_INCREMENT       ((bladerf_frequency) 0.500e6)

bool is_running = true;

void sig_handler(int signo)
{
    if (signo == SIGINT) {
        fprintf(stderr, "received SIGINT\n");
        if (!is_running) {
            fprintf(stderr, "received another SIGINT, aborting\n");
            abort();
        }
        is_running = false;
    }
}

int run_test(struct bladerf *dev)
{
    const char *board_name;
    int status;
    freq freqs[NUM_FREQS];
    //int hopseq[] = {7, 2, 14, 1, 11, 15, 3, 5, 12, 13, 0, 4, 6, 8, 9, 10};
    int hopseq[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    int16_t *samples = NULL;
    unsigned int i, f;
    bladerf_frequency center_freq;

    samples = malloc(2 * BUF_LEN * sizeof(samples[0]));
    if (samples == NULL) {
        perror("malloc");
        return BLADERF_ERR_MEM;
    }

    /* Just send a carrier tone */
    for (i = 0; i < (2 * BUF_LEN); i += 2) {
        samples[i] = samples[i+1] = 1448;;
    }

    status = bladerf_sync_config(dev, BLADERF_TX_X1,
                                 BLADERF_FORMAT_SC16_Q11,
                                 16, 4096, 8, TIMEOUT_MS);
    if (status != 0) {
        fprintf(stderr, "Failed to initialize sync i/f: %s\n",
                bladerf_strerror(status));
        goto out;
    }

    board_name = bladerf_get_board_name(dev);

    if (strcmp(board_name, "bladerf1") == 0) {
        center_freq = BLADERF1_CENTER_FREQ;
    } else if (strcmp(board_name, "bladerf2") == 0) {
        center_freq = BLADERF2_CENTER_FREQ;
    } else {
        fprintf(stderr, "Unknown board name: %s\n", board_name);
        goto out;
    }

    /* Calculate the hop frequencies */
    printf("Hop Set:\n");
    for( f = 0; f < NUM_FREQS; f++ ) {
        if( f < (NUM_FREQS/2) ) {
            freqs[f].f = center_freq - ((NUM_FREQS/2)*FREQ_INCREMENT) +
                (f*FREQ_INCREMENT);
        } else {
            freqs[f].f = center_freq + ((f-(NUM_FREQS/2)+1)*FREQ_INCREMENT);
        }
        printf("freq[%2u]=%" PRIu64 "\n", f, freqs[f].f);
    }

    /* Get the quick tune data */
    for( f = 0; f < NUM_FREQS; f++ ) {
        if (!is_running) {
            fprintf(stderr, "Stopping test...\n");
            status = 0;
            goto out;
        }

        status = bladerf_set_frequency(dev, BLADERF_CHANNEL_TX(0), freqs[f].f);
        if (status != 0) {
            fprintf(stderr, "Failed to set frequency to %" PRIu64 ": %s\n",
                    freqs[f].f, bladerf_strerror(status));
            goto out;
        }

        status = bladerf_get_quick_tune(dev, BLADERF_CHANNEL_TX(0),
                                        &freqs[f].qt);
        if (status != 0) {
            fprintf(stderr, "Failed to get quick tune %u: %s\n",
                    f, bladerf_strerror(status));
            goto out;
        }
    }

    /* Enable and run! */

    status = bladerf_enable_module(dev, BLADERF_CHANNEL_TX(0), true);
    if (status != 0) {
        fprintf(stderr, "Failed to enable module: %s\n",
                bladerf_strerror(status));
        goto out;
    }

    for (i = 0; i < ITERATIONS; i++) {
        for( f = 0; f < NUM_FREQS; f++ ) {

            if (!is_running) {
                fprintf(stderr, "Stopping test...\n");
                status = 0;
                goto out;
            }

            if( DWELL_TIME_US > 999999 ) {
                printf("hopseq[%u] = %u\n", f, hopseq[f]);

                if (strcmp(board_name, "bladerf2") == 0) {
                    printf("nios_profile = %u, rffe_profile = %u\n",
                           freqs[hopseq[f]].qt.nios_profile,
                           freqs[hopseq[f]].qt.rffe_profile);
                }
            }

            status = bladerf_schedule_retune(dev, BLADERF_CHANNEL_TX(0),
                                             BLADERF_RETUNE_NOW, 0,
                                             &freqs[hopseq[f]].qt);

            if (status != 0) {
                fprintf(stderr, "Failed to perform quick tune to index %u: "
                        "%s\n", hopseq[f], bladerf_strerror(status));
                goto out;
            }

            status = bladerf_sync_tx(dev, samples, BUF_LEN, NULL, 3500);
            if (status != 0) {
                fprintf(stderr, "Failed to TX data: %s\n",
                        bladerf_strerror(status));
                goto out;
            }

            usleep(DWELL_TIME_US);
        }
    }

out:
    free(samples);
    bladerf_enable_module(dev, BLADERF_CHANNEL_TX(0), false);
    return status;
}

int main(int argc, char *argv[])
{
    int status;
    struct bladerf *dev = NULL;
    const char *devstr = NULL;

    if (argc > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
        fprintf(stderr, "TX a tone across %d frequencies.\n", NUM_FREQS);
        fprintf(stderr, "Usage: %s [device string]\n", argv[0]);
        return 1;
    } else {
        devstr = argv[1];
    }

    if (signal(SIGINT, sig_handler) == SIG_ERR) {
        fprintf(stderr, "Unable to catch SIGINT signals\n");
    }

    bladerf_log_set_verbosity(VERBOSITY);

    status = bladerf_open(&dev, devstr);
    if (status != 0) {
        fprintf(stderr, "Unable to open device: %s\n",
                bladerf_strerror(status));
        return status;
    }

    status = run_test(dev);

    bladerf_close(dev);
    return status;
}
