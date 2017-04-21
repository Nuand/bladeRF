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

#include "compatibility.h"
#include "capabilities.h"
#include "calibration.h"
#include "flash.h"

#include "driver/smb_clock.h"
#include "driver/si5338.h"
#include "driver/dac161s055.h"
#include "driver/spi_flash.h"
#include "driver/fpga_trigger.h"
#include "driver/fx3_fw.h"
#include "lms.h"
#include "nios_pkt_retune.h"
#include "band_select.h"

#include "backend/usb/usb.h"
#include "backend/backend_config.h"

#include "expansion/xb100.h"
#include "expansion/xb200.h"
#include "expansion/xb300.h"

#include "streaming/async.h"
#include "streaming/sync.h"

#include "devinfo.h"
#include "helpers/version.h"
#include "helpers/file.h"
#include "version.h"

/******************************************************************************
 *                          bladeRF1 board state                              *
 ******************************************************************************/

/* 1 TX, 1 RX */
#define NUM_MODULES 2

struct bladerf1_board_data {
    /* Board state */
    enum {
        STATE_UNINITIALIZED,
        STATE_FIRMWARE_LOADED,
        STATE_FPGA_LOADED,
        STATE_INITIALIZED,
        STATE_CALIBRATED,
    } state;

    /* Bitmask of capabilities determined by version numbers */
    uint64_t capabilities;

    /* Format currently being used with a module, or -1 if module is not used */
    bladerf_format module_format[NUM_MODULES];

    /* Which mode of operation we use for tuning */
    bladerf_tuning_mode tuning_mode;

    /* Calibration data */
    struct calibrations {
        struct dc_cal_tbl *dc_rx;
        struct dc_cal_tbl *dc_tx;
    } cal;
    uint16_t dac_trim;

    /* Board properties */
    bladerf_fpga_size fpga_size;
    /* Data message size */
    size_t msg_size;

    /* Version information */
    struct bladerf_version fpga_version;
    struct bladerf_version fw_version;
    char fpga_version_str[BLADERF_VERSION_STR_MAX+1];
    char fw_version_str[BLADERF_VERSION_STR_MAX+1];

    /* Synchronous interface handles */
    struct bladerf_sync sync[NUM_MODULES];
};

#define CHECK_BOARD_STATE(_state) \
    do { \
        struct bladerf1_board_data *board_data = dev->board_data; \
        if (board_data->state < _state) { \
            log_error("Board state insufficient for operation " \
                      "(current \"%s\", requires \"%s\").\n", \
                      bladerf1_state_to_string[board_data->state], \
                      bladerf1_state_to_string[_state]); \
            return BLADERF_ERR_NOT_INIT; \
        } \
    } while(0)


/******************************************************************************/
/* Constants */
/******************************************************************************/

/* Board state to string map */

static const char *bladerf1_state_to_string[] = {
    [STATE_UNINITIALIZED]   = "Uninitialized",
    [STATE_FIRMWARE_LOADED] = "Firmware Loaded",
    [STATE_FPGA_LOADED]     = "FPGA Loaded",
    [STATE_INITIALIZED]     = "Initialized",
    [STATE_CALIBRATED]      = "Calibrated",
};

/* Overall RX gain range */

static const struct bladerf_range bladerf1_rx_gain_range = {
    .min = BLADERF_RXVGA1_GAIN_MIN + BLADERF_RXVGA2_GAIN_MIN,
    .max = BLADERF_LNA_GAIN_MAX_DB + BLADERF_RXVGA1_GAIN_MAX + BLADERF_RXVGA2_GAIN_MAX,
    .step = 1,
    .scale = 1,
};

/* Overall TX gain range */

static const struct bladerf_range bladerf1_tx_gain_range = {
    .min = BLADERF_TXVGA1_GAIN_MIN + BLADERF_TXVGA2_GAIN_MIN,
    .max = BLADERF_TXVGA1_GAIN_MAX + BLADERF_TXVGA2_GAIN_MAX,
    .step = 1,
    .scale = 1,
};

struct bladerf_gain_stage_info {
    const char *name;
    struct bladerf_range range;
};

/* RX gain stages */

static const struct bladerf_gain_stage_info bladerf1_rx_gain_stages[] = {
    {
        .name = "lna",
        .range = {
            .min = 0,
            .max = BLADERF_LNA_GAIN_MAX_DB,
            .step = 3,
            .scale = 1,
        },
    },
    {
        .name = "rxvga1",
        .range = {
            .min = BLADERF_RXVGA1_GAIN_MIN,
            .max = BLADERF_RXVGA1_GAIN_MAX,
            .step = 1,
            .scale = 1,
        },
    },
    {
        .name = "rxvga2",
        .range = {
            .min = BLADERF_RXVGA2_GAIN_MIN,
            .max = BLADERF_RXVGA2_GAIN_MAX,
            .step = 1,
            .scale = 1,
        },
    },
};

/* TX gain stages */

static const struct bladerf_gain_stage_info bladerf1_tx_gain_stages[] = {
    {
        .name = "txvga1",
        .range = {
            .min = BLADERF_TXVGA1_GAIN_MIN,
            .max = BLADERF_TXVGA1_GAIN_MAX,
            .step = 1,
            .scale = 1,
        },
    },
    {
        .name = "txvga2",
        .range = {
            .min = BLADERF_TXVGA2_GAIN_MIN,
            .max = BLADERF_TXVGA2_GAIN_MAX,
            .step = 1,
            .scale = 1,
        },
    },
};

/* Sample Rate Range */

static const struct bladerf_range bladerf1_sample_rate_range = {
    .min = BLADERF_SAMPLERATE_MIN,
    .max = BLADERF_SAMPLERATE_REC_MAX,
    .step = 1,
    .scale = 1,
};

/* Bandwidth Range */

static const struct bladerf_range bladerf1_bandwidth_range = {
    .min = BLADERF_BANDWIDTH_MIN,
    .max = BLADERF_BANDWIDTH_MAX,
    .step = 1,
    .scale = 1,
};

/* Frequency Range */

static const struct bladerf_range bladerf1_frequency_range = {
    .min = BLADERF_FREQUENCY_MIN,
    .max = BLADERF_FREQUENCY_MAX,
    .step = 1,
    .scale = 1,
};

static const struct bladerf_range bladerf1_xb200_frequency_range = {
    .min = 0,
    .max = 300e6,
    .step = 1,
    .scale = 1,
};

/******************************************************************************/
/* Low-level Initialization */
/******************************************************************************/

static bladerf_tuning_mode tuning_get_default_mode(struct bladerf *dev)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    bladerf_tuning_mode mode;
    const char *env_var;

    if (have_cap(board_data->capabilities, BLADERF_CAP_FPGA_TUNING)) {
        mode = BLADERF_TUNING_MODE_FPGA;
    } else {
        mode = BLADERF_TUNING_MODE_HOST;
    }

    env_var = getenv("BLADERF_DEFAULT_TUNING_MODE");

    if (env_var != NULL) {
        if (!strcasecmp("host", env_var)) {
            mode = BLADERF_TUNING_MODE_HOST;
        } else if (!strcasecmp("fpga", env_var)) {
            mode = BLADERF_TUNING_MODE_FPGA;

            /* Just a friendly reminder... */
            if (!have_cap(board_data->capabilities, BLADERF_CAP_FPGA_TUNING)) {
                log_warning("The loaded FPGA version (%u.%u.%u) does not "
                            "support the tuning mode being used to override "
                            "the default.\n",
                            board_data->fpga_version.major, board_data->fpga_version.minor,
                            board_data->fpga_version.patch);
            }
        } else {
            log_debug("Invalid tuning mode override: %s\n", env_var);
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

static int bladerf1_apply_lms_dc_cals(struct bladerf *dev)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    int status = 0;
    struct bladerf_lms_dc_cals cals;
    const bool have_rx = (board_data->cal.dc_rx != NULL);
    const bool have_tx = (board_data->cal.dc_tx != NULL);

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
        const struct bladerf_lms_dc_cals *reg_vals = &board_data->cal.dc_rx->reg_vals;

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
        const struct bladerf_lms_dc_cals *reg_vals = &board_data->cal.dc_tx->reg_vals;

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
        const struct bladerf_lms_dc_cals *reg_vals = &board_data->cal.dc_rx->reg_vals;

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
                uint64_t rx_f;
                rx_status = dev->board->get_frequency(dev, BLADERF_CHANNEL_RX(0), &rx_f);
                if (rx_status == 0) {
                    rx_status = dev->board->set_frequency(dev, BLADERF_CHANNEL_RX(0), rx_f);
                }
            }

            if (have_tx) {
                uint64_t rx_f;
                rx_status = dev->board->get_frequency(dev, BLADERF_CHANNEL_RX(0), &rx_f);
                if (rx_status == 0) {
                    rx_status = dev->board->set_frequency(dev, BLADERF_CHANNEL_RX(0), rx_f);
                }
            }

            /* Report the first of any failures */
            status = (rx_status == 0) ? tx_status : rx_status;
            if (status != 0) {
                return status;
            }
        }
    }

    /* Update device state */
    if (have_tx && have_rx)
        board_data->state = STATE_CALIBRATED;

    return 0;
}

/**
 * Initialize device registers - required after power-up, but safe
 * to call multiple times after power-up (e.g., multiple close and reopens)
 */
static int bladerf1_initialize(struct bladerf *dev)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    int status;
    uint32_t val;

    /* Readback the GPIO values to see if they are default or already set */
    status = dev->backend->config_gpio_read(dev, &val);
    if (status != 0) {
        log_debug("Failed to read GPIO config %s\n", bladerf_strerror(status));
        return status;
    }

    if ((val & 0x7f) == 0) {
        log_verbose( "Default GPIO value found - initializing device\n" );

        /* Set the GPIO pins to enable the LMS and select the low band */
        status = dev->backend->config_gpio_write(dev, 0x57);
        if (status != 0) {
            return status;
        }

        /* Disable the front ends */
        status = lms_enable_rffe(dev, BLADERF_CHANNEL_TX(0), false);
        if (status != 0) {
            return status;
        }

        status = lms_enable_rffe(dev, BLADERF_CHANNEL_RX(0), false);
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
        status = lms_config_charge_pumps(dev, BLADERF_CHANNEL_TX(0));
        if (status != 0) {
            return status;
        }

        status = lms_config_charge_pumps(dev, BLADERF_CHANNEL_RX(0));
        if (status != 0) {
            return status;
        }

        /* Set a default samplerate */
        status = si5338_set_sample_rate(dev, BLADERF_CHANNEL_TX(0), 1000000, NULL);
        if (status != 0) {
            return status;
        }

        status = si5338_set_sample_rate(dev, BLADERF_CHANNEL_RX(0), 1000000, NULL);
        if (status != 0) {
            return status;
        }

        board_data->tuning_mode = tuning_get_default_mode(dev);

        status = dev->board->set_frequency(dev, BLADERF_CHANNEL_TX(0), 2447000000U);
        if (status != 0) {
            return status;
        }

        status = dev->board->set_frequency(dev, BLADERF_CHANNEL_RX(0), 2484000000U);
        if (status != 0) {
            return status;
        }

        /* Set the calibrated VCTCXO DAC value */
        status = dac161s055_write(dev, board_data->dac_trim);
        if (status != 0) {
            return status;
        }
    } else {
        board_data->tuning_mode = tuning_get_default_mode(dev);
    }

    /* Check if we have an expansion board attached */
    status = dev->board->expansion_get_attached(dev, &dev->xb);
    if (status != 0) {
        return status;
    }

    /* Update device state */
    board_data->state = STATE_INITIALIZED;

    /* Set up LMS DC offset register calibration and initial IQ settings,
     * if any tables have been loaded already.
     *
     * This is done every time the device is opened (with an FPGA loaded),
     * as the user may change/update DC calibration tables without reloading the
     * FPGA.
     */
    status = bladerf1_apply_lms_dc_cals(dev);
    if (status != 0) {
        return status;
    }

    return 0;
}

/******************************************************************************
 *                        Generic Board Functions                             *
 ******************************************************************************/

/******************************************************************************/
/* Matches */
/******************************************************************************/

static bool bladerf1_matches(struct bladerf *dev)
{
    uint16_t vid, pid;
    int status;

    status = dev->backend->get_vid_pid(dev, &vid, &pid);
    if (status < 0) {
        return false;
    }

    if (vid == USB_NUAND_VENDOR_ID && pid == USB_NUAND_BLADERF_PRODUCT_ID) {
        return true;
    } else if (vid == USB_NUAND_LEGACY_VENDOR_ID && pid == USB_NUAND_BLADERF_LEGACY_PRODUCT_ID) {
        return true;
    }

    return false;
}

/******************************************************************************/
/* Open/close */
/******************************************************************************/

static int bladerf1_open(struct bladerf *dev, struct bladerf_devinfo *devinfo)
{
    struct bladerf1_board_data *board_data;
    struct bladerf_version required_fw_version;
    struct bladerf_version required_fpga_version;
    bladerf_dev_speed usb_speed;
    char filename[NAME_MAX];
    char *full_path;
    int status;

    /* Allocate board data */
    board_data = calloc(1, sizeof(struct bladerf1_board_data));
    if (board_data == NULL) {
        return BLADERF_ERR_MEM;
    }
    dev->board_data = board_data;

    /* Initialize board data */
    board_data->fpga_version.describe = board_data->fpga_version_str;
    board_data->fw_version.describe = board_data->fw_version_str;

    board_data->module_format[BLADERF_RX] = -1;
    board_data->module_format[BLADERF_TX] = -1;

    /* Read firmware version */
    status = dev->backend->get_fw_version(dev, &board_data->fw_version);
    if (status < 0) {
        log_debug("Failed to get FW version: %s\n",
                  bladerf_strerror(status));
        return status;
    }
    log_verbose("Read Firmware version: %s\n", board_data->fw_version.describe);

    /* Update device state */
    board_data->state = STATE_FIRMWARE_LOADED;

    /* Determine firmware capabilities */
    board_data->capabilities |= bladerf1_get_fw_capabilities(&board_data->fw_version);
    log_verbose("Capability mask before FPGA load: 0x%016"PRIx64"\n",
                 board_data->capabilities);

    /* Wait until firmware is ready */
    if (have_cap(board_data->capabilities, BLADERF_CAP_QUERY_DEVICE_READY)) {
        const unsigned int max_retries = 30;
        unsigned int i;
        int ready;

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
    } else {
        log_info("FX3 FW v%u.%u.%u does not support the \"device ready\" query.\n"
                 "\tEnsure flash-autoloading completes before opening a device.\n"
                 "\tUpgrade the FX3 firmware to avoid this message in the future.\n"
                 "\n", board_data->fw_version.major, board_data->fw_version.minor,
                 board_data->fw_version.patch);
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
    status = version_check_fw(&bladerf1_fw_compat_table,
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

    /* VCTCXO trim and FPGA size are non-fatal indicators that we've
     * trashed the calibration region of flash. If these were made fatal,
     * we wouldn't be able to open the device to restore them. */

    status = spi_flash_read_vctcxo_trim(dev, &board_data->dac_trim);
    if (status < 0) {
        log_warning("Failed to get VCTCXO trim value: %s\n",
                    bladerf_strerror(status));
        log_debug("Defaulting DAC trim to 0x8000.\n");
        board_data->dac_trim = 0x8000;
    }

    status = spi_flash_read_fpga_size(dev, &board_data->fpga_size);
    if (status < 0) {
        log_warning("Failed to get FPGA size %s\n",
                    bladerf_strerror(status));
    }

    /* This will be set in initialize() after we can determine which
     * methods the FPGA supports (based upon version number). */
    board_data->tuning_mode = BLADERF_TUNING_MODE_INVALID;

    /* Load any available calibration tables so that the LMS DC register
     * configurations may be loaded in initialize() */
    snprintf(filename, sizeof(filename), "%s_dc_rx.tbl", dev->ident.serial);
    full_path = file_find(filename);
    if (full_path != NULL) {
        log_debug("Loading RX calibration image %s\n", full_path);
        dc_cal_tbl_image_load(&board_data->cal.dc_rx, full_path);
        free(full_path);
    }
    free(full_path);

    snprintf(filename, sizeof(filename), "%s_dc_tx.tbl", dev->ident.serial);
    full_path = file_find(filename);
    if (full_path != NULL) {
        log_debug("Loading TX calibration image %s\n", full_path);
        dc_cal_tbl_image_load(&board_data->cal.dc_tx, full_path);
        free(full_path);
    }
    free(full_path);

    status = dev->backend->is_fpga_configured(dev);
    if (status < 0) {
        return status;
    } else if (status == 1) {
        board_data->state = STATE_FPGA_LOADED;
    } else if (status != 1 && board_data->fpga_size == BLADERF_FPGA_UNKNOWN) {
        log_warning("Unknown FPGA size. Skipping FPGA configuration...\n");
        log_warning("Skipping further initialization...\n");
        return 0;
    } else if (status != 1) {
        /* Try searching for an FPGA in the config search path */
        if (board_data->fpga_size == BLADERF_FPGA_40KLE) {
            full_path = file_find("hostedx40.rbf");
        } else if (board_data->fpga_size == BLADERF_FPGA_115KLE) {
            full_path = file_find("hostedx115.rbf");
        } else {
            log_error("Invalid FPGA size %d.\n", board_data->fpga_size);
            return BLADERF_ERR_UNEXPECTED;
        }

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

            board_data->state = STATE_FPGA_LOADED;
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
    board_data->capabilities |= bladerf1_get_fpga_capabilities(&board_data->fpga_version);
    log_verbose("Capability mask after FPGA load: 0x%016"PRIx64"\n",
                 board_data->capabilities);

    if (getenv("BLADERF_FORCE_LEGACY_NIOS_PKT")) {
        board_data->capabilities &= ~BLADERF_CAP_PKT_HANDLER_FMT;
        log_verbose("Using legacy packet handler format due to env var\n");
    }

    /* If the FPGA version check fails, just warn, but don't error out.
     *
     * If an error code caused this function to bail out, it would prevent a
     * user from being able to unload and reflash a bitstream being
     * "autoloaded" from SPI flash. */
    status = version_check(&bladerf1_fw_compat_table, &bladerf1_fpga_compat_table,
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

    /* Set FPGA packet protocol */
    if (have_cap(board_data->capabilities, BLADERF_CAP_PKT_HANDLER_FMT)) {
        status = dev->backend->set_fpga_protocol(dev, BACKEND_FPGA_PROTOCOL_NIOSII);
    } else {
        status = dev->backend->set_fpga_protocol(dev, BACKEND_FPGA_PROTOCOL_NIOSII_LEGACY);
    }
    if (status < 0) {
        log_error("Unable to set backend FPGA protocol: %d\n", status);
        return status;
    }

    /* Initialize the device before we try to interact with it.  In the case
     * of an autoloaded FPGA, we need to ensure the clocks are all running
     * before we can try to cancel any scheduled retunes or else the NIOS
     * hangs. */
    status = bladerf1_initialize(dev);
    if (status != 0) {
        return status;
    }

    if (have_cap(board_data->capabilities, BLADERF_CAP_SCHEDULED_RETUNE)) {
        /* Cancel any pending re-tunes that may have been left over as the
         * result of a user application crashing or forgetting to call
         * bladerf_close() */
        status = dev->board->cancel_scheduled_retunes(dev, BLADERF_CHANNEL_RX(0));
        if (status != 0) {
            log_warning("Failed to cancel any pending RX retunes: %s\n",
                    bladerf_strerror(status));
            return status;
        }

        status = dev->board->cancel_scheduled_retunes(dev, BLADERF_CHANNEL_TX(0));
        if (status != 0) {
            log_warning("Failed to cancel any pending TX retunes: %s\n",
                    bladerf_strerror(status));
            return status;
        }
    }

    return 0;
}

static void bladerf1_close(struct bladerf *dev)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    int status;

    if (board_data) {
        sync_deinit(&board_data->sync[BLADERF_CHANNEL_RX(0)]);
        sync_deinit(&board_data->sync[BLADERF_CHANNEL_TX(0)]);

        status = dev->backend->is_fpga_configured(dev);
        if (status == 1 && have_cap(board_data->capabilities, BLADERF_CAP_SCHEDULED_RETUNE)) {
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
            dev->board->cancel_scheduled_retunes(dev, BLADERF_CHANNEL_RX(0));
            dev->board->cancel_scheduled_retunes(dev, BLADERF_CHANNEL_TX(0));
        }

        /* Detach expansion board */
        switch (dev->xb) {
            case BLADERF_XB_100:
                xb100_detach(dev);
                break;
            case BLADERF_XB_200:
                xb200_detach(dev);
                break;
            case BLADERF_XB_300:
                xb300_detach(dev);
                break;
            default:
                break;
        }

        dc_cal_tbl_free(&board_data->cal.dc_rx);
        dc_cal_tbl_free(&board_data->cal.dc_tx);

        free(board_data);
    }
}

/******************************************************************************/
/* Properties */
/******************************************************************************/

static bladerf_dev_speed bladerf1_device_speed(struct bladerf *dev)
{
    int status;
    bladerf_dev_speed usb_speed;

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    status = dev->backend->get_device_speed(dev, &usb_speed);
    if (status < 0) {
        return BLADERF_DEVICE_SPEED_UNKNOWN;
    }

    return usb_speed;
}

static int bladerf1_get_serial(struct bladerf *dev, char *serial)
{
    strcpy(serial, dev->ident.serial);

    return 0;
}

static int bladerf1_get_fpga_size(struct bladerf *dev, bladerf_fpga_size *size)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    *size = board_data->fpga_size;

    return 0;
}

static int bladerf1_is_fpga_configured(struct bladerf *dev)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    return dev->backend->is_fpga_configured(dev);
}

static uint64_t bladerf1_get_capabilities(struct bladerf *dev)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    return board_data->capabilities;
}

/******************************************************************************/
/* Versions */
/******************************************************************************/

static int bladerf1_get_fpga_version(struct bladerf *dev, struct bladerf_version *version)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    memcpy(version, &board_data->fpga_version, sizeof(*version));

    return 0;
}

static int bladerf1_get_fw_version(struct bladerf *dev, struct bladerf_version *version)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    memcpy(version, &board_data->fw_version, sizeof(*version));

    return 0;
}

/******************************************************************************/
/* Enable/disable */
/******************************************************************************/

static int perform_format_deconfig(struct bladerf *dev, bladerf_direction dir);

static int bladerf1_enable_module(struct bladerf *dev, bladerf_direction dir, bool enable)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    bladerf_channel ch;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (dir != BLADERF_RX && dir != BLADERF_TX) {
        return BLADERF_ERR_INVAL;
    }

    log_debug("Enable direction: %s - %s\n",
                (dir == BLADERF_RX) ? "RX" : "TX",
                enable ? "True" : "False") ;

    if (enable == false) {
        sync_deinit(&board_data->sync[dir]);
        perform_format_deconfig(dev, dir);
    }

    ch = (dir == BLADERF_RX) ? BLADERF_CHANNEL_RX(0) : BLADERF_CHANNEL_TX(0);
    lms_enable_rffe(dev, ch, enable);
    status = dev->backend->enable_module(dev, dir, enable);

    return status;
}

/******************************************************************************/
/* Gain */
/******************************************************************************/

static bladerf_lna_gain convert_gain_to_lna_gain(int gain)
{
    switch (gain) {
        case BLADERF_LNA_GAIN_MAX_DB:
            return BLADERF_LNA_GAIN_MAX;
        case BLADERF_LNA_GAIN_MID_DB:
            return BLADERF_LNA_GAIN_MID;
        case BLADERF_LNA_GAIN_BYPASS:
            return BLADERF_LNA_GAIN_BYPASS;
        default:
            return BLADERF_LNA_GAIN_UNKNOWN;
    }
}

static int convert_lna_gain_to_gain(bladerf_lna_gain lnagain)
{
    switch (lnagain) {
        case BLADERF_LNA_GAIN_MAX:
            return BLADERF_LNA_GAIN_MAX_DB;
        case BLADERF_LNA_GAIN_MID:
            return BLADERF_LNA_GAIN_MID_DB;
        case BLADERF_LNA_GAIN_BYPASS:
            return 0;
        default:
            return -1;
    }
}

static int set_rx_gain(struct bladerf *dev, int gain)
{
    int status;
    bladerf_lna_gain lnagain;
    int rxvga1;
    int rxvga2;

    if (gain <= BLADERF_LNA_GAIN_MID_DB) {
        lnagain = BLADERF_LNA_GAIN_BYPASS;
        rxvga1 = BLADERF_RXVGA1_GAIN_MIN;
        rxvga2 = BLADERF_RXVGA2_GAIN_MIN;
    } else if (gain <= BLADERF_LNA_GAIN_MID_DB + BLADERF_RXVGA1_GAIN_MIN) {
        lnagain = BLADERF_LNA_GAIN_MID_DB;
        rxvga1 = BLADERF_RXVGA1_GAIN_MIN;
        rxvga2 = BLADERF_RXVGA2_GAIN_MIN;
    } else if (gain <= (BLADERF_LNA_GAIN_MAX_DB + BLADERF_RXVGA1_GAIN_MAX)) {
        lnagain = BLADERF_LNA_GAIN_MID;
        rxvga1 = gain - BLADERF_LNA_GAIN_MID_DB;
        rxvga2 = BLADERF_RXVGA2_GAIN_MIN;
    } else if (gain < (BLADERF_LNA_GAIN_MAX_DB + BLADERF_RXVGA1_GAIN_MAX + BLADERF_RXVGA2_GAIN_MAX)) {
        lnagain = BLADERF_LNA_GAIN_MAX;
        rxvga1 = BLADERF_RXVGA1_GAIN_MAX;
        rxvga2 = gain - (BLADERF_LNA_GAIN_MAX_DB + BLADERF_RXVGA1_GAIN_MAX);
    } else {
        lnagain = BLADERF_LNA_GAIN_MAX;
        rxvga1 = BLADERF_RXVGA1_GAIN_MAX;
        rxvga2 = BLADERF_RXVGA2_GAIN_MAX;
    }

    status = lms_lna_set_gain(dev, lnagain);
    if (status < 0) {
        return status;
    }

    status = lms_rxvga1_set_gain(dev, rxvga1);
    if (status < 0) {
        return status;
    }

    status = lms_rxvga2_set_gain(dev, rxvga2);
    if (status < 0) {
        return status;
    }

    return 0;
}

static int get_rx_gain(struct bladerf *dev, int *gain)
{
    int status;
    bladerf_lna_gain lnagain;
    int lnagain_db;
    int rxvga1;
    int rxvga2;

    status = lms_lna_get_gain(dev, &lnagain);
    if (status < 0) {
        return status;
    }

    status = lms_rxvga1_get_gain(dev, &rxvga1);
    if (status < 0) {
        return status;
    }

    status = lms_rxvga2_get_gain(dev, &rxvga2);
    if (status < 0) {
        return status;
    }

    if (lnagain == BLADERF_LNA_GAIN_BYPASS) {
        lnagain_db = 0;
    } else if (lnagain == BLADERF_LNA_GAIN_MID) {
        lnagain_db = BLADERF_LNA_GAIN_MID_DB;
    } else if (lnagain == BLADERF_LNA_GAIN_MAX) {
        lnagain_db = BLADERF_LNA_GAIN_MAX_DB;
    } else {
        return BLADERF_ERR_UNEXPECTED;
    }

    *gain = lnagain_db + rxvga1 + rxvga2;

    return 0;
}

static int set_tx_gain(struct bladerf *dev, int gain)
{
    int status;
    int txvga1;
    int txvga2;

    const int max_gain =
        (BLADERF_TXVGA1_GAIN_MAX - BLADERF_TXVGA1_GAIN_MIN)
            + BLADERF_TXVGA2_GAIN_MAX;

    if (gain < 0) {
        gain = 0;
    }

    if (gain <= BLADERF_TXVGA2_GAIN_MAX) {
        txvga1 = BLADERF_TXVGA1_GAIN_MIN;
        txvga2 = gain;
    } else if (gain <= max_gain) {
        txvga1 = BLADERF_TXVGA1_GAIN_MIN + gain - BLADERF_TXVGA2_GAIN_MAX;
        txvga2 = BLADERF_TXVGA2_GAIN_MAX;
    } else {
        txvga1 = BLADERF_TXVGA1_GAIN_MAX;
        txvga2 = BLADERF_TXVGA2_GAIN_MAX;
    }

    status = lms_txvga1_set_gain(dev, txvga1);
    if (status < 0) {
        return status;
    }

    status = lms_txvga2_set_gain(dev, txvga2);
    if (status < 0) {
        return status;
    }

    return 0;
}

static int get_tx_gain(struct bladerf *dev, int *gain)
{
    int status;
    int txvga1;
    int txvga2;

    status = lms_txvga1_get_gain(dev, &txvga1);
    if (status < 0) {
        return status;
    }

    status = lms_txvga2_get_gain(dev, &txvga2);
    if (status < 0) {
        return status;
    }

    *gain = txvga1 + txvga2;

    return 0;
}

static int bladerf1_set_gain(struct bladerf *dev, bladerf_channel ch, int gain)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (ch == BLADERF_CHANNEL_TX(0)) {
        return set_tx_gain(dev, gain);
    } else if (ch == BLADERF_CHANNEL_RX(0)) {
        return set_rx_gain(dev, gain);
    }

    return BLADERF_ERR_INVAL;
}

static int bladerf1_set_gain_mode(struct bladerf *dev, bladerf_module mod, bladerf_gain_mode mode)
{
    int status;
    uint32_t config_gpio;
    struct bladerf1_board_data *board_data = dev->board_data;

    if (mod != BLADERF_MODULE_RX) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    if (!have_cap(board_data->capabilities, BLADERF_CAP_AGC_DC_LUT) || !board_data->cal.dc_rx) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    if (board_data->cal.dc_rx->version != 2) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    if ((status = dev->backend->config_gpio_read(dev, &config_gpio))) {
        return status;
    }

    if (mode == BLADERF_GAIN_AUTOMATIC) {
        config_gpio |= BLADERF_GPIO_AGC_ENABLE;
    } else if (mode == BLADERF_GAIN_MANUAL) {
        config_gpio &= ~BLADERF_GPIO_AGC_ENABLE;
    }

    return dev->backend->config_gpio_write(dev, config_gpio);
}

static int bladerf1_get_gain_mode(struct bladerf *dev, bladerf_module mod, bladerf_gain_mode *mode)
{
    int status;
    uint32_t config_gpio;

    if ((status = dev->backend->config_gpio_read(dev, &config_gpio))) {
        return status;
    }

    if (config_gpio & BLADERF_GPIO_AGC_ENABLE) {
        *mode = BLADERF_GAIN_AUTOMATIC;
    } else {
        *mode = BLADERF_GAIN_MANUAL;
    }

    return status;
}

static int bladerf1_get_gain(struct bladerf *dev, bladerf_channel ch, int *gain)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (ch == BLADERF_CHANNEL_TX(0)) {
        return get_tx_gain(dev, gain);
    } else if (ch == BLADERF_CHANNEL_RX(0)) {
        return get_rx_gain(dev, gain);
    }

    return BLADERF_ERR_INVAL;
}

static int bladerf1_get_gain_range(struct bladerf *dev, bladerf_channel ch, struct bladerf_range *range)
{
    if (ch == BLADERF_CHANNEL_TX(0)) {
        *range = bladerf1_tx_gain_range;
    } else if (ch == BLADERF_CHANNEL_RX(0)) {
        *range = bladerf1_rx_gain_range;
    }

    return 0;
}

static int bladerf1_set_gain_stage(struct bladerf *dev, bladerf_channel ch, const char *stage, int gain)
{
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    /* TODO implement gain clamping */

    status = BLADERF_ERR_INVAL;

    if (ch == BLADERF_CHANNEL_TX(0)) {
        if (strcmp(stage, "txvga1") == 0) {
            status = lms_txvga1_set_gain(dev, gain);
        } else if (strcmp(stage, "txvga2") == 0) {
            status = lms_txvga2_set_gain(dev, gain);
        }
    } else if (ch == BLADERF_CHANNEL_RX(0)) {
        if (strcmp(stage, "rxvga1") == 0) {
            status = lms_rxvga1_set_gain(dev, gain);
        } else if (strcmp(stage, "rxvga2") == 0) {
            status = lms_rxvga2_set_gain(dev, gain);
        } else if (strcmp(stage, "lna") == 0) {
            status = lms_lna_set_gain(dev, convert_gain_to_lna_gain(gain));
        }
    }

    if (status < 0) {
        return status;
    }

    return 0;
}

static int bladerf1_get_gain_stage(struct bladerf *dev, bladerf_channel ch, const char *stage, int *gain)
{
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    status = BLADERF_ERR_INVAL;

    if (ch == BLADERF_CHANNEL_TX(0)) {
        if (strcmp(stage, "txvga1") == 0) {
            status = lms_txvga1_get_gain(dev, gain);
        } else if (strcmp(stage, "txvga2") == 0) {
            status = lms_txvga2_get_gain(dev, gain);
        }
    } else if (ch == BLADERF_CHANNEL_RX(0)) {
        if (strcmp(stage, "rxvga1") == 0) {
            status = lms_rxvga1_get_gain(dev, gain);
        } else if (strcmp(stage, "rxvga2") == 0) {
            status = lms_rxvga2_get_gain(dev, gain);
        } else if (strcmp(stage, "lna") == 0) {
            bladerf_lna_gain lnagain;
            status = lms_lna_get_gain(dev, &lnagain);
            if (status == 0) {
                *gain = convert_lna_gain_to_gain(lnagain);
            }
        }
    }

    if (status < 0) {
        return status;
    }

    return 0;
}

static int bladerf1_get_gain_stage_range(struct bladerf *dev, bladerf_channel ch, const char *stage, struct bladerf_range *range)
{
    const struct bladerf_gain_stage_info *stage_infos;
    unsigned int stage_infos_len;
    unsigned int i;

    if (ch == BLADERF_CHANNEL_TX(0)) {
        stage_infos = bladerf1_tx_gain_stages;
        stage_infos_len = ARRAY_SIZE(bladerf1_tx_gain_stages);
    } else if (ch == BLADERF_CHANNEL_RX(0)) {
        stage_infos = bladerf1_rx_gain_stages;
        stage_infos_len = ARRAY_SIZE(bladerf1_rx_gain_stages);
    } else {
        return BLADERF_ERR_INVAL;
    }

    for (i = 0; i < stage_infos_len; i++) {
        if (strcmp(stage_infos[i].name, stage) == 0) {
            *range = stage_infos[i].range;
            return 0;
        }
    }

    return BLADERF_ERR_INVAL;
}

static int bladerf1_get_gain_stages(struct bladerf *dev, bladerf_channel ch, const char **stages, unsigned int count)
{
    const struct bladerf_gain_stage_info *stage_infos;
    unsigned int stage_infos_len;
    unsigned int i;

    if (ch == BLADERF_CHANNEL_TX(0)) {
        stage_infos = bladerf1_tx_gain_stages;
        stage_infos_len = ARRAY_SIZE(bladerf1_tx_gain_stages);
    } else if (ch == BLADERF_CHANNEL_RX(0)) {
        stage_infos = bladerf1_rx_gain_stages;
        stage_infos_len = ARRAY_SIZE(bladerf1_rx_gain_stages);
    } else {
        return BLADERF_ERR_INVAL;
    }

    if (stages != NULL) {
        count = (stage_infos_len < count) ? stage_infos_len : count;

        for (i = 0; i < count; i++) {
            stages[i] = stage_infos[i].name;
        }
    }

    return stage_infos_len;
}

/******************************************************************************/
/* Sample Rate */
/******************************************************************************/

static int bladerf1_set_sample_rate(struct bladerf *dev, bladerf_channel ch, unsigned int rate, unsigned int *actual)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return si5338_set_sample_rate(dev, ch, rate, actual);
}

static int bladerf1_get_sample_rate(struct bladerf *dev, bladerf_channel ch, unsigned int *rate)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return si5338_get_sample_rate(dev, ch, rate);
}

static int bladerf1_get_sample_rate_range(struct bladerf *dev, bladerf_channel ch, struct bladerf_range *range)
{
    *range = bladerf1_sample_rate_range;
    return 0;
}

static int bladerf1_set_rational_sample_rate(struct bladerf *dev, bladerf_channel ch, struct bladerf_rational_rate *rate, struct bladerf_rational_rate *actual)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return si5338_set_rational_sample_rate(dev, ch, rate, actual);
}

static int bladerf1_get_rational_sample_rate(struct bladerf *dev, bladerf_channel ch, struct bladerf_rational_rate *rate)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return si5338_get_rational_sample_rate(dev, ch, rate);
}

/******************************************************************************/
/* Bandwidth */
/******************************************************************************/

static int bladerf1_set_bandwidth(struct bladerf *dev, bladerf_channel ch, unsigned int bandwidth, unsigned int *actual)
{
    int status;
    lms_bw bw;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (bandwidth < BLADERF_BANDWIDTH_MIN) {
        bandwidth = BLADERF_BANDWIDTH_MIN;
        log_info("Clamping bandwidth to %dHz\n", bandwidth);
    } else if (bandwidth > BLADERF_BANDWIDTH_MAX) {
        bandwidth = BLADERF_BANDWIDTH_MAX;
        log_info("Clamping bandwidth to %dHz\n", bandwidth);
    }

    bw = lms_uint2bw(bandwidth);

    status = lms_lpf_enable(dev, ch, true);
    if (status != 0) {
        return status;
    }

    status = lms_set_bandwidth(dev, ch, bw);
    if (actual != NULL) {
        if (status == 0) {
            *actual = lms_bw2uint(bw);
        } else {
            *actual = 0;
        }
    }

    return status;
}

static int bladerf1_get_bandwidth(struct bladerf *dev, bladerf_channel ch, unsigned int *bandwidth)
{
    int status;
    lms_bw bw;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    status = lms_get_bandwidth( dev, ch, &bw);
    if (status == 0) {
        *bandwidth = lms_bw2uint(bw);
    } else {
        *bandwidth = 0;
    }

    return status;
}

static int bladerf1_get_bandwidth_range(struct bladerf *dev, bladerf_channel ch, struct bladerf_range *range)
{
    *range = bladerf1_bandwidth_range;
    return 0;
}

/******************************************************************************/
/* Frequency */
/******************************************************************************/

static int bladerf1_set_frequency(struct bladerf *dev, bladerf_channel ch, uint64_t frequency)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    const bladerf_xb attached = dev->xb;
    int status;
    int16_t dc_i, dc_q;
    struct dc_cal_entry entry;
    const struct dc_cal_tbl *dc_cal =
        (ch == BLADERF_CHANNEL_RX(0)) ? board_data->cal.dc_rx : board_data->cal.dc_tx;

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    log_debug("Setting %s frequency to %u\n", channel2str(ch), frequency);

    if (attached == BLADERF_XB_200) {
        if (frequency < BLADERF_FREQUENCY_MIN) {
            status = xb200_set_path(dev, ch, BLADERF_XB200_MIX);
            if (status) {
                return status;
            }

            status = xb200_auto_filter_selection(dev, ch, frequency);
            if (status) {
                return status;
            }

            frequency = 1248000000 - frequency;
        } else {
            status = xb200_set_path(dev, ch, BLADERF_XB200_BYPASS);
            if (status) {
                return status;
            }
        }
    }

    switch (board_data->tuning_mode) {
        case BLADERF_TUNING_MODE_HOST:
            status = lms_set_frequency(dev, ch, frequency);
            if (status != 0) {
                return status;
            }

            status = band_select(dev, ch, frequency < BLADERF1_BAND_HIGH);
            break;

        case BLADERF_TUNING_MODE_FPGA: {
            status = dev->board->schedule_retune(dev, ch, BLADERF_RETUNE_NOW, frequency, NULL);
            break;
        }

        default:
            log_debug("Invalid tuning mode: %d\n", board_data->tuning_mode);
            status = BLADERF_ERR_INVAL;
            break;
    }
    if (status != 0) {
        return status;
    }

    if (dc_cal != NULL) {
        dc_cal_tbl_entry(dc_cal, frequency, &entry);

        dc_i = entry.dc_i;
        dc_q = entry.dc_q;

        status = lms_set_dc_offset_i(dev, ch, dc_i);
        if (status != 0) {
            return status;
        }

        status = lms_set_dc_offset_q(dev, ch, dc_q);
        if (status != 0) {
            return status;
        }

        if (ch == BLADERF_CHANNEL_RX(0) &&
            have_cap(board_data->capabilities, BLADERF_CAP_AGC_DC_LUT)) {

            status = dev->backend->set_agc_dc_correction(dev,
                            entry.max_dc_q, entry.max_dc_i,
                            entry.mid_dc_q, entry.mid_dc_i,
                            entry.min_dc_q, entry.min_dc_i);
            if (status != 0) {
                return status;
            }

            log_verbose("Set AGC DC offset cal (I, Q) to: Max (%d, %d) "
                        " Mid (%d, %d) Min (%d, %d)\n",
                        entry.max_dc_q, entry.max_dc_i,
                        entry.mid_dc_q, entry.mid_dc_i,
                        entry.min_dc_q, entry.min_dc_i);
        }

        log_verbose("Set %s DC offset cal (I, Q) to: (%d, %d)\n",
                    (ch == BLADERF_CHANNEL_RX(0)) ? "RX" : "TX", dc_i, dc_q);
    }

    return 0;
}

static int bladerf1_get_frequency(struct bladerf *dev, bladerf_channel ch, uint64_t *frequency)
{
    bladerf_xb200_path path;
    struct lms_freq f;
    int status = 0;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    status = lms_get_frequency(dev, ch, &f);
    if (status != 0) {
        return status;
    }

    if (f.x == 0) {
        /* If we see this, it's most often an indication that communication
         * with the LMS6002D is not occuring correctly */
        *frequency = 0 ;
        status = BLADERF_ERR_IO;
    } else {
        *frequency = lms_frequency_to_hz(&f);
    }
    if (status != 0) {
        return status;
    }

    if (dev->xb == BLADERF_XB_200) {
        status = xb200_get_path(dev, ch, &path);
        if (status != 0) {
            return status;
        }
        if (path == BLADERF_XB200_MIX) {
            *frequency = 1248000000 - *frequency;
        }
    }

    return 0;
}

static int bladerf1_get_frequency_range(struct bladerf *dev, bladerf_channel ch, struct bladerf_range *range)
{
    if (dev->xb == BLADERF_XB_200) {
        *range = bladerf1_xb200_frequency_range;
    } else {
        *range = bladerf1_frequency_range;
    }

    return 0;
}

static int bladerf1_select_band(struct bladerf *dev, bladerf_channel ch, uint64_t frequency)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return band_select(dev, ch, frequency < BLADERF1_BAND_HIGH);
}

/******************************************************************************/
/* RF ports */
/******************************************************************************/

static int bladerf1_set_rf_port(struct bladerf *dev, bladerf_channel ch, const char *port)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf1_get_rf_port(struct bladerf *dev, bladerf_channel ch, const char **port)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf1_get_rf_ports(struct bladerf *dev, bladerf_channel ch, const char **ports, unsigned int count)
{
    return BLADERF_ERR_UNSUPPORTED;
}

/******************************************************************************/
/* Scheduled Tuning */
/******************************************************************************/

static int bladerf1_get_quick_tune(struct bladerf *dev, bladerf_channel ch, struct bladerf_quick_tune *quick_tune)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return lms_get_quick_tune(dev, ch, quick_tune);
}

static int bladerf1_schedule_retune(struct bladerf *dev, bladerf_channel ch, uint64_t timestamp, uint64_t frequency, struct bladerf_quick_tune *quick_tune)

{
    struct bladerf1_board_data *board_data = dev->board_data;
    int status;
    struct lms_freq f;

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    if (!have_cap(board_data->capabilities, BLADERF_CAP_SCHEDULED_RETUNE)) {
        log_debug("This FPGA version (%u.%u.%u) does not support "
                  "scheduled retunes.\n",  board_data->fpga_version.major,
                  board_data->fpga_version.minor, board_data->fpga_version.patch);

        return BLADERF_ERR_UNSUPPORTED;
    }

    if (quick_tune == NULL) {
        status = lms_calculate_tuning_params(frequency, &f);
        if (status != 0) {
            return status;
        }
    } else {
        f.freqsel = quick_tune->freqsel;
        f.vcocap  = quick_tune->vcocap;
        f.nint    = quick_tune->nint;
        f.nfrac   = quick_tune->nfrac;
        f.flags   = quick_tune->flags;
        f.x       = 0;
        f.vcocap_result = 0;
    }

    return dev->backend->retune(dev, ch, timestamp,
                       f.nint, f.nfrac, f.freqsel, f.vcocap,
                       (f.flags & LMS_FREQ_FLAGS_LOW_BAND) != 0,
                       (f.flags & LMS_FREQ_FLAGS_FORCE_VCOCAP) != 0);
}

int bladerf1_cancel_scheduled_retunes(struct bladerf *dev, bladerf_channel ch)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    int status;

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    if (have_cap(board_data->capabilities, BLADERF_CAP_SCHEDULED_RETUNE)) {
        status = dev->backend->retune(dev, ch, NIOS_PKT_RETUNE_CLEAR_QUEUE, 0, 0, 0, 0, false, false);
    } else {
        log_debug("This FPGA version (%u.%u.%u) does not support "
                  "scheduled retunes.\n",  board_data->fpga_version.major,
                  board_data->fpga_version.minor, board_data->fpga_version.patch);

        return BLADERF_ERR_UNSUPPORTED;
    }

    return status;
}

/******************************************************************************/
/* Trigger */
/******************************************************************************/

static int bladerf1_trigger_init(struct bladerf *dev, bladerf_channel ch, bladerf_trigger_signal signal, struct bladerf_trigger *trigger)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (!have_cap(board_data->capabilities, BLADERF_CAP_TRX_SYNC_TRIG)) {
        log_debug("FPGA v%s does not support synchronization triggers.\n",
                  board_data->fpga_version.describe);
        return BLADERF_ERR_UNSUPPORTED;
    }

    return fpga_trigger_init(dev, ch, signal, trigger);
}

static int bladerf1_trigger_arm(struct bladerf *dev, const struct bladerf_trigger *trigger, bool arm, uint64_t resv1, uint64_t resv2)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (!have_cap(board_data->capabilities, BLADERF_CAP_TRX_SYNC_TRIG)) {
        log_debug("FPGA v%s does not support synchronization triggers.\n",
                  board_data->fpga_version.describe);
        return BLADERF_ERR_UNSUPPORTED;
    }

    /* resv1 & resv2 unused - may be allocated for use as timestamp and
     * other flags in the future */

    return fpga_trigger_arm(dev, trigger, arm);
}

static int bladerf1_trigger_fire(struct bladerf *dev, const struct bladerf_trigger *trigger)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (!have_cap(board_data->capabilities, BLADERF_CAP_TRX_SYNC_TRIG)) {
        log_debug("FPGA v%s does not support synchronization triggers.\n",
                  board_data->fpga_version.describe);
        return BLADERF_ERR_UNSUPPORTED;
    }

    return fpga_trigger_fire(dev, trigger);
}

static int bladerf1_trigger_state(struct bladerf *dev, const struct bladerf_trigger *trigger, bool *is_armed, bool *has_fired, bool *fire_requested, uint64_t *resv1, uint64_t *resv2)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (!have_cap(board_data->capabilities, BLADERF_CAP_TRX_SYNC_TRIG)) {
        log_debug("FPGA v%s does not support synchronization triggers.\n",
                  board_data->fpga_version.describe);
        return BLADERF_ERR_UNSUPPORTED;
    }

    status = fpga_trigger_state(dev, trigger, is_armed, has_fired, fire_requested);

    /* Reserved for future metadata (e.g., trigger counts, timestamp) */
    if (resv1 != NULL) {
        *resv1 = 0;
    }

    if (resv2 != NULL) {
        *resv2 = 0;
    }

    return status;
}

/******************************************************************************/
/* Streaming */
/******************************************************************************/

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

/**
 * Perform the neccessary device configuration for the specified format
 * (e.g., enabling/disabling timestamp support), first checking that the
 * requested format would not conflict with the other stream direction.
 *
 * @param   dev     Device handle
 * @param   dir     Direction that is currently being configured
 * @param   format  Format the channel is being configured for
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
static int perform_format_config(struct bladerf *dev, bladerf_direction dir,
                                 bladerf_format format)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    int status = 0;
    bool use_timestamps;
    bladerf_channel other;
    bool other_using_timestamps;
    uint32_t gpio_val;

    status = requires_timestamps(format, &use_timestamps);
    if (status != 0) {
        log_debug("%s: Invalid format: %d\n", __FUNCTION__, format);
        return status;
    }

    if (use_timestamps && !have_cap(board_data->capabilities, BLADERF_CAP_TIMESTAMPS)) {
        log_warning("Timestamp support requires FPGA v0.1.0 or later.\n");
        return BLADERF_ERR_UPDATE_FPGA;
    }

    switch (dir) {
        case BLADERF_RX:
            other = BLADERF_TX;
            break;

        case BLADERF_TX:
            other = BLADERF_RX;
            break;

        default:
            log_debug("Invalid direction: %d\n", dir);
            return BLADERF_ERR_INVAL;
    }

    status = requires_timestamps(board_data->module_format[other],
                                 &other_using_timestamps);

    if ((status == 0) && (other_using_timestamps != use_timestamps)) {
        log_debug("Format conflict detected: RX=%d, TX=%d\n");
        return BLADERF_ERR_INVAL;
    }

    status = dev->backend->config_gpio_read(dev, &gpio_val);
    if (status != 0) {
        return status;
    }

    if (use_timestamps) {
        gpio_val |= (BLADERF_GPIO_TIMESTAMP | BLADERF_GPIO_TIMESTAMP_DIV2);
    } else {
        gpio_val &= ~(BLADERF_GPIO_TIMESTAMP | BLADERF_GPIO_TIMESTAMP_DIV2);
    }

    status = dev->backend->config_gpio_write(dev, gpio_val);
    if (status == 0) {
        board_data->module_format[dir] = format;
    }

    return status;
}

/**
 * Deconfigure and update any state pertaining what a format that a stream
 * direction is no longer using.
 *
 * @param   dev     Device handle
 * @param   dir     Direction that is currently being deconfigured
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
static int perform_format_deconfig(struct bladerf *dev, bladerf_direction dir)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    switch (dir) {
        case BLADERF_RX:
        case BLADERF_TX:
            /* We'll reconfigure the HW when we call perform_format_config, so
             * we just need to update our stored information */
            board_data->module_format[dir] = -1;
            break;

        default:
            log_debug("%s: Invalid direction: %d\n", __FUNCTION__, dir);
            return BLADERF_ERR_INVAL;
    }

    return 0;
}

static int bladerf1_init_stream(struct bladerf_stream **stream, struct bladerf *dev, bladerf_stream_cb callback, void ***buffers, size_t num_buffers, bladerf_format format, size_t samples_per_buffer, size_t num_transfers, void *user_data)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return async_init_stream(stream, dev, callback, buffers, num_buffers,
                             format, samples_per_buffer, num_transfers, user_data);
}

static int bladerf1_stream(struct bladerf_stream *stream, bladerf_channel_layout layout)
{
    bladerf_direction dir = layout & BLADERF_DIRECTION_MASK;
    int stream_status, fmt_status;

    if (layout != BLADERF_RX_X1 && layout != BLADERF_TX_X1) {
        return -EINVAL;
    }

    fmt_status = perform_format_config(stream->dev, dir, stream->format);
    if (fmt_status != 0) {
        return fmt_status;
    }

    stream_status = async_run_stream(stream, layout);

    fmt_status = perform_format_deconfig(stream->dev, dir);
    if (fmt_status != 0) {
        return fmt_status;
    }

    return stream_status;
}

static int bladerf1_submit_stream_buffer(struct bladerf_stream *stream, void *buffer, unsigned int timeout_ms, bool nonblock)
{
    return async_submit_stream_buffer(stream, buffer, timeout_ms, nonblock);
}

static void bladerf1_deinit_stream(struct bladerf_stream *stream)
{
    async_deinit_stream(stream);
}

static int bladerf1_set_stream_timeout(struct bladerf *dev, bladerf_direction dir, unsigned int timeout)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    MUTEX_LOCK(&board_data->sync[dir].lock);
    board_data->sync[dir].stream_config.timeout_ms = timeout;
    MUTEX_UNLOCK(&board_data->sync[dir].lock);

    return 0;
}

static int bladerf1_get_stream_timeout(struct bladerf *dev, bladerf_direction dir, unsigned int *timeout)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    MUTEX_LOCK(&board_data->sync[dir].lock);
    *timeout = board_data->sync[dir].stream_config.timeout_ms;
    MUTEX_UNLOCK(&board_data->sync[dir].lock);

    return 0;
}

static int bladerf1_sync_config(struct bladerf *dev, bladerf_channel_layout layout, bladerf_format format, unsigned int num_buffers, unsigned int buffer_size, unsigned int num_transfers, unsigned int stream_timeout)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    bladerf_direction dir = layout & BLADERF_DIRECTION_MASK;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (layout != BLADERF_RX_X1 && layout != BLADERF_TX_X1) {
        return -EINVAL;
    }

    status = perform_format_config(dev, dir, format);
    if (status == 0) {
        status = sync_init(&board_data->sync[dir], dev, layout,
                           format, num_buffers, buffer_size,
                           board_data->msg_size, num_transfers,
                           stream_timeout);
        if (status != 0) {
            perform_format_deconfig(dev, dir);
        }
    }

    return status;
}

static int bladerf1_sync_tx(struct bladerf *dev, void *samples, unsigned int num_samples, struct bladerf_metadata *metadata, unsigned int timeout_ms)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    int status;

    if (!board_data->sync[BLADERF_TX].initialized) {
        return BLADERF_ERR_INVAL;
    }

    status = sync_tx(&board_data->sync[BLADERF_TX],
                     samples, num_samples, metadata, timeout_ms);

    return status;
}

static int bladerf1_sync_rx(struct bladerf *dev, void *samples, unsigned int num_samples, struct bladerf_metadata *metadata, unsigned int timeout_ms)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    int status;

    if (!board_data->sync[BLADERF_RX].initialized) {
        return BLADERF_ERR_INVAL;
    }

    status = sync_rx(&board_data->sync[BLADERF_RX],
                     samples, num_samples, metadata, timeout_ms);

    return status;
}

static int bladerf1_get_timestamp(struct bladerf *dev, bladerf_direction dir, uint64_t *value)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return dev->backend->get_timestamp(dev, dir, value);
}

/******************************************************************************/
/* SMB Clock Configuration (board-agnostic feature?) */
/******************************************************************************/

static int bladerf1_get_smb_mode(struct bladerf *dev, bladerf_smb_mode *mode)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return smb_clock_get_mode(dev, mode);
}

static int bladerf1_set_smb_mode(struct bladerf *dev, bladerf_smb_mode mode)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return smb_clock_set_mode(dev, mode);
}

static int bladerf1_get_smb_frequency(struct bladerf *dev, unsigned int *rate)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return si5338_get_smb_freq(dev, rate);
}

static int bladerf1_set_smb_frequency(struct bladerf *dev, uint32_t rate, uint32_t *actual)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return si5338_set_smb_freq(dev, rate, actual);
}

static int bladerf1_get_rational_smb_frequency(struct bladerf *dev, struct bladerf_rational_rate *rate)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return si5338_get_rational_smb_freq(dev, rate);
}

static int bladerf1_set_rational_smb_frequency(struct bladerf *dev, struct bladerf_rational_rate *rate, struct bladerf_rational_rate *actual)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return si5338_set_rational_smb_freq(dev, rate, actual);
}

/******************************************************************************/
/* DC/Phase/Gain Correction */
/******************************************************************************/

static int bladerf1_get_correction(struct bladerf *dev, bladerf_channel ch, bladerf_correction corr, int16_t *value)
{
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    switch (corr) {
        case BLADERF_CORR_FPGA_PHASE:
            status = dev->backend->get_iq_phase_correction(dev, ch, value);
            break;

        case BLADERF_CORR_FPGA_GAIN:
            status = dev->backend->get_iq_gain_correction(dev, ch, value);

            /* Undo the gain control offset */
            if (status == 0) {
                *value -= 4096;
            }
            break;

        case BLADERF_CORR_LMS_DCOFF_I:
            status = lms_get_dc_offset_i(dev, ch, value);
            break;

        case BLADERF_CORR_LMS_DCOFF_Q:
            status = lms_get_dc_offset_q(dev, ch, value);
            break;

        default:
            status = BLADERF_ERR_INVAL;
            log_debug("Invalid correction type: %d\n", corr);
            break;
    }

    return status;
}

static int bladerf1_set_correction(struct bladerf *dev, bladerf_channel ch, bladerf_correction corr, int16_t value)
{
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    switch (corr) {
        case BLADERF_CORR_FPGA_PHASE:
            status = dev->backend->set_iq_phase_correction(dev, ch, value);
            break;

        case BLADERF_CORR_FPGA_GAIN:
            /* Gain correction requires than an offset be applied */
            value += (int16_t) 4096;
            status = dev->backend->set_iq_gain_correction(dev, ch, value);
            break;

        case BLADERF_CORR_LMS_DCOFF_I:
            status = lms_set_dc_offset_i(dev, ch, value);
            break;

        case BLADERF_CORR_LMS_DCOFF_Q:
            status = lms_set_dc_offset_q(dev, ch, value);
            break;

        default:
            status = BLADERF_ERR_INVAL;
            log_debug("Invalid correction type: %d\n", corr);
            break;
    }

    return status;
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

static int bladerf1_load_fpga(struct bladerf *dev, const uint8_t *buf, size_t length)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    struct bladerf_version required_fw_version;
    struct bladerf_version required_fpga_version;
    int status;

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    if (!is_valid_fpga_size(board_data->fpga_size, length)) {
        return BLADERF_ERR_INVAL;
    }

    status = dev->backend->load_fpga(dev, buf, length);
    if (status != 0) {
        return status;
    }

    /* Update device state */
    board_data->state = STATE_FPGA_LOADED;

    /* Read FPGA version */
    status = dev->backend->get_fpga_version(dev, &board_data->fpga_version);
    if (status < 0) {
        log_debug("Failed to get FPGA version: %s\n",
                  bladerf_strerror(status));
        return status;
    }
    log_verbose("Read FPGA version: %s\n", board_data->fpga_version.describe);

    status = version_check(&bladerf1_fw_compat_table, &bladerf1_fpga_compat_table,
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

    status = bladerf1_initialize(dev);
    if (status != 0) {
        return status;
    }

    return 0;
}

static int bladerf1_flash_fpga(struct bladerf *dev, const uint8_t *buf, size_t length)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    if (!is_valid_fpga_size(board_data->fpga_size, length)) {
        return BLADERF_ERR_INVAL;
    }

    return spi_flash_write_fpga_bitstream(dev, buf, length);
}

static int bladerf1_erase_stored_fpga(struct bladerf *dev)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    return spi_flash_erase_fpga(dev);
}

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

static int bladerf1_flash_firmware(struct bladerf *dev, const uint8_t *buf, size_t length)
{
    const char env_override[] = "BLADERF_SKIP_FW_SIZE_CHECK";

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

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
}

static int bladerf1_device_reset(struct bladerf *dev)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    return dev->backend->device_reset(dev);
}

/******************************************************************************/
/* Sample Internal/Direct */
/******************************************************************************/

static int bladerf1_get_sampling(struct bladerf *dev, bladerf_sampling *sampling)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return lms_get_sampling(dev, sampling);
}

static int bladerf1_set_sampling(struct bladerf *dev, bladerf_sampling sampling)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return lms_select_sampling(dev, sampling);
}

/******************************************************************************/
/* Sample RX FPGA Mux */
/******************************************************************************/

static int bladerf1_set_rx_mux(struct bladerf *dev, bladerf_rx_mux mode)
{
    uint32_t rx_mux_val;
    uint32_t config_gpio;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    /* Validate desired mux mode */
    switch (mode) {
        case BLADERF_RX_MUX_BASEBAND_LMS:
        case BLADERF_RX_MUX_12BIT_COUNTER:
        case BLADERF_RX_MUX_32BIT_COUNTER:
        case BLADERF_RX_MUX_DIGITAL_LOOPBACK:
            rx_mux_val = ((uint32_t) mode) << BLADERF_GPIO_RX_MUX_SHIFT;
            break;

        default:
            log_debug("Invalid RX mux mode setting passed to %s(): %d\n",
                      mode, __FUNCTION__);
            return BLADERF_ERR_INVAL;
    }

    status = dev->backend->config_gpio_read(dev, &config_gpio);
    if (status != 0) {
        return status;
    }

    /* Clear out and assign the associated RX mux bits */
    config_gpio &= ~BLADERF_GPIO_RX_MUX_MASK;
    config_gpio |= rx_mux_val;

    return dev->backend->config_gpio_write(dev, config_gpio);
}

static int bladerf1_get_rx_mux(struct bladerf *dev, bladerf_rx_mux *mode)
{
    bladerf_rx_mux val;
    uint32_t config_gpio;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    status = dev->backend->config_gpio_read(dev, &config_gpio);
    if (status != 0) {
        return status;
    }

    /* Extract RX mux bits */
    config_gpio &= BLADERF_GPIO_RX_MUX_MASK;
    config_gpio >>= BLADERF_GPIO_RX_MUX_SHIFT;
    val = (bladerf_rx_mux) (config_gpio);

    /* Enure it's a valid/supported value */
    switch (val) {
        case BLADERF_RX_MUX_BASEBAND_LMS:
        case BLADERF_RX_MUX_12BIT_COUNTER:
        case BLADERF_RX_MUX_32BIT_COUNTER:
        case BLADERF_RX_MUX_DIGITAL_LOOPBACK:
            *mode = val;
            break;

        default:
            *mode = BLADERF_RX_MUX_INVALID;
            status = BLADERF_ERR_UNEXPECTED;
            log_debug("Invalid rx mux mode %d read from config gpio\n", val);
    }

    return status;
}

/******************************************************************************/
/* Tune on host or FPGA */
/******************************************************************************/

static int bladerf1_set_tuning_mode(struct bladerf *dev, bladerf_tuning_mode mode)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (mode == BLADERF_TUNING_MODE_FPGA &&
            !have_cap(board_data->capabilities, BLADERF_CAP_FPGA_TUNING)) {
        log_debug("The loaded FPGA version (%u.%u.%u) does not support the "
                  "provided tuning mode (%d)\n",
                  board_data->fpga_version.major, board_data->fpga_version.minor,
                  board_data->fpga_version.patch, mode);
        return BLADERF_ERR_UNSUPPORTED;
    }

    switch (board_data->tuning_mode) {
        case BLADERF_TUNING_MODE_HOST:
            log_debug("Tuning mode: host\n");
            break;
        case BLADERF_TUNING_MODE_FPGA:
            log_debug("Tuning mode: FPGA\n");
            break;
        default:
            assert(!"Bug encountered.");
            return BLADERF_ERR_INVAL;
    }

    board_data->tuning_mode = mode;

    return 0;
}

/******************************************************************************/
/* Expansion support */
/******************************************************************************/

static int bladerf1_expansion_attach(struct bladerf *dev, bladerf_xb xb)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    bladerf_xb attached;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    status = dev->board->expansion_get_attached(dev, &attached);
    if (status != 0) {
        return status;
    }

    if (xb != attached && attached != BLADERF_XB_NONE) {
        log_debug("%s: Switching XB types is not supported.\n", __FUNCTION__);
        return BLADERF_ERR_UNSUPPORTED;
    }

    if (xb == BLADERF_XB_100) {
        if (!have_cap(board_data->capabilities, BLADERF_CAP_MASKED_XBIO_WRITE)) {
            log_debug("%s: XB100 support requires FPGA v0.4.1 or later.\n",
                      __FUNCTION__);
            return BLADERF_ERR_UNSUPPORTED;
        }

        log_verbose("Attaching XB100\n");
        status = xb100_attach(dev);
        if (status != 0) {
            return status;
        }

        log_verbose("Enabling XB100\n");
        status = xb100_enable(dev, true);
        if (status != 0) {
            return status;
        }

        log_verbose("Initializing XB100\n");
        status = xb100_init(dev);
        if (status != 0) {
            return status;
        }
    } else if (xb == BLADERF_XB_200) {
        if (!have_cap(board_data->capabilities, BLADERF_CAP_XB200)) {
            log_debug("%s: XB200 support requires FPGA v0.0.5 or later\n",
                      __FUNCTION__);
            return BLADERF_ERR_UPDATE_FPGA;
        }

        log_verbose("Attaching XB200\n");
        status = xb200_attach(dev);
        if (status != 0) {
            return status;
        }

        log_verbose("Enabling XB200\n");
        status = xb200_enable(dev, true);
        if (status != 0) {
            return status;
        }

        log_verbose("Initializing XB200\n");
        status = xb200_init(dev);
        if (status != 0) {
            return status;
        }
    } else if (xb == BLADERF_XB_300) {
        log_verbose("Attaching XB300\n");
        status = xb300_attach(dev);
        if (status != 0) {
            return status;
        }

        log_verbose("Enabling XB300\n");
        status = xb300_enable(dev, true);
        if (status != 0) {
            return status;
        }

        log_verbose("Initializing XB300\n");
        status = xb300_init(dev);
        if (status != 0) {
            return status;
        }
    } else if (xb == BLADERF_XB_NONE) {
        log_debug("%s: Disabling an attached XB is not supported.\n",
                  __FUNCTION__);
        return BLADERF_ERR_UNSUPPORTED;
    } else {
        log_debug("%s: Unknown xb type: %d\n", __FUNCTION__, xb);
        return BLADERF_ERR_INVAL;
    }

    return 0;
}

static int bladerf1_expansion_get_attached(struct bladerf *dev, bladerf_xb *xb)
{
    int status;
    uint32_t val;

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    status = dev->backend->config_gpio_read(dev, &val);
    if (status != 0) {
        return status;
    }

    val = (val >> 30) & 0x3;

    switch (val) {
        case BLADERF_XB_NONE:
        case BLADERF_XB_100:
        case BLADERF_XB_200:
        case BLADERF_XB_300:
            *xb = val;
            status = 0;
            break;
        default:
            log_debug("Device handle contains invalid XB id: %d\n", dev->xb);
            status = BLADERF_ERR_UNEXPECTED;
            break;
    }

    return status;
}

/******************************************************************************/
/* Board binding */
/******************************************************************************/

const struct board_fns bladerf1_board_fns = {
    FIELD_INIT(.matches, bladerf1_matches),
    FIELD_INIT(.open, bladerf1_open),
    FIELD_INIT(.close, bladerf1_close),
    FIELD_INIT(.device_speed, bladerf1_device_speed),
    FIELD_INIT(.get_serial, bladerf1_get_serial),
    FIELD_INIT(.get_fpga_size, bladerf1_get_fpga_size),
    FIELD_INIT(.is_fpga_configured, bladerf1_is_fpga_configured),
    FIELD_INIT(.get_capabilities, bladerf1_get_capabilities),
    FIELD_INIT(.get_fpga_version, bladerf1_get_fpga_version),
    FIELD_INIT(.get_fw_version, bladerf1_get_fw_version),
    FIELD_INIT(.set_gain, bladerf1_set_gain),
    FIELD_INIT(.get_gain, bladerf1_get_gain),
    FIELD_INIT(.set_gain_mode, bladerf1_set_gain_mode),
    FIELD_INIT(.get_gain_mode, bladerf1_get_gain_mode),
    FIELD_INIT(.get_gain_range, bladerf1_get_gain_range),
    FIELD_INIT(.set_gain_stage, bladerf1_set_gain_stage),
    FIELD_INIT(.get_gain_stage, bladerf1_get_gain_stage),
    FIELD_INIT(.get_gain_stage_range, bladerf1_get_gain_stage_range),
    FIELD_INIT(.get_gain_stages, bladerf1_get_gain_stages),
    FIELD_INIT(.set_sample_rate, bladerf1_set_sample_rate),
    FIELD_INIT(.set_rational_sample_rate, bladerf1_set_rational_sample_rate),
    FIELD_INIT(.get_sample_rate, bladerf1_get_sample_rate),
    FIELD_INIT(.get_sample_rate_range, bladerf1_get_sample_rate_range),
    FIELD_INIT(.get_rational_sample_rate, bladerf1_get_rational_sample_rate),
    FIELD_INIT(.set_bandwidth, bladerf1_set_bandwidth),
    FIELD_INIT(.get_bandwidth, bladerf1_get_bandwidth),
    FIELD_INIT(.get_bandwidth_range, bladerf1_get_bandwidth_range),
    FIELD_INIT(.get_frequency, bladerf1_get_frequency),
    FIELD_INIT(.get_frequency_range, bladerf1_get_frequency_range),
    FIELD_INIT(.set_frequency, bladerf1_set_frequency),
    FIELD_INIT(.select_band, bladerf1_select_band),
    FIELD_INIT(.set_rf_port, bladerf1_set_rf_port),
    FIELD_INIT(.get_rf_port, bladerf1_get_rf_port),
    FIELD_INIT(.get_rf_ports, bladerf1_get_rf_ports),
    FIELD_INIT(.get_quick_tune, bladerf1_get_quick_tune),
    FIELD_INIT(.schedule_retune, bladerf1_schedule_retune),
    FIELD_INIT(.cancel_scheduled_retunes, bladerf1_cancel_scheduled_retunes),
    FIELD_INIT(.trigger_init, bladerf1_trigger_init),
    FIELD_INIT(.trigger_arm, bladerf1_trigger_arm),
    FIELD_INIT(.trigger_fire, bladerf1_trigger_fire),
    FIELD_INIT(.trigger_state, bladerf1_trigger_state),
    FIELD_INIT(.enable_module, bladerf1_enable_module),
    FIELD_INIT(.init_stream, bladerf1_init_stream),
    FIELD_INIT(.stream, bladerf1_stream),
    FIELD_INIT(.submit_stream_buffer, bladerf1_submit_stream_buffer),
    FIELD_INIT(.deinit_stream, bladerf1_deinit_stream),
    FIELD_INIT(.set_stream_timeout, bladerf1_set_stream_timeout),
    FIELD_INIT(.get_stream_timeout, bladerf1_get_stream_timeout),
    FIELD_INIT(.sync_config, bladerf1_sync_config),
    FIELD_INIT(.sync_tx, bladerf1_sync_tx),
    FIELD_INIT(.sync_rx, bladerf1_sync_rx),
    FIELD_INIT(.get_timestamp, bladerf1_get_timestamp),
    FIELD_INIT(.get_smb_mode, bladerf1_get_smb_mode),
    FIELD_INIT(.set_smb_mode, bladerf1_set_smb_mode),
    FIELD_INIT(.get_smb_frequency, bladerf1_get_smb_frequency),
    FIELD_INIT(.set_smb_frequency, bladerf1_set_smb_frequency),
    FIELD_INIT(.get_rational_smb_frequency, bladerf1_get_rational_smb_frequency),
    FIELD_INIT(.set_rational_smb_frequency, bladerf1_set_rational_smb_frequency),
    FIELD_INIT(.get_correction, bladerf1_get_correction),
    FIELD_INIT(.set_correction, bladerf1_set_correction),
    FIELD_INIT(.load_fpga, bladerf1_load_fpga),
    FIELD_INIT(.flash_fpga, bladerf1_flash_fpga),
    FIELD_INIT(.erase_stored_fpga, bladerf1_erase_stored_fpga),
    FIELD_INIT(.flash_firmware, bladerf1_flash_firmware),
    FIELD_INIT(.device_reset, bladerf1_device_reset),
    FIELD_INIT(.get_sampling, bladerf1_get_sampling),
    FIELD_INIT(.set_sampling, bladerf1_set_sampling),
    FIELD_INIT(.get_rx_mux, bladerf1_get_rx_mux),
    FIELD_INIT(.set_rx_mux, bladerf1_set_rx_mux),
    FIELD_INIT(.set_tuning_mode, bladerf1_set_tuning_mode),
    FIELD_INIT(.expansion_attach, bladerf1_expansion_attach),
    FIELD_INIT(.expansion_get_attached, bladerf1_expansion_get_attached),
    FIELD_INIT(.name, "bladerf1"),
};

/******************************************************************************
 ******************************************************************************
 *                         bladeRF1-specific Functions                        *
 ******************************************************************************
 ******************************************************************************/

/******************************************************************************/
/* TX Gain */
/******************************************************************************/

int bladerf_set_txvga2(struct bladerf *dev, int gain)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    status = lms_txvga2_set_gain(dev, gain);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_get_txvga2(struct bladerf *dev, int *gain)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    status = lms_txvga2_get_gain(dev, gain);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_set_txvga1(struct bladerf *dev, int gain)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    status = lms_txvga1_set_gain(dev, gain);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_get_txvga1(struct bladerf *dev, int *gain)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    status = lms_txvga1_get_gain(dev, gain);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

/******************************************************************************/
/* RX Gain */
/******************************************************************************/

int bladerf_set_lna_gain(struct bladerf *dev, bladerf_lna_gain gain)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    status = lms_lna_set_gain(dev, gain);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_get_lna_gain(struct bladerf *dev, bladerf_lna_gain *gain)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    status = lms_lna_get_gain(dev, gain);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_set_rxvga1(struct bladerf *dev, int gain)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    status = lms_rxvga1_set_gain(dev, gain);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_get_rxvga1(struct bladerf *dev, int *gain)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    status = lms_rxvga1_get_gain(dev, gain);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_set_rxvga2(struct bladerf *dev, int gain)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    status = lms_rxvga2_set_gain(dev, gain);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_get_rxvga2(struct bladerf *dev, int *gain)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    status = lms_rxvga2_get_gain(dev, gain);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

/******************************************************************************/
/* Loopback */
/******************************************************************************/

int bladerf_set_loopback(struct bladerf *dev, bladerf_loopback l)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (l == BLADERF_LB_FIRMWARE) {
        /* Firmware loopback was fully implemented in FW v1.7.1
         * (1.7.0 could enable it, but 1.7.1 also allowed readback,
         * so we'll enforce 1.7.1 here. */
        if (!have_cap(board_data->capabilities, BLADERF_CAP_FW_LOOPBACK)) {
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

            status = dev->backend->set_firmware_loopback(dev, true);
        }

    } else {

        /* If applicable, ensure FW loopback is disabled */
        if (have_cap(board_data->capabilities, BLADERF_CAP_FW_LOOPBACK)) {
            bool fw_lb_enabled = false;

            /* Query first, as the implementation of setting the mode
             * may interrupt running streams. The API don't guarantee that
             * switching loopback modes on the fly to work, but we can at least
             * try to avoid unnecessarily interrupting things...*/
            status = dev->backend->get_firmware_loopback(dev, &fw_lb_enabled);
            if (status != 0) {
                goto out;
            }

            if (fw_lb_enabled) {
                status = dev->backend->set_firmware_loopback(dev, false);
                if (status != 0) {
                    goto out;
                }
            }
        }

        status =  lms_set_loopback_mode(dev, l);
    }

out:
    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_get_loopback(struct bladerf *dev, bladerf_loopback *l)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    *l = BLADERF_LB_NONE;

    if (have_cap(board_data->capabilities, BLADERF_CAP_FW_LOOPBACK)) {
        bool fw_lb_enabled;
        status = dev->backend->get_firmware_loopback(dev, &fw_lb_enabled);
        if (status == 0 && fw_lb_enabled) {
            *l = BLADERF_LB_FIRMWARE;
        }
    }

    if (*l == BLADERF_LB_NONE) {
        status = lms_get_loopback_mode(dev, l);
    }

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

/******************************************************************************/
/* VCTCXO Control */
/******************************************************************************/

int bladerf_get_vctcxo_trim(struct bladerf *dev, uint16_t *trim)
{
    struct bladerf1_board_data *board_data = dev->board_data;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    *trim = board_data->dac_trim;

    MUTEX_UNLOCK(&dev->lock);

    return 0;
}

int bladerf_set_vctcxo_tamer_mode(struct bladerf *dev,
                                  bladerf_vctcxo_tamer_mode mode)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (!have_cap(board_data->capabilities, BLADERF_CAP_VCTCXO_TAMING_MODE)) {
        log_debug("FPGA %s does not support VCTCXO taming via an input source\n",
                  board_data->fpga_version.describe);
        status = BLADERF_ERR_UNSUPPORTED;
        goto exit;
    }

    status = dev->backend->set_vctcxo_tamer_mode(dev, mode);

exit:
    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_get_vctcxo_tamer_mode(struct bladerf *dev,
                                  bladerf_vctcxo_tamer_mode *mode)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (!have_cap(board_data->capabilities, BLADERF_CAP_VCTCXO_TAMING_MODE)) {
        log_debug("FPGA %s does not support VCTCXO taming via an input source\n",
                  board_data->fpga_version.describe);
        status = BLADERF_ERR_UNSUPPORTED;
        goto exit;
    }

    status = dev->backend->get_vctcxo_tamer_mode(dev, mode);

exit:
    MUTEX_UNLOCK(&dev->lock);
    return status;
}

/******************************************************************************/
/* LPF Bypass */
/******************************************************************************/

int bladerf_set_lpf_mode(struct bladerf *dev, bladerf_channel ch,
                         bladerf_lpf_mode mode)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    status = lms_lpf_set_mode(dev, ch, mode);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_get_lpf_mode(struct bladerf *dev, bladerf_channel ch,
                         bladerf_lpf_mode *mode)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    status = lms_lpf_get_mode(dev, ch, mode);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

/******************************************************************************/
/* FX3 Firmware */
/******************************************************************************/

int bladerf_jump_to_bootloader(struct bladerf *dev)
{
    int status;

    if (!dev->backend->jump_to_bootloader) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    MUTEX_LOCK(&dev->lock);

    status = dev->backend->jump_to_bootloader(dev);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

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
    uint8_t *buf;
    size_t buf_len;
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

    status = file_read_buffer(file, &buf, &buf_len);
    if (status != 0) {
        return status;
    }

    status = fx3_fw_parse(&fw, buf, buf_len);
    free(buf);
    if (status != 0) {
        return status;
    }

    assert(fw != NULL);

    status = backend_load_fw_from_bootloader(devinfo.backend, devinfo.usb_bus,
                                             devinfo.usb_addr, fw);

    fx3_fw_free(fw);

    return status;
}

int bladerf_get_fw_log(struct bladerf *dev, const char *filename)
{
    int status;
    FILE *f = NULL;
    logger_entry e;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    if (!have_cap(dev->board->get_capabilities(dev), BLADERF_CAP_READ_FW_LOG_ENTRY)) {
        struct bladerf_version fw_version;

        if (dev->board->get_fw_version(dev, &fw_version) == 0) {
            log_debug("FX3 FW v%s does not support log retrieval.\n",
                      fw_version.describe);
        }

        status = BLADERF_ERR_UNSUPPORTED;
        goto error;
    }

    if (filename != NULL) {
        f = fopen(filename, "w");
        if (f == NULL) {
            switch (errno) {
                case ENOENT:
                    status = BLADERF_ERR_NO_FILE;
                    break;
                case EACCES:
                    status = BLADERF_ERR_PERMISSION;
                    break;
                default:
                    status = BLADERF_ERR_IO;
                    break;
            }
            goto error;
        }
    } else {
        f = stdout;
    }

    do {
        status = dev->backend->read_fw_log(dev, &e);
        if (status != 0) {
            log_debug("Failed to read FW log: %s\n", bladerf_strerror(status));
            goto error;
        }

        if (e == LOG_ERR) {
            fprintf(f, "<Unexpected error>,,\n");
        } else if (e != LOG_EOF) {
            uint8_t file_id;
            uint16_t line;
            uint16_t data;
            const char *src_file;

            logger_entry_unpack(e, &file_id, &line, &data);
            src_file = logger_id_string(file_id);

            fprintf(f, "%s, %u, 0x%04x\n", src_file, line, data);
        }
    } while (e != LOG_EOF && e != LOG_ERR);

error:
    MUTEX_UNLOCK(&dev->lock);

    if (f != NULL && f != stdout) {
        fclose(f);
    }

    return status;
}

/******************************************************************************/
/* DC Calibration */
/******************************************************************************/

/* FIXME make API function */

int bladerf_calibrate_dc(struct bladerf *dev, bladerf_cal_module module)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    status = lms_calibrate_dc(dev, module);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

/******************************************************************************/
/* Low-level VTCXO Trim DAC access */
/******************************************************************************/

int bladerf_dac_write(struct bladerf *dev, uint16_t val)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    status = dac161s055_write(dev, val);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_dac_read(struct bladerf *dev, uint16_t *val)
{
    struct bladerf1_board_data *board_data = dev->board_data;
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    if (!have_cap(board_data->capabilities, BLADERF_CAP_VCTCXO_TRIMDAC_READ)) {
        log_debug("FPGA %s does not support VCTCXO trimdac readback.\n",
                  board_data->fpga_version.describe);
        status = BLADERF_ERR_UNSUPPORTED;
        goto exit;
    }

    status = dac161s055_read(dev, val);

exit:
    MUTEX_UNLOCK(&dev->lock);

    return status;
}

/******************************************************************************/
/* Low-level Si5338 access */
/******************************************************************************/

int bladerf_si5338_read(struct bladerf *dev, uint8_t address, uint8_t *val)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    status = dev->backend->si5338_read(dev,address,val);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_si5338_write(struct bladerf *dev, uint8_t address, uint8_t val)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    status = dev->backend->si5338_write(dev,address,val);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

/******************************************************************************/
/* Low-level LMS access */
/******************************************************************************/

int bladerf_lms_read(struct bladerf *dev, uint8_t address, uint8_t *val)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    status = dev->backend->lms_read(dev,address,val);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_lms_write(struct bladerf *dev, uint8_t address, uint8_t val)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    status = dev->backend->lms_write(dev,address,val);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_lms_set_dc_cals(struct bladerf *dev,
                            const struct bladerf_lms_dc_cals *dc_cals)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    status = lms_set_dc_cals(dev, dc_cals);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_lms_get_dc_cals(struct bladerf *dev,
                            struct bladerf_lms_dc_cals *dc_cals)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    status = lms_get_dc_cals(dev, dc_cals);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

/******************************************************************************/
/* Low-level Flash access */
/******************************************************************************/

int bladerf_erase_flash(struct bladerf *dev,
                        uint32_t erase_block, uint32_t count)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    status = spi_flash_erase(dev, erase_block, count);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_read_flash(struct bladerf *dev, uint8_t *buf,
                       uint32_t page, uint32_t count)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    status = spi_flash_read(dev, buf, page, count);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_write_flash(struct bladerf *dev, const uint8_t *buf,
                        uint32_t page, uint32_t count)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    status = spi_flash_write(dev, buf, page, count);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

/******************************************************************************/
/* Low-level XB SPI access */
/******************************************************************************/

int bladerf_xb_spi_write(struct bladerf *dev, uint32_t val)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    status = dev->backend->xb_spi(dev, val);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

/******************************************************************************/
/* Low-level GPIO access */
/******************************************************************************/

int bladerf_config_gpio_read(struct bladerf *dev, uint32_t *val)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    status = dev->backend->config_gpio_read(dev, val);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_config_gpio_write(struct bladerf *dev, uint32_t val)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    status = dev->backend->config_gpio_write(dev, val);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

/******************************************************************************/
/* Low-level Trigger access */
/******************************************************************************/

int bladerf_read_trigger(struct bladerf *dev,
                         bladerf_channel ch,
                         bladerf_trigger_signal trigger,
                         uint8_t *val)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    status = fpga_trigger_read(dev, ch, trigger, val);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_write_trigger(struct bladerf *dev,
                          bladerf_channel ch,
                          bladerf_trigger_signal trigger,
                          uint8_t val)
{
    int status;

    if (dev->board != &bladerf1_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    status = fpga_trigger_write(dev, ch, trigger, val);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}
