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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "rel_assert.h"

#include "libbladeRF.h"     /* Public API */
#include "bladerf_priv.h"   /* Implementation-specific items ("private") */
#include "async.h"
#include "sync.h"
#include "tuning.h"
#include "gain.h"
#include "lms.h"
#include "band_select.h"
#include "xb.h"
#include "si5338.h"
#include "file_ops.h"
#include "log.h"
#include "backend/backend.h"
#include "device_identifier.h"
#include "version.h"       /* Generated at build time */
#include "conversions.h"
#include "dc_cal_table.h"
#include "config.h"
#include "version_compat.h"
#include "capabilities.h"
#include "fpga.h"
#include "flash_fields.h"
#include "backend/usb/usb.h"
#include "fx3_fw.h"

static int probe(backend_probe_target target_device,
                 struct bladerf_devinfo **devices)
{
    int ret;
    size_t num_devices;
    struct bladerf_devinfo *devices_local;
    int status;

    status = backend_probe(target_device, &devices_local, &num_devices);

    if (status < 0) {
        ret = status;
    } else {
        assert(num_devices <= INT_MAX);
        ret = (int)num_devices;
        *devices = devices_local;
    }

    return ret;
}

/*------------------------------------------------------------------------------
 * Device discovery & initialization/deinitialization
 *----------------------------------------------------------------------------*/

int bladerf_get_device_list(struct bladerf_devinfo **devices)
{
    return probe(BACKEND_PROBE_BLADERF, devices);
}

void bladerf_free_device_list(struct bladerf_devinfo *devices)
{
    /* Admittedly, we could just have the user call free() directly,
     * but this creates a 1:1 pair of calls, and this gives us a spot
     * to do any additional cleanup here, if ever needed in the future */
    free(devices);
}

int bladerf_open_with_devinfo(struct bladerf **opened_device,
                              struct bladerf_devinfo *devinfo)
{
    struct bladerf *dev;
    struct bladerf_devinfo any_device;
    int status;

    if (devinfo == NULL) {
        bladerf_init_devinfo(&any_device);
        devinfo = &any_device;
    }

    *opened_device = NULL;

    dev = (struct bladerf *)calloc(1, sizeof(struct bladerf));
    if (dev == NULL) {
        return BLADERF_ERR_MEM;
    }

    MUTEX_INIT(&dev->ctrl_lock);
    MUTEX_INIT(&dev->sync_lock[BLADERF_MODULE_RX]);
    MUTEX_INIT(&dev->sync_lock[BLADERF_MODULE_TX]);

    dev->fpga_version.describe = calloc(1, BLADERF_VERSION_STR_MAX + 1);
    if (dev->fpga_version.describe == NULL) {
        free(dev);
        return BLADERF_ERR_MEM;
    }

    dev->fw_version.describe = calloc(1, BLADERF_VERSION_STR_MAX + 1);
    if (dev->fw_version.describe == NULL) {
        free((void*)dev->fpga_version.describe);
        free(dev);
        return BLADERF_ERR_MEM;
    }

    dev->capabilities = 0;

    status = backend_open(dev, devinfo);
    if (status != 0) {
        free((void*)dev->fw_version.describe);
        free((void*)dev->fpga_version.describe);
        free(dev);
        return status;
    }

    status = dev->fn->get_device_speed(dev, &dev->usb_speed);
    if (status < 0) {
        log_debug("Failed to get device speed: %s\n",
                  bladerf_strerror(status));
        goto error;
    }

    switch (dev->usb_speed) {
        case BLADERF_DEVICE_SPEED_SUPER:
            dev->msg_size = USB_MSG_SIZE_SS;
            break;

        case BLADERF_DEVICE_SPEED_HIGH:
            dev->msg_size = USB_MSG_SIZE_HS;
            break;

        default:
            status = BLADERF_ERR_UNEXPECTED;
            log_error("Unsupported device speed: %d\n", dev->usb_speed);
            goto error;
    }

    /* Verify that we have a sufficent firmware version before continuing. */
    status = version_check_fw(dev);
    if (status != 0) {
#ifdef LOGGING_ENABLED
        if (status == BLADERF_ERR_UPDATE_FW) {
            struct bladerf_version req;
            const unsigned int dev_maj = dev->fw_version.major;
            const unsigned int dev_min = dev->fw_version.minor;
            const unsigned int dev_pat = dev->fw_version.patch;
            unsigned int req_maj, req_min, req_pat;

            version_required_fw(dev, &req, false);
            req_maj = req.major;
            req_min = req.minor;
            req_pat = req.patch;

            log_warning("Firmware v%u.%u.%u was detected. libbladeRF v%s "
                        "requires firmware v%u.%u.%u or later. An upgrade via "
                        "the bootloader is required.\n\n",
                        dev_maj, dev_min, dev_pat,
                        LIBBLADERF_VERSION,
                        req_maj, req_min, req_pat);
        }
#endif

        goto error;
    }

    /* VCTCXO trim and FPGA size are non-fatal indicators that we've
     * trashed the calibration region of flash. If these were made fatal,
     * we wouldn't be able to open the device to restore them. */
    status = get_and_cache_vctcxo_trim(dev);
    if (status < 0) {
        log_warning("Failed to get VCTCXO trim value: %s\n",
                    bladerf_strerror(status));
    }

    status = get_and_cache_fpga_size(dev);
    if (status < 0) {
        log_warning("Failed to get FPGA size %s\n",
                    bladerf_strerror(status));
    }

    dev->auto_filter[BLADERF_MODULE_RX] = -1;
    dev->auto_filter[BLADERF_MODULE_TX] = -1;

    dev->module_format[BLADERF_MODULE_RX] = -1;
    dev->module_format[BLADERF_MODULE_TX] = -1;

    /* This will be set in init_device() after we can determine which
     * methods the FPGA supports (based upon version number). */
    dev->tuning_mode = BLADERF_TUNING_MODE_INVALID;

    /* Load any available calibration tables so that the LMS DC register
     * configurations may be loaded in init_device */
    status = config_load_dc_cals(dev);
    if (status != 0) {
        goto error;
    }

    status = FPGA_IS_CONFIGURED(dev);
    if (status > 0) {
        /* If the FPGA version check fails, just warn, but don't error out.
         *
         * If an error code caused this function to bail out, it would prevent a
         * user from being able to unload and reflash a bitstream being
         * "autoloaded" from SPI flash. */
        fpga_check_version(dev);

        if (have_cap(dev, BLADERF_CAP_SCHEDULED_RETUNE)) {
            /* Cancel any pending re-tunes that may have been left over as the
             * result of a user application crashing or forgetting to call
             * bladerf_close() */
            status = tuning_cancel_scheduled(dev, BLADERF_MODULE_RX);
            if (status != 0) {
                log_warning("Failed to cancel any pending RX retunes: %s\n",
                        bladerf_strerror(status));
            }

            status = tuning_cancel_scheduled(dev, BLADERF_MODULE_TX);
            if (status != 0) {
                log_warning("Failed to cancel any pending TX retunes: %s\n",
                        bladerf_strerror(status));
            }
        }

        status = init_device(dev);
        if (status != 0) {
            goto error;
        }
    } else {
        /* Try searching for an FPGA in the config search path */
        status = config_load_fpga(dev);
    }

error:
    if (status < 0) {
        bladerf_close(dev);
    } else {
        *opened_device = dev;
    }

    return status;
}

/* dev path becomes device specifier string (osmosdr-like) */
int bladerf_open(struct bladerf **device, const char *dev_id)
{
    struct bladerf_devinfo devinfo;
    int status;

    *device = NULL;

    /* Populate dev-info from string */
    status = str2devinfo(dev_id, &devinfo);

    if (!status) {
        status = bladerf_open_with_devinfo(device, &devinfo);
    }

    return status;
}

void bladerf_close(struct bladerf *dev)
{
    int status;

    if (dev) {

        MUTEX_LOCK(&dev->ctrl_lock);
        sync_deinit(dev->sync[BLADERF_MODULE_RX]);
        sync_deinit(dev->sync[BLADERF_MODULE_TX]);

        status = FPGA_IS_CONFIGURED(dev);
        if (status == 1 && have_cap(dev, BLADERF_CAP_SCHEDULED_RETUNE)) {

            /* We cancel schedule retunes here to avoid the device retuning
             * underneath the user, should they open it again in the future.
             *
             * This is intended to help developers avoid a situation during
             * debugging where they schedule "far" into the future, but then
             * hit a case where their program abort or exit early. If we didn't
             * cancel these scheduled retunes, they could potentially be left
             * wondering why the device is starting up or "unexpectedly"
             * switching to a different frequency later.
             */
            tuning_cancel_scheduled(dev, BLADERF_MODULE_RX);
            tuning_cancel_scheduled(dev, BLADERF_MODULE_TX);
        }

        dev->fn->close(dev);

        free((void *)dev->fpga_version.describe);
        free((void *)dev->fw_version.describe);

        dc_cal_tbl_free(&dev->cal.dc_rx);
        dc_cal_tbl_free(&dev->cal.dc_tx);

        MUTEX_UNLOCK(&dev->ctrl_lock);
        free(dev);
    }
}

void bladerf_set_usb_reset_on_open(bool enabled)
{
#   if ENABLE_USB_DEV_RESET_ON_OPEN
    bladerf_usb_reset_device_on_open = enabled;

    log_verbose("USB reset on open %s\n", enabled ? "enabled" : "disabled");
#   else
    log_verbose("%s has no effect. "
                "ENABLE_USB_DEV_RESET_ON_OPEN not set at compile-time.\n",
                __FUNCTION__);
#   endif
}

int bladerf_enable_module(struct bladerf *dev,
                            bladerf_module m, bool enable)
{
    int status;

    if (m != BLADERF_MODULE_RX && m != BLADERF_MODULE_TX) {
        return BLADERF_ERR_INVAL;
    }

    log_debug("Enable Module: %s - %s\n",
                (m == BLADERF_MODULE_RX) ? "RX" : "TX",
                enable ? "True" : "False") ;

    MUTEX_LOCK(&dev->ctrl_lock);

    if (enable == false) {
        sync_deinit(dev->sync[m]);
        dev->sync[m] = NULL;
        perform_format_deconfig(dev, m);
    }

    lms_enable_rffe(dev, m, enable);
    status = dev->fn->enable_module(dev, m, enable);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_set_loopback(struct bladerf *dev, bladerf_loopback l)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    if (l == BLADERF_LB_FIRMWARE) {
        /* Firmware loopback was fully implemented in FW v1.7.1
         * (1.7.0 could enable it, but 1.7.1 also allowed readback,
         * so we'll enforce 1.7.1 here. */
        if (!have_cap(dev, BLADERF_CAP_FW_LOOPBACK)) {
            log_warning("Firmware v1.7.1 or later is required "
                        "to use firmware loopback.\n\n");
            status = BLADERF_ERR_UPDATE_FW;
            goto out;
        } else {
            /* Samples won't reach the LMS when the device is in firmware
             * loopback mode. By placing the LMS into a loopback mode, we ensure
             * that the PAs will be disabled, and remain enabled across
             * frequency changes.
             */
            status = lms_set_loopback_mode(dev, BLADERF_LB_RF_LNA3);
            if (status != 0) {
                goto out;
            }

            status = dev->fn->set_firmware_loopback(dev, true);
        }

    } else {

        /* If applicable, ensure FW loopback is disabled */
        if (have_cap(dev, BLADERF_CAP_FW_LOOPBACK)) {
            bool fw_lb_enabled = false;

            /* Query first, as the implementation of setting the mode
             * may interrupt running streams. The API don't guarantee that
             * switching loopback modes on the fly to work, but we can at least
             * try to avoid unnecessarily interrupting things...*/
            status = dev->fn->get_firmware_loopback(dev, &fw_lb_enabled);
            if (status != 0) {
                goto out;
            }

            if (fw_lb_enabled) {
                status = dev->fn->set_firmware_loopback(dev, false);
                if (status != 0) {
                    goto out;
                }
            }
        }

        status =  lms_set_loopback_mode(dev, l);
    }

out:
    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_get_loopback(struct bladerf *dev, bladerf_loopback *l)
{
    int status = BLADERF_ERR_UNEXPECTED;
    *l = BLADERF_LB_NONE;

    MUTEX_LOCK(&dev->ctrl_lock);

    if (have_cap(dev, BLADERF_CAP_FW_LOOPBACK)) {
        bool fw_lb_enabled;
        status = dev->fn->get_firmware_loopback(dev, &fw_lb_enabled);
        if (status == 0 && fw_lb_enabled) {
            *l = BLADERF_LB_FIRMWARE;
        }
    }

    if (*l == BLADERF_LB_NONE) {
        status = lms_get_loopback_mode(dev, l);
    }

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_set_rational_sample_rate(struct bladerf *dev, bladerf_module module,
                                     struct bladerf_rational_rate *rate,
                                     struct bladerf_rational_rate *actual)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = si5338_set_rational_sample_rate(dev, module, rate, actual);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_set_sample_rate(struct bladerf *dev, bladerf_module module,
                            uint32_t rate, uint32_t *actual)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = si5338_set_sample_rate(dev, module, rate, actual);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_get_rational_sample_rate(struct bladerf *dev, bladerf_module module,
                                     struct bladerf_rational_rate *rate)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = si5338_get_rational_sample_rate(dev, module, rate);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_get_sample_rate(struct bladerf *dev, bladerf_module module,
                            unsigned int *rate)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = si5338_get_sample_rate(dev, module, rate);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_get_sampling(struct bladerf *dev, bladerf_sampling *sampling)
{
    int status = 0;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = lms_get_sampling(dev, sampling);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_set_sampling(struct bladerf *dev, bladerf_sampling sampling)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = lms_select_sampling(dev, sampling);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_set_txvga2(struct bladerf *dev, int gain)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = lms_txvga2_set_gain(dev, gain);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_get_txvga2(struct bladerf *dev, int *gain)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = lms_txvga2_get_gain(dev, gain);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_set_txvga1(struct bladerf *dev, int gain)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = lms_txvga1_set_gain(dev, gain);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_get_txvga1(struct bladerf *dev, int *gain)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = lms_txvga1_get_gain(dev, gain);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}


int bladerf_set_lna_gain(struct bladerf *dev, bladerf_lna_gain gain)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = lms_lna_set_gain(dev, gain);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_get_lna_gain(struct bladerf *dev, bladerf_lna_gain *gain)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = lms_lna_get_gain(dev, gain);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_set_rxvga1(struct bladerf *dev, int gain)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = lms_rxvga1_set_gain(dev, gain);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_get_rxvga1(struct bladerf *dev, int *gain)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = lms_rxvga1_get_gain(dev, gain);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_set_rxvga2(struct bladerf *dev, int gain)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = lms_rxvga2_set_gain(dev, gain);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_get_rxvga2(struct bladerf *dev, int *gain)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = lms_rxvga2_get_gain(dev, gain);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}


int bladerf_set_gain(struct bladerf *dev, bladerf_module mod, int gain) {
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = gain_set(dev, mod, gain);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_set_bandwidth(struct bladerf *dev, bladerf_module module,
                          unsigned int bandwidth,
                          unsigned int *actual)
{
    int status;
    lms_bw bw;

    MUTEX_LOCK(&dev->ctrl_lock);

    if (bandwidth < BLADERF_BANDWIDTH_MIN) {
        bandwidth = BLADERF_BANDWIDTH_MIN;
        log_info("Clamping bandwidth to %dHz\n", bandwidth);
    } else if (bandwidth > BLADERF_BANDWIDTH_MAX) {
        bandwidth = BLADERF_BANDWIDTH_MAX;
        log_info("Clamping bandwidth to %dHz\n", bandwidth);
    }

    bw = lms_uint2bw(bandwidth);

    status = lms_lpf_enable(dev, module, true);
    if (status != 0) {
        goto out;
    }

    status = lms_set_bandwidth(dev, module, bw);
    if (actual != NULL) {
        if (status == 0) {
            *actual = lms_bw2uint(bw);
        } else {
            *actual = 0;
        }
    }

out:
    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_get_bandwidth(struct bladerf *dev, bladerf_module module,
                            unsigned int *bandwidth)
{
    int status;
    lms_bw bw;

    MUTEX_LOCK(&dev->ctrl_lock);

    status = lms_get_bandwidth( dev, module, &bw);

    if (status == 0) {
        *bandwidth = lms_bw2uint(bw);
    } else {
        *bandwidth = 0;
    }

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_set_lpf_mode(struct bladerf *dev, bladerf_module module,
                         bladerf_lpf_mode mode)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = lms_lpf_set_mode(dev, module, mode);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_get_lpf_mode(struct bladerf *dev, bladerf_module module,
                         bladerf_lpf_mode *mode)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = lms_lpf_get_mode(dev, module, mode);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_select_band(struct bladerf *dev, bladerf_module module,
                        unsigned int frequency)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = band_select(dev, module, frequency < BLADERF_BAND_HIGH);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_set_frequency(struct bladerf *dev,
                          bladerf_module module, unsigned int frequency)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = tuning_set_freq(dev, module, frequency);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}


int bladerf_schedule_retune(struct bladerf *dev,
                            bladerf_module module,
                            uint64_t timestamp,
                            unsigned int frequency,
                            struct bladerf_quick_tune *quick_tune)

{
    int status;
    struct lms_freq f;

    MUTEX_LOCK(&dev->ctrl_lock);

    if (!have_cap(dev, BLADERF_CAP_SCHEDULED_RETUNE)) {
        log_debug("This FPGA version (%u.%u.%u) does not support "
                  "scheduled retunes.\n",  dev->fpga_version.major,
                  dev->fpga_version.minor, dev->fpga_version.patch);

        status = BLADERF_ERR_UNSUPPORTED;
        goto out;
    }

    if (quick_tune == NULL) {
        status = lms_calculate_tuning_params(frequency, &f);
        if (status == 0) {
            status = tuning_schedule(dev, module, timestamp, &f);
        }
    } else {
        f.freqsel = quick_tune->freqsel;
        f.vcocap  = quick_tune->vcocap;
        f.nint    = quick_tune->nint;
        f.nfrac   = quick_tune->nfrac;
        f.flags   = quick_tune->flags;
        f.x       = 0;
        f.vcocap_result = 0;

        status = tuning_schedule(dev, module, timestamp, &f);
    }

out:
    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_cancel_scheduled_retunes(struct bladerf *dev, bladerf_module m)
{
    int status;

    MUTEX_LOCK(&dev->ctrl_lock);

    if (have_cap(dev, BLADERF_CAP_SCHEDULED_RETUNE)) {
        status = tuning_cancel_scheduled(dev, m);
    } else {
        log_debug("This FPGA version (%u.%u.%u) does not support "
                  "scheduled retunes.\n",  dev->fpga_version.major,
                  dev->fpga_version.minor, dev->fpga_version.patch);

        status = BLADERF_ERR_UNSUPPORTED;
    }

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_get_frequency(struct bladerf *dev,
                            bladerf_module module, unsigned int *frequency)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = tuning_get_freq(dev, module, frequency);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_get_quick_tune(struct bladerf *dev,
                           bladerf_module module,
                           struct bladerf_quick_tune *quick_tune)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = lms_get_quick_tune(dev, module, quick_tune);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_set_tuning_mode(struct bladerf *dev,
                            bladerf_tuning_mode mode)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = tuning_set_mode(dev, mode);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_set_stream_timeout(struct bladerf *dev, bladerf_module module,
                               unsigned int timeout) {

    if (dev) {
        MUTEX_LOCK(&dev->ctrl_lock);
        dev->transfer_timeout[module] = timeout;
        MUTEX_UNLOCK(&dev->ctrl_lock);
        return 0;

    } else {
        return BLADERF_ERR_INVAL;
    }
}

int bladerf_get_stream_timeout(struct bladerf *dev, bladerf_module module,
                               unsigned int *timeout) {
    if (dev) {
        MUTEX_LOCK(&dev->ctrl_lock);
        *timeout = dev->transfer_timeout[module];
        MUTEX_UNLOCK(&dev->ctrl_lock);
        return 0;
    } else {
        return BLADERF_ERR_INVAL;
    }
}

int bladerf_sync_config(struct bladerf *dev,
                        bladerf_module module,
                        bladerf_format format,
                        unsigned int num_buffers,
                        unsigned int buffer_size,
                        unsigned int num_transfers,
                        unsigned int stream_timeout)
{
    int status;

    MUTEX_LOCK(&dev->ctrl_lock);

    status = perform_format_config(dev, module, format);
    if (status == 0) {
        MUTEX_LOCK(&dev->sync_lock[module]);

        dev->transfer_timeout[module] = stream_timeout;

        status = sync_init(dev, module, format, num_buffers, buffer_size,
                num_transfers, stream_timeout);

        if (status != 0) {
            perform_format_deconfig(dev, module);
        }

        MUTEX_UNLOCK(&dev->sync_lock[module]);
    }

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_sync_tx(struct bladerf *dev,
                    void *samples, unsigned int num_samples,
                    struct bladerf_metadata *metadata,
                    unsigned int timeout_ms)
{
    int status;

    MUTEX_LOCK(&dev->sync_lock[BLADERF_MODULE_TX]);
    status = sync_tx(dev, samples, num_samples, metadata, timeout_ms);
    MUTEX_UNLOCK(&dev->sync_lock[BLADERF_MODULE_TX]);

    return status;
}

int bladerf_sync_rx(struct bladerf *dev,
                    void *samples, unsigned int num_samples,
                    struct bladerf_metadata *metadata,
                    unsigned int timeout_ms)
{
    int status;

    MUTEX_LOCK(&dev->sync_lock[BLADERF_MODULE_RX]);
    status = sync_rx(dev, samples, num_samples, metadata, timeout_ms);
    MUTEX_UNLOCK(&dev->sync_lock[BLADERF_MODULE_RX]);

    return status;
}

int bladerf_init_stream(struct bladerf_stream **stream,
                        struct bladerf *dev,
                        bladerf_stream_cb callback,
                        void ***buffers,
                        size_t num_buffers,
                        bladerf_format format,
                        size_t samples_per_buffer,
                        size_t num_transfers,
                        void *data)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = async_init_stream(stream, dev, callback, buffers, num_buffers,
                               format, samples_per_buffer, num_transfers, data);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_stream(struct bladerf_stream *stream, bladerf_module module)
{
    int stream_status, fmt_status;

    MUTEX_LOCK(&stream->dev->ctrl_lock);
    fmt_status = perform_format_config(stream->dev, module, stream->format);
    MUTEX_UNLOCK(&stream->dev->ctrl_lock);

    if (fmt_status != 0) {
        return fmt_status;
    }

    /* Reminder: as we're not holding the control lock, no control calls should
     * be made in asyn_run_stream down through the backend code */
    stream_status = async_run_stream(stream, module);

    MUTEX_LOCK(&stream->dev->ctrl_lock);
    fmt_status = perform_format_deconfig(stream->dev, module);
    MUTEX_UNLOCK(&stream->dev->ctrl_lock);

    return stream_status == 0 ? fmt_status : stream_status;
}

int bladerf_submit_stream_buffer(struct bladerf_stream *stream,
                                 void *buffer,
                                 unsigned int timeout_ms)
{
    return async_submit_stream_buffer(stream, buffer, timeout_ms);
}

void bladerf_deinit_stream(struct bladerf_stream *stream)
{
    if (stream && stream->dev) {
        struct bladerf *dev = stream->dev;
        MUTEX_LOCK(&dev->ctrl_lock);
        async_deinit_stream(stream);
        MUTEX_UNLOCK(&dev->ctrl_lock);
    }
}


/*------------------------------------------------------------------------------
 * Device Info
 *----------------------------------------------------------------------------*/

int bladerf_get_serial(struct bladerf *dev, char *serial)
{
    MUTEX_LOCK(&dev->ctrl_lock);
    strcpy(serial, dev->ident.serial);
    MUTEX_UNLOCK(&dev->ctrl_lock);
    return 0;
}

int bladerf_get_vctcxo_trim(struct bladerf *dev, uint16_t *trim)
{
    MUTEX_LOCK(&dev->ctrl_lock);
    *trim = dev->dac_trim;
    MUTEX_UNLOCK(&dev->ctrl_lock);
    return 0;
}

int bladerf_get_fpga_size(struct bladerf *dev, bladerf_fpga_size *size)
{
    MUTEX_LOCK(&dev->ctrl_lock);
    *size = dev->fpga_size;
    MUTEX_UNLOCK(&dev->ctrl_lock);
    return 0;
}

int bladerf_fw_version(struct bladerf *dev, struct bladerf_version *version)
{
    MUTEX_LOCK(&dev->ctrl_lock);
    memcpy(version, &dev->fw_version, sizeof(*version));
    MUTEX_UNLOCK(&dev->ctrl_lock);
    return 0;
}

int bladerf_is_fpga_configured(struct bladerf *dev)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = FPGA_IS_CONFIGURED(dev);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_fpga_version(struct bladerf *dev, struct bladerf_version *version)
{
    MUTEX_LOCK(&dev->ctrl_lock);
    memcpy(version, &dev->fpga_version, sizeof(*version));
    MUTEX_UNLOCK(&dev->ctrl_lock);
    return 0;
}

bladerf_dev_speed bladerf_device_speed(struct bladerf *dev)
{
    bladerf_dev_speed speed;
    MUTEX_LOCK(&dev->ctrl_lock);
    speed = dev->usb_speed;
    MUTEX_UNLOCK(&dev->ctrl_lock);
    return speed;
}

/*------------------------------------------------------------------------------
 * Device Programming
 *----------------------------------------------------------------------------*/

int bladerf_erase_flash(struct bladerf *dev,
                        uint32_t erase_block, uint32_t count)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = flash_erase(dev, erase_block, count);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_read_flash(struct bladerf *dev, uint8_t *buf,
                       uint32_t page, uint32_t count)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = flash_read(dev, buf, page, count);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_write_flash(struct bladerf *dev, const uint8_t *buf,
                        uint32_t page, uint32_t count)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = flash_write(dev, buf, page, count);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_device_reset(struct bladerf *dev)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = dev->fn->device_reset(dev);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_jump_to_bootloader(struct bladerf *dev)
{
    int status;

    if (!dev->fn->jump_to_bootloader) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    MUTEX_LOCK(&dev->ctrl_lock);
    status = dev->fn->jump_to_bootloader(dev);
    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

static inline bool valid_fw_size(size_t len)
{
    /* Simple FW applications generally are significantly larger than this */
    if (len < (50 * 1024)) {
        return false;
    } else if (len > BLADERF_FLASH_BYTE_LEN_FIRMWARE) {
        return false;
    } else {
        return true;
    }
}

int bladerf_flash_firmware(struct bladerf *dev, const char *firmware_file)
{
    int status;
    uint8_t *buf = NULL;
    size_t buf_size;
    const char env_override[] = "BLADERF_SKIP_FW_SIZE_CHECK";

    MUTEX_LOCK(&dev->ctrl_lock);

    status = file_read_buffer(firmware_file, &buf, &buf_size);
    if (status != 0) {
        goto out;
    }

    /* Sanity check firmware length.
     *
     * TODO in the future, better sanity checks can be performed when
     *      using the bladerf image format currently used to backup/restore
     *      calibration data
     */
    if (!getenv(env_override) && !valid_fw_size(buf_size)) {
        log_info("Detected potentially invalid firmware file.\n");
        log_info("Define BLADERF_SKIP_FW_SIZE_CHECK in your evironment "
                "to skip this check.\n");
        status = BLADERF_ERR_INVAL;
    } else {
        status = flash_write_fx3_fw(dev, &buf, buf_size);
    }

out:
    MUTEX_UNLOCK(&dev->ctrl_lock);
    free(buf);
    return status;
}

int bladerf_load_fpga(struct bladerf *dev, const char *fpga_file)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = fpga_load_from_file(dev, fpga_file);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}


int bladerf_flash_fpga(struct bladerf *dev, const char *fpga_file)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = fpga_write_to_flash(dev, fpga_file);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_erase_stored_fpga(struct bladerf *dev)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = flash_erase_fpga(dev);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}


/*------------------------------------------------------------------------------
 * Misc.
 *----------------------------------------------------------------------------*/

const char * bladerf_strerror(int error)
{
    switch (error) {
        case BLADERF_ERR_UNEXPECTED:
            return "An unexpected error occurred";
        case BLADERF_ERR_RANGE:
            return "Provided parameter was out of the allowable range";
        case BLADERF_ERR_INVAL:
            return "Invalid operation or parameter";
        case BLADERF_ERR_MEM:
            return "A memory allocation error occurred";
        case BLADERF_ERR_IO:
            return "File or device I/O failure";
        case BLADERF_ERR_TIMEOUT:
            return "Operation timed out";
        case BLADERF_ERR_NODEV:
            return "No devices available";
        case BLADERF_ERR_UNSUPPORTED:
            return "Operation not supported";
        case BLADERF_ERR_MISALIGNED:
            return "Misaligned flash access";
        case BLADERF_ERR_CHECKSUM:
            return "Invalid checksum";
        case BLADERF_ERR_NO_FILE:
            return "File not found";
        case BLADERF_ERR_UPDATE_FPGA:
            return "An FPGA update is required";
        case BLADERF_ERR_UPDATE_FW:
            return "A firmware update is required";
        case BLADERF_ERR_TIME_PAST:
            return "Requested timestamp is in the past";
        case BLADERF_ERR_QUEUE_FULL:
            return "Could not enqueue data into full queue";
        case BLADERF_ERR_FPGA_OP:
            return "An FPGA operation reported a failure";
        case 0:
            return "Success";
        default:
            return "Unknown error code";
    }
}

void bladerf_version(struct bladerf_version *version)
{
    version->major = LIBBLADERF_VERSION_MAJOR;
    version->minor = LIBBLADERF_VERSION_MINOR;
    version->patch = LIBBLADERF_VERSION_PATCH;
    version->describe = LIBBLADERF_VERSION;
}

void bladerf_log_set_verbosity(bladerf_log_level level)
{
    log_set_verbosity(level);
#if defined(LOG_SYSLOG_ENABLED)
    log_debug("Log verbosity has been set to: %d", level);
#endif
}

/*------------------------------------------------------------------------------
 * Device identifier information
 *----------------------------------------------------------------------------*/

void bladerf_init_devinfo(struct bladerf_devinfo *info)
{
    info->backend  = BLADERF_BACKEND_ANY;

    memset(info->serial, 0, BLADERF_SERIAL_LENGTH);
    strncpy(info->serial, DEVINFO_SERIAL_ANY, BLADERF_SERIAL_LENGTH - 1);

    info->usb_bus  = DEVINFO_BUS_ANY;
    info->usb_addr = DEVINFO_ADDR_ANY;
    info->instance = DEVINFO_INST_ANY;
}

int bladerf_get_devinfo(struct bladerf *dev, struct bladerf_devinfo *info)
{
    if (dev) {
        MUTEX_LOCK(&dev->ctrl_lock);
        memcpy(info, &dev->ident, sizeof(struct bladerf_devinfo));
        MUTEX_UNLOCK(&dev->ctrl_lock);
        return 0;
    } else {
        return BLADERF_ERR_INVAL;
    }
}

int bladerf_get_devinfo_from_str(const char *devstr,
                                 struct bladerf_devinfo *info)
{
    return str2devinfo(devstr, info);
}

bool bladerf_devinfo_matches(const struct bladerf_devinfo *a,
                             const struct bladerf_devinfo *b)
{
    return bladerf_instance_matches(a, b) &&
           bladerf_serial_matches(a, b)   &&
           bladerf_bus_addr_matches(a ,b);
}


bool bladerf_devstr_matches(const char *dev_str,
                            struct bladerf_devinfo *info)
{
    int status;
    bool ret;
    struct bladerf_devinfo from_str;

    status = str2devinfo(dev_str, &from_str);
    if (status < 0) {
        ret = false;
        log_debug("Failed to parse device string: %s\n",
                  bladerf_strerror(status));
    } else {
        ret = bladerf_devinfo_matches(&from_str, info);
    }

    return ret;
}

const char * bladerf_backend_str(bladerf_backend backend)
{
    return backend2str(backend);
}

/*------------------------------------------------------------------------------
 * Si5338 register read / write functions
 *----------------------------------------------------------------------------*/

int bladerf_si5338_read(struct bladerf *dev, uint8_t address, uint8_t *val)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = dev->fn->si5338_read(dev,address,val);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_si5338_write(struct bladerf *dev, uint8_t address, uint8_t val)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = dev->fn->si5338_write(dev,address,val);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

/*------------------------------------------------------------------------------
 * LMS register access and low-level functions
 *----------------------------------------------------------------------------*/

int bladerf_lms_read(struct bladerf *dev, uint8_t address, uint8_t *val)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = dev->fn->lms_read(dev,address,val);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_lms_write(struct bladerf *dev, uint8_t address, uint8_t val)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = dev->fn->lms_write(dev,address,val);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_lms_set_dc_cals(struct bladerf *dev,
                            const struct bladerf_lms_dc_cals *dc_cals)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = lms_set_dc_cals(dev, dc_cals);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_lms_get_dc_cals(struct bladerf *dev,
                            struct bladerf_lms_dc_cals *dc_cals)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = lms_get_dc_cals(dev, dc_cals);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

/*------------------------------------------------------------------------------
 * GPIO register read / write functions
 *----------------------------------------------------------------------------*/

int bladerf_config_gpio_read(struct bladerf *dev, uint32_t *val)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = CONFIG_GPIO_READ(dev, val);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_config_gpio_write(struct bladerf *dev, uint32_t val)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = CONFIG_GPIO_WRITE(dev, val);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;

}

/*------------------------------------------------------------------------------
 * Expansion board configuration
 *----------------------------------------------------------------------------*/
int bladerf_expansion_attach(struct bladerf *dev, bladerf_xb xb)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = xb_attach(dev, xb);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_expansion_get_attached(struct bladerf *dev, bladerf_xb *xb)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    switch (dev->xb) {
        case BLADERF_XB_NONE:
        case BLADERF_XB_100:
        case BLADERF_XB_200:
            status = 0;
            *xb = dev->xb;
            break;

        default:
            log_debug("Device handle contains invalid XB id: %d\n", dev->xb);
            status = BLADERF_ERR_UNEXPECTED;
    }

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_xb200_set_filterbank(struct bladerf *dev,
                                 bladerf_module mod,
                                 bladerf_xb200_filter filter)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = xb200_set_filterbank(dev, mod, filter);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_xb200_get_filterbank(struct bladerf *dev,
                                 bladerf_module module,
                                 bladerf_xb200_filter *filter)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = xb200_get_filterbank(dev, module, filter);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_xb200_set_path(struct bladerf *dev,
                           bladerf_module module,
                           bladerf_xb200_path path)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = xb200_set_path(dev, module, path);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_xb200_get_path(struct bladerf *dev,
                                     bladerf_module module,
                                     bladerf_xb200_path *path)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = xb200_get_path(dev, module, path);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}


/*------------------------------------------------------------------------------
 * Expansion board GPIO register read / write functions
 *----------------------------------------------------------------------------*/

int bladerf_expansion_gpio_read(struct bladerf *dev, uint32_t *val)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = XB_GPIO_READ(dev, val);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_expansion_gpio_write(struct bladerf *dev, uint32_t val)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = XB_GPIO_WRITE(dev, val);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

/*------------------------------------------------------------------------------
 * Expansion board GPIO direction register read / write functions
 *----------------------------------------------------------------------------*/

int bladerf_expansion_gpio_dir_read(struct bladerf *dev, uint32_t *val)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = XB_GPIO_DIR_READ(dev, val);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_expansion_gpio_dir_write(struct bladerf *dev, uint32_t val)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = XB_GPIO_DIR_WRITE(dev, val);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

/*------------------------------------------------------------------------------
 * IQ Calibration routines
 *----------------------------------------------------------------------------*/
int bladerf_set_correction(struct bladerf *dev, bladerf_module module,
                           bladerf_correction corr, int16_t value)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    switch (corr) {
        case BLADERF_CORR_FPGA_PHASE:
            status = dev->fn->set_iq_phase_correction(dev, module, value);
            break;

        case BLADERF_CORR_FPGA_GAIN:
            /* Gain correction requires than an offset be applied */
            value += (int16_t) 4096;
            status = dev->fn->set_iq_gain_correction(dev, module, value);
            break;

        case BLADERF_CORR_LMS_DCOFF_I:
            status = lms_set_dc_offset_i(dev, module, value);
            break;

        case BLADERF_CORR_LMS_DCOFF_Q:
            status = lms_set_dc_offset_q(dev, module, value);
            break;

        default:
            status = BLADERF_ERR_INVAL;
            log_debug("Invalid correction type: %d\n", corr);
            break;
    }

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_get_correction(struct bladerf *dev, bladerf_module module,
                           bladerf_correction corr, int16_t *value)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    switch (corr) {
        case BLADERF_CORR_FPGA_PHASE:
            status = dev->fn->get_iq_phase_correction(dev, module, value);
            break;

        case BLADERF_CORR_FPGA_GAIN:
            status = dev->fn->get_iq_gain_correction(dev, module, value);

            /* Undo the gain control offset */
            if (status == 0) {
                *value -= 4096;
            }
            break;

        case BLADERF_CORR_LMS_DCOFF_I:
            status = lms_get_dc_offset_i(dev, module, value);
            break;

        case BLADERF_CORR_LMS_DCOFF_Q:
            status = lms_get_dc_offset_q(dev, module, value);
            break;

        default:
            status = BLADERF_ERR_INVAL;
            log_debug("Invalid correction type: %d\n", corr);
            break;
    }
    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

/*------------------------------------------------------------------------------
 * Get current timestamp counter
 *----------------------------------------------------------------------------*/
int bladerf_get_timestamp(struct bladerf *dev, bladerf_module module, uint64_t *value)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = dev->fn->get_timestamp(dev,module,value);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

/*------------------------------------------------------------------------------
 * VCTCXO trim DAC access
 *----------------------------------------------------------------------------*/

int bladerf_dac_write(struct bladerf *dev, uint16_t val)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = VCTCXO_DAC_WRITE(dev, val);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

int bladerf_dac_read(struct bladerf *dev, uint16_t *val)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = VCTCXO_DAC_READ(dev, val);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

/*------------------------------------------------------------------------------
 * XB SPI register write
 *----------------------------------------------------------------------------*/

int bladerf_xb_spi_write(struct bladerf *dev, uint32_t val)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = XB_SPI_WRITE(dev, val);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

/*------------------------------------------------------------------------------
 * DC Calibration routines
 *----------------------------------------------------------------------------*/
int bladerf_calibrate_dc(struct bladerf *dev, bladerf_cal_module module)
{
    int status;
    MUTEX_LOCK(&dev->ctrl_lock);

    status = lms_calibrate_dc(dev, module);

    MUTEX_UNLOCK(&dev->ctrl_lock);
    return status;
}

/*------------------------------------------------------------------------------
 * Bootloader recovery
 *----------------------------------------------------------------------------*/

int bladerf_get_bootloader_list(struct bladerf_devinfo **devices)
{
    return probe(BACKEND_PROBE_FX3_BOOTLOADER, devices);
}

int bladerf_load_fw_from_bootloader(const char *device_identifier,
                                    bladerf_backend backend,
                                    uint8_t bus, uint8_t addr,
                                    const char *file)
{
    int status;
    struct fx3_firmware *fw = NULL;
    struct bladerf_devinfo devinfo;

    if (device_identifier == NULL) {
        bladerf_init_devinfo(&devinfo);
        devinfo.backend = backend;
        devinfo.usb_bus = bus;
        devinfo.usb_addr = addr;
    } else {
        status = str2devinfo(device_identifier, &devinfo);
        if (status != 0) {
            return status;
        }
    }

    status = fx3_fw_read(file, &fw);
    if (status != 0) {
        return status;
    }

    assert(fw != NULL);

    status = backend_load_fw_from_bootloader(devinfo.backend, devinfo.usb_bus,
                                             devinfo.usb_addr, fw);


    fx3_fw_deinit(fw);
    return status;
}
