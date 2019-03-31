/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2015 Nuand LLC
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

/* This file defines functionality used to communicate with the NIOS II
 * soft-core processor running on the FPGA versions >= v0.3.0
 *
 * The host communicates with the NIOS II via USB transfers to the Cypress FX3,
 * which then communicates with the FPGA via a UART. This module packs and
 * submits the requests, and receives and unpacks the responses.
 *
 * See the files in the top-level fpga_common/ directory for description
 * and macros pertaining to the legacy packet format.
 */

#ifndef BACKEND_USB_NIOS_ACCESS_H_
#define BACKEND_USB_NIOS_ACCESS_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "board/board.h"
#include "usb.h"

/**
 * Read from the FPGA's config register
 *
 * @param       dev     Device handle
 * @param[out]  val     On success, updated with config register value
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_config_read(struct bladerf *dev, uint32_t *val);

/**
 * Write to FPGA's config register
 *
 * @param       dev     Device handle
 * @param[in]   val     Register value to write
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_config_write(struct bladerf *dev, uint32_t val);

/**
 * Read the FPGA version into the provided version structure
 *
 * @param       dev     Device handle
 * @param[out]  ver     Updated with FPGA version (upon success)
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_get_fpga_version(struct bladerf *dev, struct bladerf_version *ver);

/**
 * Read a timestamp counter value
 *
 * @param       dev         Device handle
 * @param[in]   dir         Stream direction
 * @param[out]  timestamp   On success, updated with the timestamp value
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_get_timestamp(struct bladerf *dev,
                       bladerf_direction dir,
                       uint64_t *timestamp);

/**
 * Read from an Si5338 register
 *
 * @param       dev         Device handle
 * @param[in]   addr        Register address
 * @param[out]  data        On success, updated with read data
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_si5338_read(struct bladerf *dev, uint8_t addr, uint8_t *data);

/**
 * Write to an Si5338 register
 *
 * @param       dev         Device handle
 * @param[in]   addr        Register address
 * @param[in]   data        Data to write
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_si5338_write(struct bladerf *dev, uint8_t addr, uint8_t data);

/**
 * Read from an LMS6002D register
 *
 * @param       dev         Device handle
 * @param[in]   addr        Register address
 * @param[out]  data        On success, updated with data read from the device
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_lms6_read(struct bladerf *dev, uint8_t addr, uint8_t *data);

/**
 * Write to an LMS6002D register
 *
 * @param       dev         Device handle
 * @param[in]   addr        Register address
 * @param[out]  data        Register data
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_lms6_write(struct bladerf *dev, uint8_t addr, uint8_t data);

/**
 * Read from an INA219 register
 *
 * @param       dev         Device handle
 * @param[in]   addr        Register address
 * @param[out]  data        Data
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_ina219_read(struct bladerf *dev, uint8_t addr, uint16_t *data);

/**
 * Write to an INA219 register
 *
 * @param       dev         Device handle
 * @param[in]   addr        Register address
 * @param[in]   data        Data
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_ina219_write(struct bladerf *dev, uint8_t addr, uint16_t data);

/**
 * Read the AD9361 over SPI.
 *
 * @param           dev         Device handle
 * @param[in]       cmd         Command
 * @param[out]      data        Data
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_ad9361_spi_read(struct bladerf *dev, uint16_t cmd, uint64_t *data);

/**
 * Write to the AD9361 over SPI.
 *
 * @param           dev         Device handle
 * @param[in]       cmd         Command
 * @param[in]       data        Data
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_ad9361_spi_write(struct bladerf *dev, uint16_t cmd, uint64_t data);

/**
 * Read the ADI AXI memory mapped region.
 *
 * @param           dev         Device handle
 * @param[in]       addr        Address
 * @param[out]      data        Data
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_adi_axi_read(struct bladerf *dev, uint32_t cmd, uint32_t *data);

/**
 * Write to the ADI AXI memory mapped region.
 *
 * @param           dev         Device handle
 * @param[in]       addr        Address
 * @param[in]       data        Data
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_adi_axi_write(struct bladerf *dev, uint32_t cmd, uint32_t data);

/**
 * Read RFIC command
 * 
 * @param       dev     Device handle
 * @param[in]   cmd     Command: `(command & 0xFF) + ((channel & 0xF) << 8)`
 * @param[out]  data    Data
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_rfic_command_read(struct bladerf *dev, uint16_t cmd, uint64_t *data);

/**
 * Write RFIC command
 *
 * @param       dev     Device handle
 * @param[in]   cmd     Command: `(command & 0xFF) + ((channel & 0xF) << 8)`
 * @param[in]   data    Data
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_rfic_command_write(struct bladerf *dev, uint16_t cmd, uint64_t data);

/**
 * Read RFFE control register.
 *
 * @param           dev         Device handle
 * @param[in]       len         Data buffer length
 * @param[out]      value       Value
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_rffe_control_read(struct bladerf *dev, uint32_t *value);

/**
 * Write RFFE control register.
 *
 * @param           dev         Device handle
 * @param[in]       len         Data buffer length
 * @param[in]       value       Value
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_rffe_control_write(struct bladerf *dev, uint32_t value);

/**
 * Save an RFFE fast lock profile to the Nios.
 *
 * @param           dev          Device handle
 * @param[in]       is_tx        True if TX profile, false if RX profile
 * @param[in]       rffe_profile RFFE profile to save
 * @param[in]       nios_profile Where to save the profile in the Nios
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_rffe_fastlock_save(struct bladerf *dev, bool is_tx,
                            uint8_t rffe_profile, uint16_t nios_profile);

/**
 * Write to the AD56X1 VCTCXO trim DAC.
 *
 * @param       dev         Device handle
 * @param[in]   value       Value
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_ad56x1_vctcxo_trim_dac_write(struct bladerf *dev, uint16_t value);

/**
 * Read the AD56X1 VCTCXO trim DAC.
 *
 * @param       dev         Device handle
 * @param[out]  value       Data
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_ad56x1_vctcxo_trim_dac_read(struct bladerf *dev, uint16_t *value);

/**
 * Write to the ADF400X.
 *
 * @param       dev         Device handle
 * @param[in]   addr        Address
 * @param[in]   data        Data
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_adf400x_write(struct bladerf *dev, uint8_t addr, uint32_t data);

/**
 * Read from the ADF400X.
 *
 * @param       dev         Device handle
 * @param[in]   addr        Address
 * @param[out]  data        Data
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_adf400x_read(struct bladerf *dev, uint8_t addr, uint32_t *data);

/**
 * Write to a VCTCXO trim DAC register.
 *
 * @param       dev         Device handle
 * @param[in]   addr        Register
 * @param[in]   value       Value
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_vctcxo_trim_dac_write(struct bladerf *dev,
                               uint8_t addr,
                               uint16_t value);

/**
 * Read from a VCTCXO trim DAC register.
 *
 * @param       dev         Device handle
 * @param[in]   addr        Register
 * @param[out]  value       Data
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_vctcxo_trim_dac_read(struct bladerf *dev,
                              uint8_t addr,
                              uint16_t *value);

/**
 * Write VCTCXO tamer mode selection
 *
 * @param       dev         Device handle
 * @param[in]   mode        Tamer mode to use, or BLADERF_VCTCXO_TAMER_DISABLED
 *                          to disable the tamer.
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_set_vctcxo_tamer_mode(struct bladerf *dev,
                               bladerf_vctcxo_tamer_mode mode);

/**
 * Read the current VCTCXO mode selection
 *
 * @param       dev         Device handle
 * @param[out]  mode        Current tamer mode in use.
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_get_vctcxo_tamer_mode(struct bladerf *dev,
                               bladerf_vctcxo_tamer_mode *mode);

/**
 * Read a IQ gain correction value
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[out]  value       On success, updated with phase correction value
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_get_iq_gain_correction(struct bladerf *dev,
                                bladerf_channel ch,
                                int16_t *value);

/**
 * Read a IQ phase correction value
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[out]  value       On success, updated with phase correction value
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_get_iq_phase_correction(struct bladerf *dev,
                                 bladerf_channel ch,
                                 int16_t *value);

/**
 * Write an IQ gain correction value
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[in]   value       Correction value to write
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_set_iq_gain_correction(struct bladerf *dev,
                                bladerf_channel ch,
                                int16_t value);

/**
 * Write an IQ phase correction value
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[in]   value       Correction value to write
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_set_iq_phase_correction(struct bladerf *dev,
                                 bladerf_channel ch,
                                 int16_t value);

/**
 * Write AGC DC LUT values
 *
 * @param[in]   dev         Device handle
 * @param[in]   q_max       Q DC offset at Max gain
 * @param[in]   i_max       I DC offset at Max gain
 * @param[in]   q_mid       Q DC offset at Mid gain
 * @param[in]   i_mid       I DC offset at Mid gain
 * @param[in]   q_low       Q DC offset at Low gain
 * @param[in]   i_low       I DC offset at Low gain
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_set_agc_dc_correction(struct bladerf *dev,
                               int16_t q_max,
                               int16_t i_max,
                               int16_t q_mid,
                               int16_t i_mid,
                               int16_t q_low,
                               int16_t i_low);
/**
 * Write a value to the XB-200's ADF4351 synthesizer
 *
 * @param       dev         Device handle
 * @param[in]   value       Write value
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_xb200_synth_write(struct bladerf *dev, uint32_t value);

/**
 * Read from expanion GPIO
 *
 * @param       dev         Device handle
 * @param[out]  value       On success, updated with read value
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_expansion_gpio_read(struct bladerf *dev, uint32_t *val);

/**
 * Write to expansion GPIO
 *
 * @param       dev         Device handle
 * @param[in]   mask        Mask denoting bits to write
 * @param[in]   value       Write value
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_expansion_gpio_write(struct bladerf *dev, uint32_t mask, uint32_t val);

/**
 * Read from expansion GPIO direction register
 *
 * @param       dev         Device handle
 * @param[out]  value       On success, updated with read value
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_expansion_gpio_dir_read(struct bladerf *dev, uint32_t *val);

/**
 * Write to expansion GPIO direction register
 *
 * @param       dev         Device handle
 * @param[in]   mask        Mask denoting bits to configure
 * @param[in]   output      '1' bit denotes output, '0' bit denotes input
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_expansion_gpio_dir_write(struct bladerf *dev,
                                  uint32_t mask,
                                  uint32_t outputs);

/**
 * Dummy handler for a retune request, which is not supported on
 * earlier FPGA versions.
 *
 * All of the following parameters are ignored.
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[in]   timestamp   Time to schedule retune at
 * @param[in]   nint        Integer portion of frequency multiplier
 * @param[in]   nfrac       Fractional portion of frequency multiplier
 * @param[in]   freqsel     VCO and divider selection
 * @param[in]   low_band    High vs low band selection
 * @param[in]   xb_gpio     XB configuration bits
 * @param[in]   quick_tune  Denotes quick tune should be used instead of
 *                          tuning algorithm
 *
 * @return BLADERF_ERR_UNSUPPORTED
 */
int nios_retune(struct bladerf *dev,
                bladerf_channel ch,
                uint64_t timestamp,
                uint16_t nint,
                uint32_t nfrac,
                uint8_t freqsel,
                uint8_t vcocap,
                bool low_band,
                uint8_t xb_gpio,
                bool quick_tune);

/**
 * Handler for a retune request on bladeRF2 devices. The RFFEs used in these
 * devices have a concept called fast lock profiles that store all the VCO
 * information needed to quickly retune to a different frequency. The RFFE
 * can store up to 8 profiles for RX, and a separate 8 for TX. To use more
 * profiles, they must be stored in the baseband processor (e.g. the FPGA
 * or Nios) and loaded into one of the 8 slots as needed.
 *
 * Each of these profiles consumes 16 bytes not including the timestamp and
 * RF port/switch information. This is too large to fit into a single Nios
 * packet at this time, so to improve retune time, the profiles are stored
 * in the Nios and the Host will schedule retunes by specifying the Nios
 * profile to load into the specified RFFE profile slot.
 *
 * @param       dev          Device handle
 * @param[in]   ch           Channel
 * @param[in]   timestamp    Time to schedule retune at
 * @param[in]   nios_profile Nios profile number (0-N) to load into the RFFE
 * @param[in]   rffe_profile RFFE fast lock profile number (0-7) into which
 *                           the Nios profile will be loaded.
 * @param[in]   port         RFFE port settings
 * @param[in]   spdt         RF SPDT switch settings
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_retune2(struct bladerf *dev, bladerf_channel ch,
                 uint64_t timestamp, uint16_t nios_profile,
                 uint8_t rffe_profile, uint8_t port, uint8_t spdt);

/**
 * Read trigger register value
 *
 * @param       dev        Device handle
 * @param[in]   ch         Channel
 * @param[in]   trigger    Trigger to read from
 * @param[out]  value      On success, updated with register value
 *
 *
 * @return 0 on success, BLADERF_ERR_* code on error
 */
int nios_read_trigger(struct bladerf *dev,
                      bladerf_channel ch,
                      bladerf_trigger_signal trigger,
                      uint8_t *value);

/**
 * Write trigger register value
 *
 * @param       dev        Device handle
 * @param[in]   ch         Channel
 * @param[in]   trigger    Trigger to read
 * @param[in]   value      Value to write
 *
 * @return 0 on success, BLADERF_ERR_* code on error
 */
int nios_write_trigger(struct bladerf *dev,
                       bladerf_channel ch,
                       bladerf_trigger_signal trigger,
                       uint8_t value);

#endif
