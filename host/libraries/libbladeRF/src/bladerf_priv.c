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
#include "version_compat.h"

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

int config_gpio_write(struct bladerf *dev, uint32_t val)
{
    /* If we're connected at HS, we need to use smaller DMA transfers */
    if (dev->usb_speed == BLADERF_DEVICE_SPEED_HIGH   ) {
        val |= BLADERF_GPIO_FEATURE_SMALL_DMA_XFER;
    } else if (dev->usb_speed == BLADERF_DEVICE_SPEED_SUPER) {
        val &= ~BLADERF_GPIO_FEATURE_SMALL_DMA_XFER;
    } else {
        assert(!"Encountered unknown USB speed");
        return BLADERF_ERR_UNEXPECTED;
    }

   return dev->fn->config_gpio_write(dev, val);
}

static inline int requires_timestamps(bladerf_format format, bool *required)
{
    int status = 0;

    switch (format) {
        case BLADERF_FORMAT_SC16_Q11_META:
            *required = true;
            break;

        case BLADERF_FORMAT_SC16_Q11:
            *required = false;
            break;

        default:
            return BLADERF_ERR_INVAL;
    }

    return status;
}

int perform_format_config(struct bladerf *dev, bladerf_module module,
                          bladerf_format format)
{
    int status = 0;
    bool use_timestamps;
    bladerf_module other;
    bool other_using_timestamps;
    uint32_t gpio_val;

    status = requires_timestamps(format, &use_timestamps);
    if (status != 0) {
        log_debug("%s: Invalid format: %d\n", __FUNCTION__, format);
        return status;
    }

    if (use_timestamps && version_less_than(&dev->fpga_version, 0, 1, 0)) {
        log_warning("Timestamp support requires FPGA v0.1.0 or later.\n");
        return BLADERF_ERR_UPDATE_FPGA;
    }

    switch (module) {
        case BLADERF_MODULE_RX:
            other = BLADERF_MODULE_TX;
            break;

        case BLADERF_MODULE_TX:
            other = BLADERF_MODULE_RX;
            break;

        default:
            log_debug("Invalid module: %d\n", module);
            return BLADERF_ERR_INVAL;
    }

    status = requires_timestamps(dev->module_format[other],
                                 &other_using_timestamps);

    if ((status == 0) && (other_using_timestamps != use_timestamps)) {
        log_debug("Format conflict detected: RX=%d, TX=%d\n");
        return BLADERF_ERR_INVAL;
    }

    status = CONFIG_GPIO_READ(dev, &gpio_val);
    if (status != 0) {
        return status;
    }

    if (use_timestamps) {
        gpio_val |= (BLADERF_GPIO_TIMESTAMP | BLADERF_GPIO_TIMESTAMP_DIV2);
    } else {
        gpio_val &= ~(BLADERF_GPIO_TIMESTAMP | BLADERF_GPIO_TIMESTAMP_DIV2);
    }

    status = CONFIG_GPIO_WRITE(dev, gpio_val);

    if (status == 0) {
        dev->module_format[module] = format;
    }

    return status;
}

int perform_format_deconfig(struct bladerf *dev, bladerf_module module)
{
    switch (module) {
        case BLADERF_MODULE_RX:
        case BLADERF_MODULE_TX:
            /* We'll reconfigure the HW when we call perform_format_config, so
             * we just need to update our stored information */
            dev->module_format[module] = -1;
            break;

        default:
            log_debug("%s: Invalid module: %d\n", __FUNCTION__, module);
            return BLADERF_ERR_INVAL;
    }

    return 0;
}
