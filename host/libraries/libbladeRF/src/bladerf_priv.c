/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2013 Nuand LLC
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
#include "rel_assert.h"
#include <string.h>
#include <libbladeRF.h>
#include <stddef.h>

#include "bladerf_priv.h"
#include "backend/backend.h"
#include "lms.h"
#include "tuning.h"
#include "si5338.h"
#include "log.h"
#include "dc_cal_table.h"
#include "xb.h"


/* TODO Check for truncation (e.g., odd # bytes)? */
size_t bytes_to_c16_samples(size_t n_bytes)
{
    return n_bytes / (2 * sizeof(int16_t));
}

/* TODO Overflow check? */
size_t c16_samples_to_bytes(size_t n_samples)
{
    return n_samples * 2 * sizeof(int16_t);
}

int init_device(struct bladerf *dev)
{
    int status;
    uint32_t val;

    /* Readback the GPIO values to see if they are default or already set */
    status = CONFIG_GPIO_READ( dev, &val );
    if (status != 0) {
        log_debug("Failed to read GPIO config %s\n", bladerf_strerror(status));
        return status;
    }

    if ((val & 0x7f) == 0) {
        log_verbose( "Default GPIO value found - initializing device\n" );

        /* Set the GPIO pins to enable the LMS and select the low band */
        status = CONFIG_GPIO_WRITE(dev, 0x57);
        if (status != 0) {
            return status;
        }

        /* Disable the front ends */
        status = lms_enable_rffe(dev, BLADERF_MODULE_TX, false);
        if (status != 0) {
            return status;
        }

        status = lms_enable_rffe(dev, BLADERF_MODULE_RX, false);
        if (status != 0) {
            return status;
        }

        /* Set the internal LMS register to enable RX and TX */
        status = LMS_WRITE(dev, 0x05, 0x3e);
        if (status != 0) {
            return status;
        }

        /* LMS FAQ: Improve TX spurious emission performance */
        status = LMS_WRITE(dev, 0x47, 0x40);
        if (status != 0) {
            return status;
        }

        /* LMS FAQ: Improve ADC performance */
        status = LMS_WRITE(dev, 0x59, 0x29);
        if (status != 0) {
            return status;
        }

        /* LMS FAQ: Common mode voltage for ADC */
        status = LMS_WRITE(dev, 0x64, 0x36);
        if (status != 0) {
            return status;
        }

        /* LMS FAQ: Higher LNA Gain */
        status = LMS_WRITE(dev, 0x79, 0x37);
        if (status != 0) {
            return status;
        }

        /* Set a default samplerate */
        status = si5338_set_sample_rate(dev, BLADERF_MODULE_TX, 1000000, NULL);
        if (status != 0) {
            return status;
        }

        status = si5338_set_sample_rate(dev, BLADERF_MODULE_RX, 1000000, NULL);
        if (status != 0) {
            return status;
        }

        /* Set a default frequency of 1GHz */
        status = tuning_set_freq(dev, BLADERF_MODULE_TX, 1000000000);
        if (status != 0) {
            return status;
        }

        status = tuning_set_freq(dev, BLADERF_MODULE_RX, 1000000000);
        if (status != 0) {
            return status;
        }

        /* Set the calibrated VCTCXO DAC value */
        status = DAC_WRITE(dev, dev->dac_trim);
        if (status != 0) {
            return status;
        }
    }

    return 0;
}

int populate_abs_timeout(struct timespec *t, unsigned int timeout_ms)
{
    static const int nsec_per_sec = 1000 * 1000 * 1000;
    const unsigned int timeout_sec = timeout_ms / 1000;
    int status;

    status = clock_gettime(CLOCK_REALTIME, t);
    if (status != 0) {
        return BLADERF_ERR_UNEXPECTED;
    } else {
        t->tv_sec += timeout_sec;
        t->tv_nsec += (timeout_ms % 1000) * 1000 * 1000;

        if (t->tv_nsec >= nsec_per_sec) {
            t->tv_sec += t->tv_nsec / nsec_per_sec;
            t->tv_nsec %= nsec_per_sec;
        }

        return 0;
    }
}

int load_calibration_table(struct bladerf *dev, const char *filename)
{
    int status;
    struct bladerf_image *image = NULL;
    struct dc_cal_tbl *dc_tbl = NULL;

    if (filename == NULL) {
        memset(&dev->cal, 0, sizeof(dev->cal));
        return 0;
    }

    image = bladerf_alloc_image(BLADERF_IMAGE_TYPE_INVALID, 0xffffffff, 0);
    if (image == NULL) {
        return BLADERF_ERR_MEM;
    }

    status = bladerf_image_read(image, filename);
    if (status == 0) {

        if (image->type == BLADERF_IMAGE_TYPE_RX_DC_CAL ||
            image->type == BLADERF_IMAGE_TYPE_TX_DC_CAL) {

            bladerf_module module;
            unsigned int frequency;

            dc_tbl = dc_cal_tbl_load(image->data, image->length);

            if (dc_tbl == NULL) {
                status = BLADERF_ERR_MEM;
                goto out;
            }

            if (image->type == BLADERF_IMAGE_TYPE_RX_DC_CAL) {
                free(dev->cal.dc_rx);
                module = BLADERF_MODULE_RX;

                dev->cal.dc_rx = dc_tbl;
                dev->cal.dc_rx->reg_vals.tx_lpf_i = -1;
                dev->cal.dc_rx->reg_vals.tx_lpf_q = -1;
            } else {
                free(dev->cal.dc_tx);
                module = BLADERF_MODULE_TX;

                dev->cal.dc_tx = dc_tbl;
                dev->cal.dc_tx->reg_vals.rx_lpf_i = -1;
                dev->cal.dc_tx->reg_vals.rx_lpf_q = -1;
                dev->cal.dc_tx->reg_vals.dc_ref = -1;
                dev->cal.dc_tx->reg_vals.rxvga2a_i = -1;
                dev->cal.dc_tx->reg_vals.rxvga2a_q = -1;
                dev->cal.dc_tx->reg_vals.rxvga2b_i = -1;
                dev->cal.dc_tx->reg_vals.rxvga2b_q = -1;
            }

            status = lms_set_dc_cals(dev, &dc_tbl->reg_vals);
            if (status != 0) {
                goto out;
            }

            /* Reset the module's frequency to kick off the application of the
             * new table entries */
            status = tuning_get_freq(dev, module, &frequency);
            if (status != 0) {
                goto out;
            }

            status = tuning_set_freq(dev, module, frequency);


        } else if (image->type == BLADERF_IMAGE_TYPE_RX_IQ_CAL ||
                   image->type == BLADERF_IMAGE_TYPE_TX_IQ_CAL) {

            /* TODO: Not implemented */
            status = BLADERF_ERR_UNSUPPORTED;
            goto out;
        } else {
            status = BLADERF_ERR_INVAL;
            log_debug("%s loaded nexpected image type: %d\n",
                      __FUNCTION__, image->type);
        }
    }

out:
    bladerf_free_image(image);
    return status;
}

static inline int set_rx_gain_combo(struct bladerf *dev,
                                     bladerf_lna_gain lnagain,
                                     int rxvga1, int rxvga2)
{
    int status;
    status = lms_lna_set_gain(dev, lnagain);
    if (status < 0) {
        return status;
    }

    status = lms_rxvga1_set_gain(dev, rxvga1);
    if (status < 0) {
        return status;
    }

    return lms_rxvga2_set_gain(dev, rxvga2);
}

static int set_rx_gain(struct bladerf *dev, int gain)
{
    if (gain <= BLADERF_LNA_GAIN_MID_DB) {
        return set_rx_gain_combo(dev,
                                 BLADERF_LNA_GAIN_BYPASS,
                                 BLADERF_RXVGA1_GAIN_MIN,
                                 BLADERF_RXVGA2_GAIN_MIN);
    } else if (gain <= BLADERF_LNA_GAIN_MID_DB + BLADERF_RXVGA1_GAIN_MIN) {
        return set_rx_gain_combo(dev,
                                 BLADERF_LNA_GAIN_MID_DB,
                                 BLADERF_RXVGA1_GAIN_MIN,
                                 BLADERF_RXVGA2_GAIN_MIN);
    } else if (gain <= (BLADERF_LNA_GAIN_MAX_DB + BLADERF_RXVGA1_GAIN_MAX)) {
        return set_rx_gain_combo(dev,
                                 BLADERF_LNA_GAIN_MID,
                                 gain - BLADERF_LNA_GAIN_MID_DB,
                                 BLADERF_RXVGA2_GAIN_MIN);
    } else if (gain < (BLADERF_LNA_GAIN_MAX_DB + BLADERF_RXVGA1_GAIN_MAX + BLADERF_RXVGA2_GAIN_MAX)) {
        return set_rx_gain_combo(dev,
                BLADERF_LNA_GAIN_MAX,
                BLADERF_RXVGA1_GAIN_MAX,
                gain - (BLADERF_LNA_GAIN_MAX_DB + BLADERF_RXVGA1_GAIN_MAX));
    } else {
        return set_rx_gain_combo(dev,
                                 BLADERF_LNA_GAIN_MAX,
                                 BLADERF_RXVGA1_GAIN_MAX,
                                 BLADERF_RXVGA2_GAIN_MAX);
    }
}

static inline int set_tx_gain_combo(struct bladerf *dev, int txvga1, int txvga2)
{
    int status;

    status = lms_txvga1_set_gain(dev, txvga1);
    if (status == 0) {
        status = lms_txvga2_set_gain(dev, txvga2);
    }

    return status;
}

static int set_tx_gain(struct bladerf *dev, int gain)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    if (gain < 0) {
        gain = 0;
    }

    if (gain <= BLADERF_TXVGA2_GAIN_MAX) {
        status = set_tx_gain_combo(dev, BLADERF_TXVGA1_GAIN_MIN, gain);
    } else if (gain <= ((BLADERF_TXVGA1_GAIN_MAX - BLADERF_TXVGA1_GAIN_MIN) + BLADERF_TXVGA2_GAIN_MAX)) {
        status = set_tx_gain_combo(dev,
                    BLADERF_TXVGA1_GAIN_MIN + gain - BLADERF_TXVGA2_GAIN_MAX,
                    BLADERF_TXVGA2_GAIN_MAX);
    } else {
        status =set_tx_gain_combo(dev,
                                  BLADERF_TXVGA1_GAIN_MAX,
                                  BLADERF_TXVGA2_GAIN_MAX);
    }

    return status;
}

int set_gain(struct bladerf *dev, bladerf_module module, int gain)
{
    int status;

    if (module == BLADERF_MODULE_TX) {
        status = set_tx_gain(dev, gain);
    } else if (module == BLADERF_MODULE_RX) {
        status = set_rx_gain(dev, gain);
    } else {
        status = BLADERF_ERR_INVAL;
    }

    return status;
}
