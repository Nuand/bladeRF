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
#include "lms.h"
#include "si5338.h"
#include "file_ops.h"
#include "log.h"
#include "backend/backend.h"
#include "device_identifier.h"
#include "version.h"       /* Generated at build time */
#include "conversions.h"
#include "dc_cal_table.h"

/*------------------------------------------------------------------------------
 * Device discovery & initialization/deinitialization
 *----------------------------------------------------------------------------*/

int bladerf_get_device_list(struct bladerf_devinfo **devices)
{
    int ret;
    size_t num_devices;
    struct bladerf_devinfo *devices_local;
    int status;

    status = backend_probe(&devices_local, &num_devices);

    if (status < 0) {
        ret = status;
    } else {
        assert(num_devices <= INT_MAX);
        ret = (int)num_devices;
        *devices = devices_local;
    }

    return ret;
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
    int status;

    *opened_device = NULL;

    dev = (struct bladerf *)calloc(1, sizeof(struct bladerf));
    if (dev == NULL) {
        return BLADERF_ERR_MEM;
    }

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

    if (dev->usb_speed != BLADERF_DEVICE_SPEED_HIGH &&
        dev->usb_speed != BLADERF_DEVICE_SPEED_SUPER) {
        log_debug("Unsupported device speed: %d\n", dev->usb_speed);
        goto error;
    }

    /* VCTCXO trim and FPGA size are non-fatal indicators that we've
     * trashed the calibration region of flash. If these were made fatal,
     * we wouldn't be able to open the device to restore them. */
    status = bladerf_get_and_cache_vctcxo_trim(dev);
    if (status < 0) {
        log_warning("Failed to get VCTCXO trim value: %s\n",
                    bladerf_strerror(status));
    }

    status = bladerf_get_and_cache_fpga_size(dev);
    if (status < 0) {
        log_warning("Failed to get FPGA size %s\n",
                    bladerf_strerror(status));
    }

    status = bladerf_is_fpga_configured(dev);
    if (status > 0) {
        status = bladerf_init_device(dev);
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
    if (dev) {

#ifdef ENABLE_LIBBLADERF_SYNC
        sync_deinit(dev->sync[BLADERF_MODULE_RX]);
        sync_deinit(dev->sync[BLADERF_MODULE_TX]);
#endif

        dev->fn->close(dev);

        free((void *)dev->fpga_version.describe);
        free((void *)dev->fw_version.describe);

        free(dev);
    }
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

#ifdef ENABLE_LIBBLADERF_SYNC
    if (enable == false) {
        sync_deinit(dev->sync[m]);
        dev->sync[m] = NULL;
    }
#endif

    lms_enable_rffe(dev, m, enable);
    status = dev->fn->enable_module(dev, m, enable);

    return status;
}

int bladerf_set_loopback(struct bladerf *dev, bladerf_loopback l)
{
    if (l == BLADERF_LB_FIRMWARE) {
        /* Firmware loopback was implemented in FW v1.7.0 */
        if (version_less_than(&dev->fw_version, 1, 7, 0)) {
            log_warning("Firmware v1.7.0 or later is required "
                        "for firmware loopback\n");
            return BLADERF_ERR_UNSUPPORTED;
        } else {
            return dev->fn->set_firmware_loopback(dev, true);
        }

    } else {
        return lms_set_loopback_mode(dev, l);
    }
}

int bladerf_get_loopback(struct bladerf *dev, bladerf_loopback *l)
{
    /* NOTE: Cannot detect firmware loopback mode yet */
    return lms_get_loopback_mode(dev, l);
}

int bladerf_set_rational_sample_rate(struct bladerf *dev, bladerf_module module, struct bladerf_rational_rate *rate, struct bladerf_rational_rate *actual)
{
    return si5338_set_rational_sample_rate(dev, module, rate, actual);
}

int bladerf_set_sample_rate(struct bladerf *dev, bladerf_module module, uint32_t rate, uint32_t *actual)
{
    return si5338_set_sample_rate(dev, module, rate, actual);
}

int bladerf_get_rational_sample_rate(struct bladerf *dev, bladerf_module module, struct bladerf_rational_rate *rate)
{
    return si5338_get_rational_sample_rate(dev, module, rate);
}

int bladerf_get_sample_rate(struct bladerf *dev, bladerf_module module, unsigned int *rate)
{
    return si5338_get_sample_rate(dev, module, rate);
}

int bladerf_get_sampling(struct bladerf *dev, bladerf_sampling *sampling)
{
    int status = 0, external = 0;
    uint8_t val = 0;

    status = bladerf_lms_read( dev, 0x09, &val );
    if (status) {
        log_warning( "Could not read state of ADC pin connectivity\n" );
        goto bladerf_get_sampling__done;
    }
    external = val&(1<<7) ? 1 : 0;

    status = bladerf_lms_read( dev, 0x64, &val );
    if (status) {
        log_warning( "Could not read RXVGA2 state\n" );
        goto bladerf_get_sampling__done;
    }
    external |= val&(1<<1) ? 0 : 2;

    switch(external) {
        case 0  : *sampling = BLADERF_SAMPLING_INTERNAL; break;
        case 3  : *sampling = BLADERF_SAMPLING_EXTERNAL; break;
        default : *sampling = BLADERF_SAMPLING_UNKNOWN; break;
    }

bladerf_get_sampling__done:
    return status;
}

int bladerf_set_sampling(struct bladerf *dev, bladerf_sampling sampling)
{
    return lms_select_sampling(dev, sampling);
}

int bladerf_set_txvga2(struct bladerf *dev, int gain)
{
    return lms_txvga2_set_gain(dev, gain);
}

int bladerf_get_txvga2(struct bladerf *dev, int *gain)
{
    return lms_txvga2_get_gain(dev, gain);
}

int bladerf_set_txvga1(struct bladerf *dev, int gain)
{
    return lms_txvga1_set_gain(dev, gain);
}

int bladerf_get_txvga1(struct bladerf *dev, int *gain)
{
    return lms_txvga1_get_gain(dev, gain);
}

int bladerf_set_lna_gain(struct bladerf *dev, bladerf_lna_gain gain)
{
    return lms_lna_set_gain(dev, gain);
}

int bladerf_get_lna_gain(struct bladerf *dev, bladerf_lna_gain *gain)
{
    return lms_lna_get_gain(dev, gain);
}

int bladerf_set_rxvga1(struct bladerf *dev, int gain)
{
    return lms_rxvga1_set_gain(dev, gain);
}

int bladerf_get_rxvga1(struct bladerf *dev, int *gain)
{
    return lms_rxvga1_get_gain(dev, gain);
}

int bladerf_set_rxvga2(struct bladerf *dev, int gain)
{
    return lms_rxvga2_set_gain(dev, gain);
}

int bladerf_get_rxvga2(struct bladerf *dev, int *gain)
{
    return lms_rxvga2_get_gain(dev, gain);
}

int bladerf_set_bandwidth(struct bladerf *dev, bladerf_module module,
                          unsigned int bandwidth,
                          unsigned int *actual)
{
    int status;
    lms_bw bw;

    if (bandwidth < BLADERF_BANDWIDTH_MIN) {
        bandwidth = BLADERF_BANDWIDTH_MIN;
        log_info("Clamping bandwidth to %d Hz\n", bandwidth);
    } else if (bandwidth > BLADERF_BANDWIDTH_MAX) {
        bandwidth = BLADERF_BANDWIDTH_MAX;
        log_info("Clamping bandwidth to %d Hz\n", bandwidth);
    }

    bw = lms_uint2bw(bandwidth);

    status = lms_lpf_enable(dev, module, true);
    if (status != 0) {
        return status;
    }

    status = lms_set_bandwidth(dev, module, bw);
    if (actual != NULL) {
        if (status == 0) {
            *actual = lms_bw2uint(bw);
        } else {
            *actual = 0;
        }
    }

    return status;
}

int bladerf_get_bandwidth(struct bladerf *dev, bladerf_module module,
                            unsigned int *bandwidth)
{
    lms_bw bw;
    const int status = lms_get_bandwidth( dev, module, &bw);

    if (status == 0) {
        *bandwidth = lms_bw2uint(bw);
    } else {
        *bandwidth = 0;
    }

    return status;
}

int bladerf_set_lpf_mode(struct bladerf *dev, bladerf_module module,
                         bladerf_lpf_mode mode)
{
    return lms_lpf_set_mode(dev, module, mode);
}

int bladerf_get_lpf_mode(struct bladerf *dev, bladerf_module module,
                         bladerf_lpf_mode *mode)
{
    return lms_lpf_get_mode(dev, module, mode);
}

int bladerf_select_band(struct bladerf *dev, bladerf_module module,
                        unsigned int frequency)
{
    int status;
    uint32_t gpio;
    uint32_t band;

    if (frequency < BLADERF_FREQUENCY_MIN) {
        frequency = BLADERF_FREQUENCY_MIN;
        log_info("Clamping frequency to %u\n", frequency);
    } else if (frequency > BLADERF_FREQUENCY_MAX) {
        frequency = BLADERF_FREQUENCY_MAX;
        log_info("Clamping frequency to %u\n", frequency);
    }

    band = (frequency >= BLADERF_BAND_HIGH) ? 1 : 2;

    status = lms_select_band(dev, module, frequency);
    if (status != 0) {
        return status;
    }

    status = bladerf_config_gpio_read(dev, &gpio);
    if (status != 0) {
        return status;
    }

    gpio &= ~(module == BLADERF_MODULE_TX ? (3 << 3) : (3 << 5));
    gpio |= (module == BLADERF_MODULE_TX ? (band << 3) : (band << 5));

    return bladerf_config_gpio_write(dev, gpio);
}

int bladerf_set_frequency(struct bladerf *dev,
                          bladerf_module module, unsigned int frequency)
{
    int status;
    bladerf_xb attached;
    int16_t dc_i, dc_q;
    const struct dc_cal_tbl *dc_cal =
        (module == BLADERF_MODULE_RX) ? dev->cal.dc_rx : dev->cal.dc_tx;

    status = bladerf_expansion_get_attached(dev, &attached);
    if (status)
        return status;
    if (attached == BLADERF_XB_200) {
        if (frequency < BLADERF_FREQUENCY_MIN) {
            status = bladerf_xb200_set_path(dev, module, BLADERF_XB200_MIX);
            if (status)
                return status;
            frequency = 1248000000 - frequency;
        } else {
            status = bladerf_xb200_set_path(dev, module, BLADERF_XB200_BYPASS);
            if (status)
                return status;
        }
    }

    status = lms_set_frequency(dev, module, frequency);
    if (status != 0) {
        return status;
    }

    status = bladerf_select_band(dev, module, frequency);
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

        log_verbose("Set %s DC I,Q to: %d, %d\n",
                    (module == BLADERF_MODULE_RX) ? "RX" : "TX", dc_i, dc_q);
    }

    return status;
}

int bladerf_get_frequency(struct bladerf *dev,
                            bladerf_module module, unsigned int *frequency)
{
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

    return rv;
}

int bladerf_set_stream_timeout(struct bladerf *dev, bladerf_module module,
                               unsigned int timeout) {
    if (dev) {
        dev->transfer_timeout[module] = timeout;
        return 0;
    } else {
        return BLADERF_ERR_INVAL;
    }
}

int bladerf_get_stream_timeout(struct bladerf *dev, bladerf_module module,
                               unsigned int *timeout) {
    if (dev) {
        *timeout = dev->transfer_timeout[module];
        return 0;
    } else {
        return BLADERF_ERR_INVAL;
    }
}

int CALL_CONV bladerf_sync_config(struct bladerf *dev,
                                  bladerf_module module,
                                  bladerf_format format,
                                  unsigned int num_buffers,
                                  unsigned int buffer_size,
                                  unsigned int num_transfers,
                                  unsigned int stream_timeout)
{
#ifdef ENABLE_LIBBLADERF_SYNC
    return sync_init(dev, module, format, num_buffers, buffer_size,
                     num_transfers, stream_timeout);
#else
    return BLADERF_ERR_UNSUPPORTED;
#endif
}

int bladerf_sync_tx(struct bladerf *dev,
                    void *samples, unsigned int num_samples,
                    struct bladerf_metadata *metadata,
                    unsigned int timeout_ms)
{
#ifdef ENABLE_LIBBLADERF_SYNC
    return sync_tx(dev, samples, num_samples, metadata, timeout_ms);
#else
    return BLADERF_ERR_UNSUPPORTED;
#endif
}

int bladerf_sync_rx(struct bladerf *dev,
                    void *samples, unsigned int num_samples,
                    struct bladerf_metadata *metadata,
                    unsigned int timeout_ms)
{
#ifdef ENABLE_LIBBLADERF_SYNC
    return sync_rx(dev, samples, num_samples, metadata, timeout_ms);
#else
    return BLADERF_ERR_UNSUPPORTED;
#endif
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
    return async_init_stream(stream, dev, callback, buffers, num_buffers,
                             format, samples_per_buffer, num_transfers, data);
}

/* Reminder: No device control calls may be made here */
int bladerf_stream(struct bladerf_stream *stream, bladerf_module module)
{
    return async_run_stream(stream, module);
}

int bladerf_submit_stream_buffer(struct bladerf_stream *stream,
                                 void *buffer,
                                 unsigned int timeout_ms)
{
    return async_submit_stream_buffer(stream, buffer, timeout_ms);
}

void bladerf_deinit_stream(struct bladerf_stream *stream)
{
    async_deinit_stream(stream);
}


/*------------------------------------------------------------------------------
 * Device Info
 *----------------------------------------------------------------------------*/

int bladerf_get_serial(struct bladerf *dev, char *serial)
{
    strcpy(serial, dev->ident.serial);
    return 0;
}

int bladerf_get_vctcxo_trim(struct bladerf *dev, uint16_t *trim)
{
    *trim = dev->dac_trim;
    return 0;
}

int bladerf_get_fpga_size(struct bladerf *dev, bladerf_fpga_size *size)
{
    *size = dev->fpga_size;
    return 0;
}

int bladerf_fw_version(struct bladerf *dev, struct bladerf_version *version)
{
    memcpy(version, &dev->fw_version, sizeof(*version));
    return 0;
}

int bladerf_is_fpga_configured(struct bladerf *dev)
{
    return dev->fn->is_fpga_configured(dev);
}

int bladerf_fpga_version(struct bladerf *dev, struct bladerf_version *version)
{
    memcpy(version, &dev->fpga_version, sizeof(*version));
    return 0;
}

bladerf_dev_speed bladerf_device_speed(struct bladerf *dev)
{
    return dev->usb_speed;
}

/*------------------------------------------------------------------------------
 * Device Programming
 *----------------------------------------------------------------------------*/

int bladerf_erase_flash(struct bladerf *dev,
                        uint32_t erase_block, uint32_t count)
{
    return flash_erase(dev, erase_block, count);
}

int bladerf_read_flash(struct bladerf *dev, uint8_t *buf,
                       uint32_t page, uint32_t count)
{
    return flash_read(dev, buf, page, count);
}

int bladerf_write_flash(struct bladerf *dev, const uint8_t *buf,
                        uint32_t page, uint32_t count)
{
    return flash_write(dev, buf, page, count);
}

int bladerf_device_reset(struct bladerf *dev)
{
    return dev->fn->device_reset(dev);
}

int bladerf_jump_to_bootloader(struct bladerf *dev)
{
    if (!dev->fn->jump_to_bootloader) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    return dev->fn->jump_to_bootloader(dev);
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
    uint8_t *buf;
    size_t buf_size;
    const char env_override[] = "BLADERF_SKIP_FW_SIZE_CHECK";

    status = file_read_buffer(firmware_file, &buf, &buf_size);
    if (status != 0) {
        return status;
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

    free(buf);
    return status;
}

static inline bool valid_fpga_size(size_t len)
{
    if (len < (1 * 1024 * 1024)) {
        return false;
    } else if (len > BLADERF_FLASH_BYTE_LEN_FPGA) {
        return false;
    } else {
        return true;
    }
}

int bladerf_load_fpga(struct bladerf *dev, const char *fpga_file)
{
    uint8_t *buf = NULL;
    size_t  buf_size;
    int status;

    /* TODO sanity check FPGA:
     *  - Check for x40 vs x115 and verify FPGA image size
     *  - Known header/footer on images?
     *  - Checksum/hash?
     */
    status = file_read_buffer(fpga_file, &buf, &buf_size);
    if (status != 0) {
        goto error;
    }

    if (!valid_fpga_size(buf_size)) {
        status = BLADERF_ERR_INVAL;
        goto error;
    }

    status = dev->fn->load_fpga(dev, buf, buf_size);
    if (status == 0) {
        status = bladerf_init_device(dev);
    }

error:
    free(buf);
    return status;
}


int bladerf_flash_fpga(struct bladerf *dev, const char *fpga_file)
{
    int status;
    uint8_t *buf = NULL;
    size_t buf_size;
    const char env_override[] = "BLADERF_SKIP_FPGA_SIZE_CHECK";

    status = file_read_buffer(fpga_file, &buf, &buf_size);
    if (status == 0) {
        if (!getenv(env_override) && !valid_fpga_size(buf_size)) {
            log_info("Detected potentially invalid firmware file.\n");
            log_info("Define BLADERF_SKIP_FPGA_SIZE_CHECK in your evironment "
                       "to skip this check.\n");
            status = BLADERF_ERR_INVAL;
        } else {
            status = flash_write_fpga_bitstream(dev, &buf, buf_size);
        }
    }

    free(buf);
    return status;
}

int bladerf_erase_stored_fpga(struct bladerf *dev)
{
    return flash_erase_fpga(dev);
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
        memcpy(info, &dev->ident, sizeof(struct bladerf_devinfo));
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
    return dev->fn->si5338_read(dev,address,val);
}

int bladerf_si5338_write(struct bladerf *dev, uint8_t address, uint8_t val)
{
    return dev->fn->si5338_write(dev,address,val);
}

/*------------------------------------------------------------------------------
 * LMS register read / write functions
 *----------------------------------------------------------------------------*/

int bladerf_lms_read(struct bladerf *dev, uint8_t address, uint8_t *val)
{
    return dev->fn->lms_read(dev,address,val);
}

int bladerf_lms_write(struct bladerf *dev, uint8_t address, uint8_t val)
{
    return dev->fn->lms_write(dev,address,val);
}

/*------------------------------------------------------------------------------
 * GPIO register read / write functions
 *----------------------------------------------------------------------------*/

int bladerf_config_gpio_read(struct bladerf *dev, uint32_t *val)
{
    return dev->fn->config_gpio_read(dev,val);
}

int bladerf_config_gpio_write(struct bladerf *dev, uint32_t val)
{
    /* If we're connected at HS, we need to use smaller DMA transfers */
    if (dev->usb_speed == BLADERF_DEVICE_SPEED_HIGH   ) {
        val |= BLADERF_GPIO_FEATURE_SMALL_DMA_XFER;
    } else if (dev->usb_speed == BLADERF_DEVICE_SPEED_SUPER) {
        val &= ~BLADERF_GPIO_FEATURE_SMALL_DMA_XFER;
    } else {
        log_warning("Encountered unknown USB speed in %s\n", __FUNCTION__);
        return BLADERF_ERR_UNEXPECTED;
    }

    return dev->fn->config_gpio_write(dev,val);

}

/*------------------------------------------------------------------------------
 * Expansion board GPIO register read / write functions
 *----------------------------------------------------------------------------*/

int bladerf_expansion_gpio_read(struct bladerf *dev, uint32_t *val)
{
    return dev->fn->expansion_gpio_read(dev,val);
}

int bladerf_expansion_gpio_write(struct bladerf *dev, uint32_t val)
{
    return dev->fn->expansion_gpio_write(dev,val);
}

/*------------------------------------------------------------------------------
 * Expansion board GPIO direction register read / write functions
 *----------------------------------------------------------------------------*/

int bladerf_expansion_gpio_dir_read(struct bladerf *dev, uint32_t *val)
{
    return dev->fn->expansion_gpio_dir_read(dev,val);
}

int bladerf_expansion_gpio_dir_write(struct bladerf *dev, uint32_t val)
{
    return dev->fn->expansion_gpio_dir_write(dev,val);
}

/*------------------------------------------------------------------------------
 * IQ Calibration routines
 *----------------------------------------------------------------------------*/
int bladerf_set_correction(struct bladerf *dev, bladerf_module module,
                           bladerf_correction corr, int16_t value)
{
    return dev->fn->set_correction(dev, module, corr, value);
}

int bladerf_get_correction(struct bladerf *dev, bladerf_module module,
                           bladerf_correction corr, int16_t *value)
{
    return dev->fn->get_correction(dev, module, corr, value);
}

/*------------------------------------------------------------------------------
 * Get current timestamp counter
 *----------------------------------------------------------------------------*/
int bladerf_get_timestamp(struct bladerf *dev, bladerf_module module, uint64_t *value)
{
    return dev->fn->get_timestamp(dev,module,value);
}

/*------------------------------------------------------------------------------
 * VCTCXO DAC register write
 *----------------------------------------------------------------------------*/

int bladerf_dac_write(struct bladerf *dev, uint16_t val)
{
    return dev->fn->dac_write(dev,val);
}


/*------------------------------------------------------------------------------
 * XB SPI register write
 *----------------------------------------------------------------------------*/

int bladerf_xb_spi_write(struct bladerf *dev, uint32_t val)
{
    return dev->fn->xb_spi(dev,val);
}

/*------------------------------------------------------------------------------
 * DC Calibration routines
 *----------------------------------------------------------------------------*/
 int bladerf_calibrate_dc(struct bladerf *dev, bladerf_cal_module module)
 {
    return lms_calibrate_dc(dev, module);
 }
