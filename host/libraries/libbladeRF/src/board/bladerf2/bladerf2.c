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
#include "../bladerf1/flash.h"

#include "driver/thirdparty/adi/ad9361_api.h"
#include "driver/spi_flash.h"
#include "driver/fpga_trigger.h"
#include "driver/fx3_fw.h"
#include "driver/ina219.h"

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
    /* Board state */
    enum {
        STATE_UNINITIALIZED,
        STATE_FIRMWARE_LOADED,
        STATE_FPGA_LOADED,
        STATE_INITIALIZED,
    } state;

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

    /* Synchronous interface handles */
    struct bladerf_sync sync[2];
};

#define _CHECK_BOARD_STATE(_state, _locked) \
    do { \
        struct bladerf2_board_data *board_data = dev->board_data; \
        if (board_data->state < _state) { \
            log_error("Board state insufficient for operation " \
                      "(current \"%s\", requires \"%s\").\n", \
                      bladerf2_state_to_string[board_data->state], \
                      bladerf2_state_to_string[_state]); \
            if (_locked) { \
                MUTEX_UNLOCK(&dev->lock); \
            } \
            return BLADERF_ERR_NOT_INIT; \
        } \
    } while(0)

#define CHECK_BOARD_STATE(_state)           _CHECK_BOARD_STATE(_state, false)
#define CHECK_BOARD_STATE_LOCKED(_state)    _CHECK_BOARD_STATE(_state, true)

/******************************************************************************/
/* Constants */
/******************************************************************************/

#define RFFE_CONTROL_RESET_N    0
#define RFFE_CONTROL_ENABLE     1
#define RFFE_CONTROL_TXNRX      2
#define RFFE_CONTROL_EN_AGC     3
#define RFFE_CONTROL_SYNC_IN    4
#define RFFE_CONTROL_RX_BIAS_EN 5
#define RFFE_CONTROL_RX_SW_SHIFT    6
#define RFFE_CONTROL_TX_BIAS_EN     10
#define RFFE_CONTROL_TX_SW_SHIFT    11
#define RFFE_CONTROL_SPDT_MASK      0xF

/* Board state to string map */

static const char *bladerf2_state_to_string[] = {
    [STATE_UNINITIALIZED]   = "Uninitialized",
    [STATE_FIRMWARE_LOADED] = "Firmware Loaded",
    [STATE_FPGA_LOADED]     = "FPGA Loaded",
    [STATE_INITIALIZED]     = "Initialized",
};

/* Overall RX gain range */

static const struct bladerf_range bladerf2_rx_gain_range = {
    FIELD_INIT(.min, 0),
    FIELD_INIT(.max, 77),
    FIELD_INIT(.step, 1),
    FIELD_INIT(.scale, 1),
};

/* Overall TX gain range */

static const struct bladerf_range bladerf2_tx_gain_range = {
    FIELD_INIT(.min, -89750),
    FIELD_INIT(.max, 0),
    FIELD_INIT(.step, 250),
    FIELD_INIT(.scale, 0.001),
};

struct bladerf_gain_stage_info {
    const char *name;
    struct bladerf_range range;
};

/* RX gain stages */

static const struct bladerf_gain_stage_info bladerf2_rx_gain_stages[] = {
    {
        FIELD_INIT(.name, "full"),
        FIELD_INIT(.range, {
            FIELD_INIT(.min, 0),
            FIELD_INIT(.max, 77),
            FIELD_INIT(.step, 1),
            FIELD_INIT(.scale, 1),
        }),
    },
    {
        FIELD_INIT(.name, "digital"),
        FIELD_INIT(.range, {
            FIELD_INIT(.min, 0),
            FIELD_INIT(.max, 31),
            FIELD_INIT(.step, 1),
            FIELD_INIT(.scale, 1),
        }),
    },
};

/* TX gain stages */

static const struct bladerf_gain_stage_info bladerf2_tx_gain_stages[] = {
    {
        FIELD_INIT(.name, "dsa"),
        FIELD_INIT(.range, {
            FIELD_INIT(.min, -89750),
            FIELD_INIT(.max, 0),
            FIELD_INIT(.step, 250),
            FIELD_INIT(.scale, 0.001),
        }),
    },
};

/* Sample Rate Range */

static const struct bladerf_range bladerf2_sample_rate_range = {
    FIELD_INIT(.min, 0),
    FIELD_INIT(.max, 61440000),
    FIELD_INIT(.step, 1),
    FIELD_INIT(.scale, 1),
};

/* Bandwidth Range */

static const struct bladerf_range bladerf2_bandwidth_range = {
    FIELD_INIT(.min, 200e3),
    FIELD_INIT(.max, 56e6),
    FIELD_INIT(.step, 1),
    FIELD_INIT(.scale, 1),
};

/* Frequency Range */

static const struct bladerf_range bladerf2_rx_frequency_range = {
    FIELD_INIT(.min, 70e6),
    FIELD_INIT(.max, 6000e6),
    FIELD_INIT(.step, 2),
    FIELD_INIT(.scale, 1),
};

static const struct bladerf_range bladerf2_tx_frequency_range = {
    FIELD_INIT(.min, 46.875e6),
    FIELD_INIT(.max, 6000e6),
    FIELD_INIT(.step, 2),
    FIELD_INIT(.scale, 1),
};

/* RF Ports */

struct ad9361_port_map {
    const char *name;
    unsigned int id;
};

static const struct ad9361_port_map bladerf2_rx_port_map[] = {
    {"A_BALANCED", A_BALANCED},
    {"B_BALANCED", B_BALANCED},
    {"C_BALANCED", C_BALANCED},
    {"A_N", A_N},
    {"A_P", A_P},
    {"B_N", B_N},
    {"B_P", B_P},
    {"C_N", C_N},
    {"C_P", C_P},
    {"TX_MON1", TX_MON1},
    {"TX_MON2", TX_MON2},
    {"TX_MON1_2", TX_MON1_2},
};

static const struct ad9361_port_map bladerf2_tx_port_map[] = {
    {"TXA", TXA},
    {"TXB", TXB},
};

static int bladerf2_select_band(struct bladerf *dev, bladerf_channel ch, uint64_t frequency);
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

/******************************************************************************/
/* Low-level Initialization */
/******************************************************************************/

extern AD9361_InitParam ad9361_init_params;
extern AD9361_RXFIRConfig ad9361_init_rx_fir_config;
extern AD9361_TXFIRConfig ad9361_init_tx_fir_config;
extern const float ina219_r_shunt;

static int bladerf2_initialize(struct bladerf *dev)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    struct bladerf_version required_fw_version;
    struct bladerf_version required_fpga_version;
    int status;

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

    /* Set FPGA packet protocol */
    status = dev->backend->set_fpga_protocol(dev, BACKEND_FPGA_PROTOCOL_NIOSII);
    if (status < 0) {
        log_error("Unable to set backend FPGA protocol: %d\n", status);
        return status;
    }

    /* Initialize RFFE control */
    status = dev->backend->rffe_control_write(dev, (1 << RFFE_CONTROL_ENABLE) |
                                                   (1 << RFFE_CONTROL_TXNRX));
    if (status < 0) {
        return status;
    }

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

    status = ina219_init(dev, ina219_r_shunt);
    if (status < 0) {
        return status;
    }

    status = bladerf2_select_band(dev, BLADERF_TX, board_data->phy->pdata->tx_synth_freq);
    if (status < 0) {
        return status;
    }

    status = bladerf2_select_band(dev, BLADERF_RX, board_data->phy->pdata->rx_synth_freq);
    if (status < 0) {
        return status;
    }

    /* Update device state */
    board_data->state = STATE_INITIALIZED;

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
    uint16_t vid, pid;
    int status;

    status = dev->backend->get_vid_pid(dev, &vid, &pid);
    if (status < 0) {
        return false;
    }

    if (vid == USB_NUAND_VENDOR_ID && pid == USB_NUAND_BLADERF2_PRODUCT_ID) {
        return true;
    }

    return false;
}

/******************************************************************************/
/* Open/close */
/******************************************************************************/

static int bladerf2_open(struct bladerf *dev, struct bladerf_devinfo *devinfo)
{
    struct bladerf2_board_data *board_data;
    struct bladerf_version required_fw_version;
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

    /* Update device state */
    board_data->state = STATE_FIRMWARE_LOADED;

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

    /* Check if FPGA is configured */
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

            board_data->state = STATE_FPGA_LOADED;
        } else {
            log_warning("FPGA bitstream file not found.\n");
            log_warning("Skipping further initialization...\n");
            return 0;
        }
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

    if (board_data && board_data->phy) {
        ad9361_deinit(board_data->phy);
        board_data->phy = NULL;
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

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

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

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    *size = board_data->fpga_size;

    return 0;
}

static int bladerf2_is_fpga_configured(struct bladerf *dev)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

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

    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    memcpy(version, &board_data->fpga_version, sizeof(*version));

    return 0;
}

static int bladerf2_get_fw_version(struct bladerf *dev, struct bladerf_version *version)
{
    struct bladerf2_board_data *board_data = dev->board_data;

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    memcpy(version, &board_data->fw_version, sizeof(*version));

    return 0;
}

/******************************************************************************/
/* Enable/disable */
/******************************************************************************/

static int bladerf2_enable_module(struct bladerf *dev, bladerf_direction dir, bool enable)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    int status;
    bool tx;
    uint32_t reg;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    /* Stop synchronous interface */
    if (!enable) {
        sync_deinit(&board_data->sync[dir]);
    }

    /* Read RFFE control register */
    status = dev->backend->rffe_control_read(dev, &reg);
    if (status < 0) {
        return status;
    }

    tx = (dir == BLADERF_TX);

    /* Modify */
    if (enable && tx) {
        /* Enable TX */
        reg |= (1 << RFFE_CONTROL_TXNRX);
    } else if (enable && !tx) {
        /* Enable RX */
        reg |= (1 << RFFE_CONTROL_ENABLE);
    } else if (!enable && tx) {
        /* Disable TX */
        reg &= ~(1 << RFFE_CONTROL_TXNRX);
    } else if (!enable && !tx) {
        /* Disable RX */
        reg &= ~(1 << RFFE_CONTROL_ENABLE);
    }

    /* Write RFFE control register */
    status = dev->backend->rffe_control_write(dev, reg);
    if (status < 0) {
        return status;
    }

    /* Enable module through backend */
    status = dev->backend->enable_module(dev, dir, enable);
    if (status < 0) {
        return status;
    }

    return 0;
}

/******************************************************************************/
/* Gain */
/******************************************************************************/

static int bladerf2_set_gain(struct bladerf *dev, bladerf_channel ch, int gain)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (ch & BLADERF_TX) {
        status = ad9361_set_tx_attenuation(board_data->phy, ch >> 1, -gain);
    } else {
        status = ad9361_set_rx_rf_gain(board_data->phy, ch >> 1, gain);
    }

    if (status < 0) {
        return errno_ad9361_to_bladerf(status);
    }

    return 0;
}

static int bladerf2_get_gain(struct bladerf *dev, bladerf_channel ch, int *gain)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    int status;
    uint32_t atten;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (ch & BLADERF_TX) {
        status = ad9361_get_tx_attenuation(board_data->phy, ch >> 1, &atten);
        if (status == 0) {
            *gain = -atten;
        }
    } else {
        status = ad9361_get_rx_rf_gain(board_data->phy, ch >> 1, gain);
    }

    if (status < 0) {
        return errno_ad9361_to_bladerf(status);
    }

    return 0;
}

static int bladerf2_get_gain_range(struct bladerf *dev, bladerf_channel ch, struct bladerf_range *range)
{
    if (ch & BLADERF_TX) {
        *range = bladerf2_tx_gain_range;
    } else {
        /* TODO look up rx_gain_info for current LO */
        *range = bladerf2_rx_gain_range;
    }

    return 0;
}

static int bladerf2_set_gain_stage(struct bladerf *dev, bladerf_channel ch, const char *stage, int gain)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (ch & BLADERF_TX) {
        if (strcmp(stage, "dsa") == 0) {
            return dev->board->set_gain(dev, ch, gain);
        }
    } else {
        if (strcmp(stage, "full") == 0) {
            return dev->board->set_gain(dev, ch, gain);
        } else if (strcmp(stage, "digital") == 0) {
            return BLADERF_ERR_UNSUPPORTED;
        }
    }

    return BLADERF_ERR_INVAL;
}

static int bladerf2_get_gain_stage(struct bladerf *dev, bladerf_channel ch, const char *stage, int *gain)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (ch & BLADERF_TX) {
        if (strcmp(stage, "dsa") == 0) {
            return dev->board->get_gain(dev, ch, gain);
        } else {
            return BLADERF_ERR_INVAL;
        }
    } else {
        struct rf_rx_gain rx_gain;

        status = ad9361_get_rx_gain(board_data->phy, (ch >> 1) + 1, &rx_gain);
        if (status < 0) {
            return errno_ad9361_to_bladerf(status);
        }

        if (strcmp(stage, "full") == 0) {
            *gain = rx_gain.gain_db;
        } else if(strcmp(stage, "digital") == 0) {
            *gain = rx_gain.digital_gain;
        } else {
            return BLADERF_ERR_INVAL;
        }
    }

    return 0;
}

static int bladerf2_get_gain_stage_range(struct bladerf *dev, bladerf_channel ch, const char *stage, struct bladerf_range *range)
{
    const struct bladerf_gain_stage_info *stage_infos;
    unsigned int stage_infos_len;
    unsigned int i;

    if (ch & BLADERF_TX) {
        stage_infos = bladerf2_tx_gain_stages;
        stage_infos_len = ARRAY_SIZE(bladerf2_tx_gain_stages);
    } else {
        stage_infos = bladerf2_rx_gain_stages;
        stage_infos_len = ARRAY_SIZE(bladerf2_rx_gain_stages);
    }

    for (i = 0; i < stage_infos_len; i++) {
        if (strcmp(stage_infos[i].name, stage) == 0) {
            *range = stage_infos[i].range;
            return 0;
        }
    }

    return BLADERF_ERR_INVAL;
}

static int bladerf2_get_gain_stages(struct bladerf *dev, bladerf_channel ch, const char **stages, unsigned int count)
{
    const struct bladerf_gain_stage_info *stage_infos;
    unsigned int stage_infos_len;
    unsigned int i;

    if (ch & BLADERF_TX) {
        stage_infos = bladerf2_tx_gain_stages;
        stage_infos_len = ARRAY_SIZE(bladerf2_tx_gain_stages);
    } else {
        stage_infos = bladerf2_rx_gain_stages;
        stage_infos_len = ARRAY_SIZE(bladerf2_rx_gain_stages);
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

static int bladerf2_set_sample_rate(struct bladerf *dev, bladerf_channel ch, unsigned int rate, unsigned int *actual)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (ch & BLADERF_TX) {
        status = ad9361_set_tx_sampling_freq(board_data->phy, rate);
        if (status < 0) {
            return errno_ad9361_to_bladerf(status);
        }

        if (actual) {
            status = ad9361_get_tx_sampling_freq(board_data->phy, actual);
            if (status < 0) {
                return errno_ad9361_to_bladerf(status);
            }
        }
    } else {
        status = ad9361_set_rx_sampling_freq(board_data->phy, rate);
        if (status < 0) {
            return errno_ad9361_to_bladerf(status);
        }

        if (actual) {
            status = ad9361_get_rx_sampling_freq(board_data->phy, actual);
            if (status < 0) {
                return errno_ad9361_to_bladerf(status);
            }
        }
    }

    return 0;
}

static int bladerf2_get_sample_rate(struct bladerf *dev, bladerf_channel ch, unsigned int *rate)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

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

static int bladerf2_get_sample_rate_range(struct bladerf *dev, bladerf_channel ch, struct bladerf_range *range)
{
    *range = bladerf2_sample_rate_range;
    return 0;
}

static int bladerf2_set_rational_sample_rate(struct bladerf *dev, bladerf_channel ch, struct bladerf_rational_rate *rate, struct bladerf_rational_rate *actual)
{
    int status;
    unsigned int integer_rate, actual_integer_rate;

    integer_rate = rate->integer + rate->num / rate->den;

    status = bladerf2_set_sample_rate(dev, ch, integer_rate, &actual_integer_rate);

    if (status < 0) {
        return status;
    }

    actual->integer = actual_integer_rate;
    actual->num = 0;
    actual->den = 1;

    return 0;
}

static int bladerf2_get_rational_sample_rate(struct bladerf *dev, bladerf_channel ch, struct bladerf_rational_rate *rate)
{
    int status;
    unsigned int integer_rate;

    status = bladerf2_get_sample_rate(dev, ch, &integer_rate);

    if (status < 0)
        return status;

    if (rate) {
        rate->integer = integer_rate;
        rate->num = 0;
        rate->den = 1;
    }

    return 0;

}

/******************************************************************************/
/* Bandwidth */
/******************************************************************************/

static int bladerf2_set_bandwidth(struct bladerf *dev, bladerf_channel ch, unsigned int bandwidth, unsigned int *actual)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (ch & BLADERF_TX) {
        status = ad9361_set_tx_rf_bandwidth(board_data->phy, bandwidth);
        if (status < 0) {
            return errno_ad9361_to_bladerf(status);
        }

        if (actual) {
            status = ad9361_get_tx_rf_bandwidth(board_data->phy, actual);
            if (status < 0) {
                return errno_ad9361_to_bladerf(status);
            }
        }
    } else {
        status = ad9361_set_rx_rf_bandwidth(board_data->phy, bandwidth);
        if (status < 0) {
            return errno_ad9361_to_bladerf(status);
        }

        if (actual) {
            status = ad9361_get_rx_rf_bandwidth(board_data->phy, actual);
            if (status < 0) {
                return errno_ad9361_to_bladerf(status);
            }
        }
    }

    return 0;
}

static int bladerf2_get_bandwidth(struct bladerf *dev, bladerf_channel ch, unsigned int *bandwidth)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

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

static int bladerf2_get_bandwidth_range(struct bladerf *dev, bladerf_channel ch, struct bladerf_range *range)
{
    *range = bladerf2_bandwidth_range;
    return 0;
}

/******************************************************************************/
/* Frequency */
/******************************************************************************/

static int bladerf2_select_band(struct bladerf *dev, bladerf_channel ch, uint64_t frequency)
{
    const uint64_t mid_band_point = 3000000000U;
    struct bladerf2_board_data *board_data = dev->board_data;
    int status;
    bool low_band;
    uint32_t reg;

    status = dev->backend->rffe_control_read(dev, &reg);
    if (status < 0) {
        return status;
    }

    low_band = frequency < mid_band_point;

    if (ch & BLADERF_TX) {
        reg &= ~(RFFE_CONTROL_SPDT_MASK << RFFE_CONTROL_TX_SW_SHIFT);
        if (low_band)
            reg |= 0x5 << RFFE_CONTROL_TX_SW_SHIFT;
        else
            reg |= 0xA << RFFE_CONTROL_TX_SW_SHIFT;
        status = ad9361_set_tx_rf_port_output(board_data->phy, low_band ? 0 : 1);
    } else {
        reg &= ~(RFFE_CONTROL_SPDT_MASK << RFFE_CONTROL_RX_SW_SHIFT);
        if (low_band)
            reg |= 0x5 << RFFE_CONTROL_RX_SW_SHIFT;
        else
            reg |= 0xA << RFFE_CONTROL_RX_SW_SHIFT;
        status = ad9361_set_rx_rf_port_input(board_data->phy, low_band ? 0 : 1);
    }

    if (status < 0) {
        return errno_ad9361_to_bladerf(status);
    }

    return dev->backend->rffe_control_write(dev, reg);
}

static int bladerf2_set_frequency(struct bladerf *dev, bladerf_channel ch, uint64_t frequency)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (ch & BLADERF_TX) {
        status = ad9361_set_tx_lo_freq(board_data->phy, frequency);
    } else {
        status = ad9361_set_rx_lo_freq(board_data->phy, frequency);
    }

    if (status < 0) {
        return errno_ad9361_to_bladerf(status);
    }

    return bladerf2_select_band(dev, ch, frequency);
}

static int bladerf2_get_frequency(struct bladerf *dev, bladerf_channel ch, uint64_t *frequency)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    int status;
    uint64_t lo_frequency;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

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

static int bladerf2_get_frequency_range(struct bladerf *dev, bladerf_channel ch, struct bladerf_range *range)
{
    if (ch & BLADERF_TX) {
        *range = bladerf2_tx_frequency_range;
    } else {
        *range = bladerf2_rx_frequency_range;
    }

    return 0;
}

/******************************************************************************/
/* RF ports */
/******************************************************************************/

static int bladerf2_set_rf_port(struct bladerf *dev, bladerf_channel ch, const char *port)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    const struct ad9361_port_map *port_map;
    unsigned int port_map_len;
    unsigned int i;
    uint32_t port_id;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (ch & BLADERF_TX) {
        port_map = bladerf2_tx_port_map;
        port_map_len = ARRAY_SIZE(bladerf2_tx_port_map);
    } else {
        port_map = bladerf2_rx_port_map;
        port_map_len = ARRAY_SIZE(bladerf2_rx_port_map);
    }

    for (i = 0; i < port_map_len; i++) {
        if (strcmp(port_map[i].name, port) == 0) {
            port_id = port_map[i].id;
            break;
        }
    }

    if (i == port_map_len) {
        return BLADERF_ERR_INVAL;
    }

    if (ch & BLADERF_TX) {
        status = ad9361_set_tx_rf_port_output(board_data->phy, port_id);
    } else {
        status = ad9361_set_rx_rf_port_input(board_data->phy, port_id);
    }

    if (status < 0) {
        return errno_ad9361_to_bladerf(status);
    }

    return 0;
}

static int bladerf2_get_rf_port(struct bladerf *dev, bladerf_channel ch, const char **port)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    const struct ad9361_port_map *port_map;
    unsigned int port_map_len;
    unsigned int i;
    uint32_t port_id;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (ch & BLADERF_TX) {
        port_map = bladerf2_tx_port_map;
        port_map_len = ARRAY_SIZE(bladerf2_tx_port_map);
        status = ad9361_get_tx_rf_port_output(board_data->phy, &port_id);
    } else {
        port_map = bladerf2_rx_port_map;
        port_map_len = ARRAY_SIZE(bladerf2_rx_port_map);
        status = ad9361_get_rx_rf_port_input(board_data->phy, &port_id);
    }

    if (status < 0) {
        return errno_ad9361_to_bladerf(status);
    }

    for (i = 0; i < port_map_len; i++) {
        if (port_id == port_map[i].id) {
            *port = port_map[i].name;
            break;
        }
    }

    if (i == port_map_len) {
        *port = "unknown";
        return BLADERF_ERR_UNEXPECTED;
    }

    return 0;
}

static int bladerf2_get_rf_ports(struct bladerf *dev, bladerf_channel ch, const char **ports, unsigned int count)
{
    const struct ad9361_port_map *port_map;
    unsigned int port_map_len;
    unsigned int i;

    if (ch & BLADERF_TX) {
        port_map = bladerf2_tx_port_map;
        port_map_len = ARRAY_SIZE(bladerf2_tx_port_map);
    } else {
        port_map = bladerf2_rx_port_map;
        port_map_len = ARRAY_SIZE(bladerf2_rx_port_map);
    }

    if (ports != NULL) {
        count = (port_map_len < count) ? port_map_len : count;

        for (i = 0; i < count; i++) {
            ports[i] = port_map[i].name;
        }
    }

    return port_map_len;
}

/******************************************************************************/
/* Scheduled Tuning */
/******************************************************************************/

static int bladerf2_get_quick_tune(struct bladerf *dev, bladerf_channel ch, struct bladerf_quick_tune *quick_tune)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_schedule_retune(struct bladerf *dev, bladerf_channel ch, uint64_t timestamp, uint64_t frequency, struct bladerf_quick_tune *quick_tune)

{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_cancel_scheduled_retunes(struct bladerf *dev, bladerf_channel ch)
{
    return BLADERF_ERR_UNSUPPORTED;
}

/******************************************************************************/
/* DC/Phase/Gain Correction */
/******************************************************************************/

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
            FIELD_INIT(.reg, {REG_RX1_INPUT_A_PHASE_CORR, REG_RX1_INPUT_BC_PHASE_CORR}),
            FIELD_INIT(.shift, 6),
        },
        [BLADERF_CORR_GAIN] = {
            FIELD_INIT(.reg, {REG_RX1_INPUT_A_GAIN_CORR, REG_RX1_INPUT_BC_PHASE_CORR}),
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
            FIELD_INIT(.reg, {REG_RX2_INPUT_A_PHASE_CORR, REG_RX2_INPUT_BC_PHASE_CORR}),
            FIELD_INIT(.shift, 6),
        },
        [BLADERF_CORR_GAIN] = {
            FIELD_INIT(.reg, {REG_RX2_INPUT_A_GAIN_CORR, REG_RX2_INPUT_BC_PHASE_CORR}),
            FIELD_INIT(.shift, 6),
        }
    },
    [BLADERF_CHANNEL_TX(0)].corr = {
        [BLADERF_CORR_DCOFF_I] = {
            FIELD_INIT(.reg, {REG_TX1_OUT_1_OFFSET_I, REG_TX1_OUT_2_OFFSET_I}),
            FIELD_INIT(.shift, 5),
        },
        [BLADERF_CORR_DCOFF_Q] = {
            FIELD_INIT(.reg, {REG_TX1_OUT_1_OFFSET_Q, REG_TX1_OUT_2_OFFSET_Q}),
            FIELD_INIT(.shift, 5),
        },
        [BLADERF_CORR_PHASE] = {
            FIELD_INIT(.reg, {REG_TX1_OUT_1_PHASE_CORR, REG_TX1_OUT_2_PHASE_CORR}),
            FIELD_INIT(.shift, 6),
        },
        [BLADERF_CORR_GAIN] = {
            FIELD_INIT(.reg, {REG_TX1_OUT_1_GAIN_CORR, REG_TX1_OUT_2_GAIN_CORR}),
            FIELD_INIT(.shift, 6),
        }
    },
    [BLADERF_CHANNEL_TX(1)].corr = {
        [BLADERF_CORR_DCOFF_I] = {
            FIELD_INIT(.reg, {REG_TX2_OUT_1_OFFSET_I, REG_TX2_OUT_2_OFFSET_I}),
            FIELD_INIT(.shift, 5),
        },
        [BLADERF_CORR_DCOFF_Q] = {
            FIELD_INIT(.reg, {REG_TX2_OUT_1_OFFSET_Q, REG_TX2_OUT_2_OFFSET_Q}),
            FIELD_INIT(.shift, 5),
        },
        [BLADERF_CORR_PHASE] = {
            FIELD_INIT(.reg, {REG_TX2_OUT_1_PHASE_CORR, REG_TX2_OUT_2_PHASE_CORR}),
            FIELD_INIT(.shift, 6),
        },
        [BLADERF_CORR_GAIN] = {
            FIELD_INIT(.reg, {REG_TX2_OUT_1_GAIN_CORR, REG_TX2_OUT_2_GAIN_CORR}),
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
            {REG_INPUT_A_OFFSETS_1, REG_RX1_INPUT_A_OFFSETS},
            /* Q */
            {REG_RX1_INPUT_A_OFFSETS, REG_RX1_INPUT_A_Q_OFFSET},
        },
        /* B/C band */
        {
            /* I */
            {REG_INPUT_BC_OFFSETS_1, REG_RX1_INPUT_BC_OFFSETS},
            /* Q */
            {REG_RX1_INPUT_BC_OFFSETS, REG_RX1_INPUT_BC_Q_OFFSET},
        },
    },
    /* Channel 2 */
    [BLADERF_CHANNEL_RX(1)] = {
        /* A band */
        {
            /* I */
            {REG_RX2_INPUT_A_I_OFFSET, REG_RX2_INPUT_A_OFFSETS},
            /* Q */
            {REG_RX2_INPUT_A_OFFSETS, REG_INPUT_A_OFFSETS_1},
        },
        /* B/C band */
        {
            /* I */
            {REG_RX2_INPUT_BC_I_OFFSET, REG_RX2_INPUT_BC_OFFSETS},
            /* Q */
            {REG_RX2_INPUT_BC_OFFSETS, REG_INPUT_BC_OFFSETS_1},
        },
    },
};

static const int ad9361_correction_force_bit[2][4][2] = {
    [0] = {
        [BLADERF_CORR_DCOFF_I] = {2, 6},
        [BLADERF_CORR_DCOFF_Q] = {2, 6},
        [BLADERF_CORR_PHASE] = {0, 4},
        [BLADERF_CORR_GAIN] = {0, 4},
    },
    [1] = {
        [BLADERF_CORR_DCOFF_I] = {3, 7},
        [BLADERF_CORR_DCOFF_Q] = {3, 7},
        [BLADERF_CORR_PHASE] = {1, 5},
        [BLADERF_CORR_GAIN] = {1, 5},
    },
};

static int bladerf2_get_correction(struct bladerf *dev, bladerf_channel ch,
                                   bladerf_correction corr, int16_t *value)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    int status;
    bool low_band;
    uint16_t reg, data;
    unsigned int shift;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    /* Validate channel */
    if (ch != BLADERF_CHANNEL_RX(0) && ch != BLADERF_CHANNEL_RX(1) &&
            ch != BLADERF_CHANNEL_TX(0) && ch != BLADERF_CHANNEL_TX(1)) {
        return BLADERF_ERR_INVAL;
    }

    /* Validate correction */
    if (corr != BLADERF_CORR_DCOFF_I && corr != BLADERF_CORR_DCOFF_Q &&
            corr != BLADERF_CORR_PHASE && corr != BLADERF_CORR_GAIN) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    /* Look up band */
    if (ch & BLADERF_TX) {
        uint32_t mode;

        status = ad9361_get_tx_rf_port_output(board_data->phy, &mode);
        if (status < 0) {
            return errno_ad9361_to_bladerf(status);
        }

        low_band = (mode == TXA);
    } else {
        uint32_t mode;

        status = ad9361_get_rx_rf_port_input(board_data->phy, &mode);
        if (status < 0) {
            return errno_ad9361_to_bladerf(status);
        }

        /* Check if RX RF port mode is supported */
        if (mode != A_BALANCED && mode != B_BALANCED && mode != C_BALANCED) {
            return BLADERF_ERR_UNSUPPORTED;
        }

        low_band = (mode == A_BALANCED);
    }

    if ((corr == BLADERF_CORR_DCOFF_I || corr == BLADERF_CORR_DCOFF_Q) &&
            (ch & BLADERF_DIRECTION_MASK) == BLADERF_RX) {
        /* RX DC offset corrections are stuffed in a super convoluted way in
         * the register map. See AD9361 register map page 51. */
        bool is_q = (corr == BLADERF_CORR_DCOFF_Q);
        uint8_t data_top, data_bot;
        uint16_t data;

        /* Read top register */
        status = ad9361_spi_read(board_data->phy->spi, ad9361_correction_rx_dcoff_reg_table[ch][low_band][is_q].reg_top);
        if (status < 0) {
            return errno_ad9361_to_bladerf(status);
        }
        data_top = status;

        /* Read bottom register */
        status = ad9361_spi_read(board_data->phy->spi, ad9361_correction_rx_dcoff_reg_table[ch][low_band][is_q].reg_bot);
        if (status < 0) {
            return errno_ad9361_to_bladerf(status);
        }
        data_bot = status;

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
        reg = ad9361_correction_reg_table[ch].corr[corr].reg[low_band];
        shift = ad9361_correction_reg_table[ch].corr[corr].shift;

        /* Read register and scale value */
        status = ad9361_spi_read(board_data->phy->spi, reg);
        if (status < 0) {
            return errno_ad9361_to_bladerf(status);
        }

        /* Scale 8-bit to 12-bit/13-bit */
        data = status << shift;

        /* Sign extend value */
        if (shift == 5) {
            *value = data | ((data & (1 << 12)) ? 0xf000 : 0x0000);
        } else {
            *value = data | ((data & (1 << 13)) ? 0xc000 : 0x0000);
        }
    }

    return 0;
}

static int bladerf2_set_correction(struct bladerf *dev, bladerf_channel ch,
                                   bladerf_correction corr, int16_t value)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    int status;
    bool low_band;
    uint16_t reg, data;
    unsigned int shift;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    /* Validate channel */
    if (ch != BLADERF_CHANNEL_RX(0) && ch != BLADERF_CHANNEL_RX(1) &&
            ch != BLADERF_CHANNEL_TX(0) && ch != BLADERF_CHANNEL_TX(1)) {
        return BLADERF_ERR_INVAL;
    }

    /* Validate correction */
    if (corr != BLADERF_CORR_DCOFF_I && corr != BLADERF_CORR_DCOFF_Q &&
            corr != BLADERF_CORR_PHASE && corr != BLADERF_CORR_GAIN) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    /* Look up band */
    if (ch & BLADERF_TX) {
        uint32_t mode;

        status = ad9361_get_tx_rf_port_output(board_data->phy, &mode);
        if (status < 0) {
            return errno_ad9361_to_bladerf(status);
        }

        low_band = (mode == TXA);
    } else {
        uint32_t mode;

        status = ad9361_get_rx_rf_port_input(board_data->phy, &mode);
        if (status < 0) {
            return errno_ad9361_to_bladerf(status);
        }

        /* Check if RX RF port mode is supported */
        if (mode != A_BALANCED && mode != B_BALANCED && mode != C_BALANCED) {
            return BLADERF_ERR_UNSUPPORTED;
        }

        low_band = (mode == A_BALANCED);
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
        status = ad9361_spi_read(board_data->phy->spi, ad9361_correction_rx_dcoff_reg_table[ch][low_band][is_q].reg_top);
        if (status < 0) {
            return errno_ad9361_to_bladerf(status);
        }
        data_top = status;

        /* Read bottom register */
        status = ad9361_spi_read(board_data->phy->spi, ad9361_correction_rx_dcoff_reg_table[ch][low_band][is_q].reg_bot);
        if (status < 0) {
            return errno_ad9361_to_bladerf(status);
        }
        data_bot = status;

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
        status = ad9361_spi_write(board_data->phy->spi, ad9361_correction_rx_dcoff_reg_table[ch][low_band][is_q].reg_top, data_top);
        if (status < 0) {
            return errno_ad9361_to_bladerf(status);
        }

        /* Write bottom register */
        status = ad9361_spi_write(board_data->phy->spi, ad9361_correction_rx_dcoff_reg_table[ch][low_band][is_q].reg_bot, data_bot);
        if (status < 0) {
            return errno_ad9361_to_bladerf(status);
        }
    } else {
        /* Look up correction register and value shift in table */
        reg = ad9361_correction_reg_table[ch].corr[corr].reg[low_band];
        shift = ad9361_correction_reg_table[ch].corr[corr].shift;

        /* Scale 12-bit/13-bit to 8-bit */
        data = (value >> shift) & 0xff;

        /* Write register */
        status = ad9361_spi_write(board_data->phy->spi, reg, data & 0xff);
        if (status < 0) {
            return errno_ad9361_to_bladerf(status);
        }
    }

    reg = (ch & BLADERF_TX) ? REG_TX_FORCE_BITS : REG_FORCE_BITS;

    /* Read force bit register */
    status = ad9361_spi_read(board_data->phy->spi, reg);
    if (status < 0) {
        return errno_ad9361_to_bladerf(status);
    }

    /* Modify register */
    data = status | (1 << ad9361_correction_force_bit[ch >> 1][corr][low_band]);

    /* Write force bit register */
    status = ad9361_spi_write(board_data->phy->spi, reg, data);
    if (status < 0) {
        return errno_ad9361_to_bladerf(status);
    }

    return 0;
}

/******************************************************************************/
/* Trigger */
/******************************************************************************/

static int bladerf2_trigger_init(struct bladerf *dev, bladerf_channel ch, bladerf_trigger_signal signal, struct bladerf_trigger *trigger)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return fpga_trigger_init(dev, ch, signal, trigger);
}

static int bladerf2_trigger_arm(struct bladerf *dev, const struct bladerf_trigger *trigger, bool arm, uint64_t resv1, uint64_t resv2)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return fpga_trigger_fire(dev, trigger);
}

static int bladerf2_trigger_fire(struct bladerf *dev, const struct bladerf_trigger *trigger)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return fpga_trigger_fire(dev, trigger);
}

static int bladerf2_trigger_state(struct bladerf *dev, const struct bladerf_trigger *trigger, bool *is_armed, bool *has_fired, bool *fire_requested, uint64_t *resv1, uint64_t *resv2)
{
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

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

static int bladerf2_init_stream(struct bladerf_stream **stream, struct bladerf *dev, bladerf_stream_cb callback, void ***buffers, size_t num_buffers, bladerf_format format, size_t samples_per_buffer, size_t num_transfers, void *user_data)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return async_init_stream(stream, dev, callback, buffers, num_buffers,
                             format, samples_per_buffer, num_transfers, user_data);
}

static int bladerf2_stream(struct bladerf_stream *stream, bladerf_channel_layout layout)
{
    return async_run_stream(stream, layout & BLADERF_DIRECTION_MASK);
}

static int bladerf2_submit_stream_buffer(struct bladerf_stream *stream, void *buffer, unsigned int timeout_ms, bool nonblock)
{
    return async_submit_stream_buffer(stream, buffer, timeout_ms, nonblock);
}

static void bladerf2_deinit_stream(struct bladerf_stream *stream)
{
    async_deinit_stream(stream);
}

static int bladerf2_set_stream_timeout(struct bladerf *dev, bladerf_direction dir, unsigned int timeout)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_get_stream_timeout(struct bladerf *dev, bladerf_direction dir, unsigned int *timeout)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_sync_config(struct bladerf *dev, bladerf_channel_layout layout, bladerf_format format, unsigned int num_buffers, unsigned int buffer_size, unsigned int num_transfers, unsigned int stream_timeout)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    bladerf_direction dir = layout & BLADERF_DIRECTION_MASK;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    status = sync_init(&board_data->sync[dir], dev, layout,
                       format, num_buffers, buffer_size,
                       board_data->msg_size, num_transfers,
                       stream_timeout);

    return status;
}

static int bladerf2_sync_tx(struct bladerf *dev, void *samples, unsigned int num_samples, struct bladerf_metadata *metadata, unsigned int timeout_ms)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    int status;

    if (!board_data->sync[BLADERF_TX].initialized) {
        return BLADERF_ERR_INVAL;
    }

    status = sync_tx(&board_data->sync[BLADERF_TX], samples, num_samples, metadata, timeout_ms);

    return status;
}

static int bladerf2_sync_rx(struct bladerf *dev, void *samples, unsigned int num_samples, struct bladerf_metadata *metadata, unsigned int timeout_ms)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    int status;

    if (!board_data->sync[BLADERF_RX].initialized) {
        return BLADERF_ERR_INVAL;
    }

    status = sync_rx(&board_data->sync[BLADERF_RX], samples, num_samples, metadata, timeout_ms);

    return status;
}

static int bladerf2_get_timestamp(struct bladerf *dev, bladerf_direction dir, uint64_t *value)
{
    CHECK_BOARD_STATE(STATE_INITIALIZED);

    return dev->backend->get_timestamp(dev, dir, value);
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

    status = bladerf2_initialize(dev);
    if (status != 0) {
        return status;
    }

    return 0;
}

static int bladerf2_flash_fpga(struct bladerf *dev, const uint8_t *buf, size_t length)
{
    struct bladerf2_board_data *board_data = dev->board_data;

    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    if (!is_valid_fpga_size(board_data->fpga_size, length)) {
        return BLADERF_ERR_INVAL;
    }

    return spi_flash_write_fpga_bitstream(dev, buf, length);
}

static int bladerf2_erase_stored_fpga(struct bladerf *dev)
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

static int bladerf2_flash_firmware(struct bladerf *dev, const uint8_t *buf, size_t length)
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

static int bladerf2_device_reset(struct bladerf *dev)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    return dev->backend->device_reset(dev);
}

/******************************************************************************/
/* Tuning mode */
/******************************************************************************/

static int bladerf2_set_tuning_mode(struct bladerf *dev, bladerf_tuning_mode mode)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int bladerf2_get_tuning_mode(struct bladerf *dev, bladerf_tuning_mode *mode)
{
    return BLADERF_ERR_UNSUPPORTED;
}

/******************************************************************************/
/* Loopback */
/******************************************************************************/

static int bladerf2_set_loopback(struct bladerf *dev, bladerf_loopback l)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    if (l == BLADERF_LB_NONE) {
        /* Disable digital loopback */
        status = ad9361_bist_loopback(board_data->phy, 0);
        if (status < 0) {
            return errno_ad9361_to_bladerf(status);
        }

        /* Disable firmware loopback */
        status = dev->backend->set_firmware_loopback(dev, false);
        if (status < 0) {
            return status;
        }
    } else if (l == BLADERF_LB_FIRMWARE) {
        /* Disable digital loopback */
        status = ad9361_bist_loopback(board_data->phy, 0);
        if (status < 0) {
            return errno_ad9361_to_bladerf(status);
        }

        /* Enable firmware loopback */
        status = dev->backend->set_firmware_loopback(dev, true);
        if (status < 0) {
            return status;
        }
    } else {
        return BLADERF_ERR_UNSUPPORTED;
    }

    return 0;
}

static int bladerf2_get_loopback(struct bladerf *dev, bladerf_loopback *l)
{
    struct bladerf2_board_data *board_data = dev->board_data;
    int status;
    bool fw_loopback;
    int32_t ad9361_loopback;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    /* Read firwmare loopback */
    status = dev->backend->get_firmware_loopback(dev, &fw_loopback);
    if (status < 0) {
        return status;
    }

    if (fw_loopback) {
        *l = BLADERF_LB_FIRMWARE;
        return 0;
    }

    /* Read AD9361 bist loopback */
    ad9361_get_bist_loopback(board_data->phy, &ad9361_loopback);

    if (ad9361_loopback == 1) {
        /* FIXME digital loopback value */
    }

    *l = BLADERF_LB_NONE;

    return 0;
}

/******************************************************************************/
/* Sample RX FPGA Mux */
/******************************************************************************/

static int bladerf2_set_rx_mux(struct bladerf *dev, bladerf_rx_mux mode)
{
    uint32_t rx_mux_val;
    uint32_t config_gpio;
    int status;

    CHECK_BOARD_STATE(STATE_INITIALIZED);

    /* Validate desired mux mode */
    switch (mode) {
        case BLADERF_RX_MUX_BASEBAND:
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

    status = dev->backend->config_gpio_write(dev, config_gpio);
    if (status != 0) {
        return status;
    }

    return 0;
}

static int bladerf2_get_rx_mux(struct bladerf *dev, bladerf_rx_mux *mode)
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
    val = config_gpio;

    /* Enure it's a valid/supported value */
    switch (val) {
        case BLADERF_RX_MUX_BASEBAND:
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

static int bladerf2_get_vctcxo_trim(struct bladerf *dev, uint16_t *trim)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    /* FIXME fetch factory value from SPI flash */
    *trim = 0x7fff;

    return 0;
}

static int bladerf2_trim_dac_read(struct bladerf *dev, uint16_t *trim)
{
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    return dev->backend->ad56x1_vctcxo_trim_dac_read(dev, trim);
}

static int bladerf2_trim_dac_write(struct bladerf *dev, uint16_t trim)
{
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    return dev->backend->ad56x1_vctcxo_trim_dac_write(dev, trim);
}

/******************************************************************************/
/* Low-level Trigger control access */
/******************************************************************************/

static int bladerf2_read_trigger(struct bladerf *dev, bladerf_channel ch,
                                 bladerf_trigger_signal trigger, uint8_t *val)
{
    CHECK_BOARD_STATE(STATE_FPGA_LOADED);

    return fpga_trigger_read(dev, ch, trigger, val);
}

static int bladerf2_write_trigger(struct bladerf *dev, bladerf_channel ch,
                                  bladerf_trigger_signal trigger, uint8_t val)
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

static int bladerf2_erase_flash(struct bladerf *dev, uint32_t erase_block, uint32_t count)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    return spi_flash_erase(dev, erase_block, count);
}

static int bladerf2_read_flash(struct bladerf *dev, uint8_t *buf,
                               uint32_t page, uint32_t count)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

    return spi_flash_read(dev, buf, page, count);
}

static int bladerf2_write_flash(struct bladerf *dev, const uint8_t *buf,
                                uint32_t page, uint32_t count)
{
    CHECK_BOARD_STATE(STATE_FIRMWARE_LOADED);

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
    *xb = BLADERF_XB_NONE;

    return 0;
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
    FIELD_INIT(.set_gain, bladerf2_set_gain),
    FIELD_INIT(.get_gain, bladerf2_get_gain),
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
    FIELD_INIT(.get_frequency_range, bladerf2_get_frequency_range),
    FIELD_INIT(.set_frequency, bladerf2_set_frequency),
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
    FIELD_INIT(.set_loopback, bladerf2_set_loopback),
    FIELD_INIT(.get_loopback, bladerf2_get_loopback),
    FIELD_INIT(.get_rx_mux, bladerf2_get_rx_mux),
    FIELD_INIT(.set_rx_mux, bladerf2_set_rx_mux),
    FIELD_INIT(.get_vctcxo_tamer_mode, bladerf2_get_vctcxo_tamer_mode),
    FIELD_INIT(.set_vctcxo_tamer_mode, bladerf2_set_vctcxo_tamer_mode),
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
 *                         bladeRF2-specific Functions                        *
 ******************************************************************************
 ******************************************************************************/

/******************************************************************************/
/* Low level AD9361 Accessors */
/******************************************************************************/

int bladerf_ad9361_read(struct bladerf *dev, uint16_t address, uint8_t *val)
{
    int status;
    uint64_t data;

    if (dev->board != &bladerf2_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_FPGA_LOADED);

    address = AD_READ | AD_CNT(1) | address;

    status = dev->backend->ad9361_spi_read(dev, address, &data);

    if (status == 0) {
        *val = (data >> 56) & 0xff;
    }

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_ad9361_write(struct bladerf *dev, uint16_t address, uint8_t val)
{
    int status;
    uint64_t data;

    if (dev->board != &bladerf2_board_fns)
        return BLADERF_ERR_UNSUPPORTED;

    MUTEX_LOCK(&dev->lock);

    CHECK_BOARD_STATE_LOCKED(STATE_FPGA_LOADED);

    address = AD_WRITE | AD_CNT(1) | address;

    data = (((uint64_t)val) << 56);

    status = dev->backend->ad9361_spi_write(dev, address, data);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}
