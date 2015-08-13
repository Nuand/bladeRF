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
#include "band_select.h"
#include "xb.h"
#include "dc_cal_table.h"
#include "log.h"
#include "capabilities.h"

static bool fpga_supports_tuning_mode(struct bladerf *dev,
                                      bladerf_tuning_mode mode)
{
    switch (mode) {
        case BLADERF_TUNING_MODE_HOST:
            return true;

        case BLADERF_TUNING_MODE_FPGA:
            return have_cap(dev, BLADERF_CAP_FPGA_TUNING);

        default:
            return false;
    }
}

bladerf_tuning_mode tuning_get_default_mode(struct bladerf *dev)
{
    bladerf_tuning_mode mode = BLADERF_TUNING_MODE_INVALID;
    const char *str = getenv("BLADERF_DEFAULT_TUNING_MODE");

    if (str != NULL) {
        if (!strcasecmp("host", str)) {
            mode = BLADERF_TUNING_MODE_HOST;
        } else if (!strcasecmp("fpga", str)) {
            mode = BLADERF_TUNING_MODE_FPGA;

            /* Just a friendly reminder... */
            if (!fpga_supports_tuning_mode(dev, mode)) {
                log_warning("The loaded FPGA version (%u.%u.%u) does not "
                            "support the tuning mode being used to override "
                            "the default.\n",
                            dev->fpga_version.major, dev->fpga_version.minor,
                            dev->fpga_version.patch);
            }
        } else {
            log_debug("Invalid tuning mode override: %s\n", str);
        }
    }

    if (mode == BLADERF_TUNING_MODE_INVALID) {
        if (fpga_supports_tuning_mode(dev, BLADERF_TUNING_MODE_FPGA)) {
            /* Defaulting to host tuning mode until issue #417 is resolved
             *
             * mode = BLADERF_TUNING_MODE_FPGA;
             */
            mode = BLADERF_TUNING_MODE_HOST;
        } else {
            mode = BLADERF_TUNING_MODE_HOST;
        }
    }

    switch (mode) {
        case BLADERF_TUNING_MODE_HOST:
            log_debug("Default tuning mode: host\n");
            break;

        case BLADERF_TUNING_MODE_FPGA:
            log_debug("Default tuning mode: FPGA\n");
            break;

        default:
            assert(!"Bug encountered.");
            mode = BLADERF_TUNING_MODE_HOST;
    }

    return mode;
}

int tuning_set_mode(struct bladerf *dev, bladerf_tuning_mode mode)
{
    int status = 0;

    if (fpga_supports_tuning_mode(dev, mode)) {
        dev->tuning_mode = mode;
    } else {
        status = BLADERF_ERR_UNSUPPORTED;

        log_debug("The loaded FPGA version (%u.%u.%u) does not support the "
                  "provided tuning mode (%d)\n",
                  dev->fpga_version.major, dev->fpga_version.minor,
                  dev->fpga_version.patch, mode);
    }

    switch (dev->tuning_mode) {
        case BLADERF_TUNING_MODE_HOST:
            log_debug("Tuning mode: host\n");
            break;

        case BLADERF_TUNING_MODE_FPGA:
            log_debug("Tuning mode: FPGA\n");
            break;

        default:
            assert(!"Bug encountered.");
            status = BLADERF_ERR_INVAL;
    }

    return status;
}

int tuning_set_freq(struct bladerf *dev, bladerf_module module,
                    unsigned int frequency)
{
    int status;
    const bladerf_xb attached = dev->xb;
    int16_t dc_i, dc_q;
    const struct dc_cal_tbl *dc_cal =
        (module == BLADERF_MODULE_RX) ? dev->cal.dc_rx : dev->cal.dc_tx;

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

    switch (dev->tuning_mode) {
        case BLADERF_TUNING_MODE_HOST:
            status = lms_set_frequency(dev, module, frequency);
            if (status != 0) {
                return status;
            }

            status = band_select(dev, module, frequency < BLADERF_BAND_HIGH);
            break;

        case BLADERF_TUNING_MODE_FPGA: {
            struct lms_freq f;

            status = lms_calculate_tuning_params(frequency, &f);
            if (status == 0) {
                /* The band selection will occur in the NIOS II */
                status = tuning_schedule(dev, module, BLADERF_RETUNE_NOW, &f);
            }
            break;
        }

        default:
            log_debug("Invalid tuning mode: %d\n", dev->tuning_mode);
            status = BLADERF_ERR_INVAL;
            break;
    }

    if (status != 0) {
        return status;
    }

    if (dc_cal != NULL) {
        dc_cal_tbl_vals(dc_cal, frequency, &dc_i, &dc_q);

        status = lms_set_dc_offset_i(dev, module, dc_i);
        if (status != 0) {
            return status;
        }

        status = lms_set_dc_offset_q(dev, module, dc_q);
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
        /* If we see this, it's most often an indication that communication
         * with the LMS6002D is not occuring correctly */
        *frequency = 0 ;
        rv = BLADERF_ERR_IO;
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
