/*
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

#include "libbladeRF.h"
#include "tuning.h"
#include "bladerf_priv.h"
#include "lms.h"
#include "xb.h"
#include "dc_cal_table.h"
#include "log.h"


int tuning_select_band(struct bladerf *dev, bladerf_module module,
                       unsigned int frequency)
{
    int status;
    uint32_t gpio;
    uint32_t band;

    if (frequency < BLADERF_FREQUENCY_MIN) {
        frequency = BLADERF_FREQUENCY_MIN;
        log_info("Clamping frequency to %uHz\n", frequency);
    } else if (frequency > BLADERF_FREQUENCY_MAX) {
        frequency = BLADERF_FREQUENCY_MAX;
        log_info("Clamping frequency to %uHz\n", frequency);
    }

    band = (frequency >= BLADERF_BAND_HIGH) ? 1 : 2;

    status = lms_select_band(dev, module, frequency);
    if (status != 0) {
        return status;
    }

    status = CONFIG_GPIO_READ(dev, &gpio);
    if (status != 0) {
        return status;
    }

    gpio &= ~(module == BLADERF_MODULE_TX ? (3 << 3) : (3 << 5));
    gpio |= (module == BLADERF_MODULE_TX ? (band << 3) : (band << 5));

    return CONFIG_GPIO_WRITE(dev, gpio);
}

int tuning_set_freq(struct bladerf *dev, bladerf_module module,
                    unsigned int frequency)
{
    int status;
    bladerf_xb attached;
    int16_t dc_i, dc_q;
    const struct dc_cal_tbl *dc_cal =
        (module == BLADERF_MODULE_RX) ? dev->cal.dc_rx : dev->cal.dc_tx;

    status = xb_get_attached(dev, &attached);
    if (status) {
        return status;
    }

    if (attached == BLADERF_XB_200) {

        if (frequency < BLADERF_FREQUENCY_MIN) {

            status = xb200_set_path(dev, module, BLADERF_XB200_MIX);
            if (status) {
                return status;
            }

            status = xb200_auto_filter_selection(dev, module, frequency);
            if (status) {
                return status;
            }

            frequency = 1248000000 - frequency;

        } else {
            status = xb200_set_path(dev, module, BLADERF_XB200_BYPASS);
            if (status)
                return status;
        }
    }

    status = lms_set_frequency(dev, module, frequency);
    if (status != 0) {
        return status;
    }

    status = tuning_select_band(dev, module, frequency);
    if (status != 0) {
        return status;
    }

    if (dc_cal != NULL) {
        dc_cal_tbl_vals(dc_cal, frequency, &dc_i, &dc_q);

        status = dev->fn->set_correction(dev, module,
                                         BLADERF_CORR_LMS_DCOFF_I, dc_i);
        if (status != 0) {
            return status;
        }

        status = dev->fn->set_correction(dev, module,
                                         BLADERF_CORR_LMS_DCOFF_Q, dc_q);
        if (status != 0) {
            return status;
        }

        log_verbose("Set %s DC offset cal (I, Q) to: (%d, %d)\n",
                    (module == BLADERF_MODULE_RX) ? "RX" : "TX", dc_i, dc_q);
    }

    return status;
}

int tuning_get_freq(struct bladerf *dev, bladerf_module module,
                    unsigned int *frequency)
{
    bladerf_xb attached;
    bladerf_xb200_path path;
    struct lms_freq f;
    int rv = 0;

    rv = lms_get_frequency( dev, module, &f );
    if (rv != 0) {
        return rv;
    }

    if( f.x == 0 ) {
        *frequency = 0 ;
        rv = BLADERF_ERR_INVAL;
    } else {
        *frequency = lms_frequency_to_hz(&f);
    }
    if (rv != 0) {
        return rv;
    }

    rv = xb_get_attached(dev, &attached);
    if (rv != 0) {
        return rv;
    }
    if (attached == BLADERF_XB_200) {
        rv = xb200_get_path(dev, module, &path);
        if (rv != 0) {
            return rv;
        }
        if (path == BLADERF_XB200_MIX) {
            *frequency = 1248000000 - *frequency;
        }
    }

    return rv;
}



