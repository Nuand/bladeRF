/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014-2015 Nuand LLC
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
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include "devcfg.h"
#include "conversions.h"

#define DEVCFG_DFLT_TX_FREQ    1000000000
#define DEVCFG_DFLT_TXVGA1     (-14)
#define DEVCFG_DFLT_TXVGA2     0

#define DEVCFG_DFLT_RX_FREQ    1000000000
#define DEVCFG_DFLT_LNAGAIN    BLADERF_LNA_GAIN_MAX
#define DEVCFG_DFLT_RXVGA1     30
#define DEVCFG_DFLT_RXVGA2     0

#define DEVCFG_DFLT_SAMPLERATE 2000000
#define DEVCFG_DFLT_BANDWIDTH  1500000

#define DEVCFG_DFLT_SAMPLES_PER_BUF    8192
#define DEVCFG_DFLT_NUM_BUFFERS        64
#define DEVCFG_DFLT_NUM_TRANSFERS      16
#define DEVCFG_DFLT_STREAM_TIMEMOUT_MS 5000
#define DEVCFG_DFLT_SYNC_TIMEOUT_MS    (2 * DEVCFG_DFLT_STREAM_TIMEMOUT_MS)

#define OPTION_HELP             'h'
#define OPTION_DEVICE           'd'
#define OPTION_LOOPBACK         'l'
#define OPTION_VERBOSITY        'v'
#define OPTION_SAMPLERATE       's'
#define OPTION_RX_SAMPLERATE    0x80
#define OPTION_TX_SAMPLERATE    0x81
#define OPTION_BANDWIDTH        'b'
#define OPTION_RX_BANDWIDTH     0x82
#define OPTION_TX_BANDWIDTH     0x83
#define OPTION_FREQUENCY        'f'
#define OPTION_RX_FREQUENCY     0x84
#define OPTION_TX_FREQUENCY     0x85
#define OPTION_LNAGAIN          0x86
#define OPTION_RXVGA1           0x87
#define OPTION_RXVGA2           0x88
#define OPTION_TXVGA1           0x89
#define OPTION_TXVGA2           0x8a
#define OPTION_SAMPLES_PER_BUF  0x91
#define OPTION_NUM_BUFFERS      0x92
#define OPTION_NUM_TRANSFERS    0x93
#define OPTION_STREAM_TIMEOUT   0x94
#define OPTION_SYNC_TIMEOUT     0x95
#define OPTION_ENABLE_XB200     0x96

static const numeric_suffix hz_suffixes[] = {
    { "K",   1000 },
    { "KHz", 1000 },
    { "M",   1000000 },
    { "MHz", 1000000 },
    { "G",   1000000000 },
    { "GHz", 1000000000 },
};

static const struct option devcfg_long_options[] =
{
    { "help",                 no_argument,        0,  OPTION_HELP },
    { "device",               required_argument,  0,  OPTION_DEVICE },
    { "verbosity",            required_argument,  0,  OPTION_VERBOSITY },

    { "samplerate",           required_argument,  0,  OPTION_SAMPLERATE },
    { "rx-samplerate",        required_argument,  0,  OPTION_RX_SAMPLERATE  },
    { "tx-samplerate",        required_argument,  0,  OPTION_TX_SAMPLERATE },

    { "bandwidth",            required_argument,  0,  OPTION_BANDWIDTH },
    { "rx-bandwidth",         required_argument,  0,  OPTION_RX_BANDWIDTH },
    { "tx-bandwidth",         required_argument,  0,  OPTION_TX_BANDWIDTH },

    { "frequency",            required_argument,  0,  OPTION_FREQUENCY },
    { "rx-frequency",         required_argument,  0,  OPTION_RX_FREQUENCY },
    { "tx-frequency",         required_argument,  0,  OPTION_TX_FREQUENCY },

    { "lna-gain",             required_argument,  0,  OPTION_LNAGAIN },
    { "rxvga1-gain",          required_argument,  0,  OPTION_RXVGA1 },
    { "rxvga2-gain",          required_argument,  0,  OPTION_RXVGA2 },

    { "txvga1-gain",          required_argument,  0,  OPTION_TXVGA1 },
    { "txvga2-gain",          required_argument,  0,  OPTION_TXVGA2 },

    { "loopback",             required_argument,  0,  OPTION_LOOPBACK },

    { "xb200",                no_argument,        0,  OPTION_ENABLE_XB200 },

    { "samples-per-buffer",   required_argument,  0,  OPTION_SAMPLES_PER_BUF },
    { "num-buffers",          required_argument,  0,  OPTION_NUM_BUFFERS },
    { "num-transfers",        required_argument,  0,  OPTION_NUM_TRANSFERS },
    { "stream-timeout",       required_argument,  0,  OPTION_STREAM_TIMEOUT },
    { "sync-timeout",         required_argument,  0,  OPTION_SYNC_TIMEOUT },
};

void devcfg_init(struct devcfg *c)
{
    c->device_specifier = NULL;

    c->tx_frequency     = DEVCFG_DFLT_TX_FREQ;
    c->tx_bandwidth     = DEVCFG_DFLT_BANDWIDTH;
    c->tx_samplerate    = DEVCFG_DFLT_SAMPLERATE;
    c->txvga1           = DEVCFG_DFLT_TXVGA1;
    c->txvga2           = DEVCFG_DFLT_TXVGA2;

    c->rx_frequency     = DEVCFG_DFLT_TX_FREQ;
    c->rx_bandwidth     = DEVCFG_DFLT_BANDWIDTH;
    c->rx_samplerate    = DEVCFG_DFLT_SAMPLERATE;
    c->lnagain          = DEVCFG_DFLT_LNAGAIN;
    c->rxvga1           = DEVCFG_DFLT_RXVGA1;
    c->rxvga2           = DEVCFG_DFLT_RXVGA2;

    c->loopback = BLADERF_LB_NONE;

    c->verbosity = BLADERF_LOG_LEVEL_INFO;

    c->enable_xb200 = false;

    c->samples_per_buffer   = DEVCFG_DFLT_SAMPLES_PER_BUF;
    c->num_buffers          = DEVCFG_DFLT_NUM_BUFFERS;
    c->num_transfers        = DEVCFG_DFLT_NUM_TRANSFERS;
    c->stream_timeout_ms    = DEVCFG_DFLT_STREAM_TIMEMOUT_MS;
    c->sync_timeout_ms      = DEVCFG_DFLT_SYNC_TIMEOUT_MS;
}

void devcfg_deinit(struct devcfg *c)
{
    free(c->device_specifier);
}

void devcfg_print_common_help(const char *title)
{
    if (title != NULL) {
        printf("%s", title);
    }

    printf("  -h, --help                Show this help text.\n");
    printf("  -d, --device <str>        Open the specified device.\n");
    printf("  -v, --verbosity <level>   Set the libbladeRF verbosity level.\n");
    printf("  --xb200                   Enable the XB-200. This will remain enabled\n");
    printf("                              until the device is power-cycled.\n");
    printf("\n");
    printf("  -f, --frequency <freq>    Set RX and TX to the specified frequency.\n");
    printf("  --rx-frequency <freq>     Set RX to the specified frequency.\n");
    printf("  --tx-frequency <freq>     Set TX to the specified frequency.\n");
    printf("\n");
    printf("  -s, --samplerate <rate>   Set RX and TX to the specified sample rate.\n");
    printf("  --rx-samplerate <rate>    Set RX to the specified sample rate.\n");
    printf("  --tx-samplerate <rate>    Set RX to the specified sample rate.\n");
    printf("\n");
    printf("  -b, --bandwidth <bw>      Set RX and TX to the specified bandwidth.\n");
    printf("  --rx-bandwidth <bw>       Set RX to the specified bandwidth.\n");
    printf("  --tx-bandwidth <bw>       Set TX to the specified bandwidth.\n");
    printf("\n");
    printf("  --lna-gain <gain>         Set the RX LNA to the specified gain.\n");
    printf("                                Options are: bypass, mid, max\n");
    printf("  --rxvga1-gain <gain>      Set RX VGA1 to the specified gain.\n");
    printf("  --rxvga2-gain <gain>      Set RX VGA2 to the specified gain.\n");
    printf("  --txvga1-gain <gain>      Set TX VGA1 to the specified gain.\n");
    printf("  --txvga2-gain <gain>      Set TX VGA2 to the specified gain.\n");
    printf("\n");
    printf("  --num-buffers <n>         Allocate <n> sample buffers.\n");
    printf("  --samples-per-buffer <n>  Allocate <n> samples in each sample buffer.\n");
    printf("  --num-transfers <n>       Utilize up to <n> simultaneous USB transfers.\n");
    printf("  --stream-timeout <n>      Set stream timeout to <n> milliseconds.\n");
    printf("  --sync-timeout <n>        Set sync function timeout to <n> milliseconds.\n");

}

struct option * devcfg_get_long_options(const struct option *app_options)
{
    struct option *ret;
    size_t app_size = 0;
    const size_t devcfg_size = sizeof(devcfg_long_options);

    while (app_options[app_size].name != NULL) {
        app_size++;
    }

    /* Account for 0-entry */
    app_size = (app_size + 1) * sizeof(app_options[0]);

    ret = malloc(devcfg_size + app_size);

    if (ret != NULL) {
        memcpy(ret, &devcfg_long_options, devcfg_size);
        memcpy(ret + ARRAY_SIZE(devcfg_long_options) , app_options, app_size);
    }

    return ret;
}

int devcfg_handle_args(int argc, char **argv, const char *option_str,
                       const struct option *long_options, struct devcfg *config)
{
    int c;
    bool ok;
    int status;
    unsigned int freq_min;

    while ((c = getopt_long(argc, argv, option_str, long_options, NULL)) >= 0) {

        if (config->enable_xb200) {
            freq_min = BLADERF_FREQUENCY_MIN_XB200;
        } else {
            freq_min = BLADERF_FREQUENCY_MIN;
        }

        switch (c) {

            case OPTION_HELP:
                return 1;
                break;

            case OPTION_DEVICE:
                if (config->device_specifier == NULL) {
                    config->device_specifier = strdup(optarg);
                    if (config->device_specifier == NULL) {
                        perror("strdup");
                        return -1;
                    }
                }
                break;

            case OPTION_LOOPBACK:
                status = str2loopback(optarg, &config->loopback);
                if (status != 0) {
                    fprintf(stderr, "Invalid loopback mode: %s\n", optarg);
                    return -1;
                }
                break;

            case OPTION_VERBOSITY:
                config->verbosity = str2loglevel(optarg, &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid verbosity level: %s\n", optarg);
                    return -1;
                }
                break;

            case OPTION_ENABLE_XB200:
                config->enable_xb200 = true;
                break;

            case OPTION_FREQUENCY:
                config->rx_frequency =
                    str2uint_suffix(optarg,
                                    freq_min,
                                    BLADERF_FREQUENCY_MAX,
                                    hz_suffixes, ARRAY_SIZE(hz_suffixes),
                                    &ok);

                if (!ok) {
                    fprintf(stderr, "Invalid frequency: %s\n", optarg);
                    return -1;
                } else {
                    config->tx_frequency = config->rx_frequency;
                }
                break;

            case OPTION_RX_FREQUENCY:
                config->rx_frequency =
                    str2uint_suffix(optarg,
                                    freq_min,
                                    BLADERF_FREQUENCY_MAX,
                                    hz_suffixes, ARRAY_SIZE(hz_suffixes),
                                    &ok);

                if (!ok) {
                    fprintf(stderr, "Invalid RX frequency: %s\n", optarg);
                    return -1;
                }
                break;

            case OPTION_TX_FREQUENCY:
                config->tx_frequency =
                    str2uint_suffix(optarg,
                                    freq_min,
                                    BLADERF_FREQUENCY_MAX,
                                    hz_suffixes, ARRAY_SIZE(hz_suffixes),
                                    &ok);

                if (!ok) {
                    fprintf(stderr, "Invalid TX frequency: %s\n", optarg);
                    return -1;
                }
                break;

            case OPTION_SAMPLERATE:
                config->rx_samplerate =
                    str2uint_suffix(optarg,
                                    BLADERF_SAMPLERATE_MIN,
                                    BLADERF_SAMPLERATE_REC_MAX,
                                    hz_suffixes, ARRAY_SIZE(hz_suffixes),
                                    &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid sample rate: %s\n", optarg);
                    return -1;
                } else {
                    config->tx_samplerate = config->rx_samplerate;
                }
                break;

            case OPTION_RX_SAMPLERATE:
                config->rx_samplerate =
                    str2uint_suffix(optarg,
                                    BLADERF_SAMPLERATE_MIN,
                                    BLADERF_SAMPLERATE_REC_MAX,
                                    hz_suffixes, ARRAY_SIZE(hz_suffixes),
                                    &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid RX sample rate: %s\n", optarg);
                    return -1;
                }
                break;

            case OPTION_TX_SAMPLERATE:
                config->tx_samplerate =
                    str2uint_suffix(optarg,
                                    BLADERF_SAMPLERATE_MIN,
                                    BLADERF_SAMPLERATE_REC_MAX,
                                    hz_suffixes, ARRAY_SIZE(hz_suffixes),
                                    &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid TX sample rate: %s\n", optarg);
                    return -1;
                }
                break;

            case OPTION_BANDWIDTH:
                config->rx_bandwidth =
                    str2uint_suffix(optarg,
                                    BLADERF_BANDWIDTH_MIN,
                                    BLADERF_BANDWIDTH_MAX,
                                    hz_suffixes, ARRAY_SIZE(hz_suffixes),
                                    &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid bandwidth: %s\n", optarg);
                    return -1;
                } else {
                    config->tx_bandwidth = config->rx_bandwidth;
                }
                break;

            case OPTION_RX_BANDWIDTH:
                config->rx_bandwidth =
                    str2uint_suffix(optarg,
                                    BLADERF_BANDWIDTH_MIN,
                                    BLADERF_BANDWIDTH_MAX,
                                    hz_suffixes, ARRAY_SIZE(hz_suffixes),
                                    &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid RX bandwidth: %s\n", optarg);
                    return -1;
                }
                break;

            case OPTION_TX_BANDWIDTH:
                config->tx_bandwidth =
                    str2uint_suffix(optarg,
                                    BLADERF_BANDWIDTH_MIN,
                                    BLADERF_BANDWIDTH_MAX,
                                    hz_suffixes, ARRAY_SIZE(hz_suffixes),
                                    &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid TX bandwidth: %s\n", optarg);
                    return -1;
                }
                break;

            case OPTION_LNAGAIN:
                status = str2lnagain(optarg, &config->lnagain);
                if (status != 0) {
                    fprintf(stderr, "Invalid RX LNA gain: %s\n", optarg);
                    return -1;
                }
                break;

            case OPTION_RXVGA1:
                config->rxvga1 = str2int(optarg,
                                         BLADERF_RXVGA1_GAIN_MIN,
                                         BLADERF_RXVGA1_GAIN_MAX,
                                         &ok);

                if (!ok) {
                    fprintf(stderr, "Invalid RXVGA1 gain: %s\n", optarg);
                    return -1;
                }
                break;

            case OPTION_RXVGA2:
                config->rxvga2 = str2int(optarg,
                                         BLADERF_RXVGA2_GAIN_MIN,
                                         BLADERF_RXVGA2_GAIN_MAX,
                                         &ok);

                if (!ok) {
                    fprintf(stderr, "Invalid RXVGA1 gain: %s\n", optarg);
                    return -1;
                }
                break;

            case OPTION_TXVGA1:
                config->txvga1 = str2int(optarg,
                                         BLADERF_TXVGA1_GAIN_MIN,
                                         BLADERF_TXVGA1_GAIN_MAX,
                                         &ok);

                if (!ok) {
                    fprintf(stderr, "Invalid TXVGA1 gain: %s\n", optarg);
                    return -1;
                }
                break;

            case OPTION_TXVGA2:
                config->txvga2 = str2int(optarg,
                                         BLADERF_TXVGA2_GAIN_MIN,
                                         BLADERF_TXVGA2_GAIN_MAX,
                                         &ok);

                if (!ok) {
                    fprintf(stderr, "Invalid TXVGA2 gain: %s\n", optarg);
                    return -1;
                }
                break;

            case OPTION_NUM_BUFFERS:
                config->num_buffers = str2uint(optarg, 1, UINT_MAX, &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid buffer count: %s\n", optarg);
                    return -1;
                }
                break;

            case OPTION_SAMPLES_PER_BUF:
                config->samples_per_buffer = str2uint(optarg,
                                                      1024, UINT_MAX, &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid buffer size (in samples): %s\n",
                            optarg);
                    return -1;
                }
                break;

            case OPTION_NUM_TRANSFERS:
                config->num_transfers = str2uint(optarg, 1, UINT_MAX, &ok);

                if (!ok) {
                    fprintf(stderr, "Invalid transfer count: %s\n", optarg);
                    return -1;
                }
                break;

            case OPTION_STREAM_TIMEOUT:
                config->stream_timeout_ms = str2uint(optarg, 0, UINT_MAX, &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid stream timeout: %s\n", optarg);
                    return -1;
                }
                break;

            case OPTION_SYNC_TIMEOUT:
                config->sync_timeout_ms = str2uint(optarg, 0, UINT_MAX, &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid sync function timeout: %s\n", optarg);
                    return -1;
                }
                break;
        }
    }

    return 0;
}

int devcfg_apply(struct bladerf *dev, const struct devcfg *c)
{
    int status;

    bladerf_log_set_verbosity(c->verbosity);

    status = bladerf_set_loopback(dev, c->loopback);
    if (status != 0) {
        fprintf(stderr, "Failed to set loopback: %s\n",
                bladerf_strerror(status));
        return -1;
    }

    if (c->enable_xb200) {
        status = bladerf_expansion_attach(dev, BLADERF_XB_200);
        if (status != 0) {
            fprintf(stderr, "Failed to attach XB-200.\n");
            return -1;
        }
    }

    status = bladerf_set_frequency(dev, BLADERF_MODULE_RX, c->rx_frequency);
    if (status != 0) {
        fprintf(stderr, "Failed to set RX frequency: %s\n",
                bladerf_strerror(status));
        return -1;
    }

    status = bladerf_set_frequency(dev, BLADERF_MODULE_TX, c->tx_frequency);
    if (status != 0) {
        fprintf(stderr, "Failed to set TX frequency: %s\n",
                bladerf_strerror(status));
        return -1;
    }

    status = bladerf_set_sample_rate(dev, BLADERF_MODULE_RX, c->rx_samplerate, NULL);
    if (status != 0) {
        fprintf(stderr, "Failed to set RX sample rate:%s\n",
                bladerf_strerror(status));
        return -1;
    }

    status = bladerf_set_sample_rate(dev, BLADERF_MODULE_TX, c->tx_samplerate, NULL);
    if (status != 0) {
        fprintf(stderr, "Failed to set TX sample rate: %s\n",
                bladerf_strerror(status));
        return -1;
    }

    status = bladerf_set_bandwidth(dev, BLADERF_MODULE_RX, c->rx_bandwidth, NULL);
    if (status != 0) {
        fprintf(stderr, "Failed to set RX bandwidth: %s\n",
                bladerf_strerror(status));
        return -1;
    }

    status = bladerf_set_bandwidth(dev, BLADERF_MODULE_TX, c->tx_bandwidth, NULL);
    if (status != 0) {
        fprintf(stderr, "Failed to set RX bandwidth: %s\n",
                bladerf_strerror(status));
        return -1;
    }

    status = bladerf_set_lna_gain(dev, c->lnagain);
    if (status != 0) {
        fprintf(stderr, "Failed to set RX LNA gain: %s\n",
                bladerf_strerror(status));
        return -1;
    }

    status = bladerf_set_rxvga1(dev, c->rxvga1);
    if (status != 0) {
        fprintf(stderr, "Failed to set RX VGA1 gain: %s\n",
                bladerf_strerror(status));
        return -1;
    }

    status = bladerf_set_rxvga2(dev, c->rxvga2);
    if (status != 0) {
        fprintf(stderr, "Failed to set RX VGA2 gain: %s\n",
                bladerf_strerror(status));
        return -1;
    }

    status = bladerf_set_txvga1(dev, c->txvga1);
    if (status != 0) {
        fprintf(stderr, "Failed to set TX VGA1 gain: %s\n",
                bladerf_strerror(status));
        return -1;
    }

    status = bladerf_set_txvga2(dev, c->txvga2);
    if (status != 0) {
        fprintf(stderr, "Failed to set TX VGA2 gain: %s\n",
                bladerf_strerror(status));
        return -1;
    }

    return 0;
}

int devcfg_perform_sync_config(struct bladerf *dev,
                               bladerf_module module,
                               bladerf_format format,
                               const struct devcfg *config,
                               bool enable_module)
{
    int status = bladerf_sync_config(dev, module, format,
                                     config->num_buffers,
                                     config->samples_per_buffer,
                                     config->num_transfers,
                                     config->stream_timeout_ms);

    if (status != 0) {
        fprintf(stderr, "Failed to configure %s: %s\n",
                module == BLADERF_MODULE_RX ? "RX" : "TX",
                bladerf_strerror(status));
        return -1;
    }

    if (enable_module) {
        status = bladerf_enable_module(dev, module, true);
        if (status != 0) {
            fprintf(stderr, "Failed to enable %s module: %s\n",
                    module == BLADERF_MODULE_RX ? "RX" : "TX",
                    bladerf_strerror(status));
        }
    }

    return 0;
}
