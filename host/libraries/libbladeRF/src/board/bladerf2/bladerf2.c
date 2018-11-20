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

#include <libbladeRF.h>

#include "bladeRF.h"
#include "host_config.h"

#include "log.h"
#define LOGGER_ID_STRING
#include "logger_id.h"
#include "rel_assert.h"

#include "../bladerf1/flash.h"
#include "board/board.h"
#include "capabilities.h"
#include "compatibility.h"

#include "ad936x.h"
#include "ad936x_helpers.h"

#include "driver/fpga_trigger.h"
#include "driver/fx3_fw.h"
#include "driver/ina219.h"
#include "driver/spi_flash.h"

#include "backend/backend_config.h"
#include "backend/usb/usb.h"

#include "streaming/async.h"
#include "streaming/sync.h"

#include "conversions.h"
#include "devinfo.h"
#include "helpers/file.h"
#include "helpers/version.h"
#include "helpers/wallclock.h"
#include "version.h"

#include "bladerf2_common.h"
#include "common.h"


/******************************************************************************/
/* Forward declarations */
/******************************************************************************/

static int bladerf2_read_flash_vctcxo_trim(struct bladerf *dev, uint16_t *trim);


/******************************************************************************/
/* Constants */
/******************************************************************************/

// clang-format off

/* REFIN frequency range */
static struct bladerf_range const bladerf2_pll_refclk_range = {
    FIELD_INIT(.min, 5000000),
    FIELD_INIT(.max, 300000000),
    FIELD_INIT(.step, 1),
    FIELD_INIT(.scale, 1),
};

/* Loopback modes */
static struct bladerf_loopback_modes const bladerf2_loopback_modes[] = {
    {
        FIELD_INIT(.name, "none"),
        FIELD_INIT(.mode, BLADERF_LB_NONE)
    },
    {
        FIELD_INIT(.name, "firmware"),
        FIELD_INIT(.mode, BLADERF_LB_FIRMWARE)
    },
    {
        FIELD_INIT(.name, "rf_bist"),
        FIELD_INIT(.mode, BLADERF_LB_RFIC_BIST)
    },
};
// clang-format on


/******************************************************************************/
/* Low-level Initialization */
/******************************************************************************/

static int _bladerf2_initialize(struct bladerf *dev)
{
    struct bladerf2_board_data *board_data;
    struct ad9361_rf_phy *phy;
    struct bladerf_version required_fw_version, required_fpga_version;
    uint32_t reg;
    int status;

    /* Test for uninitialized dev struct */
    NULL_CHECK(dev);
    NULL_CHECK(dev->board_data);

    /* Initialize board_data struct and members */
    board_data                      = dev->board_data;
    board_data->rxfir_orig          = BLADERF_RFIC_RXFIR_DEFAULT;
    board_data->txfir_orig          = BLADERF_RFIC_TXFIR_DEFAULT;
    board_data->low_samplerate_mode = false;

    /* Read FPGA version */
    CHECK_STATUS(
        dev->backend->get_fpga_version(dev, &board_data->fpga_version));

    log_verbose("Read FPGA version: %s\n", board_data->fpga_version.describe);

    /* Determine FPGA capabilities */
    board_data->capabilities |=
        bladerf2_get_fpga_capabilities(&board_data->fpga_version);

    log_verbose("Capability mask after FPGA load: 0x%016" PRIx64 "\n",
                board_data->capabilities);

    /* If the FPGA version check fails, just warn, but don't error out.
     *
     * If an error code caused this function to bail out, it would prevent a
     * user from being able to unload and reflash a bitstream being
     * "autoloaded" from SPI flash. */
    status =
        version_check(&bladerf2_fw_compat_table, &bladerf2_fpga_compat_table,
                      &board_data->fw_version, &board_data->fpga_version,
                      &required_fw_version, &required_fpga_version);
    if (status < 0) {
#if LOGGING_ENABLED
        if (BLADERF_ERR_UPDATE_FPGA == status) {
            log_warning(
                "FPGA v%u.%u.%u was detected. Firmware v%u.%u.%u "
                "requires FPGA v%u.%u.%u or later. Please load a "
                "different FPGA version before continuing.\n\n",
                board_data->fpga_version.major, board_data->fpga_version.minor,
                board_data->fpga_version.patch, board_data->fw_version.major,
                board_data->fw_version.minor, board_data->fw_version.patch,
                required_fpga_version.major, required_fpga_version.minor,
                required_fpga_version.patch);
        } else if (BLADERF_ERR_UPDATE_FW == status) {
            log_warning(
                "FPGA v%u.%u.%u was detected, which requires firmware "
                "v%u.%u.%u or later. The device firmware is currently "
                "v%u.%u.%u. Please upgrade the device firmware before "
                "continuing.\n\n",
                board_data->fpga_version.major, board_data->fpga_version.minor,
                board_data->fpga_version.patch, required_fw_version.major,
                required_fw_version.minor, required_fw_version.patch,
                board_data->fw_version.major, board_data->fw_version.minor,
                board_data->fw_version.patch);
        }
#endif
    }

    /* Set FPGA packet protocol */
    CHECK_STATUS(
        dev->backend->set_fpga_protocol(dev, BACKEND_FPGA_PROTOCOL_NIOSII));

    /* Initialize RFFE control */
    CHECK_STATUS(dev->backend->rffe_control_write(
        dev, (1 << RFFE_CONTROL_ENABLE) | (1 << RFFE_CONTROL_TXNRX)));

    /* Initialize INA219 */
    /* For reasons unknown, this fails if done immediately after
     * ad9361_set_rx_fir_config when DEBUG is not defined. It shouldn't make
     * a difference, but it does. TODO: Investigate/fix this */
    CHECK_STATUS(ina219_init(dev, ina219_r_shunt));

    /* Initialize AD9361 */
    log_debug("%s: ad9361_init starting\n", __FUNCTION__);
    CHECK_AD936X(
        ad9361_init(&board_data->phy, &bladerf2_rfic_init_params, dev));
    log_debug("%s: ad9361_init complete\n", __FUNCTION__);

    if (NULL == board_data->phy || NULL == board_data->phy->pdata) {
        RETURN_ERROR_STATUS("ad9361_init struct initialization",
                            BLADERF_ERR_UNEXPECTED);
    }

    phy = board_data->phy;

    /* Force AD9361 to a non-default freq. This will entice it to do a proper
     * re-tuning when we set it back to the default freq later on. */
    CHECK_AD936X(ad9361_set_tx_lo_freq(phy, RESET_FREQUENCY));
    CHECK_AD936X(ad9361_set_rx_lo_freq(phy, RESET_FREQUENCY));

    /* Set up AD9361 FIR filters */
    /* TODO: permit specification of filter taps, for the truly brave */
    CHECK_STATUS(bladerf_set_rfic_rx_fir(dev, BLADERF_RFIC_RXFIR_DEFAULT));
    CHECK_STATUS(bladerf_set_rfic_tx_fir(dev, BLADERF_RFIC_TXFIR_DEFAULT));

    /* Disable AD9361 until we need it */
    CHECK_STATUS(dev->backend->rffe_control_read(dev, &reg));

    reg &= ~(1 << RFFE_CONTROL_TXNRX);
    reg &= ~(1 << RFFE_CONTROL_ENABLE);

    CHECK_STATUS(dev->backend->rffe_control_write(dev, reg));

    /* Mute TX channels */
    board_data->tx_mute[0] = false;
    board_data->tx_mute[1] = false;

    CHECK_STATUS(txmute_set(phy, BLADERF_CHANNEL_TX(0), true));
    CHECK_STATUS(txmute_set(phy, BLADERF_CHANNEL_TX(1), true));

    /* Update device state */
    board_data->state = STATE_INITIALIZED;

    /* Set RFIC to desired frequency */
    CHECK_STATUS(dev->board->set_frequency(dev, BLADERF_CHANNEL_TX(0),
                                           phy->pdata->tx_synth_freq));
    CHECK_STATUS(dev->board->set_frequency(dev, BLADERF_CHANNEL_RX(0),
                                           phy->pdata->rx_synth_freq));

    /* Initialize VCTCXO trim DAC to stored value */
    uint16_t *trimval = &(board_data->trimdac_stored_value);

    CHECK_STATUS(bladerf2_read_flash_vctcxo_trim(dev, trimval));
    CHECK_STATUS(dev->backend->ad56x1_vctcxo_trim_dac_write(dev, *trimval));

    board_data->trim_source = TRIM_SOURCE_TRIM_DAC;

    /* Configure PLL */
    CHECK_STATUS(bladerf_set_pll_refclk(dev, BLADERF_REFIN_DEFAULT));

    /* Reset current quick tune profile number */
    board_data->quick_tune_rx_profile = 0;
    board_data->quick_tune_tx_profile = 0;

    log_debug("%s: complete\n", __FUNCTION__);

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
    NULL_CHECK(dev);
    NULL_CHECK(dev->backend);

    uint16_t vid, pid;
    int status;

    status = dev->backend->get_vid_pid(dev, &vid, &pid);
    if (status < 0) {
        log_error("%s: get_vid_pid returned status %s\n", __FUNCTION__,
                  bladerf_strerror(status));
        return false;
    }

    if (USB_NUAND_VENDOR_ID == vid && USB_NUAND_BLADERF2_PRODUCT_ID == pid) {
        return true;
    }

    return false;
}


/******************************************************************************/
/* Open/close */
/******************************************************************************/

static int bladerf2_open(struct bladerf *dev, struct bladerf_devinfo *devinfo)
{
    NULL_CHECK(dev);
    NULL_CHECK(dev->backend);

    struct bladerf2_board_data *board_data;
    struct bladerf_version required_fw_version;
    char *full_path;
    bladerf_dev_speed usb_speed;
    size_t i;
    int ready, status;

    size_t const MAX_RETRIES = 30;

    /* Allocate board data */
    board_data = calloc(1, sizeof(struct bladerf2_board_data));
    if (NULL == board_data) {
        RETURN_ERROR_STATUS("calloc board_data", BLADERF_ERR_MEM);
    }
    dev->board_data = board_data;
    board_data->phy = NULL;

    /* Allocate flash architecture */
    dev->flash_arch = calloc(1, sizeof(struct bladerf_flash_arch));
    if (NULL == dev->flash_arch) {
        return BLADERF_ERR_MEM;
    }

    /* Initialize board data */
    board_data->fpga_version.describe = board_data->fpga_version_str;
    board_data->fw_version.describe   = board_data->fw_version_str;

    board_data->module_format[BLADERF_RX] = -1;
    board_data->module_format[BLADERF_TX] = -1;

    dev->flash_arch->status          = STATE_UNINITIALIZED;
    dev->flash_arch->manufacturer_id = 0x0;
    dev->flash_arch->device_id       = 0x0;

    /* Read firmware version */
    CHECK_STATUS(dev->backend->get_fw_version(dev, &board_data->fw_version));

    log_verbose("Read Firmware version: %s\n", board_data->fw_version.describe);

    /* Determine firmware capabilities */
    board_data->capabilities |=
        bladerf2_get_fw_capabilities(&board_data->fw_version);

    log_verbose("Capability mask before FPGA load: 0x%016" PRIx64 "\n",
                board_data->capabilities);

    /* Update device state */
    board_data->state = STATE_FIRMWARE_LOADED;

    /* Wait until firmware is ready */
    for (i = 0; i < MAX_RETRIES; i++) {
        ready = dev->backend->is_fw_ready(dev);
        if (ready != 1) {
            if (0 == i) {
                log_info("Waiting for device to become ready...\n");
            } else {
                log_debug("Retry %02u/%02u.\n", i + 1, MAX_RETRIES);
            }
            usleep(1000000);
        } else {
            break;
        }
    }

    if (ready != 1) {
        RETURN_ERROR_STATUS("is_fw_ready", BLADERF_ERR_TIMEOUT);
    }

    /* Determine data message size */
    CHECK_STATUS(dev->backend->get_device_speed(dev, &usb_speed));

    switch (usb_speed) {
        case BLADERF_DEVICE_SPEED_SUPER:
            board_data->msg_size = USB_MSG_SIZE_SS;
            break;
        case BLADERF_DEVICE_SPEED_HIGH:
            board_data->msg_size = USB_MSG_SIZE_HS;
            break;
        default:
            log_error("%s: unsupported device speed (%d)\n", __FUNCTION__,
                      usb_speed);
            return BLADERF_ERR_UNSUPPORTED;
    }

    /* Verify that we have a sufficent firmware version before continuing. */
    status = version_check_fw(&bladerf2_fw_compat_table,
                              &board_data->fw_version, &required_fw_version);
    if (status != 0) {
#ifdef LOGGING_ENABLED
        if (BLADERF_ERR_UPDATE_FW == status) {
            log_warning("Firmware v%u.%u.%u was detected. libbladeRF v%s "
                        "requires firmware v%u.%u.%u or later. An upgrade via "
                        "the bootloader is required.\n\n",
                        &board_data->fw_version.major,
                        &board_data->fw_version.minor,
                        &board_data->fw_version.patch, LIBBLADERF_VERSION,
                        required_fw_version.major, required_fw_version.minor,
                        required_fw_version.patch);
        }
#endif
        return status;
    }

    /* Probe SPI flash architecture information */
    if (have_cap(board_data->capabilities, BLADERF_CAP_FW_FLASH_ID)) {
        status = spi_flash_read_flash_id(dev, &dev->flash_arch->manufacturer_id,
                                         &dev->flash_arch->device_id);
        if (status < 0) {
            log_error("Failed to probe SPI flash ID information.\n");
        }
    } else {
        log_debug("FX3 firmware v%u.%u.%u does not support SPI flash ID. A "
                  "firmware update is recommended in order to probe the SPI "
                  "flash ID information.\n",
                  board_data->fw_version.major, board_data->fw_version.minor,
                  board_data->fw_version.patch);
    }

    /* Decode SPI flash ID information to figure out its architecture.
     * We need to know a little about the flash architecture before we can
     * read anything from it, including FPGA size and other cal data.
     * If the firmware does not have the capability to get the flash ID,
     * sane defaults will be chosen.
     *
     * Not checking return code because it is irrelevant. */
    spi_flash_decode_flash_architecture(dev, &board_data->fpga_size);

    /* Get FPGA size */
    status = spi_flash_read_fpga_size(dev, &board_data->fpga_size);
    if (status < 0) {
        log_warning("Failed to get FPGA size %s\n", bladerf_strerror(status));
    }

    if (getenv("BLADERF_FORCE_FPGA_A9")) {
        log_info("BLADERF_FORCE_FPGA_A9 is set, assuming A9 FPGA\n");
        board_data->fpga_size = BLADERF_FPGA_A9;
    }

    /* If the flash architecture could not be decoded earlier, try again now
     * that the FPGA size is known. */
    if (dev->flash_arch->status != STATUS_SUCCESS) {
        status =
            spi_flash_decode_flash_architecture(dev, &board_data->fpga_size);
        if (status < 0) {
            log_debug("Assumptions were made about the SPI flash architecture! "
                      "Flash commands may not function as expected.\n");
        }
    }

    /* Skip further work if BLADERF_FORCE_NO_FPGA_PRESENT is set */
    if (getenv("BLADERF_FORCE_NO_FPGA_PRESENT")) {
        log_debug("Skipping FPGA configuration and initialization - "
                  "BLADERF_FORCE_NO_FPGA_PRESENT is set.\n");
        return 0;
    }

    /* Check if FPGA is configured */
    status = dev->backend->is_fpga_configured(dev);
    if (status < 0) {
        RETURN_ERROR_STATUS("is_fpga_configured", status);
    } else if (1 == status) {
        board_data->state = STATE_FPGA_LOADED;
    } else if (status != 1 && BLADERF_FPGA_UNKNOWN == board_data->fpga_size) {
        log_warning("Unknown FPGA size. Skipping FPGA configuration...\n");
        log_warning("Skipping further initialization...\n");
        return 0;
    } else if (status != 1) {
        /* Try searching for an FPGA in the config search path */
        switch (board_data->fpga_size) {
            case BLADERF_FPGA_A4:
                full_path = file_find("hostedxA4.rbf");
                break;

            case BLADERF_FPGA_A9:
                full_path = file_find("hostedxA9.rbf");
                break;

            default:
                log_error("%s: invalid FPGA size: %d\n", __FUNCTION__,
                          board_data->fpga_size);
                return BLADERF_ERR_UNEXPECTED;
        }

        if (full_path != NULL) {
            uint8_t *buf;
            size_t buf_size;

            log_debug("Loading FPGA from: %s\n", full_path);

            status = file_read_buffer(full_path, &buf, &buf_size);
            free(full_path);
            full_path = NULL;

            if (status != 0) {
                RETURN_ERROR_STATUS("file_read_buffer", status);
            }

            CHECK_STATUS(dev->backend->load_fpga(dev, buf, buf_size));

            board_data->state = STATE_FPGA_LOADED;
        } else {
            log_warning("FPGA bitstream file not found.\n");
            log_warning("Skipping further initialization...\n");
            return 0;
        }
    }

    /* Initialize the board */
    CHECK_STATUS(_bladerf2_initialize(dev));

    if (have_cap(board_data->capabilities, BLADERF_CAP_SCHEDULED_RETUNE)) {
        /* Cancel any pending re-tunes that may have been left over as the
         * result of a user application crashing or forgetting to call
         * bladerf_close() */
        status =
            dev->board->cancel_scheduled_retunes(dev, BLADERF_CHANNEL_RX(0));
        if (status != 0) {
            log_warning("Failed to cancel any pending RX retunes: %s\n",
                        bladerf_strerror(status));
            return status;
        }

        status =
            dev->board->cancel_scheduled_retunes(dev, BLADERF_CHANNEL_TX(0));
        if (status != 0) {
            log_warning("Failed to cancel any pending TX retunes: %s\n",
                        bladerf_strerror(status));
            return status;
        }
    }

    return 0;
}

static void bladerf2_close(struct bladerf *dev)
{
    if (dev != NULL) {
        struct bladerf2_board_data *board_data = dev->board_data;
        struct bladerf_flash_arch *flash_arch  = dev->flash_arch;

        if (board_data != NULL) {
            sync_deinit(&board_data->sync[BLADERF_CHANNEL_RX(0)]);
            sync_deinit(&board_data->sync[BLADERF_CHANNEL_TX(0)]);

            if (dev->backend->is_fpga_configured(dev) &&
                have_cap(board_data->capabilities,
                         BLADERF_CAP_SCHEDULED_RETUNE)) {
                /* Cancel scheduled retunes here to avoid the device retuning
                 * underneath the user should they open it again in the future.
                 *
                 * This is intended to help developers avoid a situation during
                 * debugging where they schedule "far" into the future, but hit
                 * a case where their program aborts or exits early. If we do
                 * not cancel these scheduled retunes, the device could start
                 * up and/or "unexpectedly" switch to a different frequency.
                 */
                dev->board->cancel_scheduled_retunes(dev, BLADERF_CHANNEL_RX(0));
                dev->board->cancel_scheduled_retunes(dev, BLADERF_CHANNEL_TX(0));
            }

            if (board_data->phy != NULL) {
                ad9361_deinit(board_data->phy);
                board_data->phy = NULL;
            }

            free(board_data);
            board_data = NULL;
        }

        if (flash_arch != NULL) {
            free(flash_arch);
            flash_arch = NULL;
        }
    }
}


/******************************************************************************/
/* Properties */
/******************************************************************************/

static bladerf_dev_speed bladerf2_device_speed(struct bladerf *dev)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    bladerf_dev_speed usb_speed;
    int status;

    status = dev->backend->get_device_speed(dev, &usb_speed);
    if (status < 0) {
        log_error("%s: get_device_speed failed: %s\n", __FUNCTION__,
                  bladerf_strerror(status));
        return BLADERF_DEVICE_SPEED_UNKNOWN;
    }

    return usb_speed;
}

static int bladerf2_get_serial(struct bladerf *dev, char *serial)
{
    CHECK_BOARD_STATE(STATE_UNINITIALIZED);
    NULL_CHECK(serial);

    // TODO: don't use strcpy
    strcpy(serial, dev->ident.serial);

    return 0;
}

static int bladerf2_get_fpga_size(struct bladerf *dev, bladerf_fpga_size *size)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);
    NULL_CHECK(size);

    struct bladerf2_board_data *board_data = dev->board_data;

    *size = board_data->fpga_size;

    return 0;
}

static int bladerf2_get_flash_size(struct bladerf *dev,
                                   uint32_t *size,
                                   bool *is_guess)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);
    NULL_CHECK(size);
    NULL_CHECK(is_guess);

    *size     = dev->flash_arch->tsize_bytes;
    *is_guess = (dev->flash_arch->status != STATUS_SUCCESS);

    return 0;
}

static int bladerf2_is_fpga_configured(struct bladerf *dev)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    return dev->backend->is_fpga_configured(dev);
}

static int bladerf2_get_fpga_source(struct bladerf *dev,
                                    bladerf_fpga_source *source)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);
    NULL_CHECK(source);

    struct bladerf2_board_data *board_data = dev->board_data;

    if (!have_cap(board_data->capabilities, BLADERF_CAP_FW_FPGA_SOURCE)) {
        log_debug("%s: not supported by firmware\n", __FUNCTION__);
        *source = BLADERF_FPGA_SOURCE_UNKNOWN;
        return BLADERF_ERR_UNSUPPORTED;
    }

    *source = dev->backend->get_fpga_source(dev);

    return 0;
}

static uint64_t bladerf2_get_capabilities(struct bladerf *dev)
{
    CHECK_BOARD_STATE(STATE_UNINITIALIZED);

    struct bladerf2_board_data *board_data = dev->board_data;

    return board_data->capabilities;
}

static size_t bladerf2_get_channel_count(struct bladerf *dev,
                                         bladerf_direction dir)
{
    return 2;
}


/******************************************************************************/
/* Versions */
/******************************************************************************/

static int bladerf2_get_fpga_version(struct bladerf *dev,
                                     struct bladerf_version *version)
{
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);
    NULL_CHECK(version);

    struct bladerf2_board_data *board_data = dev->board_data;

    memcpy(version, &board_data->fpga_version, sizeof(*version));

    return 0;
}

static int bladerf2_get_fw_version(struct bladerf *dev,
                                   struct bladerf_version *version)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);
    NULL_CHECK(version);

    struct bladerf2_board_data *board_data = dev->board_data;

    memcpy(version, &board_data->fw_version, sizeof(*version));

    return 0;
}


/******************************************************************************/
/* Enable/disable */
/******************************************************************************/

static int bladerf2_enable_module(struct bladerf *dev,
                                  bladerf_channel ch,
                                  bool enable)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;

    bladerf_direction dir = BLADERF_CHANNEL_IS_TX(ch) ? BLADERF_TX : BLADERF_RX;

    uint32_t reg;       /* RFFE register value */
    uint32_t reg_old;   /* Original RFFE register value */
    int ch_bit;         /* RFFE MIMO channel bit */
    int dir_bit;        /* RFFE RX/TX enable bit */
    bool ch_pending;    /* Requested channel state differs */
    bool dir_enable;    /* Direction is enabled */
    bool dir_pending;   /* Requested direction state differs */
    bool backend_clear; /* Backend requires reset */

    bladerf_frequency freq = 0;

    static uint64_t lastrun = 0; /* nsec value at last run */

    /* Look up the RFFE bits affecting this channel */
    ch_bit  = _get_rffe_control_bit_for_ch(ch);
    dir_bit = _get_rffe_control_bit_for_dir(dir);
    if (ch_bit < 0 || dir_bit < 0) {
        RETURN_ERROR_STATUS("_get_rffe_control_bit", BLADERF_ERR_INVAL);
    }

    /* Query the current frequency if necessary */
    if (enable) {
        CHECK_STATUS(dev->board->get_frequency(dev, ch, &freq));
    }

    /**
     * If we are moving from 0 active channels to 1 active channel:
     *  Channel:    Setup SPDT, MIMO, TX Mute
     *  Direction:  Setup ENABLE/TXNRX, RFIC port
     *  Backend:    Enable
     *
     * If we are moving from 1 active channel to 0 active channels:
     *  Channel:    Teardown SPDT, MIMO, TX Mute
     *  Direction:  Teardown ENABLE/TXNRX, RFIC port, Sync
     *  Backend:    Disable
     *
     * If we are enabling an nth channel, where n > 1:
     *  Channel:    Setup SPDT, MIMO, TX Mute
     *  Direction:  no change
     *  Backend:    Clear
     *
     * If we are disabling an nth channel, where n > 1:
     *  Channel:    Teardown SPDT, MIMO, TX Mute
     *  Direction:  no change
     *  Backend:    no change
     */

    /* Read RFFE control register */
    CHECK_STATUS(dev->backend->rffe_control_read(dev, &reg));

    reg_old = reg;

    ch_pending = _rffe_ch_enabled(reg, ch) != enable;

    /* Channel Setup/Teardown */
    if (ch_pending) {
        /* Modify SPDT bits */
        CHECK_STATUS(_modify_spdt_bits_by_freq(&reg, ch, enable, freq));

        /* Modify MIMO channel enable bits */
        if (enable) {
            reg |= (1 << ch_bit);
        } else {
            reg &= ~(1 << ch_bit);
        }

        /* Set/unset TX mute */
        if (BLADERF_CHANNEL_IS_TX(ch)) {
            txmute_set(phy, ch, !enable);
        }

        /* Warn the user if the sample rate isn't reasonable */
        if (enable) {
            check_total_sample_rate(dev);
        }
    }

    dir_enable  = enable || does_rffe_dir_have_enabled_ch(reg, dir);
    dir_pending = _rffe_dir_enabled(reg, dir) != dir_enable;

    /* Direction Setup/Teardown */
    if (dir_pending) {
        /* Modify ENABLE/TXNRX bits */
        if (dir_enable) {
            reg |= (1 << dir_bit);
        } else {
            reg &= ~(1 << dir_bit);
        }

        /* Select RFIC port */
        CHECK_STATUS(set_ad9361_port_by_freq(phy, ch, dir_enable, freq));

        /* Tear down sync interface if required */
        if (!dir_enable) {
            sync_deinit(&board_data->sync[dir]);
            perform_format_deconfig(dev, dir);
        }
    }

    /* Reset FIFO if we are enabling an additional RX channel */
    backend_clear = enable && !dir_pending && BLADERF_RX == dir;

    /* Debug logging */
    uint64_t nsec = wallclock_get_current_nsec();

    log_debug("%s: %s%d ch_en=%d ch_pend=%d dir_en=%d dir_pend=%d be_clr=%d "
              "reg=0x%08x->0x%08x nsec=%" PRIu64 " (delta: %" PRIu64 ")\n",
              __FUNCTION__, BLADERF_TX == dir ? "TX" : "RX", (ch >> 1) + 1,
              enable, ch_pending, dir_enable, dir_pending, backend_clear,
              reg_old, reg, nsec, nsec - lastrun);

    lastrun = nsec;

    /* Write RFFE control register */
    if (reg_old != reg) {
        CHECK_STATUS(dev->backend->rffe_control_write(dev, reg));
    } else {
        log_debug("%s: reg value unchanged? (%08x)\n", __FUNCTION__, reg);
    }

    /* Backend Setup/Teardown/Reset */
    if (dir_pending || backend_clear) {
        if (!dir_enable || backend_clear) {
            CHECK_STATUS(dev->backend->enable_module(dev, dir, false));
        }

        if (dir_enable || backend_clear) {
            CHECK_STATUS(dev->backend->enable_module(dev, dir, true));
        }
    }

    return 0;
}

/******************************************************************************/
/* Gain */
/******************************************************************************/

static int _get_gain_range(struct bladerf *dev,
                           bladerf_channel ch,
                           char const *stage,
                           struct bladerf_gain_range const **gainrange)
{
    NULL_CHECK(gainrange);

    struct bladerf_gain_range const *ranges = NULL;
    size_t ranges_len;
    bladerf_frequency frequency = 0;
    size_t i;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        ranges     = bladerf2_tx_gain_ranges;
        ranges_len = ARRAY_SIZE(bladerf2_tx_gain_ranges);
    } else {
        ranges     = bladerf2_rx_gain_ranges;
        ranges_len = ARRAY_SIZE(bladerf2_rx_gain_ranges);
    }

    CHECK_STATUS(dev->board->get_frequency(dev, ch, &frequency));

    for (i = 0; i < ranges_len; ++i) {
        // if the frequency range matches, and either:
        //  both the range name and the stage name are null, or
        //  neither name is null and the strings match
        // then we found our match
        struct bladerf_gain_range const *range = &(ranges[i]);
        struct bladerf_range const *rfreq      = &(range->frequency);

        if (is_within_range(rfreq, frequency) &&
            ((NULL == range->name && NULL == stage) ||
             (range->name != NULL && stage != NULL &&
              (strcmp(range->name, stage) == 0)))) {
            *gainrange = range;
            return 0;
        }
    }

    return BLADERF_ERR_INVAL;
}

static int _get_gain_offset(struct bladerf *dev,
                            bladerf_channel ch,
                            float *offset)
{
    NULL_CHECK(offset);

    struct bladerf_gain_range const *gainrange = NULL;

    CHECK_STATUS(_get_gain_range(dev, ch, NULL, &gainrange));

    *offset = gainrange->offset;

    return 0;
}

static int bladerf2_get_gain_stage_range(struct bladerf *dev,
                                         bladerf_channel ch,
                                         char const *stage,
                                         struct bladerf_range const **range)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);
    NULL_CHECK(range);

    struct bladerf_gain_range const *ranges = NULL;
    size_t ranges_len;
    bladerf_frequency frequency;
    size_t i;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        ranges     = bladerf2_tx_gain_ranges;
        ranges_len = ARRAY_SIZE(bladerf2_tx_gain_ranges);
    } else {
        ranges     = bladerf2_rx_gain_ranges;
        ranges_len = ARRAY_SIZE(bladerf2_rx_gain_ranges);
    }

    CHECK_STATUS(dev->board->get_frequency(dev, ch, &frequency));

    for (i = 0; i < ranges_len; ++i) {
        struct bladerf_gain_range const *r = &(ranges[i]);
        struct bladerf_range const *rfreq  = &(r->frequency);

        // if the frequency range matches, and either:
        //  both the range name and the stage name are null, or
        //  neither name is null and the strings match
        // then we found our match
        if (is_within_range(rfreq, frequency) &&
            ((NULL == r->name && NULL == stage) ||
             (r->name != NULL && stage != NULL &&
              (strcmp(r->name, stage) == 0)))) {
            *range = &(r->gain);
            return 0;
        }
    }

    return BLADERF_ERR_INVAL;
}

static int bladerf2_get_gain_range(struct bladerf *dev,
                                   bladerf_channel ch,
                                   struct bladerf_range const **range)
{
    return dev->board->get_gain_stage_range(dev, ch, NULL, range);
}

static int bladerf2_get_gain_modes(struct bladerf *dev,
                                   bladerf_channel ch,
                                   struct bladerf_gain_modes const **modes)
{
    NULL_CHECK(modes);

    struct bladerf_gain_modes const *mode_infos;
    unsigned int mode_infos_len;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        mode_infos     = NULL;
        mode_infos_len = 0;
    } else {
        mode_infos     = bladerf2_rx_gain_modes;
        mode_infos_len = ARRAY_SIZE(bladerf2_rx_gain_modes);
    }

    *modes = mode_infos;

    return mode_infos_len;
}

static int bladerf2_get_gain_stages(struct bladerf *dev,
                                    bladerf_channel ch,
                                    char const **stages,
                                    size_t count)
{
    NULL_CHECK(dev);

    struct bladerf_gain_range const *ranges = NULL;
    char const **names                      = NULL;
    size_t stage_count                      = 0;
    unsigned int ranges_len;
    size_t i, j;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        ranges     = bladerf2_tx_gain_ranges;
        ranges_len = ARRAY_SIZE(bladerf2_tx_gain_ranges);
    } else {
        ranges     = bladerf2_rx_gain_ranges;
        ranges_len = ARRAY_SIZE(bladerf2_rx_gain_ranges);
    }

    names = calloc(ranges_len + 1, sizeof(char *));
    if (NULL == names) {
        RETURN_ERROR_STATUS("calloc names", BLADERF_ERR_MEM);
    }

    // Iterate through all the ranges...
    for (i = 0; i < ranges_len; ++i) {
        struct bladerf_gain_range const *range = &(ranges[i]);

        if (NULL == range->name) {
            // this is system gain, skip it
            continue;
        }

        // loop through the output array to make sure we record this one
        for (j = 0; j < ranges_len; ++j) {
            if (NULL == names[j]) {
                // Made it to the end of names without finding a match
                names[j] = range->name;
                ++stage_count;
                break;
            } else if (strcmp(range->name, names[j]) == 0) {
                // found a match, break
                break;
            }
        }
    }

    if (NULL != stages && 0 != count) {
        count = (stage_count < count) ? stage_count : count;

        for (i = 0; i < count; i++) {
            stages[i] = names[i];
        }
    }

    free((char **)names);
    return (int)stage_count;
}

static int bladerf2_get_gain_mode(struct bladerf *dev,
                                  bladerf_channel ch,
                                  bladerf_gain_mode *mode)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);
    NULL_CHECK(mode);

    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;
    uint8_t const rfic_channel             = ch >> 1;
    uint8_t gc_mode;
    bool ok;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        RETURN_ERROR_STATUS("bladerf2_get_gain_mode(tx)",
                            BLADERF_ERR_UNSUPPORTED);
    }

    /* Get the gain */
    CHECK_AD936X(ad9361_get_rx_gain_control_mode(phy, rfic_channel, &gc_mode));

    /* Mode conversion */
    *mode = gainmode_ad9361_to_bladerf(gc_mode, &ok);
    if (!ok) {
        RETURN_INVAL("mode", "is not valid");
    }

    return 0;
}

static int bladerf2_set_gain_mode(struct bladerf *dev,
                                  bladerf_channel ch,
                                  bladerf_gain_mode mode)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;
    uint8_t const rfic_channel             = ch >> 1;
    enum rf_gain_ctrl_mode gc_mode;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        RETURN_ERROR_STATUS("bladerf2_set_gain_mode(tx)",
                            BLADERF_ERR_UNSUPPORTED);
    }

    /* Channel conversion */
    switch (ch) {
        case BLADERF_CHANNEL_RX(0):
            gc_mode = bladerf2_rfic_init_params.gc_rx1_mode;
            break;

        case BLADERF_CHANNEL_RX(1):
            gc_mode = bladerf2_rfic_init_params.gc_rx2_mode;
            break;

        default:
            log_error("%s: unknown channel index (%d)\n", __FUNCTION__, ch);
            return BLADERF_ERR_UNSUPPORTED;
    }

    /* Mode conversion */
    if (mode != BLADERF_GAIN_DEFAULT) {
        bool ok;

        gc_mode = gainmode_bladerf_to_ad9361(mode, &ok);
        if (!ok) {
            RETURN_INVAL("mode", "is not valid");
        }
    }

    /* Set the mode! */
    CHECK_AD936X(ad9361_set_rx_gain_control_mode(phy, rfic_channel, gc_mode));

    return 0;
}

static int bladerf2_get_gain(struct bladerf *dev, bladerf_channel ch, int *gain)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);
    NULL_CHECK(gain);

    float offset;
    int val = 0;

    CHECK_STATUS(_get_gain_offset(dev, ch, &offset));

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        CHECK_STATUS(dev->board->get_gain_stage(dev, ch, "dsa", &val));
    } else {
        CHECK_STATUS(dev->board->get_gain_stage(dev, ch, "full", &val));
    }

    *gain = __round_int(val + offset);

    return 0;
}

static int bladerf2_set_gain(struct bladerf *dev, bladerf_channel ch, int gain)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    float offset = 0.0f;

    CHECK_STATUS(_get_gain_offset(dev, ch, &offset));

    gain -= __round_int(offset);

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        return dev->board->set_gain_stage(dev, ch, "dsa", gain);
    } else {
        return dev->board->set_gain_stage(dev, ch, "full", gain);
    }
}

static int bladerf2_get_gain_stage(struct bladerf *dev,
                                   bladerf_channel ch,
                                   char const *stage,
                                   int *gain)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);
    NULL_CHECK(stage);
    NULL_CHECK(gain);

    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;
    struct bladerf_range const *range      = NULL;
    uint8_t const rfic_channel             = ch >> 1;

    CHECK_STATUS(dev->board->get_gain_stage_range(dev, ch, stage, &range));

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        if (strcmp(stage, "dsa") == 0) {
            uint32_t atten;

            if (board_data->tx_mute[rfic_channel]) {
                /* TX is muted, get cached value */
                atten = txmute_get_cached(phy, ch);
            } else {
                /* Get actual value from hardware */
                CHECK_AD936X(
                    ad9361_get_tx_attenuation(phy, rfic_channel, &atten));
            }

            /* Flip sign, unscale */
            *gain = -(__unscale_int(range, atten));
            return 0;
        }
    } else {
        struct rf_rx_gain rx_gain;

        CHECK_AD936X(ad9361_get_rx_gain(phy, rfic_channel + 1, &rx_gain));

        if (strcmp(stage, "full") == 0) {
            *gain = __unscale_int(range, rx_gain.gain_db);
            return 0;
        } else if (strcmp(stage, "digital") == 0) {
            *gain = __unscale_int(range, rx_gain.digital_gain);
            return 0;
        }
    }

    log_warning("%s: gain stage '%s' invalid\n", __FUNCTION__, stage);
    return 0;
}

static int bladerf2_set_gain_stage(struct bladerf *dev,
                                   bladerf_channel ch,
                                   char const *stage,
                                   int gain)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);
    NULL_CHECK(stage);

    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;
    uint8_t const rfic_channel             = ch >> 1;
    struct bladerf_range const *range      = NULL;

    int val;

    CHECK_STATUS(dev->board->get_gain_stage_range(dev, ch, stage, &range));

    /* Scale/round/clamp as required */
    val = __scale_int(range, clamp_to_range(range, gain));

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        if (strcmp(stage, "dsa") == 0) {
            if (board_data->tx_mute[rfic_channel]) {
                /* TX not currently active, so cache the value */
                CHECK_STATUS(txmute_set_cached(phy, ch, -val));
            } else {
                /* TX is active, so apply immediately */
                CHECK_AD936X(
                    ad9361_set_tx_attenuation(phy, rfic_channel, -val));
            }

            return 0;
        }
    } else {
        if (strcmp(stage, "full") == 0) {
            CHECK_AD936X(ad9361_set_rx_rf_gain(phy, rfic_channel, val));
            return 0;
        }
    }

    log_warning("%s: gain stage '%s' invalid\n", __FUNCTION__, stage);
    return 0;
}


/******************************************************************************/
/* Sample Rate */
/******************************************************************************/

static int bladerf2_get_sample_rate_range(struct bladerf *dev,
                                          bladerf_channel ch,
                                          struct bladerf_range const **range)
{
    NULL_CHECK(range);

    *range = &bladerf2_sample_rate_range;

    return 0;
}

static int bladerf2_get_rational_sample_rate(struct bladerf *dev,
                                             bladerf_channel ch,
                                             struct bladerf_rational_rate *rate)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);
    NULL_CHECK(rate);

    bladerf_sample_rate integer_rate;

    CHECK_STATUS(dev->board->get_sample_rate(dev, ch, &integer_rate));

    rate->integer = integer_rate;
    rate->num     = 0;
    rate->den     = 1;

    return 0;
}

static int bladerf2_set_rational_sample_rate(
    struct bladerf *dev,
    bladerf_channel ch,
    struct bladerf_rational_rate *rate,
    struct bladerf_rational_rate *actual)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);
    NULL_CHECK(rate);

    bladerf_sample_rate integer_rate, actual_integer_rate;

    integer_rate = (bladerf_sample_rate)(rate->integer + rate->num / rate->den);

    CHECK_STATUS(dev->board->set_sample_rate(dev, ch, integer_rate,
                                             &actual_integer_rate));

    if (actual != NULL) {
        CHECK_STATUS(dev->board->get_rational_sample_rate(dev, ch, actual));
    }

    return 0;
}

static int bladerf2_get_sample_rate(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bladerf_sample_rate *rate)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);
    NULL_CHECK(rate);

    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        CHECK_AD936X(ad9361_get_tx_sampling_freq(phy, rate));
    } else {
        CHECK_AD936X(ad9361_get_rx_sampling_freq(phy, rate));
    }

    return 0;
}

static int bladerf2_set_sample_rate(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bladerf_sample_rate rate,
                                    bladerf_sample_rate *actual)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;
    struct bladerf_range const *range      = NULL;

    CHECK_STATUS(dev->board->get_sample_rate_range(dev, ch, &range));

    if (!is_within_range(range, rate)) {
        return BLADERF_ERR_RANGE;
    }

    /* If the requested sample rate is outside of the native range, we must
     * adjust the FIR filtering. */
    if (is_within_range(&bladerf2_sample_rate_range_4x, rate)) {
        bladerf_rfic_rxfir rxfir;
        bladerf_rfic_txfir txfir;

        CHECK_STATUS(bladerf_get_rfic_rx_fir(dev, &rxfir));

        CHECK_STATUS(bladerf_get_rfic_tx_fir(dev, &txfir));

        if (rxfir != BLADERF_RFIC_RXFIR_DEC4 ||
            txfir != BLADERF_RFIC_TXFIR_INT4 ||
            !board_data->low_samplerate_mode) {
            log_debug("enabling 4x decimation/interpolation filters\n");

            board_data->rxfir_orig = rxfir;
            CHECK_STATUS(bladerf_set_rfic_rx_fir(dev, BLADERF_RFIC_RXFIR_DEC4));

            board_data->txfir_orig = txfir;
            CHECK_STATUS(bladerf_set_rfic_tx_fir(dev, BLADERF_RFIC_TXFIR_INT4));

            board_data->low_samplerate_mode = true;
        }
    }

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        CHECK_AD936X(ad9361_set_tx_sampling_freq(phy, rate));
    } else {
        CHECK_AD936X(ad9361_set_rx_sampling_freq(phy, rate));
    }

    if (board_data->low_samplerate_mode &&
        !is_within_range(&bladerf2_sample_rate_range_4x, rate)) {
        log_debug("disabling 4x decimation/interpolation filters\n");

        CHECK_STATUS(bladerf_set_rfic_rx_fir(dev, board_data->rxfir_orig));

        CHECK_STATUS(bladerf_set_rfic_tx_fir(dev, board_data->txfir_orig));

        board_data->low_samplerate_mode = false;
    }

    if (actual != NULL) {
        CHECK_STATUS(dev->board->get_sample_rate(dev, ch, actual));
    }

    /* Warn the user if this isn't achievable */
    check_total_sample_rate(dev);

    return 0;
}


/******************************************************************************/
/* Bandwidth */
/******************************************************************************/

static int bladerf2_get_bandwidth_range(struct bladerf *dev,
                                        bladerf_channel ch,
                                        struct bladerf_range const **range)
{
    NULL_CHECK(range);

    *range = &bladerf2_bandwidth_range;

    return 0;
}

static int bladerf2_get_bandwidth(struct bladerf *dev,
                                  bladerf_channel ch,
                                  bladerf_bandwidth *bandwidth)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);
    NULL_CHECK(bandwidth);

    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        CHECK_AD936X(ad9361_get_tx_rf_bandwidth(phy, bandwidth));
    } else {
        CHECK_AD936X(ad9361_get_rx_rf_bandwidth(phy, bandwidth));
    }

    return 0;
}

static int bladerf2_set_bandwidth(struct bladerf *dev,
                                  bladerf_channel ch,
                                  bladerf_bandwidth bandwidth,
                                  bladerf_bandwidth *actual)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;
    struct bladerf_range const *range      = NULL;

    CHECK_STATUS(dev->board->get_bandwidth_range(dev, ch, &range));

    bandwidth = (bladerf_bandwidth)clamp_to_range(range, bandwidth);

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        CHECK_AD936X(ad9361_set_tx_rf_bandwidth(phy, bandwidth));
    } else {
        CHECK_AD936X(ad9361_set_rx_rf_bandwidth(phy, bandwidth));
    }

    if (actual != NULL) {
        return dev->board->get_bandwidth(dev, ch, actual);
    }

    return 0;
}


/******************************************************************************/
/* Frequency */
/******************************************************************************/

static int bladerf2_get_frequency_range(struct bladerf *dev,
                                        bladerf_channel ch,
                                        const struct bladerf_range **range)
{
    NULL_CHECK(range);

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        *range = &bladerf2_tx_frequency_range;
    } else {
        *range = &bladerf2_rx_frequency_range;
    }

    return 0;
}

static int bladerf2_select_band(struct bladerf *dev,
                                bladerf_channel ch,
                                bladerf_frequency frequency)
{
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;
    uint32_t reg;
    size_t i;

    /* Read RFFE control register */
    CHECK_STATUS(dev->backend->rffe_control_read(dev, &reg));

    /* We have to do this for all the channels sharing the same LO. */
    for (i = 0; i < 2; ++i) {
        bladerf_channel bch = BLADERF_CHANNEL_IS_TX(ch) ? BLADERF_CHANNEL_TX(i)
                                                        : BLADERF_CHANNEL_RX(i);

        /* Is this channel enabled? */
        bool enable = _rffe_ch_enabled(reg, bch);

        /* Update SPDT bits accordingly */
        CHECK_STATUS(_modify_spdt_bits_by_freq(&reg, bch, enable, frequency));
    }

    CHECK_STATUS(dev->backend->rffe_control_write(dev, reg));

    /* Set AD9361 port */
    CHECK_STATUS(
        set_ad9361_port_by_freq(phy, ch, _rffe_ch_enabled(reg, ch), frequency));

    return 0;
}

static int bladerf2_get_frequency(struct bladerf *dev,
                                  bladerf_channel ch,
                                  bladerf_frequency *frequency)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);
    NULL_CHECK(frequency);

    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        CHECK_AD936X(ad9361_get_tx_lo_freq(phy, frequency));
    } else {
        CHECK_AD936X(ad9361_get_rx_lo_freq(phy, frequency));
    }

    return 0;
}

static int bladerf2_set_frequency(struct bladerf *dev,
                                  bladerf_channel ch,
                                  bladerf_frequency frequency)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;
    struct bladerf_range const *range      = NULL;

    CHECK_STATUS(dev->board->get_frequency_range(dev, ch, &range));

    if (!is_within_range(range, frequency)) {
        return BLADERF_ERR_RANGE;
    }

    /* Set up band selection */
    CHECK_STATUS(dev->board->select_band(dev, ch, frequency));

    /* Change LO frequency */
    if (BLADERF_CHANNEL_IS_TX(ch)) {
        CHECK_AD936X(ad9361_set_tx_lo_freq(phy, frequency));
    } else {
        CHECK_AD936X(ad9361_set_rx_lo_freq(phy, frequency));
    }

    return 0;
}


/******************************************************************************/
/* RF ports */
/******************************************************************************/

static int bladerf2_set_rf_port(struct bladerf *dev,
                                bladerf_channel ch,
                                const char *port)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    struct bladerf2_board_data *board_data      = dev->board_data;
    struct ad9361_rf_phy *phy                   = board_data->phy;
    struct bladerf_rfic_port_name_map const *pm = NULL;
    unsigned int pm_len                         = 0;
    uint32_t port_id                            = UINT32_MAX;
    size_t i;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        pm     = bladerf2_tx_port_map;
        pm_len = ARRAY_SIZE(bladerf2_tx_port_map);
    } else {
        pm     = bladerf2_rx_port_map;
        pm_len = ARRAY_SIZE(bladerf2_rx_port_map);
    }

    for (i = 0; i < pm_len; i++) {
        if (strcmp(pm[i].name, port) == 0) {
            port_id = pm[i].id;
            break;
        }
    }

    if (UINT32_MAX == port_id) {
        RETURN_INVAL("port", "is not valid");
    }

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        CHECK_AD936X(ad9361_set_tx_rf_port_output(phy, port_id));
    } else {
        CHECK_AD936X(ad9361_set_rx_rf_port_input(phy, port_id));
    }

    return 0;
}

static int bladerf2_get_rf_port(struct bladerf *dev,
                                bladerf_channel ch,
                                char const **port)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);
    NULL_CHECK(port);

    struct bladerf2_board_data *board_data      = dev->board_data;
    struct ad9361_rf_phy *phy                   = board_data->phy;
    struct bladerf_rfic_port_name_map const *pm = NULL;
    unsigned int pm_len                         = 0;
    uint32_t port_id;
    bool ok;
    size_t i;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        pm     = bladerf2_tx_port_map;
        pm_len = ARRAY_SIZE(bladerf2_tx_port_map);
        CHECK_AD936X(ad9361_get_tx_rf_port_output(phy, &port_id));
    } else {
        pm     = bladerf2_rx_port_map;
        pm_len = ARRAY_SIZE(bladerf2_rx_port_map);
        CHECK_AD936X(ad9361_get_rx_rf_port_input(phy, &port_id));
    }

    ok = false;

    for (i = 0; i < pm_len; i++) {
        if (port_id == pm[i].id) {
            *port = pm[i].name;
            ok    = true;
            break;
        }
    }

    if (!ok) {
        *port = "unknown";
        log_error("%s: unexpected port_id %u\n", __FUNCTION__, port_id);
        return BLADERF_ERR_UNEXPECTED;
    }

    return 0;
}

static int bladerf2_get_rf_ports(struct bladerf *dev,
                                 bladerf_channel ch,
                                 char const **ports,
                                 unsigned int count)
{
    struct bladerf_rfic_port_name_map const *pm = NULL;
    unsigned int pm_len;
    size_t i;

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        pm     = bladerf2_tx_port_map;
        pm_len = ARRAY_SIZE(bladerf2_tx_port_map);
    } else {
        pm     = bladerf2_rx_port_map;
        pm_len = ARRAY_SIZE(bladerf2_rx_port_map);
    }

    if (ports != NULL) {
        count = (pm_len < count) ? pm_len : count;

        for (i = 0; i < count; i++) {
            ports[i] = pm[i].name;
        }
    }

    return pm_len;
}


/******************************************************************************/
/* Scheduled Tuning */
/******************************************************************************/

static int bladerf2_get_quick_tune(struct bladerf *dev,
                                   bladerf_channel ch,
                                   struct bladerf_quick_tune *quick_tune)
{
    const struct band_port_map *port_map;
    struct bladerf2_board_data *board_data;
    bladerf_frequency freq;
    int status;

    if (NULL == quick_tune) {
        RETURN_INVAL("quick_tune", "is null");
    }

    if (ch != BLADERF_CHANNEL_RX(0) && ch != BLADERF_CHANNEL_RX(1) &&
        ch != BLADERF_CHANNEL_TX(0) && ch != BLADERF_CHANNEL_TX(1)) {
        RETURN_INVAL_ARG("channel", ch, "is not valid");
    }

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    board_data = dev->board_data;

    status = bladerf2_get_frequency(dev, ch, &freq);
    if (status < 0) {
        RETURN_ERROR_STATUS("bladerf2_get_frequency", status);
    }

    port_map = _get_band_port_map_by_freq(ch, freq);

    if (BLADERF_CHANNEL_IS_TX(ch)) {

        if( board_data->quick_tune_tx_profile < NUM_BBP_FASTLOCK_PROFILES ) {
            /* Assign Nios and RFFE profile numbers */
            quick_tune->nios_profile = board_data->quick_tune_tx_profile++;
            log_verbose("Quick tune assigned Nios TX fast lock index: %u\n",
                        quick_tune->nios_profile);
            quick_tune->rffe_profile = quick_tune->nios_profile %
                NUM_RFFE_FASTLOCK_PROFILES;
            log_verbose("Quick tune assigned RFFE TX fast lock index: %u\n",
                        quick_tune->rffe_profile);
        } else {
            log_error("Reached maximum number of TX quick tune profiles.");
            return BLADERF_ERR_UNEXPECTED;
        }

        /* Create a fast lock profile in the RFIC */
        status = ad9361_tx_fastlock_store(board_data->phy,
                                          quick_tune->rffe_profile);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_tx_fastlock_store", status);
        }

        /* Save the fast lock profile to quick_tune structure */
        /*status = ad9361_tx_fastlock_save(board_data->phy,
                                         quick_tune->rffe_profile,
                                         &quick_tune->rffe_profile_data[0]);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_tx_fastlock_save", status);
        }*/

        /* Save a copy of the TX fast lock profile to the Nios */
        dev->backend->rffe_fastlock_save(dev, true, quick_tune->rffe_profile,
                                         quick_tune->nios_profile);

        /* Set the TX band */
        quick_tune->port = (port_map->rfic_port << 6);

        /* Set the TX SPDTs */
        quick_tune->spdt = (port_map->spdt << 6) | (port_map->spdt << 4);

    } else {

        if( board_data->quick_tune_rx_profile < NUM_BBP_FASTLOCK_PROFILES ) {
            /* Assign Nios and RFFE profile numbers */
            quick_tune->nios_profile = board_data->quick_tune_rx_profile++;
            log_verbose("Quick tune assigned Nios RX fast lock index: %u\n",
                        quick_tune->nios_profile);
            quick_tune->rffe_profile = quick_tune->nios_profile %
                NUM_RFFE_FASTLOCK_PROFILES;
            log_verbose("Quick tune assigned RFFE RX fast lock index: %u\n",
                        quick_tune->rffe_profile);
        } else {
            log_error("Reached maximum number of RX quick tune profiles.");
            return BLADERF_ERR_UNEXPECTED;
        }

        /* Create a fast lock profile in the RFIC */
        status = ad9361_rx_fastlock_store(board_data->phy,
                                          quick_tune->rffe_profile);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_rx_fastlock_store", status);
        }

        /* Save the fast lock profile to quick_tune structure */
        /*status = ad9361_rx_fastlock_save(board_data->phy,
                                         quick_tune->rffe_profile,
                                         &quick_tune->rffe_profile_data[0]);
        if (status < 0) {
            RETURN_ERROR_AD9361("ad9361_rx_fastlock_save", status);
        }*/

        /* Save a copy of the RX fast lock profile to the Nios */
        dev->backend->rffe_fastlock_save(dev, false, quick_tune->rffe_profile,
                                         quick_tune->nios_profile);

        /* Set the RX bit */
        quick_tune->port = NIOS_PKT_RETUNE2_PORT_IS_RX_MASK;

        /* Set the RX band */
        if (port_map->rfic_port < 3) {
            quick_tune->port |= (3 << (port_map->rfic_port << 1));
        } else {
            quick_tune->port |= (1 << (port_map->rfic_port - 3));
        }

        /* Set the RX SPDTs */
        quick_tune->spdt = (port_map->spdt << 2) | (port_map->spdt);
    }

    return 0;
}

static int bladerf2_schedule_retune(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bladerf_timestamp timestamp,
                                    bladerf_frequency frequency,
                                    struct bladerf_quick_tune *quick_tune)
{
    struct bladerf2_board_data *board_data = dev->board_data;

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    if (!have_cap(board_data->capabilities, BLADERF_CAP_SCHEDULED_RETUNE)) {
        log_debug("This FPGA version (%u.%u.%u) does not support "
                  "scheduled retunes.\n",
                  board_data->fpga_version.major,
                  board_data->fpga_version.minor,
                  board_data->fpga_version.patch);

        return BLADERF_ERR_UNSUPPORTED;
    }

    if (quick_tune == NULL) {
        log_error("Scheduled retunes require a quick tune parameter\n");
        return BLADERF_ERR_UNSUPPORTED;
    }

    return dev->backend->retune2(dev, ch, timestamp,
                                 quick_tune->nios_profile,
                                 quick_tune->rffe_profile,
                                 quick_tune->port,
                                 quick_tune->spdt);
}

static int bladerf2_cancel_scheduled_retunes(struct bladerf *dev,
                                             bladerf_channel ch)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    int status;

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    if (have_cap(board_data->capabilities, BLADERF_CAP_SCHEDULED_RETUNE)) {
        status = dev->backend->retune2(dev, ch, NIOS_PKT_RETUNE2_CLEAR_QUEUE,
                                       0, 0, 0, 0);
    } else {
        log_debug("This FPGA version (%u.%u.%u) does not support "
                  "scheduled retunes.\n",
                  board_data->fpga_version.major,
                  board_data->fpga_version.minor,
                  board_data->fpga_version.patch);

        return BLADERF_ERR_UNSUPPORTED;
    }

    return status;
}


/******************************************************************************/
/* DC/Phase/Gain Correction */
/******************************************************************************/

// clang-format off
static const struct {
    struct {
        uint16_t reg[2];        /* Low/High band */
        unsigned int shift;     /* Value scaling */
    } corr[4];
} ad9361_correction_reg_table[4] = {
    [BLADERF_CHANNEL_RX(0)].corr = {
        [BLADERF_CORR_DCOFF_I] = {
            FIELD_INIT(.reg, {0, 0}),  /* More complex look up */
            FIELD_INIT(.shift, 0),
        },
        [BLADERF_CORR_DCOFF_Q] = {
            FIELD_INIT(.reg, {0, 0}),  /* More complex look up */
            FIELD_INIT(.shift, 0),
        },
        [BLADERF_CORR_PHASE] = {
            FIELD_INIT(.reg, {  AD936X_REG_RX1_INPUT_A_PHASE_CORR,
                                AD936X_REG_RX1_INPUT_BC_PHASE_CORR  }),
            FIELD_INIT(.shift, 6),
        },
        [BLADERF_CORR_GAIN] = {
            FIELD_INIT(.reg, {  AD936X_REG_RX1_INPUT_A_GAIN_CORR,
                                AD936X_REG_RX1_INPUT_BC_PHASE_CORR  }),
            FIELD_INIT(.shift, 6),
        }
    },
    [BLADERF_CHANNEL_RX(1)].corr = {
        [BLADERF_CORR_DCOFF_I] = {
            FIELD_INIT(.reg, {0, 0}), /* More complex look up */
            FIELD_INIT(.shift, 0),
        },
        [BLADERF_CORR_DCOFF_Q] = {
            FIELD_INIT(.reg, {0, 0}), /* More complex look up */
            FIELD_INIT(.shift, 0),
        },
        [BLADERF_CORR_PHASE] = {
            FIELD_INIT(.reg, {  AD936X_REG_RX2_INPUT_A_PHASE_CORR,
                                AD936X_REG_RX2_INPUT_BC_PHASE_CORR  }),
            FIELD_INIT(.shift, 6),
        },
        [BLADERF_CORR_GAIN] = {
            FIELD_INIT(.reg, {  AD936X_REG_RX2_INPUT_A_GAIN_CORR,
                                AD936X_REG_RX2_INPUT_BC_PHASE_CORR  }),
            FIELD_INIT(.shift, 6),
        }
    },
    [BLADERF_CHANNEL_TX(0)].corr = {
        [BLADERF_CORR_DCOFF_I] = {
            FIELD_INIT(.reg, {  AD936X_REG_TX1_OUT_1_OFFSET_I,
                                AD936X_REG_TX1_OUT_2_OFFSET_I   }),
            FIELD_INIT(.shift, 5),
        },
        [BLADERF_CORR_DCOFF_Q] = {
            FIELD_INIT(.reg, {  AD936X_REG_TX1_OUT_1_OFFSET_Q,
                                AD936X_REG_TX1_OUT_2_OFFSET_Q   }),
            FIELD_INIT(.shift, 5),
        },
        [BLADERF_CORR_PHASE] = {
            FIELD_INIT(.reg, {  AD936X_REG_TX1_OUT_1_PHASE_CORR,
                                AD936X_REG_TX1_OUT_2_PHASE_CORR }),
            FIELD_INIT(.shift, 6),
        },
        [BLADERF_CORR_GAIN] = {
            FIELD_INIT(.reg, {  AD936X_REG_TX1_OUT_1_GAIN_CORR,
                                AD936X_REG_TX1_OUT_2_GAIN_CORR  }),
            FIELD_INIT(.shift, 6),
        }
    },
    [BLADERF_CHANNEL_TX(1)].corr = {
        [BLADERF_CORR_DCOFF_I] = {
            FIELD_INIT(.reg, {  AD936X_REG_TX2_OUT_1_OFFSET_I,
                                AD936X_REG_TX2_OUT_2_OFFSET_I   }),
            FIELD_INIT(.shift, 5),
        },
        [BLADERF_CORR_DCOFF_Q] = {
            FIELD_INIT(.reg, {  AD936X_REG_TX2_OUT_1_OFFSET_Q,
                                AD936X_REG_TX2_OUT_2_OFFSET_Q   }),
            FIELD_INIT(.shift, 5),
        },
        [BLADERF_CORR_PHASE] = {
            FIELD_INIT(.reg, {  AD936X_REG_TX2_OUT_1_PHASE_CORR,
                                AD936X_REG_TX2_OUT_2_PHASE_CORR }),
            FIELD_INIT(.shift, 6),
        },
        [BLADERF_CORR_GAIN] = {
            FIELD_INIT(.reg, {  AD936X_REG_TX2_OUT_1_GAIN_CORR,
                                AD936X_REG_TX2_OUT_2_GAIN_CORR  }),
            FIELD_INIT(.shift, 6),
        }
    },
};

static const struct {
    uint16_t reg_top;
    uint16_t reg_bot;
} ad9361_correction_rx_dcoff_reg_table[4][2][2] = {
    /* Channel 1 */
    [BLADERF_CHANNEL_RX(0)] = {
        /* A band */
        {
            /* I */
            {AD936X_REG_INPUT_A_OFFSETS_1, AD936X_REG_RX1_INPUT_A_OFFSETS},
            /* Q */
            {AD936X_REG_RX1_INPUT_A_OFFSETS, AD936X_REG_RX1_INPUT_A_Q_OFFSET},
        },
        /* B/C band */
        {
            /* I */
            {AD936X_REG_INPUT_BC_OFFSETS_1, AD936X_REG_RX1_INPUT_BC_OFFSETS},
            /* Q */
            {AD936X_REG_RX1_INPUT_BC_OFFSETS, AD936X_REG_RX1_INPUT_BC_Q_OFFSET},
        },
    },
    /* Channel 2 */
    [BLADERF_CHANNEL_RX(1)] = {
        /* A band */
        {
            /* I */
            {AD936X_REG_RX2_INPUT_A_I_OFFSET, AD936X_REG_RX2_INPUT_A_OFFSETS},
            /* Q */
            {AD936X_REG_RX2_INPUT_A_OFFSETS, AD936X_REG_INPUT_A_OFFSETS_1},
        },
        /* B/C band */
        {
            /* I */
            {AD936X_REG_RX2_INPUT_BC_I_OFFSET, AD936X_REG_RX2_INPUT_BC_OFFSETS},
            /* Q */
            {AD936X_REG_RX2_INPUT_BC_OFFSETS, AD936X_REG_INPUT_BC_OFFSETS_1},
        },
    },
};

static const int ad9361_correction_force_bit[2][4][2] = {
    [0] = {
        [BLADERF_CORR_DCOFF_I] = {2, 6},
        [BLADERF_CORR_DCOFF_Q] = {2, 6},
        [BLADERF_CORR_PHASE]   = {0, 4},
        [BLADERF_CORR_GAIN]    = {0, 4},
    },
    [1] = {
        [BLADERF_CORR_DCOFF_I] = {3, 7},
        [BLADERF_CORR_DCOFF_Q] = {3, 7},
        [BLADERF_CORR_PHASE]   = {1, 5},
        [BLADERF_CORR_GAIN]    = {1, 5},
    },
};
// clang-format on

static int bladerf2_get_correction(struct bladerf *dev,
                                   bladerf_channel ch,
                                   bladerf_correction corr,
                                   int16_t *value)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);
    NULL_CHECK(value);

    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;
    bool low_band;
    uint16_t reg, data;
    unsigned int shift;
    int32_t val;

    /* Validate channel */
    if (ch != BLADERF_CHANNEL_RX(0) && ch != BLADERF_CHANNEL_RX(1) &&
        ch != BLADERF_CHANNEL_TX(0) && ch != BLADERF_CHANNEL_TX(1)) {
        RETURN_INVAL_ARG("channel", ch, "is not valid");
    }

    /* Validate correction */
    if (corr != BLADERF_CORR_DCOFF_I && corr != BLADERF_CORR_DCOFF_Q &&
        corr != BLADERF_CORR_PHASE && corr != BLADERF_CORR_GAIN) {
        RETURN_ERROR_STATUS("corr", BLADERF_ERR_UNSUPPORTED);
    }

    /* Look up band */
    if (BLADERF_CHANNEL_IS_TX(ch)) {
        uint32_t mode;

        CHECK_AD936X(ad9361_get_tx_rf_port_output(phy, &mode));

        low_band = (mode == AD936X_TXA);
    } else {
        uint32_t mode;

        CHECK_AD936X(ad9361_get_rx_rf_port_input(phy, &mode));

        /* Check if RX RF port mode is supported */
        if (mode != AD936X_A_BALANCED && mode != AD936X_B_BALANCED &&
            mode != AD936X_C_BALANCED) {
            RETURN_ERROR_STATUS("mode", BLADERF_ERR_UNSUPPORTED);
        }

        low_band = (mode == AD936X_A_BALANCED);
    }

    if ((corr == BLADERF_CORR_DCOFF_I || corr == BLADERF_CORR_DCOFF_Q) &&
        (ch & BLADERF_DIRECTION_MASK) == BLADERF_RX) {
        /* RX DC offset corrections are stuffed in a super convoluted way in
         * the register map. See AD9361 register map page 51. */
        bool is_q = (corr == BLADERF_CORR_DCOFF_Q);
        uint8_t data_top, data_bot;
        uint16_t data;

        /* Read top register */
        val = ad9361_spi_read(
            phy->spi,
            ad9361_correction_rx_dcoff_reg_table[ch][low_band][is_q].reg_top);
        if (val < 0) {
            RETURN_ERROR_AD9361("ad9361_spi_read(top)", val);
        }

        data_top = val;

        /* Read bottom register */
        val = ad9361_spi_read(
            phy->spi,
            ad9361_correction_rx_dcoff_reg_table[ch][low_band][is_q].reg_bot);
        if (val < 0) {
            RETURN_ERROR_AD9361("ad9361_spi_read(bottom)", val);
        }

        data_bot = val;

        if (ch == BLADERF_CHANNEL_RX(0)) {
            if (!is_q) {
                /*    top: | x x x x 9 8 7 6 | */
                /* bottom: | 5 4 3 2 1 0 x x | */
                data = ((data_top & 0xf) << 6) | (data_bot >> 2);
            } else {
                /*    top: | x x x x x x 9 8 | */
                /* bottom: | 7 6 5 4 3 2 1 0 | */
                data = ((data_top & 0x3) << 8) | data_bot;
            }
        } else {
            if (!is_q) {
                /*    top: | 9 8 7 6 5 4 3 2 | */
                /* bottom: | x x x x x x 1 0 | */
                data = (data_top << 2) | (data_bot & 0x3);
            } else {
                /*    top: | x x 9 8 7 6 5 4 | */
                /* bottom: | 3 2 1 0 x x x x | */
                data = (data_top << 4) | (data_bot >> 4);
            }
        }

        /* Scale 10-bit to 13-bit */
        data = data << 3;

        /* Sign extend value */
        *value = data | ((data & (1 << 12)) ? 0xf000 : 0x0000);
    } else {
        /* Look up correction register and value shift in table */
        reg   = ad9361_correction_reg_table[ch].corr[corr].reg[low_band];
        shift = ad9361_correction_reg_table[ch].corr[corr].shift;

        /* Read register and scale value */
        val = ad9361_spi_read(phy->spi, reg);
        if (val < 0) {
            RETURN_ERROR_AD9361("ad9361_spi_read(reg)", val);
        }

        /* Scale 8-bit to 12-bit/13-bit */
        data = val << shift;

        /* Sign extend value */
        if (shift == 5) {
            *value = data | ((data & (1 << 12)) ? 0xf000 : 0x0000);
        } else {
            *value = data | ((data & (1 << 13)) ? 0xc000 : 0x0000);
        }
    }

    return 0;
}

static int bladerf2_set_correction(struct bladerf *dev,
                                   bladerf_channel ch,
                                   bladerf_correction corr,
                                   int16_t value)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;
    bool low_band;
    uint16_t reg, data;
    unsigned int shift;
    int32_t val;

    /* Validate channel */
    if (ch != BLADERF_CHANNEL_RX(0) && ch != BLADERF_CHANNEL_RX(1) &&
        ch != BLADERF_CHANNEL_TX(0) && ch != BLADERF_CHANNEL_TX(1)) {
        RETURN_INVAL_ARG("channel", ch, "is not valid");
    }

    /* Validate correction */
    if (corr != BLADERF_CORR_DCOFF_I && corr != BLADERF_CORR_DCOFF_Q &&
        corr != BLADERF_CORR_PHASE && corr != BLADERF_CORR_GAIN) {
        RETURN_ERROR_STATUS("corr", BLADERF_ERR_UNSUPPORTED);
    }

    /* Look up band */
    if (BLADERF_CHANNEL_IS_TX(ch)) {
        uint32_t mode;

        CHECK_AD936X(ad9361_get_tx_rf_port_output(phy, &mode));

        low_band = (mode == AD936X_TXA);
    } else {
        uint32_t mode;

        CHECK_AD936X(ad9361_get_rx_rf_port_input(phy, &mode));

        /* Check if RX RF port mode is supported */
        if (mode != AD936X_A_BALANCED && mode != AD936X_B_BALANCED &&
            mode != AD936X_C_BALANCED) {
            RETURN_ERROR_STATUS("mode", BLADERF_ERR_UNSUPPORTED);
        }

        low_band = (mode == AD936X_A_BALANCED);
    }

    if ((corr == BLADERF_CORR_DCOFF_I || corr == BLADERF_CORR_DCOFF_Q) &&
        (ch & BLADERF_DIRECTION_MASK) == BLADERF_RX) {
        /* RX DC offset corrections are stuffed in a super convoluted way in
         * the register map. See AD9361 register map page 51. */
        bool is_q = (corr == BLADERF_CORR_DCOFF_Q);
        uint8_t data_top, data_bot;

        /* Scale 13-bit to 10-bit */
        data = value >> 3;

        /* Read top register */
        val = ad9361_spi_read(
            phy->spi,
            ad9361_correction_rx_dcoff_reg_table[ch][low_band][is_q].reg_top);
        if (val < 0) {
            RETURN_ERROR_AD9361("ad9361_spi_read(top)", val);
        }

        data_top = val;

        /* Read bottom register */
        val = ad9361_spi_read(
            phy->spi,
            ad9361_correction_rx_dcoff_reg_table[ch][low_band][is_q].reg_bot);
        if (val < 0) {
            RETURN_ERROR_AD9361("ad9361_spi_read(bottom)", val);
        }

        data_bot = val;

        /* Modify registers */
        if (ch == BLADERF_CHANNEL_RX(0)) {
            if (!is_q) {
                /*    top: | x x x x 9 8 7 6 | */
                /* bottom: | 5 4 3 2 1 0 x x | */
                data_top = (data_top & 0xf0) | ((data >> 6) & 0x0f);
                data_bot = (data_bot & 0x03) | ((data & 0x3f) << 2);
            } else {
                /*    top: | x x x x x x 9 8 | */
                /* bottom: | 7 6 5 4 3 2 1 0 | */
                data_top = (data_top & 0xfc) | ((data >> 8) & 0x03);
                data_bot = data & 0xff;
            }
        } else {
            if (!is_q) {
                /*    top: | 9 8 7 6 5 4 3 2 | */
                /* bottom: | x x x x x x 1 0 | */
                data_top = (data >> 2) & 0xff;
                data_bot = (data_bot & 0xfc) | (data & 0x03);
            } else {
                /*    top: | x x 9 8 7 6 5 4 | */
                /* bottom: | 3 2 1 0 x x x x | */
                data_top = (data & 0xc0) | ((data >> 4) & 0x3f);
                data_bot = (data & 0x0f) | ((data & 0x0f) << 4);
            }
        }

        /* Write top register */
        CHECK_AD936X(ad9361_spi_write(
            phy->spi,
            ad9361_correction_rx_dcoff_reg_table[ch][low_band][is_q].reg_top,
            data_top));

        /* Write bottom register */
        CHECK_AD936X(ad9361_spi_write(
            phy->spi,
            ad9361_correction_rx_dcoff_reg_table[ch][low_band][is_q].reg_bot,
            data_bot));
    } else {
        /* Look up correction register and value shift in table */
        reg   = ad9361_correction_reg_table[ch].corr[corr].reg[low_band];
        shift = ad9361_correction_reg_table[ch].corr[corr].shift;

        /* Scale 12-bit/13-bit to 8-bit */
        data = (value >> shift) & 0xff;

        /* Write register */
        CHECK_AD936X(ad9361_spi_write(phy->spi, reg, data & 0xff));
    }

    reg = (BLADERF_CHANNEL_IS_TX(ch)) ? AD936X_REG_TX_FORCE_BITS
                                      : AD936X_REG_FORCE_BITS;

    /* Read force bit register */
    val = ad9361_spi_read(phy->spi, reg);
    if (val < 0) {
        RETURN_ERROR_AD9361("ad9361_spi_read(force)", val);
    }

    /* Modify register */
    data = val | (1 << ad9361_correction_force_bit[ch >> 1][corr][low_band]);

    /* Write force bit register */
    CHECK_AD936X(ad9361_spi_write(phy->spi, reg, data));

    return 0;
}


/******************************************************************************/
/* Trigger */
/******************************************************************************/

static int bladerf2_trigger_init(struct bladerf *dev,
                                 bladerf_channel ch,
                                 bladerf_trigger_signal signal,
                                 struct bladerf_trigger *trigger)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);
    NULL_CHECK(trigger);

    return fpga_trigger_init(dev, ch, signal, trigger);
}

static int bladerf2_trigger_arm(struct bladerf *dev,
                                struct bladerf_trigger const *trigger,
                                bool arm,
                                uint64_t resv1,
                                uint64_t resv2)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);
    NULL_CHECK(trigger);

    return fpga_trigger_arm(dev, trigger, arm);
}

static int bladerf2_trigger_fire(struct bladerf *dev,
                                 struct bladerf_trigger const *trigger)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);
    NULL_CHECK(trigger);

    return fpga_trigger_fire(dev, trigger);
}

static int bladerf2_trigger_state(struct bladerf *dev,
                                  struct bladerf_trigger const *trigger,
                                  bool *is_armed,
                                  bool *has_fired,
                                  bool *fire_requested,
                                  uint64_t *resv1,
                                  uint64_t *resv2)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);
    NULL_CHECK(trigger);
    NULL_CHECK(is_armed);
    NULL_CHECK(has_fired);
    NULL_CHECK(fire_requested);

    /* Reserved for future metadata (e.g., trigger counts, timestamp) */
    if (resv1 != NULL) {
        *resv1 = 0;
    }

    if (resv2 != NULL) {
        *resv2 = 0;
    }

    return fpga_trigger_state(dev, trigger, is_armed, has_fired,
                              fire_requested);
}


/******************************************************************************/
/* Streaming */
/******************************************************************************/

static int bladerf2_init_stream(struct bladerf_stream **stream,
                                struct bladerf *dev,
                                bladerf_stream_cb callback,
                                void ***buffers,
                                size_t num_buffers,
                                bladerf_format format,
                                size_t samples_per_buffer,
                                size_t num_transfers,
                                void *user_data)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return async_init_stream(stream, dev, callback, buffers, num_buffers,
                             format, samples_per_buffer, num_transfers,
                             user_data);
}

static int bladerf2_stream(struct bladerf_stream *stream,
                           bladerf_channel_layout layout)
{
    bladerf_direction dir = layout & BLADERF_DIRECTION_MASK;
    int rv;

    switch (layout) {
        case BLADERF_RX_X1:
        case BLADERF_RX_X2:
        case BLADERF_TX_X1:
        case BLADERF_TX_X2:
            break;
        default:
            return -EINVAL;
    }

    CHECK_STATUS(perform_format_config(stream->dev, dir, stream->format));

    rv = async_run_stream(stream, layout);

    CHECK_STATUS(perform_format_deconfig(stream->dev, dir));

    return rv;
}

static int bladerf2_submit_stream_buffer(struct bladerf_stream *stream,
                                         void *buffer,
                                         unsigned int timeout_ms,
                                         bool nonblock)
{
    return async_submit_stream_buffer(stream, buffer, timeout_ms, nonblock);
}

static void bladerf2_deinit_stream(struct bladerf_stream *stream)
{
    async_deinit_stream(stream);
}

static int bladerf2_set_stream_timeout(struct bladerf *dev,
                                       bladerf_direction dir,
                                       unsigned int timeout)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    struct bladerf2_board_data *board_data = dev->board_data;

    WITH_MUTEX(&board_data->sync[dir].lock,
               { board_data->sync[dir].stream_config.timeout_ms = timeout; });

    return 0;
}

static int bladerf2_get_stream_timeout(struct bladerf *dev,
                                       bladerf_direction dir,
                                       unsigned int *timeout)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);
    NULL_CHECK(timeout);

    struct bladerf2_board_data *board_data = dev->board_data;

    WITH_MUTEX(&board_data->sync[dir].lock,
               { *timeout = board_data->sync[dir].stream_config.timeout_ms; });

    return 0;
}

static int bladerf2_sync_config(struct bladerf *dev,
                                bladerf_channel_layout layout,
                                bladerf_format format,
                                unsigned int num_buffers,
                                unsigned int buffer_size,
                                unsigned int num_transfers,
                                unsigned int stream_timeout)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    struct bladerf2_board_data *board_data = dev->board_data;

    bladerf_direction dir = layout & BLADERF_DIRECTION_MASK;
    int status;

    switch (layout) {
        case BLADERF_RX_X1:
        case BLADERF_RX_X2:
        case BLADERF_TX_X1:
        case BLADERF_TX_X2:
            break;
        default:
            return -EINVAL;
    }

    status = perform_format_config(dev, dir, format);
    if (0 == status) {
        status = sync_init(&board_data->sync[dir], dev, layout, format,
                           num_buffers, buffer_size, board_data->msg_size,
                           num_transfers, stream_timeout);
        if (status != 0) {
            perform_format_deconfig(dev, dir);
        }
    }

    return status;
}

static int bladerf2_sync_tx(struct bladerf *dev,
                            void const *samples,
                            unsigned int num_samples,
                            struct bladerf_metadata *metadata,
                            unsigned int timeout_ms)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    struct bladerf2_board_data *board_data = dev->board_data;

    if (!board_data->sync[BLADERF_TX].initialized) {
        RETURN_INVAL("sync tx", "not initialized");
    }

    return sync_tx(&board_data->sync[BLADERF_TX], samples, num_samples,
                   metadata, timeout_ms);
}

static int bladerf2_sync_rx(struct bladerf *dev,
                            void *samples,
                            unsigned int num_samples,
                            struct bladerf_metadata *metadata,
                            unsigned int timeout_ms)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    struct bladerf2_board_data *board_data = dev->board_data;

    if (!board_data->sync[BLADERF_RX].initialized) {
        RETURN_INVAL("sync rx", "not initialized");
    }

    return sync_rx(&board_data->sync[BLADERF_RX], samples, num_samples,
                   metadata, timeout_ms);
}

static int bladerf2_get_timestamp(struct bladerf *dev,
                                  bladerf_direction dir,
                                  bladerf_timestamp *value)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);
    NULL_CHECK(value);

    return dev->backend->get_timestamp(dev, dir, value);
}


/******************************************************************************/
/* FPGA/Firmware Loading/Flashing */
/******************************************************************************/

static int bladerf2_load_fpga(struct bladerf *dev,
                              uint8_t const *buf,
                              size_t length)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);
    NULL_CHECK(buf);

    struct bladerf2_board_data *board_data = dev->board_data;

    if (!is_valid_fpga_size(board_data->fpga_size, length)) {
        RETURN_INVAL("fpga file", "incorrect file size");
    }

    CHECK_STATUS(dev->backend->load_fpga(dev, buf, length));

    /* Update device state */
    board_data->state = STATE_FPGA_LOADED;

    CHECK_STATUS(_bladerf2_initialize(dev));

    return 0;
}

static int bladerf2_flash_fpga(struct bladerf *dev,
                               uint8_t const *buf,
                               size_t length)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);
    NULL_CHECK(buf);

    struct bladerf2_board_data *board_data = dev->board_data;

    if (!is_valid_fpga_size(board_data->fpga_size, length)) {
        RETURN_INVAL("fpga file", "incorrect file size");
    }

    return spi_flash_write_fpga_bitstream(dev, buf, length);
}

static int bladerf2_erase_stored_fpga(struct bladerf *dev)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    return spi_flash_erase_fpga(dev);
}

static int bladerf2_flash_firmware(struct bladerf *dev,
                                   uint8_t const *buf,
                                   size_t length)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);
    NULL_CHECK(buf);

    char const env_override[] = "BLADERF_SKIP_FW_SIZE_CHECK";

    /* Sanity check firmware length.
     *
     * TODO in the future, better sanity checks can be performed when
     *      using the bladerf image format currently used to backup/restore
     *      calibration data
     */
    if (!getenv(env_override) && !is_valid_fw_size(length)) {
        log_info("Detected potentially invalid firmware file.\n");
        log_info("Define BLADERF_SKIP_FW_SIZE_CHECK in your environment "
                 "to skip this check.\n");
        RETURN_INVAL_ARG("firmware size", length, "is not valid");
    }

    return spi_flash_write_fx3_fw(dev, buf, length);
}

static int bladerf2_device_reset(struct bladerf *dev)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    return dev->backend->device_reset(dev);
}


/******************************************************************************/
/* Tuning mode */
/******************************************************************************/

static int bladerf2_set_tuning_mode(struct bladerf *dev,
                                    bladerf_tuning_mode mode)
{
    struct bladerf2_board_data *board_data = dev->board_data;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (mode == BLADERF_TUNING_MODE_FPGA &&
        !have_cap(board_data->capabilities, BLADERF_CAP_FPGA_TUNING)) {
        log_debug("The loaded FPGA version (%u.%u.%u) does not support the "
                  "provided tuning mode (%d)\n",
                  board_data->fpga_version.major, board_data->fpga_version.minor,
                  board_data->fpga_version.patch, mode);
        return BLADERF_ERR_UNSUPPORTED;
    }

    switch (mode) {
        case BLADERF_TUNING_MODE_HOST:
            log_debug("Tuning mode: host\n");
            break;
        default:
            assert(!"Invalid tuning mode.");
            return BLADERF_ERR_INVAL;
    }

    board_data->tuning_mode = mode;

    return 0;
}

static int bladerf2_get_tuning_mode(struct bladerf *dev,
                                    bladerf_tuning_mode *mode)
{
    struct bladerf2_board_data *board_data = dev->board_data;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    *mode = board_data->tuning_mode;

    return 0;
}


/******************************************************************************/
/* Loopback */
/******************************************************************************/

static int bladerf2_get_loopback_modes(
    struct bladerf *dev, struct bladerf_loopback_modes const **modes)
{
    if (modes != NULL) {
        *modes = bladerf2_loopback_modes;
    }

    return ARRAY_SIZE(bladerf2_loopback_modes);
}

static int bladerf2_set_loopback(struct bladerf *dev, bladerf_loopback mode)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;
    bool firmware_loopback                 = false;
    int32_t bist_loopback                  = 0;

    switch (mode) {
        case BLADERF_LB_NONE:
            break;
        case BLADERF_LB_FIRMWARE:
            firmware_loopback = true;
            break;
        case BLADERF_LB_RFIC_BIST:
            bist_loopback = 1;
            break;
        default:
            log_error("%s: unknown loopback mode (%d)\n", __FUNCTION__, mode);
            return BLADERF_ERR_UNEXPECTED;
    }

    /* Set digital loopback state */
    CHECK_AD936X(ad9361_bist_loopback(phy, bist_loopback));

    /* Set firmware loopback state */
    CHECK_STATUS(dev->backend->set_firmware_loopback(dev, firmware_loopback));

    return 0;
}

static int bladerf2_get_loopback(struct bladerf *dev, bladerf_loopback *mode)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);
    NULL_CHECK(mode);

    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;
    int32_t ad9361_loopback;
    bool fw_loopback;

    /* Read firwmare loopback */
    CHECK_STATUS(dev->backend->get_firmware_loopback(dev, &fw_loopback));

    if (fw_loopback) {
        *mode = BLADERF_LB_FIRMWARE;
        return 0;
    }

    /* Note: this returns void */
    ad9361_get_bist_loopback(phy, &ad9361_loopback);

    if (ad9361_loopback == 1) {
        *mode = BLADERF_LB_RFIC_BIST;
        return 0;
    }

    *mode = BLADERF_LB_NONE;

    return 0;
}


/******************************************************************************/
/* Sample RX FPGA Mux */
/******************************************************************************/

static int bladerf2_set_rx_mux(struct bladerf *dev, bladerf_rx_mux mode)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    uint32_t rx_mux_val, config_gpio;

    /* Validate desired mux mode */
    switch (mode) {
        case BLADERF_RX_MUX_BASEBAND:
        case BLADERF_RX_MUX_12BIT_COUNTER:
        case BLADERF_RX_MUX_32BIT_COUNTER:
        case BLADERF_RX_MUX_DIGITAL_LOOPBACK:
            rx_mux_val = ((uint32_t)mode) << BLADERF_GPIO_RX_MUX_SHIFT;
            break;

        default:
            log_debug("Invalid RX mux mode setting passed to %s(): %d\n", mode,
                      __FUNCTION__);
            RETURN_INVAL_ARG("bladerf_rx_mux", mode, "is invalid");
    }

    CHECK_STATUS(dev->backend->config_gpio_read(dev, &config_gpio));

    /* Clear out and assign the associated RX mux bits */
    config_gpio &= ~BLADERF_GPIO_RX_MUX_MASK;
    config_gpio |= rx_mux_val;

    CHECK_STATUS(dev->backend->config_gpio_write(dev, config_gpio));

    return 0;
}

static int bladerf2_get_rx_mux(struct bladerf *dev, bladerf_rx_mux *mode)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);
    NULL_CHECK(mode);

    bladerf_rx_mux val;
    uint32_t config_gpio;

    CHECK_STATUS(dev->backend->config_gpio_read(dev, &config_gpio));

    /* Extract RX mux bits */
    config_gpio &= BLADERF_GPIO_RX_MUX_MASK;
    config_gpio >>= BLADERF_GPIO_RX_MUX_SHIFT;
    val = config_gpio;

    /* Ensure it's a valid/supported value */
    switch (val) {
        case BLADERF_RX_MUX_BASEBAND:
        case BLADERF_RX_MUX_12BIT_COUNTER:
        case BLADERF_RX_MUX_32BIT_COUNTER:
        case BLADERF_RX_MUX_DIGITAL_LOOPBACK:
            *mode = val;
            break;

        default:
            *mode = BLADERF_RX_MUX_INVALID;
            log_debug("Invalid rx mux mode %d read from config gpio\n", val);
            return BLADERF_ERR_UNEXPECTED;
    }

    return 0;
}


/******************************************************************************/
/* Low-level VCTCXO Tamer Mode */
/******************************************************************************/

static int bladerf2_set_vctcxo_tamer_mode(struct bladerf *dev,
                                          bladerf_vctcxo_tamer_mode mode)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_get_vctcxo_tamer_mode(struct bladerf *dev,
                                          bladerf_vctcxo_tamer_mode *mode)
{
    return BLADERF_ERR_UNSUPPORTED;
}


/******************************************************************************/
/* Low-level VCTCXO Trim DAC access */
/******************************************************************************/

static int _bladerf2_get_trim_dac_enable(struct bladerf *dev, bool *enable)
{
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);
    NULL_CHECK(enable);

    uint16_t trim;

    // Read current trim DAC setting
    CHECK_STATUS(dev->backend->ad56x1_vctcxo_trim_dac_read(dev, &trim));

    // Determine if it's enabled...
    *enable = (TRIMDAC_EN_ACTIVE == (trim >> TRIMDAC_EN));

    log_debug("trim DAC is %s\n", (*enable ? "enabled" : "disabled"));

    if ((trim >> TRIMDAC_EN) != TRIMDAC_EN_ACTIVE &&
        (trim >> TRIMDAC_EN) != TRIMDAC_EN_HIGHZ) {
        log_warning("unknown trim DAC state: 0x%x\n", (trim >> TRIMDAC_EN));
    }

    return 0;
}

static int _bladerf2_set_trim_dac_enable(struct bladerf *dev, bool enable)
{
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    struct bladerf2_board_data *board_data = dev->board_data;
    uint16_t trim;
    bool current_state;

    // See if we have anything to do
    CHECK_STATUS(_bladerf2_get_trim_dac_enable(dev, &current_state));

    if (enable == current_state) {
        log_debug("trim DAC already %s, nothing to do\n",
                  enable ? "enabled" : "disabled");
        return 0;
    }

    // Read current trim DAC setting
    CHECK_STATUS(dev->backend->ad56x1_vctcxo_trim_dac_read(dev, &trim));

    // Set the trim DAC to high z if applicable
    if (!enable && trim != (TRIMDAC_EN_HIGHZ << TRIMDAC_EN)) {
        board_data->trimdac_last_value = trim;
        log_debug("saving current trim DAC value: 0x%04x\n", trim);
        trim = TRIMDAC_EN_HIGHZ << TRIMDAC_EN;
    } else if (enable && trim == (TRIMDAC_EN_HIGHZ << TRIMDAC_EN)) {
        trim = board_data->trimdac_last_value;
        log_debug("restoring old trim DAC value: 0x%04x\n", trim);
    }

    // Write back the trim DAC setting
    CHECK_STATUS(dev->backend->ad56x1_vctcxo_trim_dac_write(dev, trim));

    // Update our state flag
    board_data->trim_source = enable ? TRIM_SOURCE_TRIM_DAC : TRIM_SOURCE_NONE;

    return 0;
}

/**
 * @brief      Read the VCTCXO trim value from the SPI flash
 *
 * Retrieves the factory VCTCXO value from the SPI flash. This function
 * should not be used while sample streaming is in progress.
 *
 * @param      dev   Device handle
 * @param      trim  Pointer to populate with the trim value
 *
 * @return     0 on success, value from \ref RETCODES list on failure
 */
static int bladerf2_read_flash_vctcxo_trim(struct bladerf *dev, uint16_t *trim)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);
    NULL_CHECK(trim);

    int status;

    status = spi_flash_read_vctcxo_trim(dev, trim);
    if (status < 0) {
        log_warning("Failed to get VCTCXO trim value: %s\n",
                    bladerf_strerror(status));
        log_debug("Defaulting DAC trim to 0x1ffc.\n");
        *trim = 0x1ffc;
        return 0;
    }

    return status;
}

static int bladerf2_get_vctcxo_trim(struct bladerf *dev, uint16_t *trim)
{
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);
    NULL_CHECK(trim);

    struct bladerf2_board_data *board_data = dev->board_data;

    *trim = board_data->trimdac_stored_value;

    return 0;
}

static int bladerf2_trim_dac_read(struct bladerf *dev, uint16_t *trim)
{
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);
    NULL_CHECK(trim);

    return dev->backend->ad56x1_vctcxo_trim_dac_read(dev, trim);
}

static int bladerf2_trim_dac_write(struct bladerf *dev, uint16_t trim)
{
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    struct bladerf2_board_data *board_data = dev->board_data;
    uint16_t trim_control = (trim >> TRIMDAC_EN) & TRIMDAC_EN_MASK;
    uint16_t trim_value   = trim & TRIMDAC_MASK;
    bool enable;

    log_debug("requested trim 0x%04x (control 0x%01x value 0x%04x)\n", trim,
              trim_control, trim_value);

    // Is the trimdac enabled?
    CHECK_STATUS(_bladerf2_get_trim_dac_enable(dev, &enable));

    // If the trimdac is not enabled, save this value for later but don't
    // apply it.
    if (!enable && trim_control != TRIMDAC_EN_HIGHZ) {
        log_warning("trim DAC is disabled. New value will be saved until "
                    "trim DAC is enabled\n");
        board_data->trimdac_last_value = trim_value;
        return 0;
    }

    return dev->backend->ad56x1_vctcxo_trim_dac_write(dev, trim);
}


/******************************************************************************/
/* Low-level Trigger control access */
/******************************************************************************/

static int bladerf2_read_trigger(struct bladerf *dev,
                                 bladerf_channel ch,
                                 bladerf_trigger_signal trigger,
                                 uint8_t *val)
{
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);
    NULL_CHECK(val);

    return fpga_trigger_read(dev, ch, trigger, val);
}

static int bladerf2_write_trigger(struct bladerf *dev,
                                  bladerf_channel ch,
                                  bladerf_trigger_signal trigger,
                                  uint8_t val)
{
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    return fpga_trigger_write(dev, ch, trigger, val);
}


/******************************************************************************/
/* Low-level Configuration GPIO access */
/******************************************************************************/

static int bladerf2_config_gpio_read(struct bladerf *dev, uint32_t *val)
{
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);
    NULL_CHECK(val);

    return dev->backend->config_gpio_read(dev, val);
}

static int bladerf2_config_gpio_write(struct bladerf *dev, uint32_t val)
{
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    return dev->backend->config_gpio_write(dev, val);
}


/******************************************************************************/
/* Low-level SPI Flash access */
/******************************************************************************/

static int bladerf2_erase_flash(struct bladerf *dev,
                                uint32_t erase_block,
                                uint32_t count)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    return spi_flash_erase(dev, erase_block, count);
}

static int bladerf2_read_flash(struct bladerf *dev,
                               uint8_t *buf,
                               uint32_t page,
                               uint32_t count)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);
    NULL_CHECK(buf);

    return spi_flash_read(dev, buf, page, count);
}

static int bladerf2_write_flash(struct bladerf *dev,
                                uint8_t const *buf,
                                uint32_t page,
                                uint32_t count)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);
    NULL_CHECK(buf);

    return spi_flash_write(dev, buf, page, count);
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
    NULL_CHECK(xb);

    *xb = BLADERF_XB_NONE;

    return 0;
}


/******************************************************************************/
/* Board binding */
/******************************************************************************/

struct board_fns const bladerf2_board_fns = {
    FIELD_INIT(.matches, bladerf2_matches),
    FIELD_INIT(.open, bladerf2_open),
    FIELD_INIT(.close, bladerf2_close),
    FIELD_INIT(.device_speed, bladerf2_device_speed),
    FIELD_INIT(.get_serial, bladerf2_get_serial),
    FIELD_INIT(.get_fpga_size, bladerf2_get_fpga_size),
    FIELD_INIT(.get_flash_size, bladerf2_get_flash_size),
    FIELD_INIT(.is_fpga_configured, bladerf2_is_fpga_configured),
    FIELD_INIT(.get_fpga_source, bladerf2_get_fpga_source),
    FIELD_INIT(.get_capabilities, bladerf2_get_capabilities),
    FIELD_INIT(.get_channel_count, bladerf2_get_channel_count),
    FIELD_INIT(.get_fpga_version, bladerf2_get_fpga_version),
    FIELD_INIT(.get_fw_version, bladerf2_get_fw_version),
    FIELD_INIT(.set_gain, bladerf2_set_gain),
    FIELD_INIT(.get_gain, bladerf2_get_gain),
    FIELD_INIT(.set_gain_mode, bladerf2_set_gain_mode),
    FIELD_INIT(.get_gain_mode, bladerf2_get_gain_mode),
    FIELD_INIT(.get_gain_modes, bladerf2_get_gain_modes),
    FIELD_INIT(.get_gain_range, bladerf2_get_gain_range),
    FIELD_INIT(.set_gain_stage, bladerf2_set_gain_stage),
    FIELD_INIT(.get_gain_stage, bladerf2_get_gain_stage),
    FIELD_INIT(.get_gain_stage_range, bladerf2_get_gain_stage_range),
    FIELD_INIT(.get_gain_stages, bladerf2_get_gain_stages),
    FIELD_INIT(.set_sample_rate, bladerf2_set_sample_rate),
    FIELD_INIT(.set_rational_sample_rate, bladerf2_set_rational_sample_rate),
    FIELD_INIT(.get_sample_rate, bladerf2_get_sample_rate),
    FIELD_INIT(.get_sample_rate_range, bladerf2_get_sample_rate_range),
    FIELD_INIT(.get_rational_sample_rate, bladerf2_get_rational_sample_rate),
    FIELD_INIT(.set_bandwidth, bladerf2_set_bandwidth),
    FIELD_INIT(.get_bandwidth, bladerf2_get_bandwidth),
    FIELD_INIT(.get_bandwidth_range, bladerf2_get_bandwidth_range),
    FIELD_INIT(.get_frequency, bladerf2_get_frequency),
    FIELD_INIT(.set_frequency, bladerf2_set_frequency),
    FIELD_INIT(.get_frequency_range, bladerf2_get_frequency_range),
    FIELD_INIT(.select_band, bladerf2_select_band),
    FIELD_INIT(.set_rf_port, bladerf2_set_rf_port),
    FIELD_INIT(.get_rf_port, bladerf2_get_rf_port),
    FIELD_INIT(.get_rf_ports, bladerf2_get_rf_ports),
    FIELD_INIT(.get_quick_tune, bladerf2_get_quick_tune),
    FIELD_INIT(.schedule_retune, bladerf2_schedule_retune),
    FIELD_INIT(.cancel_scheduled_retunes, bladerf2_cancel_scheduled_retunes),
    FIELD_INIT(.get_correction, bladerf2_get_correction),
    FIELD_INIT(.set_correction, bladerf2_set_correction),
    FIELD_INIT(.trigger_init, bladerf2_trigger_init),
    FIELD_INIT(.trigger_arm, bladerf2_trigger_arm),
    FIELD_INIT(.trigger_fire, bladerf2_trigger_fire),
    FIELD_INIT(.trigger_state, bladerf2_trigger_state),
    FIELD_INIT(.enable_module, bladerf2_enable_module),
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
    FIELD_INIT(.load_fpga, bladerf2_load_fpga),
    FIELD_INIT(.flash_fpga, bladerf2_flash_fpga),
    FIELD_INIT(.erase_stored_fpga, bladerf2_erase_stored_fpga),
    FIELD_INIT(.flash_firmware, bladerf2_flash_firmware),
    FIELD_INIT(.device_reset, bladerf2_device_reset),
    FIELD_INIT(.set_tuning_mode, bladerf2_set_tuning_mode),
    FIELD_INIT(.get_tuning_mode, bladerf2_get_tuning_mode),
    FIELD_INIT(.get_loopback_modes, bladerf2_get_loopback_modes),
    FIELD_INIT(.set_loopback, bladerf2_set_loopback),
    FIELD_INIT(.get_loopback, bladerf2_get_loopback),
    FIELD_INIT(.get_rx_mux, bladerf2_get_rx_mux),
    FIELD_INIT(.set_rx_mux, bladerf2_set_rx_mux),
    FIELD_INIT(.set_vctcxo_tamer_mode, bladerf2_set_vctcxo_tamer_mode),
    FIELD_INIT(.get_vctcxo_tamer_mode, bladerf2_get_vctcxo_tamer_mode),
    FIELD_INIT(.get_vctcxo_trim, bladerf2_get_vctcxo_trim),
    FIELD_INIT(.trim_dac_read, bladerf2_trim_dac_read),
    FIELD_INIT(.trim_dac_write, bladerf2_trim_dac_write),
    FIELD_INIT(.read_trigger, bladerf2_read_trigger),
    FIELD_INIT(.write_trigger, bladerf2_write_trigger),
    FIELD_INIT(.config_gpio_read, bladerf2_config_gpio_read),
    FIELD_INIT(.config_gpio_write, bladerf2_config_gpio_write),
    FIELD_INIT(.erase_flash, bladerf2_erase_flash),
    FIELD_INIT(.read_flash, bladerf2_read_flash),
    FIELD_INIT(.write_flash, bladerf2_write_flash),
    FIELD_INIT(.expansion_attach, bladerf2_expansion_attach),
    FIELD_INIT(.expansion_get_attached, bladerf2_expansion_get_attached),
    FIELD_INIT(.name, "bladerf2"),
};


/******************************************************************************
 ******************************************************************************
 *                         bladeRF2-specific Functions *
 ******************************************************************************
 ******************************************************************************/

/******************************************************************************/
/* Bias Tee Control */
/******************************************************************************/

int bladerf_get_bias_tee(struct bladerf *dev, bladerf_channel ch, bool *enable)
{
    CHECK_BOARD_IS_BLADERF2(dev);
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);
    NULL_CHECK(enable);

    WITH_MUTEX(&dev->lock, {
        uint32_t reg;
        uint32_t shift;

        shift = BLADERF_CHANNEL_IS_TX(ch) ? RFFE_CONTROL_TX_BIAS_EN
                                          : RFFE_CONTROL_RX_BIAS_EN;

        /* Read RFFE control register */
        CHECK_STATUS_LOCKED(dev->backend->rffe_control_read(dev, &reg));

        /* Check register value */
        *enable = (reg >> shift) & 0x1;
    });

    return 0;
}

int bladerf_set_bias_tee(struct bladerf *dev, bladerf_channel ch, bool enable)
{
    CHECK_BOARD_IS_BLADERF2(dev);
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    WITH_MUTEX(&dev->lock, {
        uint32_t reg;
        uint32_t shift;

        shift = BLADERF_CHANNEL_IS_TX(ch) ? RFFE_CONTROL_TX_BIAS_EN
                                          : RFFE_CONTROL_RX_BIAS_EN;

        /* Read RFFE control register */
        CHECK_STATUS_LOCKED(dev->backend->rffe_control_read(dev, &reg));

        /* Clear register value */
        reg &= ~(1 << shift);

        /* Set register value */
        if (enable) {
            reg |= (1 << shift);
        }

        /* Write RFFE control register */
        log_debug("%s: rffe_control_write %08x\n", __FUNCTION__, reg);
        CHECK_STATUS_LOCKED(dev->backend->rffe_control_write(dev, reg));
    });

    return 0;
}


/******************************************************************************/
/* Low level RFIC Accessors */
/******************************************************************************/

int bladerf_get_rfic_register(struct bladerf *dev,
                              uint16_t address,
                              uint8_t *val)
{
    CHECK_BOARD_IS_BLADERF2(dev);
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);
    NULL_CHECK(val);

    WITH_MUTEX(&dev->lock, {
        uint64_t data;

        address |= (AD936X_READ | AD936X_CNT(1));

        CHECK_AD936X_LOCKED(dev->backend->ad9361_spi_read(dev, address, &data));

        *val = (data >> 56) & 0xff;
    });

    return 0;
}

int bladerf_set_rfic_register(struct bladerf *dev,
                              uint16_t address,
                              uint8_t val)
{
    CHECK_BOARD_IS_BLADERF2(dev);
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    WITH_MUTEX(&dev->lock, {
        uint64_t data = (((uint64_t)val) << 56);

        address |= (AD936X_WRITE | AD936X_CNT(1));

        CHECK_AD936X_LOCKED(dev->backend->ad9361_spi_write(dev, address, data));
    });

    return 0;
}

int bladerf_get_rfic_temperature(struct bladerf *dev, float *val)
{
    CHECK_BOARD_IS_BLADERF2(dev);
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);
    NULL_CHECK(val);

    struct bladerf2_board_data *board_data = dev->board_data;
    struct ad9361_rf_phy *phy              = board_data->phy;

    WITH_MUTEX(&dev->lock, { *val = ad9361_get_temp(phy) / 1000.0F; });

    return 0;
}

int bladerf_get_rfic_rssi(struct bladerf *dev,
                          bladerf_channel ch,
                          int *pre_rssi,
                          int *sym_rssi)
{
    CHECK_BOARD_IS_BLADERF2(dev);
    CHECK_BOARD_STATE(STATE_INITIALIZED);
    NULL_CHECK(pre_rssi);
    NULL_CHECK(sym_rssi);

    WITH_MUTEX(&dev->lock, {
        struct bladerf2_board_data *board_data = dev->board_data;
        struct ad9361_rf_phy *phy              = board_data->phy;
        uint8_t const rfic_channel             = ch >> 1;
        int pre;
        int sym;

        if (BLADERF_CHANNEL_IS_TX(ch)) {
            uint32_t rssi = 0;

            CHECK_AD936X_LOCKED(ad9361_get_tx_rssi(phy, rfic_channel, &rssi));

            pre = __round_int(rssi / 1000.0);
            sym = __round_int(rssi / 1000.0);
        } else {
            struct rf_rssi rssi;

            CHECK_AD936X_LOCKED(ad9361_get_rx_rssi(phy, rfic_channel, &rssi));

            pre = __round_int(rssi.preamble / (float)rssi.multiplier);
            sym = __round_int(rssi.symbol / (float)rssi.multiplier);
        }

        *pre_rssi = -pre;
        *sym_rssi = -sym;
    });

    return 0;
}

int bladerf_get_rfic_ctrl_out(struct bladerf *dev, uint8_t *ctrl_out)
{
    CHECK_BOARD_IS_BLADERF2(dev);
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);
    NULL_CHECK(ctrl_out);

    WITH_MUTEX(&dev->lock, {
        uint32_t reg;

        /* Read RFFE control register */
        CHECK_STATUS_LOCKED(dev->backend->rffe_control_read(dev, &reg));

        *ctrl_out = (uint8_t)((reg >> RFFE_CONTROL_CTRL_OUT) & 0xFF);
    });

    return 0;
}

int bladerf_get_rfic_rx_fir(struct bladerf *dev, bladerf_rfic_rxfir *rxfir)
{
    CHECK_BOARD_IS_BLADERF2(dev);
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);
    NULL_CHECK(rxfir);

    WITH_MUTEX(&dev->lock, {
        struct bladerf2_board_data *board_data = dev->board_data;

        *rxfir = board_data->rxfir;
    });

    return 0;
}

int bladerf_set_rfic_rx_fir(struct bladerf *dev, bladerf_rfic_rxfir rxfir)
{
    CHECK_BOARD_IS_BLADERF2(dev);
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    WITH_MUTEX(&dev->lock, {
        struct bladerf2_board_data *board_data = dev->board_data;
        struct ad9361_rf_phy *phy              = board_data->phy;
        AD9361_RXFIRConfig *fir_config         = NULL;
        uint8_t enable;

        if (BLADERF_RFIC_RXFIR_CUSTOM == rxfir) {
            log_warning("custom FIR not implemented, assuming default\n");
            rxfir = BLADERF_RFIC_RXFIR_DEFAULT;
        }

        switch (rxfir) {
            case BLADERF_RFIC_RXFIR_BYPASS:
                fir_config = &bladerf2_rfic_rx_fir_config;
                enable     = 0;
                break;
            case BLADERF_RFIC_RXFIR_DEC1:
                fir_config = &bladerf2_rfic_rx_fir_config;
                enable     = 1;
                break;
            case BLADERF_RFIC_RXFIR_DEC2:
                fir_config = &bladerf2_rfic_rx_fir_config_dec2;
                enable     = 1;
                break;
            case BLADERF_RFIC_RXFIR_DEC4:
                fir_config = &bladerf2_rfic_rx_fir_config_dec4;
                enable     = 1;
                break;
            default:
                assert(!"Bug: unhandled rxfir selection");
                MUTEX_UNLOCK(&dev->lock);
                return BLADERF_ERR_UNEXPECTED;
        }

        CHECK_AD936X_LOCKED(ad9361_set_rx_fir_config(phy, *fir_config));

        CHECK_AD936X_LOCKED(ad9361_set_rx_fir_en_dis(phy, enable));

        board_data->rxfir = rxfir;
    });

    return 0;
}

int bladerf_get_rfic_tx_fir(struct bladerf *dev, bladerf_rfic_txfir *txfir)
{
    CHECK_BOARD_IS_BLADERF2(dev);
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);
    NULL_CHECK(txfir);

    WITH_MUTEX(&dev->lock, {
        struct bladerf2_board_data *board_data = dev->board_data;

        *txfir = board_data->txfir;
    });

    return 0;
}

int bladerf_set_rfic_tx_fir(struct bladerf *dev, bladerf_rfic_txfir txfir)
{
    CHECK_BOARD_IS_BLADERF2(dev);
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    WITH_MUTEX(&dev->lock, {
        struct bladerf2_board_data *board_data = dev->board_data;
        struct ad9361_rf_phy *phy              = board_data->phy;
        AD9361_TXFIRConfig *fir_config         = NULL;
        uint8_t enable;

        if (BLADERF_RFIC_TXFIR_CUSTOM == txfir) {
            log_warning("custom FIR not implemented, assuming default\n");
            txfir = BLADERF_RFIC_TXFIR_DEFAULT;
        }

        switch (txfir) {
            case BLADERF_RFIC_TXFIR_BYPASS:
                fir_config = &bladerf2_rfic_tx_fir_config;
                enable     = 0;
                break;
            case BLADERF_RFIC_TXFIR_INT1:
                fir_config = &bladerf2_rfic_tx_fir_config;
                enable     = 1;
                break;
            case BLADERF_RFIC_TXFIR_INT2:
                fir_config = &bladerf2_rfic_tx_fir_config_int2;
                enable     = 1;
                break;
            case BLADERF_RFIC_TXFIR_INT4:
                fir_config = &bladerf2_rfic_tx_fir_config_int4;
                enable     = 1;
                break;
            default:
                assert(!"Bug: unhandled txfir selection");
                MUTEX_UNLOCK(&dev->lock);
                return BLADERF_ERR_UNEXPECTED;
        }

        CHECK_AD936X(ad9361_set_tx_fir_config(phy, *fir_config));

        CHECK_AD936X(ad9361_set_tx_fir_en_dis(phy, enable));

        board_data->txfir = txfir;
    });

    return 0;
}


/******************************************************************************/
/* Low level PLL Accessors */
/******************************************************************************/
static int bladerf_pll_configure(struct bladerf *dev, uint16_t R, uint16_t N)
{
    CHECK_BOARD_IS_BLADERF2(dev);
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    uint32_t init_array[3];
    bool is_enabled;
    uint8_t i;

    if (R < 1 || R > 16383) {
        RETURN_INVAL("R", "outside range [1,16383]");
    }

    if (N < 1 || N > 8191) {
        RETURN_INVAL("N", "outside range [1,8191]");
    }

    /* Get the present state... */
    CHECK_STATUS(bladerf_get_pll_enable(dev, &is_enabled));

    /* Enable the chip if applicable */
    if (!is_enabled) {
        CHECK_STATUS(bladerf_set_pll_enable(dev, true));
    }

    /* Register 0: Reference Counter Latch */
    init_array[0] = 0;
    /* R Counter: */
    init_array[0] |= (R & ((1 << 14) - 1)) << 2;
    /* Hardcoded values: */
    /* Anti-backlash: 00 (2.9 ns) */
    /* Lock detect precision: 0 (three cycles) */

    /* Register 1: N Counter Latch */
    init_array[1] = 1;
    /* N Counter: */
    init_array[1] |= (N & ((1 << 13) - 1)) << 8;
    /* Hardcoded values: */
    /* CP Gain: 0 (Setting 1) */

    /* Register 2: Function Latch */
    init_array[2] = 2;
    /* Hardcoded values: */
    /* Counter operation: 0 (Normal) */
    /* Power down control: 00 (Normal) */
    /* Muxout control: 0x1 (digital lock detect) */
    init_array[2] |= (1 & ((1 << 3) - 1)) << 4;
    /* PD Polarity: 1 (positive) */
    init_array[2] |= 1 << 7;
    /* CP three-state: 0 (normal) */
    /* Fastlock Mode: 00 (disabled) */
    /* Timer Counter Control: 0000 (3 PFD cycles) */
    /* Current Setting 1: 111 (5 mA) */
    init_array[2] |= 0x7 << 15;
    /* Current Setting 2: 111 (5 mA) */
    init_array[2] |= 0x7 << 18;

    /* Write the values to the chip */
    for (i = 0; i < ARRAY_SIZE(init_array); ++i) {
        CHECK_STATUS(bladerf_set_pll_register(dev, i, init_array[i]));
    }

    /* Re-disable the chip if applicable */
    if (!is_enabled) {
        CHECK_STATUS(bladerf_set_pll_enable(dev, false));
    }

    return 0;
}

static int bladerf_pll_calculate_ratio(bladerf_frequency ref_freq,
                                       bladerf_frequency clock_freq,
                                       uint16_t *R,
                                       uint16_t *N)
{
    NULL_CHECK(R);
    NULL_CHECK(N);

    size_t const Rmax = 16383;
    double const tol  = 0.00001;
    double target     = (double)clock_freq / (double)ref_freq;
    uint16_t R_try, N_try;

    struct bladerf_range const clock_frequency_range = {
        FIELD_INIT(.min, 5000000),
        FIELD_INIT(.max, 400000000),
        FIELD_INIT(.step, 1),
        FIELD_INIT(.scale, 1),
    };

    if (!is_within_range(&bladerf2_pll_refclk_range, ref_freq)) {
        return BLADERF_ERR_RANGE;
    }

    if (!is_within_range(&clock_frequency_range, clock_freq)) {
        return BLADERF_ERR_RANGE;
    }

    for (R_try = 1; R_try < Rmax; ++R_try) {
        double ratio, delta;

        N_try = (uint16_t)(target * R_try + 0.5);

        if (N_try > 8191) {
            continue;
        }

        ratio = (double)N_try / (double)R_try;
        delta = (ratio > target) ? (ratio - target) : (target - ratio);

        if (delta < tol) {
            *R = R_try;
            *N = N_try;

            return 0;
        }
    }

    RETURN_INVAL("requested ratio", "not achievable");
}

int bladerf_get_pll_lock_state(struct bladerf *dev, bool *locked)
{
    CHECK_BOARD_IS_BLADERF2(dev);
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);
    NULL_CHECK(locked);

    WITH_MUTEX(&dev->lock, {
        uint32_t reg;

        /* Read RFFE control register */
        CHECK_STATUS_LOCKED(dev->backend->rffe_control_read(dev, &reg));

        *locked = (reg >> RFFE_CONTROL_ADF_MUXOUT) & 0x1;
    });

    return 0;
}

int bladerf_get_pll_enable(struct bladerf *dev, bool *enabled)
{
    CHECK_BOARD_IS_BLADERF2(dev);
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);
    NULL_CHECK(enabled);

    WITH_MUTEX(&dev->lock, {
        uint32_t data;

        CHECK_STATUS_LOCKED(dev->backend->config_gpio_read(dev, &data));

        *enabled = (data >> CFG_GPIO_PLL_EN) & 0x01;
    });

    return 0;
}

int bladerf_set_pll_enable(struct bladerf *dev, bool enable)
{
    CHECK_BOARD_IS_BLADERF2(dev);
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    WITH_MUTEX(&dev->lock, {
        struct bladerf2_board_data *board_data = dev->board_data;
        uint32_t data;

        // Disable the trim DAC when we're using the PLL
        if (enable) {
            CHECK_STATUS_LOCKED(_bladerf2_set_trim_dac_enable(dev, false));
        }

        // Read current config GPIO value
        CHECK_STATUS_LOCKED(dev->backend->config_gpio_read(dev, &data));

        // Set the PLL enable bit accordingly
        data &= ~(1 << CFG_GPIO_PLL_EN);
        data |= ((enable ? 1 : 0) << CFG_GPIO_PLL_EN);

        // Write back the config GPIO
        CHECK_STATUS_LOCKED(dev->backend->config_gpio_write(dev, data));

        // Update our state flag
        board_data->trim_source = enable ? TRIM_SOURCE_PLL : TRIM_SOURCE_NONE;

        // Enable the trim DAC if we're done with the
        // PLL
        if (!enable) {
            CHECK_STATUS_LOCKED(_bladerf2_set_trim_dac_enable(dev, true));
        }
    });

    return 0;
}

int bladerf_get_pll_refclk_range(struct bladerf *dev,
                                 const struct bladerf_range **range)
{
    NULL_CHECK(range);

    *range = &bladerf2_pll_refclk_range;

    return 0;
}

int bladerf_get_pll_refclk(struct bladerf *dev, bladerf_frequency *frequency)
{
    CHECK_BOARD_IS_BLADERF2(dev);
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);
    NULL_CHECK(frequency);

    uint8_t const R_LATCH_REG   = 0;
    size_t const R_LATCH_SHIFT  = 2;
    uint32_t const R_LATCH_MASK = 0x3fff;
    uint8_t const N_LATCH_REG   = 1;
    size_t const N_LATCH_SHIFT  = 8;
    uint32_t const N_LATCH_MASK = 0x1fff;
    uint32_t reg;
    uint16_t R, N;

    // Get the current R value (latch 0, bits 2-15)
    CHECK_STATUS(bladerf_get_pll_register(dev, R_LATCH_REG, &reg));
    R = (reg >> R_LATCH_SHIFT) & R_LATCH_MASK;

    // Get the current N value (latch 1, bits 8-20)
    CHECK_STATUS(bladerf_get_pll_register(dev, N_LATCH_REG, &reg));
    N = (reg >> N_LATCH_SHIFT) & N_LATCH_MASK;

    // We assume the system clock frequency is
    // BLADERF_VCTCXO_FREQUENCY. If it isn't, do your
    // own math
    *frequency = R * BLADERF_VCTCXO_FREQUENCY / N;

    return 0;
}

int bladerf_set_pll_refclk(struct bladerf *dev, bladerf_frequency frequency)
{
    CHECK_BOARD_IS_BLADERF2(dev);
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    uint16_t R, N;

    // We assume the system clock frequency is
    // BLADERF_VCTCXO_FREQUENCY. If it isn't, do your
    // own math
    CHECK_STATUS(bladerf_pll_calculate_ratio(frequency,
                                             BLADERF_VCTCXO_FREQUENCY, &R, &N));

    CHECK_STATUS(bladerf_pll_configure(dev, R, N));

    return 0;
}

int bladerf_get_pll_register(struct bladerf *dev,
                             uint8_t address,
                             uint32_t *val)
{
    CHECK_BOARD_IS_BLADERF2(dev);
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);
    NULL_CHECK(val);

    WITH_MUTEX(&dev->lock, {
        uint32_t data;

        address &= 0x03;

        CHECK_STATUS_LOCKED(dev->backend->adf400x_read(dev, address, &data));

        *val = data;
    });

    return 0;
}

int bladerf_set_pll_register(struct bladerf *dev, uint8_t address, uint32_t val)
{
    CHECK_BOARD_IS_BLADERF2(dev);
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    WITH_MUTEX(&dev->lock, {
        uint32_t data;

        address &= 0x03;

        data = val;

        CHECK_STATUS_LOCKED(dev->backend->adf400x_write(dev, address, data));
    });

    return 0;
}


/******************************************************************************/
/* Low level Power Source Accessors */
/******************************************************************************/

int bladerf_get_power_source(struct bladerf *dev, bladerf_power_sources *src)
{
    CHECK_BOARD_IS_BLADERF2(dev);
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);
    NULL_CHECK(src);

    WITH_MUTEX(&dev->lock, {
        uint32_t data;

        CHECK_STATUS_LOCKED(dev->backend->config_gpio_read(dev, &data));

        if ((data >> CFG_GPIO_POWERSOURCE) & 0x01) {
            *src = BLADERF_PS_USB_VBUS;
        } else {
            *src = BLADERF_PS_DC;
        }
    });

    return 0;
}


/******************************************************************************/
/* Low level clock source selection accessors */
/******************************************************************************/

int bladerf_get_clock_select(struct bladerf *dev, bladerf_clock_select *sel)
{
    CHECK_BOARD_IS_BLADERF2(dev);
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);
    NULL_CHECK(sel);

    WITH_MUTEX(&dev->lock, {
        uint32_t gpio;

        CHECK_STATUS_LOCKED(dev->backend->config_gpio_read(dev, &gpio));

        if ((gpio & (1 << CFG_GPIO_CLOCK_SELECT)) == 0x0) {
            *sel = CLOCK_SELECT_ONBOARD;
        } else {
            *sel = CLOCK_SELECT_EXTERNAL;
        }
    });

    return 0;
}

int bladerf_set_clock_select(struct bladerf *dev, bladerf_clock_select sel)
{
    CHECK_BOARD_IS_BLADERF2(dev);
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    WITH_MUTEX(&dev->lock, {
        uint32_t gpio;

        CHECK_STATUS_LOCKED(dev->backend->config_gpio_read(dev, &gpio));

        // Set the clock select bit(s) accordingly
        switch (sel) {
            case CLOCK_SELECT_ONBOARD:
                gpio &= ~(1 << CFG_GPIO_CLOCK_SELECT);
                break;
            case CLOCK_SELECT_EXTERNAL:
                gpio |= (1 << CFG_GPIO_CLOCK_SELECT);
                break;
            default:
                break;
        }

        // Write back the config GPIO
        CHECK_STATUS_LOCKED(dev->backend->config_gpio_write(dev, gpio));
    });

    return 0;
}


/******************************************************************************/
/* Low level clock buffer output accessors */
/******************************************************************************/

int bladerf_get_clock_output(struct bladerf *dev, bool *state)
{
    CHECK_BOARD_IS_BLADERF2(dev);
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);
    NULL_CHECK(state);

    WITH_MUTEX(&dev->lock, {
        uint32_t gpio;

        CHECK_STATUS_LOCKED(dev->backend->config_gpio_read(dev, &gpio));

        *state = ((gpio & (1 << CFG_GPIO_CLOCK_OUTPUT)) != 0x0);
    });

    return 0;
}

int bladerf_set_clock_output(struct bladerf *dev, bool enable)
{
    CHECK_BOARD_IS_BLADERF2(dev);
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    WITH_MUTEX(&dev->lock, {
        uint32_t gpio;

        CHECK_STATUS_LOCKED(dev->backend->config_gpio_read(dev, &gpio));

        // Set or clear the clock output enable bit
        gpio &= ~(1 << CFG_GPIO_CLOCK_OUTPUT);
        gpio |= ((enable ? 1 : 0) << CFG_GPIO_CLOCK_OUTPUT);

        // Write back the config GPIO
        CHECK_STATUS_LOCKED(dev->backend->config_gpio_write(dev, gpio));
    });

    return 0;
}


/******************************************************************************/
/* Low level INA219 Accessors */
/******************************************************************************/

int bladerf_get_pmic_register(struct bladerf *dev,
                              bladerf_pmic_register reg,
                              void *val)
{
    CHECK_BOARD_IS_BLADERF2(dev);
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);
    NULL_CHECK(val);

    int rv;

    WITH_MUTEX(&dev->lock, {
        switch (reg) {
            case BLADERF_PMIC_CONFIGURATION:
            case BLADERF_PMIC_CALIBRATION:
            default:
                rv = BLADERF_ERR_UNSUPPORTED;
                break;

            case BLADERF_PMIC_VOLTAGE_SHUNT:
                rv = ina219_read_shunt_voltage(dev, (float *)val);
                break;

            case BLADERF_PMIC_VOLTAGE_BUS:
                rv = ina219_read_bus_voltage(dev, (float *)val);
                break;

            case BLADERF_PMIC_POWER:
                rv = ina219_read_power(dev, (float *)val);
                break;

            case BLADERF_PMIC_CURRENT:
                rv = ina219_read_current(dev, (float *)val);
                break;
        }
    });

    return rv;
}


/******************************************************************************/
/* Low level RF Switch Accessors */
/******************************************************************************/

int bladerf_get_rf_switch_config(struct bladerf *dev,
                                 bladerf_rf_switch_config *config)
{
    CHECK_BOARD_IS_BLADERF2(dev);
    CHECK_BOARD_STATE(STATE_INITIALIZED);
    NULL_CHECK(config);

    WITH_MUTEX(&dev->lock, {
        struct bladerf2_board_data *board_data = dev->board_data;
        struct ad9361_rf_phy *phy              = board_data->phy;
        uint32_t val;
        uint32_t reg;

        /* Get AD9361 status */
        CHECK_AD936X_LOCKED(ad9361_get_tx_rf_port_output(phy, &val));

        config->tx1_rfic_port = val;
        config->tx2_rfic_port = val;

        CHECK_AD936X_LOCKED(ad9361_get_rx_rf_port_input(phy, &val));

        config->rx1_rfic_port = val;
        config->rx2_rfic_port = val;

        /* Read RFFE control register */
        CHECK_STATUS_LOCKED(dev->backend->rffe_control_read(dev, &reg));

        config->rx1_spdt_port =
            (reg >> RFFE_CONTROL_RX_SPDT_1) & RFFE_CONTROL_SPDT_MASK;
        config->rx2_spdt_port =
            (reg >> RFFE_CONTROL_RX_SPDT_2) & RFFE_CONTROL_SPDT_MASK;
        config->tx1_spdt_port =
            (reg >> RFFE_CONTROL_TX_SPDT_1) & RFFE_CONTROL_SPDT_MASK;
        config->tx2_spdt_port =
            (reg >> RFFE_CONTROL_TX_SPDT_2) & RFFE_CONTROL_SPDT_MASK;
    });

    return 0;
}
