/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014 Nuand LLC
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
#include <stdlib.h>
#include "bladerf_priv.h"
#include "dc_cal_table.h"
#include "fpga.h"
#include "file_ops.h"
#include "log.h"
#include "config.h"
#include "parse.h"

static inline void load_dc_cal(struct bladerf *dev, const char *file)
{
    int status;
    struct bladerf_image *img;

    img = bladerf_alloc_image(BLADERF_IMAGE_TYPE_INVALID, 0, 0);
    if (img == NULL) {
        return;
    }

    status = bladerf_image_read(img, file);
    if (status != 0) {
        log_debug("Failed to open image file (%s): %s\n",
                  file, bladerf_strerror(status));
        goto out;
    }

    switch (img->type) {
        case BLADERF_IMAGE_TYPE_RX_DC_CAL:
            dc_cal_tbl_free(&dev->cal.dc_rx);
            dev->cal.dc_rx = dc_cal_tbl_load(img->data, img->length);
            break;

        case BLADERF_IMAGE_TYPE_TX_DC_CAL:
            dc_cal_tbl_free(&dev->cal.dc_tx);
            dev->cal.dc_tx = dc_cal_tbl_load(img->data, img->length);
            break;

        default:
            log_debug("%s is not an RX DC calibration table.\n", file);
    }

out:
    bladerf_free_image(img);
}

int config_load_fpga(struct bladerf *dev)
{
    int status = 0;
    char *filename = NULL;

    if (dev->fpga_size == BLADERF_FPGA_40KLE) {
        filename = file_find("hostedx40.rbf");
    } else if (dev->fpga_size == BLADERF_FPGA_115KLE) {
        filename = file_find("hostedx115.rbf");
    } else {
        /* Unexpected, but this will be complained about elsewhere */
        return 0;
    }

    if (filename != NULL) {
        log_debug("Loading FPGA from: %s\n", filename);
        status = fpga_load_from_file(dev, filename);
    }

    free(filename);
    return status;
}

int config_load_dc_cals(struct bladerf *dev)
{
    char *filename;
    char *full_path;

    filename = calloc(1, FILENAME_MAX + 1);
    if (filename == NULL) {
        return BLADERF_ERR_MEM;
    }

    strncat(filename, dev->ident.serial, FILENAME_MAX);
    strncat(filename, "_dc_rx.tbl", FILENAME_MAX - BLADERF_SERIAL_LENGTH);
    full_path = file_find(filename);
    if (full_path != NULL) {
        log_debug("Loading %s\n", full_path);
        load_dc_cal(dev, full_path);
        free(full_path);
    }

    memset(filename, 0, FILENAME_MAX + 1);
    strncat(filename, dev->ident.serial, FILENAME_MAX);
    strncat(filename, "_dc_tx.tbl", FILENAME_MAX - BLADERF_SERIAL_LENGTH);
    full_path = file_find(filename);
    if (full_path != NULL) {
        log_debug("Loading %s\n", full_path);
        load_dc_cal(dev, full_path);
        free(full_path);
    }

    free(filename);
    return 0;
}

const struct numeric_suffix freq_suffixes[] = {
    { "G",      1000 * 1000 * 1000 },
    { "GHz",    1000 * 1000 * 1000 },
    { "M",      1000 * 1000 },
    { "MHz",    1000 * 1000 },
    { "k",      1000 } ,
    { "kHz",    1000 }
};
#define NUM_FREQ_SUFFIXES (sizeof(freq_suffixes) / sizeof(freq_suffixes[0]))

static int apply_config_options(struct bladerf *dev, struct config_options opt)
{
    int status;
    uint64_t freq;
    unsigned int bw;
    uint32_t val;
    bool ok;
    bladerf_gain_mode gain_mode;
    bladerf_sampling sampling_mode;
    bladerf_vctcxo_tamer_mode tamer_mode = BLADERF_VCTCXO_TAMER_INVALID;

    struct bladerf_rational_rate rate, actual;

    status = BLADERF_ERR_INVAL;

    if (!strcasecmp(opt.key, "fpga")) {
        status = bladerf_load_fpga(dev, opt.value);
        if (status < 0) {
            log_warning("Config line %d: could not load FPGA from `%s'\n",
                    opt.lineno, opt.value);
        }
        return status;
    } else if (!strcasecmp(opt.key, "frequency")) {
        freq = str2uint64_suffix( opt.value,
                0, BLADERF_FREQUENCY_MAX,
                freq_suffixes, NUM_FREQ_SUFFIXES, &ok);
        if (!ok)
            return BLADERF_ERR_INVAL;

        status = bladerf_set_frequency(dev, BLADERF_MODULE_RX, (unsigned int)freq);
        if (status < 0)
            return status;

        status = bladerf_set_frequency(dev, BLADERF_MODULE_TX, (unsigned int)freq);
    } else if (!strcasecmp(opt.key, "samplerate")) {
        freq = str2uint64_suffix( opt.value,
                0, UINT64_MAX,
                freq_suffixes, NUM_FREQ_SUFFIXES, &ok);
        if (!ok)
            return BLADERF_ERR_INVAL;

        rate.integer = freq;
        rate.num = 0;
        rate.den = 1;

        status = bladerf_set_rational_sample_rate(dev, BLADERF_MODULE_RX,
                                                    &rate, &actual);
        if (status < 0)
            return status;

        status = bladerf_set_rational_sample_rate(dev, BLADERF_MODULE_TX,
                                                    &rate, &actual);
    } else if (!strcasecmp(opt.key, "bandwdith")) {
        bw = str2uint_suffix( opt.value,
                BLADERF_BANDWIDTH_MIN, BLADERF_BANDWIDTH_MAX,
                freq_suffixes, NUM_FREQ_SUFFIXES, &ok);
        if (!ok)
            return BLADERF_ERR_INVAL;

        status = bladerf_set_bandwidth(dev, BLADERF_MODULE_RX, bw, NULL);
        if (status < 0)
            return status;

        status = bladerf_set_bandwidth(dev, BLADERF_MODULE_TX, bw, NULL);
    } else if (!strcasecmp(opt.key, "agc")) {
        gain_mode = str2bool(opt.value, &ok) ? BLADERF_GAIN_AUTOMATIC
                                            : BLADERF_GAIN_MANUAL;
        printf("%s %d \n", opt.value, ok);
        if (!ok)
            return BLADERF_ERR_INVAL;

        status = bladerf_set_gain_mode(dev, BLADERF_MODULE_RX, gain_mode);
    } else if (!strcasecmp(opt.key, "gpio")) {
        val = str2uint(opt.key, 0, -1, &ok);
        if (!ok)
            return BLADERF_ERR_INVAL;

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
        if (!ok)
            return BLADERF_ERR_INVAL;

        status = bladerf_dac_write(dev, val);
    } else if (!strcasecmp(opt.value, "vctcxo_tamer")) {
        if (!strcasecmp(opt.value, "disabled")     || !strcasecmp(opt.value, "off")) {
            tamer_mode = BLADERF_VCTCXO_TAMER_DISABLED;
        } else if (!strcasecmp(opt.value, "1PPS")  || !strcasecmp(opt.value, "1 PPS")) {
            tamer_mode = BLADERF_VCTCXO_TAMER_1_PPS;
        } else if (!strcasecmp(opt.value, "10MHZ") || !strcasecmp(opt.value, "10 MHZ")) {
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
        log_warning("Error massage for option (%s) on line %d:\n%s\n",
                opt.key, opt.lineno, bladerf_strerror(status));

    return status;
}

int config_load_options_file(struct bladerf *dev)
{
    char *filename = NULL;
    int status = 0;

    uint8_t *buf = NULL;
    size_t  buf_size;

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
            log_warning("Invalid config option `%s' on line %d\n",
                            optv[j].key, optv[j].lineno);
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
