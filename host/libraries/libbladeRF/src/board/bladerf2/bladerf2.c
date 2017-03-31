/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2017 Nuand LLC
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

#include <string.h>
#include <errno.h>

#include <libbladeRF.h>

#include "log.h"
#include "conversions.h"
#define LOGGER_ID_STRING
#include "logger_id.h"
#include "logger_entry.h"
#include "bladeRF.h"

#include "board/board.h"
#include "capabilities.h"
#include "compatibility.h"

#include "driver/thirdparty/adi/ad9361_api.h"
#include "driver/spi_flash.h"
#include "driver/fpga_trigger.h"
#include "driver/fx3_fw.h"

#include "backend/usb/usb.h"
#include "backend/backend_config.h"

#include "streaming/async.h"
#include "streaming/sync.h"

#include "devinfo.h"
#include "helpers/version.h"
#include "helpers/file.h"
#include "version.h"

/******************************************************************************
 *                          bladeRF2 board state                              *
 ******************************************************************************/

struct bladerf2_board_data {
    /* AD9361 PHY Handle */
    struct ad9361_rf_phy *phy;

    /* Bitmask of capabilities determined by version numbers */
    uint64_t capabilities;

    /* Board properties */
    bladerf_fpga_size fpga_size;
    /* Data message size */
    size_t msg_size;

    /* Version information */
    struct bladerf_version fpga_version;
    struct bladerf_version fw_version;
    char fpga_version_str[BLADERF_VERSION_STR_MAX+1];
    char fw_version_str[BLADERF_VERSION_STR_MAX+1];
};

/******************************************************************************/
/* Helpers */
/******************************************************************************/

static int errno_ad9361_to_bladerf(int err)
{
    switch (err) {
        case EIO: return BLADERF_ERR_IO;
        case EAGAIN: return BLADERF_ERR_WOULD_BLOCK;
        case ENOMEM: return BLADERF_ERR_MEM;
        case EFAULT: return BLADERF_ERR_UNEXPECTED;
        case ENODEV: return BLADERF_ERR_NODEV;
        case EINVAL: return BLADERF_ERR_INVAL;
        case ETIMEDOUT: return BLADERF_ERR_TIMEOUT;
    }

    return BLADERF_ERR_UNEXPECTED;
}

/* FIXME move to libbladeRF.h */
#define BLADERF_TX (1<<0)

/******************************************************************************/
/* Low-level Initialization */
/******************************************************************************/

extern AD9361_InitParam ad9361_init_params;
extern AD9361_RXFIRConfig ad9361_init_rx_fir_config;
extern AD9361_TXFIRConfig ad9361_init_tx_fir_config;

static int bladerf2_initialize(struct bladerf *dev)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    int status;

    status = ad9361_init(&board_data->phy, &ad9361_init_params, dev);
    if (status < 0) {
        log_error("AD9361 initialization error: %d\n", status);
        return errno_ad9361_to_bladerf(status);
    }

    status = ad9361_set_tx_fir_config(board_data->phy, ad9361_init_tx_fir_config);
    if (status < 0) {
        return errno_ad9361_to_bladerf(status);
    }

    status = ad9361_set_rx_fir_config(board_data->phy, ad9361_init_rx_fir_config);
    if (status < 0) {
        return errno_ad9361_to_bladerf(status);
    }

    return 0;
}

/******************************************************************************
 *                        Generic Board Functions                             *
 ******************************************************************************/

/******************************************************************************/
/* Matches */
/******************************************************************************/

static bool bladerf2_matches(struct bladerf *dev)
{
    struct bladerf_usb *usb;
    uint16_t vid, pid;
    int status;

    if (strcmp(dev->backend->name, "usb") != 0)
        return false;

    usb = dev->backend_data;

    status = usb->fn->get_vid_pid(usb->driver, &vid, &pid);
    if (status < 0)
        return false;

    if (vid == USB_NUAND_VENDOR_ID && pid == USB_NUAND_BLADERF2_PRODUCT_ID)
        return true;

    return false;
}

/******************************************************************************/
/* Open/close */
/******************************************************************************/

static int bladerf2_open(struct bladerf *dev, struct bladerf_devinfo *devinfo)
{
    struct bladerf2_board_data *board_data;
    struct bladerf_version required_fw_version;
    struct bladerf_version required_fpga_version;
    char *full_path;
    bladerf_dev_speed usb_speed;
    const unsigned int max_retries = 30;
    unsigned int i;
    int ready;
    int status = 0;

    /* Allocate board data */
    board_data = calloc(1, sizeof(struct bladerf2_board_data));
    if (board_data == NULL) {
        return BLADERF_ERR_MEM;
    }
    dev->board_data = board_data;

    /* Initialize board data */
    board_data->fpga_version.describe = board_data->fpga_version_str;
    board_data->fw_version.describe = board_data->fw_version_str;

    /* Read firmware version */
    status = dev->backend->get_fw_version(dev, &board_data->fw_version);
    if (status < 0) {
        log_debug("Failed to get FW version: %s\n",
                  bladerf_strerror(status));
        return status;
    }
    log_verbose("Read Firmware version: %s\n", board_data->fw_version.describe);

    /* Determine firmware capabilities */
    board_data->capabilities |= bladerf2_get_fw_capabilities(&board_data->fw_version);
    log_verbose("Capability mask before FPGA load: 0x%016"PRIx64"\n",
                 board_data->capabilities);

    /* Wait until firmware is ready */
    for (i = 0; i < max_retries; i++) {
        ready = dev->backend->is_fw_ready(dev);
        if (ready != 1) {
            if (i == 0) {
                log_info("Waiting for device to become ready...\n");
            } else {
                log_debug("Retry %02u/%02u.\n", i + 1, max_retries);
            }
            usleep(1000000);
        } else {
            break;
        }
    }
    if (i >= max_retries) {
        log_debug("Timed out while waiting for device.\n");
        return BLADERF_ERR_TIMEOUT;
    }

    /* Determine data message size */
    status = dev->backend->get_device_speed(dev, &usb_speed);
    if (status < 0) {
        log_debug("Failed to get device speed: %s\n",
                  bladerf_strerror(status));
        return status;
    }
    switch (usb_speed) {
        case BLADERF_DEVICE_SPEED_SUPER:
            board_data->msg_size = USB_MSG_SIZE_SS;
            break;
        case BLADERF_DEVICE_SPEED_HIGH:
            board_data->msg_size = USB_MSG_SIZE_HS;
            break;
        default:
            log_error("Unsupported device speed: %d\n", usb_speed);
            return BLADERF_ERR_UNEXPECTED;
    }

    /* Verify that we have a sufficent firmware version before continuing. */
    status = version_check_fw(&bladerf2_fw_compat_table,
                              &board_data->fw_version, &required_fw_version);
    if (status != 0) {
#ifdef LOGGING_ENABLED
        if (status == BLADERF_ERR_UPDATE_FW) {
            log_warning("Firmware v%u.%u.%u was detected. libbladeRF v%s "
                        "requires firmware v%u.%u.%u or later. An upgrade via "
                        "the bootloader is required.\n\n",
                        &board_data->fw_version.major,
                        &board_data->fw_version.minor,
                        &board_data->fw_version.patch,
                        LIBBLADERF_VERSION,
                        required_fw_version.major,
                        required_fw_version.minor,
                        required_fw_version.patch);
        }
#endif
        return status;
    }

    /* Set FPGA protocol */
    status = dev->backend->set_fpga_protocol(dev, BACKEND_FPGA_PROTOCOL_NIOSII);
    if (status < 0) {
        log_error("Unable to set backend FPGA protocol: %d\n", status);
        return status;
    }

    /* Check if FPGA is configured */
    status = dev->backend->is_fpga_configured(dev);
    if (status < 0) {
        return status;
    } else if (status != 1 && board_data->fpga_size == BLADERF_FPGA_UNKNOWN) {
        log_warning("Unknown FPGA size. Skipping FPGA configuration...\n");
        log_warning("Skipping further initialization...\n");
        return 0;
    } else if (status != 1) {
        /* Try searching for an FPGA in the config search path */
        full_path = file_find("hosted.rbf");
        if (full_path) {
            uint8_t *buf;
            size_t buf_size;

            log_debug("Loading FPGA from: %s\n", full_path);

            status = file_read_buffer(full_path, &buf, &buf_size);
            free(full_path);
            if (status != 0) {
                return status;
            }

            status = dev->backend->load_fpga(dev, buf, buf_size);
            if (status != 0) {
                log_warning("Failure loading FPGA: %s\n", bladerf_strerror(status));
                return status;
            }
        } else {
            log_warning("FPGA bitstream file not found.\n");
            log_warning("Skipping further initialization...\n");
            return 0;
        }
    }

    /* Read FPGA version */
    status = dev->backend->get_fpga_version(dev, &board_data->fpga_version);
    if (status < 0) {
        log_debug("Failed to get FPGA version: %s\n",
                  bladerf_strerror(status));
        return status;
    }
    log_verbose("Read FPGA version: %s\n", board_data->fpga_version.describe);

    /* Determine FPGA capabilities */
    board_data->capabilities |= bladerf2_get_fpga_capabilities(&board_data->fpga_version);
    log_verbose("Capability mask after FPGA load: 0x%016"PRIx64"\n",
                 board_data->capabilities);

    /* If the FPGA version check fails, just warn, but don't error out.
     *
     * If an error code caused this function to bail out, it would prevent a
     * user from being able to unload and reflash a bitstream being
     * "autoloaded" from SPI flash. */
    status = version_check(&bladerf2_fw_compat_table, &bladerf2_fpga_compat_table,
                           &board_data->fw_version, &board_data->fpga_version,
                           &required_fw_version, &required_fpga_version);
    if (status < 0) {
#if LOGGING_ENABLED
        if (status == BLADERF_ERR_UPDATE_FPGA) {
            log_warning("FPGA v%u.%u.%u was detected. Firmware v%u.%u.%u "
                        "requires FPGA v%u.%u.%u or later. Please load a "
                        "different FPGA version before continuing.\n\n",
                        board_data->fpga_version.major,
                        board_data->fpga_version.minor,
                        board_data->fpga_version.patch,
                        board_data->fw_version.major,
                        board_data->fw_version.minor,
                        board_data->fw_version.patch,
                        required_fpga_version.major,
                        required_fpga_version.minor,
                        required_fpga_version.patch);
        } else if (status == BLADERF_ERR_UPDATE_FW) {
            log_warning("FPGA v%u.%u.%u was detected, which requires firmware "
                        "v%u.%u.%u or later. The device firmware is currently "
                        "v%u.%u.%u. Please upgrade the device firmware before "
                        "continuing.\n\n",
                        board_data->fpga_version.major,
                        board_data->fpga_version.minor,
                        board_data->fpga_version.patch,
                        required_fw_version.major,
                        required_fw_version.minor,
                        required_fw_version.patch,
                        board_data->fw_version.major,
                        board_data->fw_version.minor,
                        board_data->fw_version.patch);
        }
#endif
    }

    /* Initialize the board */
    status = bladerf2_initialize(dev);
    if (status < 0) {
        return status;
    }

    return 0;
}

static void bladerf2_close(struct bladerf *dev)
{
    struct bladerf2_board_data *board_data = dev->board_data;

    if (board_data->phy) {
        /* FIXME ad9361 driver provides no free() */
        free(board_data->phy->spi);
        free(board_data->phy->gpio);
        #ifndef AXI_ADC_NOT_PRESENT
        free(board_data->phy->adc_conv);
        free(board_data->phy->adc_state);
        #endif
        free(board_data->phy->clk_refin);
        free(board_data->phy->pdata);
        free(board_data->phy);
    }

    if (board_data) {
        free(board_data);
    }
}

/******************************************************************************/
/* Properties */
/******************************************************************************/

static bladerf_dev_speed bladerf2_device_speed(struct bladerf *dev)
{
    int status;
    bladerf_dev_speed usb_speed;

    status = dev->backend->get_device_speed(dev, &usb_speed);
    if (status < 0) {
        return BLADERF_DEVICE_SPEED_UNKNOWN;
    }

    return usb_speed;
}

static int bladerf2_get_serial(struct bladerf *dev, char *serial)
{
    strcpy(serial, dev->ident.serial);
    return 0;
}

static int bladerf2_get_fpga_size(struct bladerf *dev, bladerf_fpga_size *size)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    *size = board_data->fpga_size;
    return 0;
}

static int bladerf2_is_fpga_configured(struct bladerf *dev)
{
    return dev->backend->is_fpga_configured(dev);
}

static uint64_t bladerf2_get_capabilities(struct bladerf *dev)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    return board_data->capabilities;
}

/******************************************************************************/
/* Versions */
/******************************************************************************/

static int bladerf2_get_fpga_version(struct bladerf *dev, struct bladerf_version *version)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    memcpy(version, &board_data->fpga_version, sizeof(*version));
    return 0;
}

static int bladerf2_get_fw_version(struct bladerf *dev, struct bladerf_version *version)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    memcpy(version, &board_data->fw_version, sizeof(*version));
    return 0;
}

/******************************************************************************/
/* Enable/disable */
/******************************************************************************/

static int bladerf2_enable_module(struct bladerf *dev, bladerf_module ch, bool enable)
{
    return 0;
}

/******************************************************************************/
/* Gain */
/******************************************************************************/

static int bladerf2_set_gain(struct bladerf *dev, bladerf_module ch, int gain)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    int status;

    if (ch & BLADERF_TX) {
        return BLADERF_ERR_UNSUPPORTED;
    } else {
        status = ad9361_set_rx_rf_gain(board_data->phy, ch >> 1, gain);
    }

    if (status < 0) {
        return errno_ad9361_to_bladerf(status);
    }

    return 0;
}

#if 0
static int bladerf2_get_gain(struct bladerf *dev, bladerf_module ch, int *gain)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    int status;

    if (ch & BLADERF_TX) {
        return BLADERF_ERR_UNSUPPORTED;
    } else {
        status = ad9361_get_rx_rf_gain(dev->board_data->phy, ch >> 1, gain);
    }

    if (status < 0) {
        return errno_ad9361_to_bladerf(status);
    }

    return 0;
}
#endif

/******************************************************************************/
/* Sample Rate */
/******************************************************************************/

static int bladerf2_set_sample_rate(struct bladerf *dev, bladerf_module ch, unsigned int rate, unsigned int *actual)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    int status;

    if (ch & BLADERF_TX) {
        status = ad9361_set_tx_sampling_freq(board_data->phy, rate);
        if (status < 0) {
            return errno_ad9361_to_bladerf(status);
        }
        status = ad9361_get_tx_sampling_freq(board_data->phy, actual);
        if (status < 0) {
            return errno_ad9361_to_bladerf(status);
        }
    } else {
        status = ad9361_set_rx_sampling_freq(board_data->phy, rate);
        if (status < 0) {
            return errno_ad9361_to_bladerf(status);
        }
        status = ad9361_get_rx_sampling_freq(board_data->phy, actual);
        if (status < 0) {
            return errno_ad9361_to_bladerf(status);
        }
    }

    return 0;
}

static int bladerf2_get_sample_rate(struct bladerf *dev, bladerf_module ch, unsigned int *rate)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    int status;

    if (ch & BLADERF_TX) {
        status = ad9361_get_tx_sampling_freq(board_data->phy, rate);
    } else {
        status = ad9361_get_rx_sampling_freq(board_data->phy, rate);
    }

    if (status < 0) {
        return errno_ad9361_to_bladerf(status);
    }

    return 0;
}

static int bladerf2_set_rational_sample_rate(struct bladerf *dev, bladerf_module ch, struct bladerf_rational_rate *rate, struct bladerf_rational_rate *actual)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_get_rational_sample_rate(struct bladerf *dev, bladerf_module ch, struct bladerf_rational_rate *rate)
{
    return BLADERF_ERR_UNSUPPORTED;
}

/******************************************************************************/
/* Bandwidth */
/******************************************************************************/

static int bladerf2_set_bandwidth(struct bladerf *dev, bladerf_module ch, unsigned int bandwidth, unsigned int *actual)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    int status;

    if (ch & BLADERF_TX) {
        status = ad9361_set_tx_rf_bandwidth(board_data->phy, bandwidth);
        if (status < 0) {
            return errno_ad9361_to_bladerf(status);
        }
        status = ad9361_get_tx_rf_bandwidth(board_data->phy, actual);
        if (status < 0) {
            return errno_ad9361_to_bladerf(status);
        }
    } else {
        status = ad9361_set_rx_rf_bandwidth(board_data->phy, bandwidth);
        if (status < 0) {
            return errno_ad9361_to_bladerf(status);
        }
        status = ad9361_get_rx_rf_bandwidth(board_data->phy, actual);
        if (status < 0) {
            return errno_ad9361_to_bladerf(status);
        }
    }

    return 0;
}

static int bladerf2_get_bandwidth(struct bladerf *dev, bladerf_module ch, unsigned int *bandwidth)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    int status;

    if (ch & BLADERF_TX) {
        status = ad9361_get_tx_rf_bandwidth(board_data->phy, bandwidth);
    } else {
        status = ad9361_get_rx_rf_bandwidth(board_data->phy, bandwidth);
    }

    if (status < 0) {
        return errno_ad9361_to_bladerf(status);
    }

    return 0;
}

/******************************************************************************/
/* Frequency */
/******************************************************************************/

static int bladerf2_set_frequency(struct bladerf *dev, bladerf_module ch, unsigned int frequency)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    int status;

    if (ch & BLADERF_TX) {
        status = ad9361_set_tx_lo_freq(board_data->phy, frequency);
    } else {
        status = ad9361_set_rx_lo_freq(board_data->phy, frequency);
    }

    if (status < 0) {
        return errno_ad9361_to_bladerf(status);
    }

    /* FIXME ad9361_set_rx_rf_port_input / ad9361_set_tx_rf_port_output */

    return 0;
}

static int bladerf2_get_frequency(struct bladerf *dev, bladerf_module ch, unsigned int *frequency)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    int status;
    uint64_t lo_frequency;

    if (ch & BLADERF_TX) {
        status = ad9361_get_tx_lo_freq(board_data->phy, &lo_frequency);
    } else {
        status = ad9361_get_rx_lo_freq(board_data->phy, &lo_frequency);
    }

    if (status < 0) {
        return errno_ad9361_to_bladerf(status);
    }

    *frequency = lo_frequency;

    return 0;
}

static int bladerf2_select_band(struct bladerf *dev, bladerf_module ch, unsigned int frequency)
{
    return BLADERF_ERR_UNSUPPORTED;
}

/******************************************************************************/
/* Scheduled Tuning */
/******************************************************************************/

static int bladerf2_get_quick_tune(struct bladerf *dev, bladerf_module ch, struct bladerf_quick_tune *quick_tune)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_schedule_retune(struct bladerf *dev, bladerf_module ch, uint64_t timestamp, unsigned int frequency, struct bladerf_quick_tune *quick_tune)

{
    return BLADERF_ERR_UNSUPPORTED;
}

int bladerf2_cancel_scheduled_retunes(struct bladerf *dev, bladerf_module ch)
{
    return BLADERF_ERR_UNSUPPORTED;
}

/******************************************************************************/
/* Trigger */
/******************************************************************************/

static int bladerf2_trigger_init(struct bladerf *dev, bladerf_module ch, bladerf_trigger_signal signal, struct bladerf_trigger *trigger)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_trigger_arm(struct bladerf *dev, const struct bladerf_trigger *trigger, bool arm, uint64_t resv1, uint64_t resv2)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_trigger_fire(struct bladerf *dev, const struct bladerf_trigger *trigger)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_trigger_state(struct bladerf *dev, const struct bladerf_trigger *trigger, bool *is_armed, bool *has_fired, bool *fire_requested, uint64_t *resv1, uint64_t *resv2)
{
    return BLADERF_ERR_UNSUPPORTED;
}

/******************************************************************************/
/* Streaming */
/******************************************************************************/

static int bladerf2_init_stream(struct bladerf_stream **stream, struct bladerf *dev, bladerf_stream_cb callback, void ***buffers, size_t num_buffers, bladerf_format format, size_t samples_per_buffer, size_t num_transfers, void *user_data)
{
    return async_init_stream(stream, dev, callback, buffers, num_buffers,
                             format, samples_per_buffer, num_transfers, user_data);
}

static int bladerf2_stream(struct bladerf_stream *stream, bladerf_module ch)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_submit_stream_buffer(struct bladerf_stream *stream, void *buffer, unsigned int timeout_ms, bool nonblock)
{
    return async_submit_stream_buffer(stream, buffer, timeout_ms, nonblock);
}

static void bladerf2_deinit_stream(struct bladerf_stream *stream)
{
    async_deinit_stream(stream);
}

static int bladerf2_set_stream_timeout(struct bladerf *dev, bladerf_module ch, unsigned int timeout)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_get_stream_timeout(struct bladerf *dev, bladerf_module ch, unsigned int *timeout)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_sync_config(struct bladerf *dev, bladerf_module ch, bladerf_format format, unsigned int num_buffers, unsigned int buffer_size, unsigned int num_transfers, unsigned int stream_timeout)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_sync_tx(struct bladerf *dev, void *samples, unsigned int num_samples, struct bladerf_metadata *metadata, unsigned int timeout_ms)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_sync_rx(struct bladerf *dev, void *samples, unsigned int num_samples, struct bladerf_metadata *metadata, unsigned int timeout_ms)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_get_timestamp(struct bladerf *dev, bladerf_module ch, uint64_t *value)
{
    return dev->backend->get_timestamp(dev, ch, value);
}

/******************************************************************************/
/* SMB Clock Configuration (board-agnostic feature?) */
/******************************************************************************/

static int bladerf2_get_smb_mode(struct bladerf *dev, bladerf_smb_mode *mode)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_set_smb_mode(struct bladerf *dev, bladerf_smb_mode mode)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_get_smb_frequency(struct bladerf *dev, unsigned int *rate)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_set_smb_frequency(struct bladerf *dev, uint32_t rate, uint32_t *actual)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_get_rational_smb_frequency(struct bladerf *dev, struct bladerf_rational_rate *rate)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_set_rational_smb_frequency(struct bladerf *dev, struct bladerf_rational_rate *rate, struct bladerf_rational_rate *actual)
{
    return BLADERF_ERR_UNSUPPORTED;
}

/******************************************************************************/
/* DC/Phase/Gain Correction */
/******************************************************************************/

static int bladerf2_get_correction(struct bladerf *dev, bladerf_module ch, bladerf_correction corr, int16_t *value)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_set_correction(struct bladerf *dev, bladerf_module ch, bladerf_correction corr, int16_t value)
{
    return BLADERF_ERR_UNSUPPORTED;
}

/******************************************************************************/
/* FPGA/Firmware Loading/Flashing */
/******************************************************************************/

/* We do not build FPGAs with compression enabled. Therfore, they
 * will always have a fixed file size.
 */
#define FPGA_SIZE_X40   (1191788)
#define FPGA_SIZE_X115  (3571462)

static bool is_valid_fpga_size(bladerf_fpga_size fpga, size_t len)
{
    static const char env_override[] = "BLADERF_SKIP_FPGA_SIZE_CHECK";
    bool valid;

    switch (fpga) {
        case BLADERF_FPGA_40KLE:
            valid = (len == FPGA_SIZE_X40);
            break;

        case BLADERF_FPGA_115KLE:
            valid = (len == FPGA_SIZE_X115);
            break;

        default:
            log_debug("Unknown FPGA type (%d). Using relaxed size criteria.\n",
                      fpga);

            if (len < (1 * 1024 * 1024)) {
                valid = false;
            } else if (len > BLADERF_FLASH_BYTE_LEN_FPGA) {
                valid = false;
            } else {
                valid = true;
            }
            break;
    }

    /* Provide a means to override this check. This is intended to allow
     * folks who know what they're doing to work around this quickly without
     * needing to make a code change. (e.g., someone building a custom FPGA
     * image that enables compressoin) */
    if (getenv(env_override)) {
        log_info("Overriding FPGA size check per %s\n", env_override);
        valid = true;
    }

    if (!valid) {
        log_warning("Detected potentially incorrect FPGA file.\n");

        log_debug("If you are certain this file is valid, you may define\n"
                  "BLADERF_SKIP_FPGA_SIZE_CHECK in your environment to skip this check.\n\n");
    }

    return valid;
}

static int bladerf2_load_fpga(struct bladerf *dev, const uint8_t *buf, size_t length)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    struct bladerf_version required_fw_version;
    struct bladerf_version required_fpga_version;
    int status;

    if (!is_valid_fpga_size(board_data->fpga_size, length)) {
        return BLADERF_ERR_INVAL;
    }

    status = dev->backend->load_fpga(dev, buf, length);
    if (status != 0) {
        return status;
    }

    /* Read FPGA version */
    status = dev->backend->get_fpga_version(dev, &board_data->fpga_version);
    if (status < 0) {
        log_debug("Failed to get FPGA version: %s\n",
                  bladerf_strerror(status));
        return status;
    }
    log_verbose("Read FPGA version: %s\n", board_data->fpga_version.describe);

    status = version_check(&bladerf2_fw_compat_table, &bladerf2_fpga_compat_table,
                           &board_data->fw_version, &board_data->fpga_version,
                           &required_fw_version, &required_fpga_version);
    if (status != 0) {
#if LOGGING_ENABLED
        if (status == BLADERF_ERR_UPDATE_FPGA) {
            log_warning("FPGA v%u.%u.%u was detected. Firmware v%u.%u.%u "
                        "requires FPGA v%u.%u.%u or later. Please load a "
                        "different FPGA version before continuing.\n\n",
                        board_data->fpga_version.major,
                        board_data->fpga_version.minor,
                        board_data->fpga_version.patch,
                        board_data->fw_version.major,
                        board_data->fw_version.minor,
                        board_data->fw_version.patch,
                        required_fpga_version.major,
                        required_fpga_version.minor,
                        required_fpga_version.patch);
        } else if (status == BLADERF_ERR_UPDATE_FW) {
            log_warning("FPGA v%u.%u.%u was detected, which requires firmware "
                        "v%u.%u.%u or later. The device firmware is currently "
                        "v%u.%u.%u. Please upgrade the device firmware before "
                        "continuing.\n\n",
                        board_data->fpga_version.major,
                        board_data->fpga_version.minor,
                        board_data->fpga_version.patch,
                        required_fw_version.major,
                        required_fw_version.minor,
                        required_fw_version.patch,
                        board_data->fw_version.major,
                        board_data->fw_version.minor,
                        board_data->fw_version.patch);
        }
#endif
        return status;
    }

    status = bladerf2_initialize(dev);
    if (status != 0) {
        return status;
    }

    return 0;
}

static int bladerf2_flash_fpga(struct bladerf *dev, const uint8_t *buf, size_t length)
{
    #if 0
    struct bladerf2_board_data *board_data = dev->board_data;

    if (!is_valid_fpga_size(board_data->fpga_size, length)) {
        return BLADERF_ERR_INVAL;
    }

    return spi_flash_write_fpga_bitstream(dev, buf, length);
    #else
    return BLADERF_ERR_UNSUPPORTED;
    #endif
}

static int bladerf2_erase_stored_fpga(struct bladerf *dev)
{
    #if 0
    return spi_flash_erase_fpga(dev);
    #else
    return BLADERF_ERR_UNSUPPORTED;
    #endif
}

#if 0
static bool is_valid_fw_size(size_t len)
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
#endif

static int bladerf2_flash_firmware(struct bladerf *dev, const uint8_t *buf, size_t length)
{
    #if 0
    const char env_override[] = "BLADERF_SKIP_FW_SIZE_CHECK";

    /* Sanity check firmware length.
     *
     * TODO in the future, better sanity checks can be performed when
     *      using the bladerf image format currently used to backup/restore
     *      calibration data
     */
    if (!getenv(env_override) && !is_valid_fw_size(length)) {
        log_info("Detected potentially invalid firmware file.\n");
        log_info("Define BLADERF_SKIP_FW_SIZE_CHECK in your evironment "
                "to skip this check.\n");
        return BLADERF_ERR_INVAL;
    }

    return spi_flash_write_fx3_fw(dev, buf, length);
    #else
    return BLADERF_ERR_UNSUPPORTED;
    #endif
}

static int bladerf2_device_reset(struct bladerf *dev)
{
    return dev->backend->device_reset(dev);
}

/******************************************************************************/
/* Sample Internal/Direct */
/******************************************************************************/

static int bladerf2_get_sampling(struct bladerf *dev, bladerf_sampling *sampling)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_set_sampling(struct bladerf *dev, bladerf_sampling sampling)
{
    return BLADERF_ERR_UNSUPPORTED;
}

/******************************************************************************/
/* Sample RX FPGA Mux */
/******************************************************************************/

static int bladerf2_set_rx_mux(struct bladerf *dev, bladerf_rx_mux mode)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_get_rx_mux(struct bladerf *dev, bladerf_rx_mux *mode)
{
    return BLADERF_ERR_UNSUPPORTED;
}

/******************************************************************************/
/* Tune on host or FPGA */
/******************************************************************************/

static int bladerf2_set_tuning_mode(struct bladerf *dev, bladerf_tuning_mode mode)
{
    return BLADERF_ERR_UNSUPPORTED;
}

/******************************************************************************/
/* Expansion support */
/******************************************************************************/

static int bladerf2_expansion_attach(struct bladerf *dev, bladerf_xb xb)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_expansion_get_attached(struct bladerf *dev, bladerf_xb *xb)
{
    return BLADERF_ERR_UNSUPPORTED;
}

/******************************************************************************/
/* Board binding */
/******************************************************************************/

const struct board_fns bladerf2_board_fns = {
    FIELD_INIT(.matches, bladerf2_matches),
    FIELD_INIT(.open, bladerf2_open),
    FIELD_INIT(.close, bladerf2_close),
    FIELD_INIT(.device_speed, bladerf2_device_speed),
    FIELD_INIT(.get_serial, bladerf2_get_serial),
    FIELD_INIT(.get_fpga_size, bladerf2_get_fpga_size),
    FIELD_INIT(.is_fpga_configured, bladerf2_is_fpga_configured),
    FIELD_INIT(.get_capabilities, bladerf2_get_capabilities),
    FIELD_INIT(.get_fpga_version, bladerf2_get_fpga_version),
    FIELD_INIT(.get_fw_version, bladerf2_get_fw_version),
    FIELD_INIT(.enable_module, bladerf2_enable_module),
    FIELD_INIT(.set_gain, bladerf2_set_gain),
    FIELD_INIT(.set_sample_rate, bladerf2_set_sample_rate),
    FIELD_INIT(.set_rational_sample_rate, bladerf2_set_rational_sample_rate),
    FIELD_INIT(.get_sample_rate, bladerf2_get_sample_rate),
    FIELD_INIT(.get_rational_sample_rate, bladerf2_get_rational_sample_rate),
    FIELD_INIT(.set_bandwidth, bladerf2_set_bandwidth),
    FIELD_INIT(.get_bandwidth, bladerf2_get_bandwidth),
    FIELD_INIT(.get_frequency, bladerf2_get_frequency),
    FIELD_INIT(.set_frequency, bladerf2_set_frequency),
    FIELD_INIT(.select_band, bladerf2_select_band),
    FIELD_INIT(.get_quick_tune, bladerf2_get_quick_tune),
    FIELD_INIT(.schedule_retune, bladerf2_schedule_retune),
    FIELD_INIT(.cancel_scheduled_retunes, bladerf2_cancel_scheduled_retunes),
    FIELD_INIT(.trigger_init, bladerf2_trigger_init),
    FIELD_INIT(.trigger_arm, bladerf2_trigger_arm),
    FIELD_INIT(.trigger_fire, bladerf2_trigger_fire),
    FIELD_INIT(.trigger_state, bladerf2_trigger_state),
    FIELD_INIT(.init_stream, bladerf2_init_stream),
    FIELD_INIT(.stream, bladerf2_stream),
    FIELD_INIT(.submit_stream_buffer, bladerf2_submit_stream_buffer),
    FIELD_INIT(.deinit_stream, bladerf2_deinit_stream),
    FIELD_INIT(.set_stream_timeout, bladerf2_set_stream_timeout),
    FIELD_INIT(.get_stream_timeout, bladerf2_get_stream_timeout),
    FIELD_INIT(.sync_config, bladerf2_sync_config),
    FIELD_INIT(.sync_tx, bladerf2_sync_tx),
    FIELD_INIT(.sync_rx, bladerf2_sync_rx),
    FIELD_INIT(.get_timestamp, bladerf2_get_timestamp),
    FIELD_INIT(.get_smb_mode, bladerf2_get_smb_mode),
    FIELD_INIT(.set_smb_mode, bladerf2_set_smb_mode),
    FIELD_INIT(.get_smb_frequency, bladerf2_get_smb_frequency),
    FIELD_INIT(.set_smb_frequency, bladerf2_set_smb_frequency),
    FIELD_INIT(.get_rational_smb_frequency, bladerf2_get_rational_smb_frequency),
    FIELD_INIT(.set_rational_smb_frequency, bladerf2_set_rational_smb_frequency),
    FIELD_INIT(.get_correction, bladerf2_get_correction),
    FIELD_INIT(.set_correction, bladerf2_set_correction),
    FIELD_INIT(.load_fpga, bladerf2_load_fpga),
    FIELD_INIT(.flash_fpga, bladerf2_flash_fpga),
    FIELD_INIT(.erase_stored_fpga, bladerf2_erase_stored_fpga),
    FIELD_INIT(.flash_firmware, bladerf2_flash_firmware),
    FIELD_INIT(.device_reset, bladerf2_device_reset),
    FIELD_INIT(.get_sampling, bladerf2_get_sampling),
    FIELD_INIT(.set_sampling, bladerf2_set_sampling),
    FIELD_INIT(.get_rx_mux, bladerf2_get_rx_mux),
    FIELD_INIT(.set_rx_mux, bladerf2_set_rx_mux),
    FIELD_INIT(.set_tuning_mode, bladerf2_set_tuning_mode),
    FIELD_INIT(.expansion_attach, bladerf2_expansion_attach),
    FIELD_INIT(.expansion_get_attached, bladerf2_expansion_get_attached),
};

/******************************************************************************
 ******************************************************************************
 *                         bladeRF2-specific Functions                        *
 ******************************************************************************
 ******************************************************************************/

/* TBD */
