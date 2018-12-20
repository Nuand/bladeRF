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

#ifndef BOARD_BOARD_H_
#define BOARD_BOARD_H_

#include <stdint.h>

#include <libbladeRF.h>

#include "host_config.h"
#include "thread.h"

#include "backend/backend.h"

/* Device capabilities are stored in a 64-bit mask.
 *
 * FPGA-oriented capabilities start at bit 0.
 * FX3-oriented capabilities start at bit 24.
 * Other/mixed capabilities start at bit 48.
 */

/**
 * Prior to FPGA 0.0.4, the DAC register were located at a different address
 */
#define BLADERF_CAP_UPDATED_DAC_ADDR (1 << 0)

/**
 * FPGA version 0.0.5 introduced XB-200 support
 */
#define BLADERF_CAP_XB200 (1 << 1)

/**
 * FPGA version 0.1.0 introduced timestamp support
 */
#define BLADERF_CAP_TIMESTAMPS (1 << 2)

/**
 * FPGA v0.2.0 introduced scheduled retune support on the bladeRF 1, and FPGA
 * v0.10.0 introduced it on the bladeRF 2.
 */
#define BLADERF_CAP_SCHEDULED_RETUNE (1 << 3)

/**
 * FPGA version 0.3.0 introduced new packet handler formats that pack
 * operations into a single requests.
 */
#define BLADERF_CAP_PKT_HANDLER_FMT (1 << 4)

/**
 * A bug fix in FPGA version 0.3.2 allowed support for reading back
 * the current VCTCXO trim dac value.
 */
#define BLADERF_CAP_VCTCXO_TRIMDAC_READ (1 << 5)

/**
 * FPGA v0.4.0 introduced the ability to write LMS6002D TX/RX PLL
 * NINT & NFRAC regiters atomically.
 */
#define BLADERF_CAP_ATOMIC_NINT_NFRAC_WRITE (1 << 6)

/**
 * FPGA v0.4.1 fixed an issue with masked writes to the expansion GPIO
 * and expansion GPIO direction registers.
 *
 * To help users avoid getting bitten by this bug, we'll mark this
 * as a capability and disallow masked writes unless an FPGA with the
 * fix is being used.
 */
#define BLADERF_CAP_MASKED_XBIO_WRITE (1 << 7)

/**
 * FPGA v0.5.0 introduces the ability to tame the VCTCXO via a 1 pps or 10 MHz
 * input source on the mini expansion header.
 */
#define BLADERF_CAP_VCTCXO_TAMING_MODE (1 << 8)

/**
 * FPGA v0.6.0 introduced trx synchronization trigger via J71-4
 */
#define BLADERF_CAP_TRX_SYNC_TRIG (1 << 9)

/**
 * FPGA v0.7.0 introduced AGC DC correction Look-Up-Table
 */
#define BLADERF_CAP_AGC_DC_LUT (1 << 10)

/**
 * FPGA v0.2.0 introduced NIOS-based tuning support on the bladeRF 1, and FPGA
 * v0.10.1 introduced it on the bladeRF 2.
 */
#define BLADERF_CAP_FPGA_TUNING (1 << 11)

/**
 * Firmware 1.7.1 introduced firmware-based loopback
 */
#define BLADERF_CAP_FW_LOOPBACK (((uint64_t)1) << 32)

/**
 * FX3 firmware version 1.8.0 introduced the ability to query when the
 * device has become ready for use by the host.  This was done to avoid
 * opening and attempting to use the device while flash-based FPGA autoloading
 * was occurring.
 */
#define BLADERF_CAP_QUERY_DEVICE_READY (((uint64_t)1) << 33)

/**
 * FX3 firmware v1.9.0 introduced a vendor request by which firmware log
 * events could be retrieved.
 */
#define BLADERF_CAP_READ_FW_LOG_ENTRY (((uint64_t)1) << 34)

/**
 * FX3 firmware v2.1.0 introduced support for bladeRF 2
 */
#define BLADERF_CAP_FW_SUPPORTS_BLADERF2 (((uint64_t)1) << 35)

/**
 * FX3 firmware v2.3.0 introduced support for reporting the SPI Flash
 * manufacturer ID and device ID.
 */
#define BLADERF_CAP_FW_FLASH_ID (((uint64_t)1) << 36)

/**
 * FX3 firmware v2.3.1 introduced support for querying the source of the
 * currently-configured FPGA (e.g. flash autoload, host, etc)
 */
#define BLADERF_CAP_FW_FPGA_SOURCE (((uint64_t)1) << 37)

struct bladerf {
    /* Handle lock - to ensure atomic access to control and configuration
     * operations */
    MUTEX lock;

    /* Identifying information */
    struct bladerf_devinfo ident;

    /* Backend-specific implementations */
    const struct backend_fns *backend;

    /* Backend's private data */
    void *backend_data;

    /* Board-specific implementations */
    const struct board_fns *board;

    /* Flash architecture */
    struct bladerf_flash_arch *flash_arch;

    /* Board's private data */
    void *board_data;

    /* XB attached */
    bladerf_xb xb;

    /* XB's private data */
    void *xb_data;
};

struct board_fns {
    /* Board is compatible with backend */
    bool (*matches)(struct bladerf *dev);

    /* Open/close */
    int (*open)(struct bladerf *dev, struct bladerf_devinfo *devinfo);
    void (*close)(struct bladerf *dev);

    /* Properties */
    bladerf_dev_speed (*device_speed)(struct bladerf *dev);
    int (*get_serial)(struct bladerf *dev, char *serial);
    int (*get_fpga_size)(struct bladerf *dev, bladerf_fpga_size *size);
    int (*get_fpga_bytes)(struct bladerf *dev, size_t *size);
    int (*get_flash_size)(struct bladerf *dev, uint32_t *size, bool *is_guess);
    int (*is_fpga_configured)(struct bladerf *dev);
    int (*get_fpga_source)(struct bladerf *dev, bladerf_fpga_source *source);
    uint64_t (*get_capabilities)(struct bladerf *dev);
    size_t (*get_channel_count)(struct bladerf *dev, bladerf_direction dir);

    /* Versions */
    int (*get_fpga_version)(struct bladerf *dev,
                            struct bladerf_version *version);
    int (*get_fw_version)(struct bladerf *dev, struct bladerf_version *version);

    /* Gain */
    int (*set_gain)(struct bladerf *dev, bladerf_channel ch, bladerf_gain gain);
    int (*get_gain)(struct bladerf *dev,
                    bladerf_channel ch,
                    bladerf_gain *gain);
    int (*set_gain_mode)(struct bladerf *dev,
                         bladerf_channel ch,
                         bladerf_gain_mode mode);
    int (*get_gain_mode)(struct bladerf *dev,
                         bladerf_channel ch,
                         bladerf_gain_mode *mode);
    int (*get_gain_modes)(struct bladerf *dev,
                          bladerf_channel ch,
                          const struct bladerf_gain_modes **modes);
    int (*get_gain_range)(struct bladerf *dev,
                          bladerf_channel ch,
                          const struct bladerf_range **range);
    int (*set_gain_stage)(struct bladerf *dev,
                          bladerf_channel ch,
                          const char *stage,
                          bladerf_gain gain);
    int (*get_gain_stage)(struct bladerf *dev,
                          bladerf_channel ch,
                          const char *stage,
                          bladerf_gain *gain);
    int (*get_gain_stage_range)(struct bladerf *dev,
                                bladerf_channel ch,
                                const char *stage,
                                const struct bladerf_range **range);
    int (*get_gain_stages)(struct bladerf *dev,
                           bladerf_channel ch,
                           const char **stages,
                           size_t count);

    /* Sample Rate */
    int (*set_sample_rate)(struct bladerf *dev,
                           bladerf_channel ch,
                           bladerf_sample_rate rate,
                           bladerf_sample_rate *actual);
    int (*set_rational_sample_rate)(struct bladerf *dev,
                                    bladerf_channel ch,
                                    struct bladerf_rational_rate *rate,
                                    struct bladerf_rational_rate *actual);
    int (*get_sample_rate)(struct bladerf *dev,
                           bladerf_channel ch,
                           bladerf_sample_rate *rate);
    int (*get_sample_rate_range)(struct bladerf *dev,
                                 bladerf_channel ch,
                                 const struct bladerf_range **range);
    int (*get_rational_sample_rate)(struct bladerf *dev,
                                    bladerf_channel ch,
                                    struct bladerf_rational_rate *rate);

    /* Bandwidth */
    int (*set_bandwidth)(struct bladerf *dev,
                         bladerf_channel ch,
                         bladerf_bandwidth bandwidth,
                         bladerf_bandwidth *actual);
    int (*get_bandwidth)(struct bladerf *dev,
                         bladerf_channel ch,
                         bladerf_bandwidth *bandwidth);
    int (*get_bandwidth_range)(struct bladerf *dev,
                               bladerf_channel ch,
                               const struct bladerf_range **range);

    /* Frequency */
    int (*get_frequency)(struct bladerf *dev,
                         bladerf_channel ch,
                         bladerf_frequency *frequency);
    int (*set_frequency)(struct bladerf *dev,
                         bladerf_channel ch,
                         bladerf_frequency frequency);
    int (*get_frequency_range)(struct bladerf *dev,
                               bladerf_channel ch,
                               const struct bladerf_range **range);
    int (*select_band)(struct bladerf *dev,
                       bladerf_channel ch,
                       bladerf_frequency frequency);

    /* RF Ports */
    int (*set_rf_port)(struct bladerf *dev,
                       bladerf_channel ch,
                       const char *port);
    int (*get_rf_port)(struct bladerf *dev,
                       bladerf_channel ch,
                       const char **port);
    int (*get_rf_ports)(struct bladerf *dev,
                        bladerf_channel ch,
                        const char **ports,
                        unsigned int count);

    /* Scheduled Tuning */
    int (*get_quick_tune)(struct bladerf *dev,
                          bladerf_channel ch,
                          struct bladerf_quick_tune *quick_tune);
    int (*schedule_retune)(struct bladerf *dev,
                           bladerf_channel ch,
                           bladerf_timestamp timestamp,
                           bladerf_frequency frequency,
                           struct bladerf_quick_tune *quick_tune);
    int (*cancel_scheduled_retunes)(struct bladerf *dev, bladerf_channel ch);

    /* DC/Phase/Gain Correction */
    int (*get_correction)(struct bladerf *dev,
                          bladerf_channel ch,
                          bladerf_correction corr,
                          int16_t *value);
    int (*set_correction)(struct bladerf *dev,
                          bladerf_channel ch,
                          bladerf_correction corr,
                          int16_t value);

    /* Trigger */
    int (*trigger_init)(struct bladerf *dev,
                        bladerf_channel ch,
                        bladerf_trigger_signal signal,
                        struct bladerf_trigger *trigger);
    int (*trigger_arm)(struct bladerf *dev,
                       const struct bladerf_trigger *trigger,
                       bool arm,
                       uint64_t resv1,
                       uint64_t resv2);
    int (*trigger_fire)(struct bladerf *dev,
                        const struct bladerf_trigger *trigger);
    int (*trigger_state)(struct bladerf *dev,
                         const struct bladerf_trigger *trigger,
                         bool *is_armed,
                         bool *has_fired,
                         bool *fire_requested,
                         uint64_t *resv1,
                         uint64_t *resv2);

    /* Streaming */
    int (*enable_module)(struct bladerf *dev, bladerf_channel ch, bool enable);
    int (*init_stream)(struct bladerf_stream **stream,
                       struct bladerf *dev,
                       bladerf_stream_cb callback,
                       void ***buffers,
                       size_t num_buffers,
                       bladerf_format format,
                       size_t samples_per_buffer,
                       size_t num_transfers,
                       void *user_data);
    int (*stream)(struct bladerf_stream *stream, bladerf_channel_layout layout);
    int (*submit_stream_buffer)(struct bladerf_stream *stream,
                                void *buffer,
                                unsigned int timeout_ms,
                                bool nonblock);
    void (*deinit_stream)(struct bladerf_stream *stream);
    int (*set_stream_timeout)(struct bladerf *dev,
                              bladerf_direction dir,
                              unsigned int timeout);
    int (*get_stream_timeout)(struct bladerf *dev,
                              bladerf_direction dir,
                              unsigned int *timeout);
    int (*sync_config)(struct bladerf *dev,
                       bladerf_channel_layout layout,
                       bladerf_format format,
                       unsigned int num_buffers,
                       unsigned int buffer_size,
                       unsigned int num_transfers,
                       unsigned int stream_timeout);
    int (*sync_tx)(struct bladerf *dev,
                   const void *samples,
                   unsigned int num_samples,
                   struct bladerf_metadata *metadata,
                   unsigned int timeout_ms);
    int (*sync_rx)(struct bladerf *dev,
                   void *samples,
                   unsigned int num_samples,
                   struct bladerf_metadata *metadata,
                   unsigned int timeout_ms);
    int (*get_timestamp)(struct bladerf *dev,
                         bladerf_direction dir,
                         bladerf_timestamp *timestamp);

    /* FPGA/Firmware Loading/Flashing */
    int (*load_fpga)(struct bladerf *dev, const uint8_t *buf, size_t length);
    int (*flash_fpga)(struct bladerf *dev, const uint8_t *buf, size_t length);
    int (*erase_stored_fpga)(struct bladerf *dev);
    int (*flash_firmware)(struct bladerf *dev,
                          const uint8_t *buf,
                          size_t length);
    int (*device_reset)(struct bladerf *dev);

    /* Tuning mode */
    int (*set_tuning_mode)(struct bladerf *dev, bladerf_tuning_mode mode);
    int (*get_tuning_mode)(struct bladerf *dev, bladerf_tuning_mode *mode);

    /* Loopback */
    int (*get_loopback_modes)(struct bladerf *dev,
                              const struct bladerf_loopback_modes **modes);
    int (*set_loopback)(struct bladerf *dev, bladerf_loopback l);
    int (*get_loopback)(struct bladerf *dev, bladerf_loopback *l);

    /* Sample RX FPGA Mux */
    int (*get_rx_mux)(struct bladerf *dev, bladerf_rx_mux *mode);
    int (*set_rx_mux)(struct bladerf *dev, bladerf_rx_mux mode);

    /* Low-level VCTCXO Tamer Mode */
    int (*set_vctcxo_tamer_mode)(struct bladerf *dev,
                                 bladerf_vctcxo_tamer_mode mode);
    int (*get_vctcxo_tamer_mode)(struct bladerf *dev,
                                 bladerf_vctcxo_tamer_mode *mode);

    /* Low-level VCTCXO Trim DAC access */
    int (*get_vctcxo_trim)(struct bladerf *dev, uint16_t *trim);
    int (*trim_dac_read)(struct bladerf *dev, uint16_t *trim);
    int (*trim_dac_write)(struct bladerf *dev, uint16_t trim);

    /* Low-level Trigger control access */
    int (*read_trigger)(struct bladerf *dev,
                        bladerf_channel ch,
                        bladerf_trigger_signal trigger,
                        uint8_t *val);
    int (*write_trigger)(struct bladerf *dev,
                         bladerf_channel ch,
                         bladerf_trigger_signal trigger,
                         uint8_t val);

    /* Low-level Configuration GPIO access */
    int (*config_gpio_read)(struct bladerf *dev, uint32_t *val);
    int (*config_gpio_write)(struct bladerf *dev, uint32_t val);

    /* Low-level SPI flash access */
    int (*erase_flash)(struct bladerf *dev,
                       uint32_t erase_block,
                       uint32_t count);
    int (*read_flash)(struct bladerf *dev,
                      uint8_t *buf,
                      uint32_t page,
                      uint32_t count);
    int (*write_flash)(struct bladerf *dev,
                       const uint8_t *buf,
                       uint32_t page,
                       uint32_t count);

    /* Expansion support */
    int (*expansion_attach)(struct bladerf *dev, bladerf_xb xb);
    int (*expansion_get_attached)(struct bladerf *dev, bladerf_xb *xb);

    /* Board name */
    const char *name;
};

/* Information about the (SPI) flash architecture */
struct bladerf_flash_arch {
    enum { STATUS_UNINITIALIZED, STATUS_SUCCESS, STATUS_ASSUMED } status;

    uint8_t manufacturer_id; /**< Raw manufacturer ID */
    uint8_t device_id;       /**< Raw device ID */
    uint32_t tsize_bytes;    /**< Total size of flash, in bytes */
    uint32_t psize_bytes;    /**< Flash page size, in bytes */
    uint32_t ebsize_bytes;   /**< Flash erase block size, in bytes */
    uint32_t num_pages;      /**< Size of flash, in pages */
    uint32_t num_ebs;        /**< Size of flash, in erase blocks */
};

/* Boards */
extern const struct board_fns *bladerf_boards[];
extern const unsigned int bladerf_boards_len;

#endif
