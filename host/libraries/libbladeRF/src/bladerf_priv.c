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
#include "capabilities.h"

static inline int apply_lms_dc_cals(struct bladerf *dev)
{
    int status = 0;
    struct bladerf_lms_dc_cals cals;
    const bool have_rx = BLADERF_HAS_RX_DC_CAL(dev);
    const bool have_tx = BLADERF_HAS_TX_DC_CAL(dev);

    cals.lpf_tuning = -1;
    cals.tx_lpf_i   = -1;
    cals.tx_lpf_q   = -1;
    cals.rx_lpf_i   = -1;
    cals.rx_lpf_q   = -1;
    cals.dc_ref     = -1;
    cals.rxvga2a_i  = -1;
    cals.rxvga2a_q  = -1;
    cals.rxvga2b_i  = -1;
    cals.rxvga2b_q  = -1;

    if (have_rx) {
        const struct bladerf_lms_dc_cals *reg_vals = &dev->cal.dc_rx->reg_vals;

        cals.lpf_tuning = reg_vals->lpf_tuning;
        cals.rx_lpf_i   = reg_vals->rx_lpf_i;
        cals.rx_lpf_q   = reg_vals->rx_lpf_q;
        cals.dc_ref     = reg_vals->dc_ref;
        cals.rxvga2a_i  = reg_vals->rxvga2a_i;
        cals.rxvga2a_q  = reg_vals->rxvga2a_q;
        cals.rxvga2b_i  = reg_vals->rxvga2b_i;
        cals.rxvga2b_q  = reg_vals->rxvga2b_q;

        log_verbose("Fetched register values from RX DC cal table.\n");
    }

    if (have_tx) {
        const struct bladerf_lms_dc_cals *reg_vals = &dev->cal.dc_tx->reg_vals;

        cals.tx_lpf_i = reg_vals->tx_lpf_i;
        cals.tx_lpf_q = reg_vals->tx_lpf_q;

        if (have_rx) {
            if (cals.lpf_tuning != reg_vals->lpf_tuning) {
                log_warning("LPF tuning mismatch in tables. "
                            "RX=0x%04x, TX=0x%04x",
                            cals.lpf_tuning, reg_vals->lpf_tuning);
            }
        } else {
            /* Have TX cal but no RX cal -- use the RX values that came along
             * for the ride when the TX table was generated */
            cals.rx_lpf_i   = reg_vals->rx_lpf_i;
            cals.rx_lpf_q   = reg_vals->rx_lpf_q;
            cals.dc_ref     = reg_vals->dc_ref;
            cals.rxvga2a_i  = reg_vals->rxvga2a_i;
            cals.rxvga2a_q  = reg_vals->rxvga2a_q;
            cals.rxvga2b_i  = reg_vals->rxvga2b_i;
            cals.rxvga2b_q  = reg_vals->rxvga2b_q;
        }

        log_verbose("Fetched register values from TX DC cal table.\n");
    }

    /* No TX table was loaded, so load LMS TX register cals from the RX table,
     * if available */
    if (have_rx && !have_tx) {
        const struct bladerf_lms_dc_cals *reg_vals = &dev->cal.dc_rx->reg_vals;

        cals.tx_lpf_i   = reg_vals->tx_lpf_i;
        cals.tx_lpf_q   = reg_vals->tx_lpf_q;
    }

    if (have_rx || have_tx) {
        status = lms_set_dc_cals(dev, &cals);

        /* Force a re-tune so that we can apply the appropriate I/Q DC offset
         * values from our calibration table */
        if (status == 0) {
            int rx_status = 0;
            int tx_status = 0;

            if (have_rx) {
                unsigned int rx_f;
                rx_status = tuning_get_freq(dev, BLADERF_MODULE_RX, &rx_f);
                if (rx_status == 0) {
                    rx_status = tuning_set_freq(dev, BLADERF_MODULE_RX, rx_f);
                }
            }

            if (have_tx) {
                unsigned int rx_f;
                rx_status = tuning_get_freq(dev, BLADERF_MODULE_RX, &rx_f);
                if (rx_status == 0) {
                    rx_status = tuning_set_freq(dev, BLADERF_MODULE_RX, rx_f);
                }
            }

            /* Report the first of any failures */
            status = (rx_status == 0) ? tx_status : rx_status;
        }
    }

    return status;
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

        /* Power down DC calibration comparators until they are need, as they
         * have been shown to introduce undesirable artifacts into our signals.
         * (This is documented in the LMS6 FAQ). */

        status = lms_set(dev, 0x3f, 0x80);  /* TX LPF DC cal comparator */
        if (status != 0) {
            return status;
        }

        status = lms_set(dev, 0x5f, 0x80);  /* RX LPF DC cal comparator */
        if (status != 0) {
            return status;
        }

        status = lms_set(dev, 0x6e, 0xc0);  /* RXVGA2A/B DC cal comparators */
        if (status != 0) {
            return status;
        }

        /* Configure charge pump current offsets */
        status = lms_config_charge_pumps(dev, BLADERF_MODULE_TX);
        if (status != 0) {
            return status;
        }

        status = lms_config_charge_pumps(dev, BLADERF_MODULE_RX);
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

        dev->tuning_mode = tuning_get_default_mode(dev);

        status = tuning_set_freq(dev, BLADERF_MODULE_TX, 2447000000U);
        if (status != 0) {
            return status;
        }

        status = tuning_set_freq(dev, BLADERF_MODULE_RX, 2484000000U);
        if (status != 0) {
            return status;
        }

        /* Set the calibrated VCTCXO DAC value */
        status = VCTCXO_DAC_WRITE(dev, dev->dac_trim);
        if (status != 0) {
            return status;
        }
    } else {
        dev->tuning_mode = tuning_get_default_mode(dev);
    }

    /* Check if we have an expansion board attached */
    status = xb_get_attached(dev, &dev->xb);
    if (status != 0) {
        return status;
    }

    /* Set up LMS DC offset register calibration and initial IQ settings,
     * if any tables have been loaded already.
     *
     * This is done every time the device is opened (with an FPGA loaded),
     * as the user may change/update DC calibration tables without reloading the
     * FPGA.
     */
    status = apply_lms_dc_cals(dev);

    return status;
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

    if (use_timestamps && !have_cap(dev, BLADERF_CAP_TIMESTAMPS)) {
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

int check_module(bladerf_module m)
{
    int status;

    switch (m) {
        case BLADERF_MODULE_RX:
        case BLADERF_MODULE_TX:
            status = 0;
            break;

        default:
            log_debug("Invalid module: %d\n", m);
            status = BLADERF_ERR_INVAL;
    }

    return status;
}

int check_xb200_filter(bladerf_xb200_filter f)
{
    int status;

    switch (f) {
        case BLADERF_XB200_50M:
        case BLADERF_XB200_144M:
        case BLADERF_XB200_222M:
        case BLADERF_XB200_CUSTOM:
        case BLADERF_XB200_AUTO_3DB:
        case BLADERF_XB200_AUTO_1DB:
            status = 0;
            break;

        default:
            log_debug("Invalid XB200 filter: %d\n", f);
            status = BLADERF_ERR_INVAL;
            break;
    }

    return status;
}

int check_xb200_path(bladerf_xb200_path p)
{
    int status;

    switch (p) {
        case BLADERF_XB200_BYPASS:
        case BLADERF_XB200_MIX:
            status = 0;
            break;

        default:
            status = BLADERF_ERR_INVAL;
            log_debug("Invalid XB200 path: %d\n", p);
            break;
    }

    return status;
}
