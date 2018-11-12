/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2013-2018 Nuand LLC
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

#include <libbladeRF.h>

#include "conversions.h"
#include "helpers/file.h"
#include "log.h"
#include "parse.h"

/******************************************************************************/
/* Config file stuff */
/******************************************************************************/

const struct numeric_suffix freq_suffixes[] = { { "G", 1000 * 1000 * 1000 },
                                                { "GHz", 1000 * 1000 * 1000 },
                                                { "M", 1000 * 1000 },
                                                { "MHz", 1000 * 1000 },
                                                { "k", 1000 },
                                                { "kHz", 1000 } };
#define NUM_FREQ_SUFFIXES (sizeof(freq_suffixes) / sizeof(freq_suffixes[0]))
#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)

static int apply_config_options(struct bladerf *dev, struct config_options opt)
{
    int status;
    bladerf_frequency freq;
    bladerf_bandwidth bw;
    uint32_t val;
    bool ok;
    bladerf_gain_mode gain_mode;
    const struct bladerf_range *rx_range = NULL;
    const struct bladerf_range *tx_range = NULL;
    bladerf_sampling sampling_mode;
    bladerf_vctcxo_tamer_mode tamer_mode = BLADERF_VCTCXO_TAMER_INVALID;

    struct bladerf_rational_rate rate, actual;

    status = BLADERF_ERR_INVAL;

    if (!strcasecmp(opt.key, "biastee_tx")) {
        bool enable = false;

        status = str2bool(opt.value, &enable);
        if (status < 0) {
            return BLADERF_ERR_INVAL;
        }

        status = bladerf_set_bias_tee(dev, BLADERF_CHANNEL_TX(0), enable);
        if (status < 0) {
            return status;
        }
    } else if (!strcasecmp(opt.key, "biastee_rx")) {
        bool enable = false;

        status = str2bool(opt.value, &enable);
        if (status < 0) {
            return BLADERF_ERR_INVAL;
        }

        status = bladerf_set_bias_tee(dev, BLADERF_CHANNEL_RX(0), enable);
        if (status < 0) {
            return status;
        }
    } else if (!strcasecmp(opt.key, "fpga")) {
        status = bladerf_load_fpga(dev, opt.value);
        if (status < 0) {
            log_warning("Config line %d: could not load FPGA from `%s'\n",
                        opt.lineno, opt.value);
        }
        return status;
    } else if (!strcasecmp(opt.key, "frequency")) {
        status =
            bladerf_get_frequency_range(dev, BLADERF_CHANNEL_RX(0), &rx_range);
        if (status < 0) {
            return status;
        }

        status =
            bladerf_get_frequency_range(dev, BLADERF_CHANNEL_TX(0), &tx_range);
        if (status < 0) {
            return status;
        }

        freq = str2uint64_suffix(opt.value, MAX(rx_range->min, tx_range->min),
                                 MIN(rx_range->max, tx_range->max),
                                 freq_suffixes, NUM_FREQ_SUFFIXES, &ok);
        if (!ok) {
            return BLADERF_ERR_INVAL;
        }

        status = bladerf_set_frequency(dev, BLADERF_CHANNEL_RX(0), freq);
        if (status < 0) {
            return status;
        }

        status = bladerf_set_frequency(dev, BLADERF_CHANNEL_TX(0), freq);
    } else if (!strcasecmp(opt.key, "samplerate")) {
        status = bladerf_get_sample_rate_range(dev, BLADERF_CHANNEL_RX(0),
                                               &rx_range);
        if (status < 0) {
            return status;
        }

        status = bladerf_get_sample_rate_range(dev, BLADERF_CHANNEL_TX(0),
                                               &tx_range);
        if (status < 0) {
            return status;
        }

        freq = str2uint64_suffix(opt.value, MAX(rx_range->min, tx_range->min),
                                 MIN(rx_range->max, tx_range->max),
                                 freq_suffixes, NUM_FREQ_SUFFIXES, &ok);
        if (!ok) {
            return BLADERF_ERR_INVAL;
        }

        rate.integer = freq;
        rate.num     = 0;
        rate.den     = 1;

        status = bladerf_set_rational_sample_rate(dev, BLADERF_CHANNEL_RX(0),
                                                  &rate, &actual);
        if (status < 0) {
            return status;
        }

        status = bladerf_set_rational_sample_rate(dev, BLADERF_CHANNEL_TX(0),
                                                  &rate, &actual);
    } else if (!strcasecmp(opt.key, "bandwidth")) {
        status =
            bladerf_get_bandwidth_range(dev, BLADERF_CHANNEL_RX(0), &rx_range);
        if (status < 0) {
            return status;
        }

        status =
            bladerf_get_bandwidth_range(dev, BLADERF_CHANNEL_TX(0), &tx_range);
        if (status < 0) {
            return status;
        }

        if (MIN(rx_range->max, tx_range->max) >= UINT32_MAX) {
            return BLADERF_ERR_INVAL;
        }

        bw = str2uint_suffix(
            opt.value, (bladerf_bandwidth)MAX(rx_range->min, tx_range->min),
            (bladerf_bandwidth)MIN(rx_range->max, tx_range->max), freq_suffixes,
            NUM_FREQ_SUFFIXES, &ok);
        if (!ok) {
            return BLADERF_ERR_INVAL;
        }

        status = bladerf_set_bandwidth(dev, BLADERF_CHANNEL_RX(0), bw, NULL);
        if (status < 0) {
            return status;
        }

        status = bladerf_set_bandwidth(dev, BLADERF_CHANNEL_TX(0), bw, NULL);
    } else if (!strcasecmp(opt.key, "agc")) {
        bool agcval = false;

        status = str2bool(opt.value, &agcval);
        if (status != 0) {
            return BLADERF_ERR_INVAL;
        }

        gain_mode = agcval ? BLADERF_GAIN_AUTOMATIC : BLADERF_GAIN_MANUAL;
        status = bladerf_set_gain_mode(dev, BLADERF_CHANNEL_RX(0), gain_mode);
    } else if (!strcasecmp(opt.key, "gpio")) {
        val = str2uint(opt.key, 0, -1, &ok);
        if (!ok) {
            return BLADERF_ERR_INVAL;
        }

        status = bladerf_config_gpio_write(dev, val);
    } else if (!strcasecmp(opt.key, "sampling")) {
        if (!strcasecmp(opt.value, "internal")) {
            sampling_mode = BLADERF_SAMPLING_INTERNAL;
        } else if (!strcasecmp(opt.value, "external")) {
            sampling_mode = BLADERF_SAMPLING_EXTERNAL;
        } else {
            return BLADERF_ERR_INVAL;
        }

        status = bladerf_set_sampling(dev, sampling_mode);
    } else if (!strcasecmp(opt.key, "trimdac")) {
        val = str2uint(opt.value, 0, -1, &ok);
        if (!ok) {
            return BLADERF_ERR_INVAL;
        }

        status = bladerf_dac_write(dev, val);
    } else if (!strcasecmp(opt.value, "vctcxo_tamer")) {
        if (!strcasecmp(opt.value, "disabled") ||
            !strcasecmp(opt.value, "off")) {
            tamer_mode = BLADERF_VCTCXO_TAMER_DISABLED;
        } else if (!strcasecmp(opt.value, "1PPS") ||
                   !strcasecmp(opt.value, "1 PPS")) {
            tamer_mode = BLADERF_VCTCXO_TAMER_1_PPS;
        } else if (!strcasecmp(opt.value, "10MHZ") ||
                   !strcasecmp(opt.value, "10 MHZ")) {
            tamer_mode = BLADERF_VCTCXO_TAMER_10_MHZ;
        } else if (!strcasecmp(opt.value, "10M")) {
            tamer_mode = BLADERF_VCTCXO_TAMER_10_MHZ;
        } else {
            return BLADERF_ERR_INVAL;
        }

        status = bladerf_set_vctcxo_tamer_mode(dev, tamer_mode);
    } else {
        log_warning("Invalid key `%s' on line %d\n", opt.key, opt.lineno);
    }

    if (status < 0)
        log_warning("Error message for option (%s) on line %d:\n%s\n", opt.key,
                    opt.lineno, bladerf_strerror(status));

    return status;
}

int config_load_options_file(struct bladerf *dev)
{
    char *filename = NULL;
    int status     = 0;

    uint8_t *buf = NULL;
    size_t buf_size;

    int optc;
    int j;
    struct config_options *optv;

    filename = file_find("bladeRF.conf");
    if (!filename) {
        filename = file_find("bladerf.conf");

        /* A missing file that is optional is not an error */
        if (!filename) {
            return 0;
        }
    }

    status = file_read_buffer(filename, &buf, &buf_size);
    if (status < 0) {
        goto out;
    }

    optc = str2options(dev, (const char *)buf, buf_size, &optv);
    if (optc < 0) {
        status = BLADERF_ERR_INVAL;
        goto out_buf;
    }

    for (j = 0; j < optc; j++) {
        status = apply_config_options(dev, optv[j]);
        if (status < 0) {
            log_warning("Invalid config option `%s' on line %d\n", optv[j].key,
                        optv[j].lineno);
            break;
        }
    }

    free_opts(optv, optc);

out_buf:
    free(buf);
out:
    free(filename);
    return status;
}
