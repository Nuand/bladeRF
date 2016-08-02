/**
 * @brief   Gets configuration options from command line arguments
 *
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2016 Nuand LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <getopt.h>
#include <sys/stat.h>
#include <errno.h>

#include "config.h"
#include "conversions.h"

#ifdef DEBUG_CONFIG
#   define pr_dbg(...) fprintf(stderr, "[CONFIG] " __VA_ARGS__)
#else
#   define pr_dbg(...) do {} while (0)
#endif

#define OPTIONS "hd:r:o:t:i:q"

#define OPTION_HELP     'h'
#define OPTION_DEVICE   'd'
#define OPTION_QUIET    'q'

#define OPTION_RXFREQ   'r'
#define OPTION_INPUT    'i'
#define OPTION_RXLNA    0x80
#define OPTION_RXVGA1   0x81
#define OPTION_RXVGA2   0x82

#define OPTION_TXFREQ   't'
#define OPTION_OUTPUT   'o'
#define OPTION_TXVGA1   0x90
#define OPTION_TXVGA2   0x91

#define RX_FREQ_DEFAULT 904000000
#define RX_LNA_DEFAULT  BLADERF_LNA_GAIN_MAX
#define RX_VGA1_DEFAULT BLADERF_RXVGA1_GAIN_MAX
#define RX_VGA2_DEFAULT BLADERF_RXVGA2_GAIN_MIN

#define TX_FREQ_DEFAULT 924000000
#define TX_VGA1_DEFAULT BLADERF_TXVGA1_GAIN_MAX
#define TX_VGA2_DEFAULT BLADERF_TXVGA2_GAIN_MIN

static struct option long_options[] = {
    { "help",     no_argument,        NULL,   OPTION_HELP     },
    { "device",   required_argument,  NULL,   OPTION_DEVICE   },
    { "quiet",    no_argument,        NULL,   OPTION_QUIET    },

    { "output",   required_argument,  NULL,   OPTION_OUTPUT   },
    { "rx-lna",   required_argument,  NULL,   OPTION_RXLNA    },
    { "rx-vga1",  required_argument,  NULL,   OPTION_RXVGA1   },
    { "rx-vga2",  required_argument,  NULL,   OPTION_RXVGA2   },
    { "rx-freq",  required_argument,  NULL,   OPTION_RXFREQ   },

    { "input",    required_argument,  NULL,   OPTION_INPUT    },
    { "tx-vga1",  required_argument,  NULL,   OPTION_TXVGA1   },
    { "tx-vga2",  required_argument,  NULL,   OPTION_TXVGA2   },
    { "tx-freq",  required_argument,  NULL,   OPTION_TXFREQ   },

    { NULL,       0,                  NULL,   0               },
};

const struct numeric_suffix freq_suffixes[] = {
    { "k",      1000 },
    { "KHz",    1000 },

    { "M",      1000 * 1000 },
    { "MHz",    1000 * 1000 },

    { "G",      1000 * 1000 * 1000 },
    { "GHz",    1000 * 1000 * 1000 },
};

static const size_t num_freq_suffixes =
    sizeof(freq_suffixes) / sizeof(freq_suffixes[0]);

static struct config *alloc_config_with_defaults()
{
    struct config *config;

    config = calloc(1, sizeof(*config));
    if (!config) {
        perror("calloc");
        return NULL;
    }

    /* RX defaults */
    config->rx_output            = NULL;

    config->params.rx_freq        = RX_FREQ_DEFAULT;

    config->params.rx_lna_gain    = RX_LNA_DEFAULT;
    config->params.rx_vga1_gain    = RX_VGA1_DEFAULT;
    config->params.rx_vga2_gain    = RX_VGA2_DEFAULT;


    /* TX defaults */
    config->tx_input            = NULL;

    config->params.tx_freq        = TX_FREQ_DEFAULT;

    config->params.tx_vga1_gain    = TX_VGA1_DEFAULT;
    config->params.tx_vga2_gain    = TX_VGA2_DEFAULT;

    return config;
}

/* Initialize configuration items not provided by user */
static int init_remaining_params(struct config *config)
{
    int status;

    if (config->bladerf_dev == NULL) {
        status = bladerf_open(&config->bladerf_dev, NULL);
        if (status != 0) {
            fprintf(stderr, "Failed to open any available bladeRF: %s\n",
                    bladerf_strerror(status));
            return status;
        }
    }

    if (config->rx_output == NULL) {
        config->rx_output = stdout;
    }

    if (config->tx_input == NULL) {
        config->tx_input = stdin;
        config->tx_filesize = -1;
    }

    return 0;
}

int config_init_from_cmdline(int argc, char * const argv[],
                             struct config **config_out)
{
    int status = 0;
    struct config *config = NULL;
    int c, idx;
    bool valid;
    struct stat file_info;

    config = alloc_config_with_defaults();
    if (config == NULL) {
        pr_dbg("%s: Failed to alloc config.\n", __FUNCTION__);
        return -2;
    }

    while ((c = getopt_long(argc, argv, OPTIONS, long_options, &idx)) != -1) {
        switch (c) {
            case OPTION_HELP:
                status = 1;
                goto out;

            case OPTION_DEVICE:
                if (config->bladerf_dev == NULL) {
                    status = bladerf_open(&config->bladerf_dev, optarg);
                    if (status < 0) {
                        fprintf(stderr, "Failed to open bladeRF=%s: %s\n",
                                optarg, bladerf_strerror(status));
                        goto out;
                    }
                }
                break;

            case OPTION_OUTPUT:
                if (config->rx_output == NULL) {
                    if (!strcasecmp(optarg, "stdout")) {
                        config->rx_output = stdout;
                    } else {
                        config->rx_output = fopen(optarg, "wb");
                        if (config->rx_output == NULL) {
                            status = -errno;
                            fprintf(stderr, "Failed to open %s for writing: %s\n",
                                    optarg, strerror(-status));
                            goto out;
                        }
                    }
                }
                break;

            case OPTION_RXLNA:
                status = str2lnagain(optarg, &config->params.rx_lna_gain);
                if (status != 0) {
                    fprintf(stderr, "Invalid RX LNA gain: %s\n", optarg);
                    goto out;
                }
                break;

            case OPTION_RXVGA1:
                config->params.rx_vga1_gain =
                    str2int(optarg,
                            BLADERF_RXVGA1_GAIN_MIN,
                            BLADERF_RXVGA1_GAIN_MAX,
                            &valid);

                if (!valid) {
                    status = -1;
                    fprintf(stderr, "Invalid RX VGA1 gain: %s\n", optarg);
                    goto out;
                }
                break;

            case OPTION_RXVGA2:
                config->params.rx_vga2_gain =
                    str2int(optarg,
                            BLADERF_RXVGA2_GAIN_MIN,
                            BLADERF_RXVGA2_GAIN_MAX,
                            &valid);

                if (!valid) {
                    status = -1;
                    fprintf(stderr, "Invalid RX VGA2 gain: %s\n", optarg);
                    goto out;
                }
                break;

            case OPTION_RXFREQ:
                config->params.rx_freq =
                    str2uint_suffix(optarg,
                                    BLADERF_FREQUENCY_MIN,
                                    BLADERF_FREQUENCY_MAX,
                                    freq_suffixes, num_freq_suffixes,
                                    &valid);

                if (!valid) {
                    status = -1;
                    fprintf(stderr, "Invalid RX frequency: %s\n", optarg);
                    goto out;
                }
                break;


            case OPTION_INPUT:
                if (config->tx_input == NULL) {
                    if (!strcasecmp(optarg, "stdin")) {
                        config->tx_input = stdin;
                        config->tx_filesize = -1;
                    } else {
                        config->tx_input = fopen(optarg, "rb");
                        if (config->tx_input == NULL) {
                            status = -errno;
                            fprintf(stderr, "Failed to open %s for reading: %s\n",
                                    optarg, strerror(-status));
                            goto out;
                        }
                        //Get file size
                        status = stat(optarg, &file_info);
                        if (status != 0){
                            fprintf(stderr, "Couldn't filesize of %s: %s\n",
                                    optarg, strerror(status));
                        }
                        config->tx_filesize = file_info.st_size;
                    }
                }
                break;

            case OPTION_TXVGA1:
                config->params.tx_vga1_gain =
                    str2int(optarg,
                            BLADERF_TXVGA1_GAIN_MIN,
                            BLADERF_TXVGA2_GAIN_MAX,
                            &valid);

                if (!valid) {
                    status = -1;
                    fprintf(stderr, "Invalid TX VGA1 gain: %s\n", optarg);
                    goto out;
                }
                break;

            case OPTION_TXVGA2:
                config->params.tx_vga2_gain =
                    str2int(optarg,
                            BLADERF_TXVGA2_GAIN_MIN,
                            BLADERF_TXVGA2_GAIN_MAX,
                            &valid);

                if (!valid) {
                    status = -1;
                    fprintf(stderr, "Invalid TX VGA2 gain: %s\n", optarg);
                    goto out;
                }
                break;

            case OPTION_TXFREQ:
                config->params.tx_freq =
                    str2uint_suffix(optarg,
                                    BLADERF_FREQUENCY_MIN,
                                    BLADERF_FREQUENCY_MAX,
                                    freq_suffixes, num_freq_suffixes,
                                    &valid);
                if (!valid) {
                    status = -1;
                    fprintf(stderr, "Invalid TX frequency: %s\n", optarg);
                    goto out;
                }
                break;

            case OPTION_QUIET:
                config->quiet = true;
                break;
        }
    }

    /* Initialize any properties not covered by user input */
    status = init_remaining_params(config);

out:
    if (status != 0) {
        config_deinit(config);
        *config_out = NULL;
    } else {
        *config_out = config;
    }

    return status;
}

void config_deinit(struct config *config)
{
    if (config == NULL) {
        pr_dbg("%s: NULL ptr\n", __FUNCTION__);
        return;
    }

    pr_dbg("Deinitializing...\n");

    if (config->bladerf_dev != NULL) {
        pr_dbg("\tClosing bladerf.\n");
        bladerf_close(config->bladerf_dev);
        config->bladerf_dev = NULL;
    }
if (config->rx_output != NULL) {
        fclose(config->rx_output);
        config->rx_output = NULL;
    }

    if (config->tx_input != NULL) {
        fclose(config->tx_input);
        config->tx_input = NULL;
    }

    free(config);
}

void config_print_options()
{
    printf(

"   -h, --help              Show this help text.\n"
"   -d, --device <str>      Open the specified bladeRF device.\n"
"                            Any available device is used if not specified.\n"
"   -q, --quiet             Suppress printing of banner/exit messages.\n"
"\n"
"   -r, --rx-freq <freq>    RX frequency in Hz. Default: %d\n"
"   -o, --output <file>     RX data output. stdout is used if not specified.\n"
"   --rx-lna <value>        RX LNA gain. Values: bypass, mid, max (default)\n"
"   --rx-vga1 <value>       RX VGA1 gain. Range: %d to %d. Default = %d.\n"
"   --rx-vga2 <value>       RX VGA2 gain. Range: %d to %d. Default = %d.\n"
"\n"
"   -t, --tx-freq <freq>    TX frequency. Default: %d\n"
"   -i, --input <file>      TX data input. stdin is used if not specified.\n"
"   --tx-vga1 <value>       TX VGA1 gain. Range: %d to %d. Default = %d.\n"
"   --tx-vga2 <value>       TX VGA2 gain. Range: %d to %d. Default = %d.\n",

    RX_FREQ_DEFAULT,
    BLADERF_RXVGA1_GAIN_MIN, BLADERF_RXVGA1_GAIN_MAX, RX_VGA1_DEFAULT,
    BLADERF_RXVGA2_GAIN_MIN, BLADERF_RXVGA2_GAIN_MAX, RX_VGA2_DEFAULT,

    TX_FREQ_DEFAULT,
    BLADERF_TXVGA1_GAIN_MIN, BLADERF_TXVGA1_GAIN_MAX, TX_VGA1_DEFAULT,
    BLADERF_TXVGA2_GAIN_MIN, BLADERF_TXVGA2_GAIN_MAX, TX_VGA2_DEFAULT

    );
}

#ifdef CONFIG_TEST
void print_test_help(const char *argv0)
{
    printf("Config structure test\n");
    printf("Usage: %s [options]\n\n", argv0);
    printf("Options:\n");
    config_print_options();
    printf("\n");
}

void print_config(const struct config *config)
{
    printf("bladeRF handle:     %p\n", config->bladerf_dev);
    printf("\n");
    printf("RX Parameters:\n");
    printf("    Output handle:  %p\n", config->rx_output);
    printf("    Frequency (Hz): %u\n", config->params.rx_freq);
    printf("    LNA gain:       %d\n", config->params.rx_lna_gain);
    printf("    VGA1 gain:      %d\n", config->params.rx_vga1_gain);
    printf("    VGA2 gain:      %d\n", config->params.rx_vga2_gain);
    printf("\n");
    printf("TX Parameters:\n");
    printf("    Input handle:   %p\n", config->tx_input);
    printf("    Frequency (Hz): %u\n", config->params.tx_freq);
    printf("    VGA1 gain:      %d\n", config->params.tx_vga1_gain);
    printf("    VGA2 gain:      %d\n", config->params.tx_vga2_gain);
    printf("\n");
}

int main(int argc, char *argv[])
{
    int status;
    struct config *config = NULL;

    status = config_init_from_cmdline(argc, argv, &config);
    if (status < 0) {
        return status;
    } else if (status == 1) {
        print_test_help(argv[0]);
        return 0;
    } else if (status > 0) {
        fprintf(stderr, "Unexpected return value: %d\n", status);
        return status;
    }

    print_config(config);
    config_deinit(config);
    return EXIT_SUCCESS;
}
#endif
