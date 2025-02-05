/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2023 Nuand LLC
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
#include <stdio.h>
#include <getopt.h>
#include "log.h"

struct test_case {
    unsigned int burst_len;     /* Length of a burst, in samples */
    unsigned int iterations;
    unsigned int period;
    unsigned int fill;
    unsigned int init_ts_delay;
    bladerf_frequency frequency;
    bladerf_gain gain;
    char* dev_tx_str;
    char* dev_rx_str;
    bool just_tx;
    bool compare;
};

static struct option const long_options[] = {
    { "txdev", required_argument, NULL, 't' },
    { "rxdev", required_argument, NULL, 'r' },
    { "samprate", required_argument, NULL, 's' },
    { "burst", required_argument, NULL, 'b' },
    { "compare", no_argument, NULL, 'c' },
    { "period", required_argument, NULL, 'p' },
    { "fill", required_argument, NULL, 'f' },
    { "loop", no_argument, NULL, 'l' },
    { "iterations", required_argument, NULL, 'i' },
    { "verbosity", required_argument, NULL, 'v' },
    { "help", no_argument, NULL, 'h' },
    { NULL, 0, NULL, 0 },
};

char* getopt_str(const struct option* long_options) {
    size_t len = 0;

    for (const struct option* opt = long_options; opt->name; opt++) {
        len++;
        if (opt->has_arg == required_argument) {
            len++;
        }
    }

    char* opt_str = (char*) malloc((len + 1) * sizeof(char));
    if (!opt_str) {
        return NULL;
    }

    int i = 0;
    for (const struct option* opt = long_options; opt->name; opt++) {
        opt_str[i++] = opt->val;
        if (opt->has_arg == required_argument) {
            opt_str[i++] = ':';
        }
    }
    opt_str[i] = '\0';

    return opt_str;
}

static void usage()
{
    printf("TXRX Hardware Loop Test\n\n");

    printf("Test configuration:\n");
    printf("    -t, --txdev <value>       TX bladeRF device string.\n");
    printf("    -r, --rxdev <value>       RX bladeRF device string.\n");
    printf("    -s, --samprate <value>    Sample Rate\n");
    printf("    -b, --burst <value>       Number of samples in a burst.\n");
    printf("    -p, --period <value>      Length between timestamps in samples\n");
    printf("    -f, --fill <value>        %% of burst to fill with [2000,2000]\n");
    printf("                                others set to [0,0]\n");
    printf("    -l, --loop                Enables RX device for TX capture\n");
    printf("    -c, --compare             Outputs CSV RX capture from TX device\n");
    printf("                                for multi-device fill comparison\n");
    printf("    -i, --iterations          Number of pulses\n");
    printf("\n");

    printf("Misc options:\n");
    printf("    -h, --help                  Show this help text\n");
    printf("    -v, --lib-verbosity <level> Set libbladeRF verbosity (Default: warning)\n");
    printf("\n");
    printf("\n");

    printf("Loop setup:\n");
    printf("    A bladeRF device will TX into the other bladeRF device’s\n"
           "    RX port over SMA and a 20dB attenuator. See the following\n"
           "    tested config.\n\n");
    printf("        bladeRF micro 2.0 TX -> 20dB att. -> SMA -> RX bladeRF x115\n");
    printf("\n\n");

    printf("Parameter Definitions:\n");
    printf("    -------------------------------------------------------------------------------------------------------------\n");
    printf("    |  50%% MAX MAX |    50%% 0 0 0 0  |           <--- gap --->           |  50%% MAX MAX |    50%% 0 0 0 0  |\n");
    printf("    -------------------------------------------------------------------------------------------------------------\n");
    printf("     <---- fill -->\n");
    printf("     <------------ burst ---------->\n");
    printf("     <---------------------------------- period ----------------------->\n");
    printf("\n");
    printf("\n");

    printf("Example:\n");
    printf("    Generate a pulse using 50%% of a 50k sample burst every 100ms.\n");
    printf("    Note: The sample rate is preset to 1MSPS.\n");
    printf("\n");
    printf("        ./libbladeRF_test_txrx_hwloop -f 50 -b 50000 -p 100000 --txdev <devstr>\n");
    printf("\n");
    printf("    Burst of 1ms, fill 0.9ms, and periodicity is 50ms into the RX\n");
    printf("    of another bladeRF device.\n");
    printf("\n");
    printf("        ./libbladeRF_test_txrx_hwloop --burst 1000 --fill 90 --period 50000 -l --txdev <devstr> --rxdev <devstr>\n");
    printf("\n\n");
}

int module_config_enable(struct bladerf *dev, bladerf_channel ch,
                     struct test_case *test, struct app_params *params, bladerf_format sample_format) {
    int status;

    status = bladerf_set_frequency(dev, ch, test->frequency);
    if (status != 0) {
        log_error("Failed to set ch %d frequency = %" PRIu64 ": %s\n", ch,
                  test->frequency, bladerf_strerror(status));
        return status;
    }

    status = bladerf_set_sample_rate(dev, ch, params->samplerate, NULL);
    if (status != 0) {
        log_error("Failed to set ch %d sample rate = %u: %s\n", ch, sample_rate,
                  bladerf_strerror(status));
        return status;
    }

    status = bladerf_sync_config(dev, ch, sample_format,
                                 params->num_buffers, params->buf_size,
                                 params->num_xfers, params->timeout_ms);
    if (status != 0) {
        fprintf(stderr, "Failed to configure %s sync i/f: %s\n",
                channel2str(ch), bladerf_strerror(status));
        return status;
    }

    status = bladerf_enable_module(dev, ch, true);
    if (status != 0) {
        fprintf(stderr, "Failed to enable %s module: %s\n",
                channel2str(ch), bladerf_strerror(status));
        return status;
    }

    return 0;
}

int init_devices(struct bladerf** dev_tx, struct bladerf** dev_rx, struct app_params *p, struct test_case *tc) {
    int status;

    /** TX init */
    status = bladerf_open(dev_tx, tc->dev_tx_str);
    if (status != 0) {
        fprintf(stderr, "Failed to open TX device: %s\n", bladerf_strerror(status));
        fprintf(stderr, "↳ Most likely an incorrect device string.\n");
        return status;
    }

    status = module_config_enable(*dev_tx, BLADERF_MODULE_TX, tc, p, BLADERF_FORMAT_SC16_Q11_META);
    if (status != 0) {
        fprintf(stderr, "Failed to configure module: %s\n",
                bladerf_strerror(status));
        return status;
    }

    /** RX init */
    if (tc->just_tx == false && tc->compare == true) {
        printf("Mode: TX -> RX (+RF loopback)\n");
        printf("    +---------------+       +---------------+\n");
        printf("    | TX Device   TX|---┬-->|RX   RX Device |\n");
        printf("    |               |   |   |               |\n");
        printf("    |             RX|<--┘   |               |\n");
        printf("    +---------------+       +---------------+\n");
    } else if (tc->just_tx == true && tc->compare == true) {
        printf("Mode: TX -> TX\n");
        printf("    +---------------+       +---------------+\n");
        printf("    | TX Device   TX|---┬   |RX   RX Device |\n");
        printf("    |               |   |   |               |\n");
        printf("    |             RX|<--┘   |               |\n");
        printf("    +---------------+       +---------------+\n");
    } else if (tc->just_tx == false && tc->compare == false) {
        printf("Mode: TX -> RX\n");
        printf("    +---------------+       +---------------+\n");
        printf("    | TX Device   TX|------>|RX   RX Device |\n");
        printf("    |               |       |               |\n");
        printf("    |             RX|       |               |\n");
        printf("    +---------------+       +---------------+\n");
    } else {
        printf("Mode: TX Only\n");
        printf("    +---------------+   v   +---------------+\n");
        printf("    | TX Device   TX|---┘   |RX   RX Device |\n");
        printf("    |               |       |               |\n");
        printf("    |             RX|       |               |\n");
        printf("    +---------------+       +---------------+\n");
        return 0;
    }

    if (tc->just_tx == false) {
        status = bladerf_open(dev_rx, tc->dev_rx_str);
        if (status != 0) {
            fprintf(stderr, "Failed to open RX device: %s\n", bladerf_strerror(status));
            fprintf(stderr, "↳ Most likely an incorrect device string.\n");
            return status;
        }

        status = module_config_enable(*dev_rx, BLADERF_MODULE_RX, tc, p, BLADERF_FORMAT_SC16_Q11);
        if (status != 0) {
            fprintf(stderr, "Failed to configure RX on RX device: %s\n",
                    bladerf_strerror(status));
            return status;
        }
    }

    /** Must come after  RX device open to recieve an input*/
    if (tc->compare == true) {
        status = bladerf_set_gain_mode(*dev_tx, BLADERF_MODULE_RX, BLADERF_GAIN_MGC);
        if (status != 0) {
            fprintf(stderr, "Failed to set loopback compare gain mode: %s\n", bladerf_strerror(status));
            return -1;
        }

        status = bladerf_set_gain(*dev_tx, BLADERF_MODULE_RX, tc->gain);
        if (status != 0) {
            fprintf(stderr, "Failed to set gain loopback compare RX gain: %s\n", bladerf_strerror(status));
            return -1;
        }

        status = module_config_enable(*dev_tx, BLADERF_MODULE_RX, tc, p, BLADERF_FORMAT_SC16_Q11_META);
        if (status != 0) {
            fprintf(stderr, "Failed to configure RX on TX device: %s\n",
                    bladerf_strerror(status));
            return status;
        }
    }

    return 0;
}

typedef struct {
    struct bladerf *dev;
    struct test_case *tc;
    bool is_rx_device;
} thread_args;

void *rx_task(void *args) {
    char* dev_rx_filename = "samples.csv";
    char* dev_tx_filename = "compare.csv";
    char* file_name = malloc(strlen(dev_rx_filename) + 1);

    struct bladerf_metadata meta;
    int status;
    FILE *file;

    memset(&meta, 0, sizeof(meta));
    meta.flags = BLADERF_META_FLAG_RX_NOW;

    /** Sample Setup */
    thread_args *rx_args = (thread_args *)args;
    unsigned int period = rx_args->tc->iterations*rx_args->tc->period;
    unsigned int initial_delay = rx_args->tc->init_ts_delay;
    unsigned int num_rx_samples =  period + initial_delay;
    int16_t *samples = calloc(num_rx_samples, 2*sizeof(int16_t));

    status = bladerf_sync_rx(rx_args->dev, samples, num_rx_samples, &meta, 1000);
    if (status == BLADERF_ERR_TIMEOUT) {
        fprintf(stderr, "RX Timeout: %s\n", bladerf_strerror(status));
        goto cleanup;
    } else if (status!= 0) {
        fprintf(stderr, "Failed to sync RX: %s\n", bladerf_strerror(status));
        status = BLADERF_ERR_UNEXPECTED;
        goto cleanup;
    }

    status = bladerf_enable_module(rx_args->dev, BLADERF_MODULE_RX, false);
    if (status != 0) {
        fprintf(stderr, "Failed to disable RX module on %s board: %s\n",
                (rx_args->is_rx_device) ? "RX":"TX", bladerf_strerror(status));
    }

    if (rx_args->is_rx_device == true)
        strcpy(file_name, dev_rx_filename);
    else
        strcpy(file_name, dev_tx_filename);

    file = fopen(file_name, "w");
    if (file == NULL)
        fprintf(stderr, "Failed to open output file: %s\n", file_name);

    fprintf(file, "I,Q\n");
    for (unsigned int i = 0; i < 2*num_rx_samples; i+=2) {
        if (file != NULL)
            fprintf(file, "%i,%i\n", samples[i], samples[i+1]);
    }

    if (file != NULL)
        printf("\nRX samples written to \"%s\"\n", file_name);

cleanup:
    free(samples);
    free(file_name);
    pthread_exit(NULL);
    fclose(file);
}
