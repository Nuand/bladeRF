/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014-2015 Nuand LLC
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
 * soft-core processor running on the FPGA versions prior to v0.2.x.
 * (Although post v0.2.x FPGAs support this for reverse compatibility).
 *
 * The host communicates with the NIOS II via USB transfers to the Cypress FX3,
 * which then communicates with the FPGA via a UART. This module packs and
 * submits the requests, and receives and unpacks the responses.
 *
 * See the files in the top-level fpga_common/ directory for description
 * and macros pertaining to the legacy packet format.
 */

#ifndef BACKEND_USB_NIOS_LEGACY_ACCESS_H_
#define BACKEND_USB_NIOS_LEGACY_ACCESS_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "board/board.h"
#include "usb.h"

/**
 * Perform a read from device/blocks connected via NIOS II PIO.
 *
 * @param       dev     Device handle
 * @param[in]   addr    Virtual "register address." Use NIOS_LEGACY_PIO_ADDR_*
 *                      definitions.
 * @param[out]  data    Read data
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_pio_read(struct bladerf *dev, uint8_t addr, uint32_t *data);

/**
 * Perform a write to device/blocks connected via NIOS II PIO.
 *
 * @param       dev     Device handle
 * @param[in]   addr    Virtual "register address." Use NIOS_LEGACY_PIO_ADDR_*
 *                      definitions.
 * @param[in]   data    Data to write
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_pio_write(struct bladerf *dev, uint8_t addr, uint32_t data);

/**
 * Read from the FPGA's config register
 *
 * @param       dev     Device handle
 * @param[out]  val     On success, updated with config register value
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_config_read(struct bladerf *dev, uint32_t *val);

/**
 * Write to FPGA's config register
 *
 * @param       dev     Device handle
 * @param[in]   val     Register value to write
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_config_write(struct bladerf *dev, uint32_t val);

/**
 * Read the FPGA version into the provided version structure
 *
 * @param       dev     Device handle
 * @param[out]  ver     Updated with FPGA version (upon success)
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_get_fpga_version(struct bladerf *dev,
                                 struct bladerf_version *ver);

/**
 * Read a timestamp counter value
 *
 * @param       dev         Device handle
 * @param[in]   dir         Stream direction
 * @param[out]  timestamp   On success, updated with the timestamp value
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_get_timestamp(struct bladerf *dev,
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
int nios_legacy_si5338_read(struct bladerf *dev, uint8_t addr, uint8_t *data);

/**
 * Write to an Si5338 register
 *
 * @param       dev         Device handle
 * @param[in]   addr        Register address
 * @param[in]   data        Data to write
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_si5338_write(struct bladerf *dev, uint8_t addr, uint8_t data);

/**
 * Read from an LMS6002D register
 *
 * @param       dev         Device handle
 * @param[in]   addr        Register address
 * @param[out]  data        On success, updated with data read from the device
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_lms6_read(struct bladerf *dev, uint8_t addr, uint8_t *data);

/**
 * Write to an LMS6002D register
 *
 * @param       dev         Device handle
 * @param[in]   addr        Register address
 * @param[out]  data        Register data
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_lms6_write(struct bladerf *dev, uint8_t addr, uint8_t data);

/**
 * Read from an INA219 register
 *
 * @param       dev         Device handle
 * @param[in]   addr        Register address
 * @param[out]  data        Data
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_ina219_read(struct bladerf *dev, uint8_t addr, uint16_t *data);

/**
 * Write to an INA219 register
 *
 * @param       dev         Device handle
 * @param[in]   addr        Register address
 * @param[in]   data        Data
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_ina219_write(struct bladerf *dev, uint8_t addr, uint16_t data);

/**
 * Read the AD9361 over SPI.
 *
 * @param           dev         Device handle
 * @param[in]       cmd         Command
 * @param[out]      data        Data
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_ad9361_spi_read(struct bladerf *dev,
                                uint16_t cmd,
                                uint64_t *data);

/**
 * Write to the AD9361 over SPI.
 *
 * @param           dev         Device handle
 * @param[in]       cmd         Command
 * @param[in]       data        Data
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_ad9361_spi_write(struct bladerf *dev,
                                 uint16_t cmd,
                                 uint64_t data);

/**
 * Read the ADI AXI memory mapped region.
 *
 * @param           dev         Device handle
 * @param[in]       addr        Address
 * @param[out]      data        Data
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_adi_axi_read(struct bladerf *dev,
                             uint32_t addr,
                             uint32_t *data);

/**
 * Write to the ADI AXI memory mapped region.
 *
 * @param           dev         Device handle
 * @param[in]       addr        Address
 * @param[in]       data        Data
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_adi_axi_write(struct bladerf *dev,
                              uint32_t addr,
                              uint32_t data);

/**
 * Read RFIC command
 *
 * @param       dev     Device handle
 * @param[in]   cmd     Command
 * @param[out]  data    Data
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_rfic_command_read(struct bladerf *dev,
                                  uint16_t cmd,
                                  uint64_t *data);

/**
 * Write RFIC command
 *
 * @param       dev     Device handle
 * @param[in]   cmd     Command
 * @param[in]   data    Data
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_rfic_command_write(struct bladerf *dev,
                                   uint16_t cmd,
                                   uint64_t data);

/**
 * Read RFFE control register.
 *
 * @param           dev         Device handle
 * @param[in]       len         Data buffer length
 * @param[out]      value       Value
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_rffe_control_read(struct bladerf *dev, uint32_t *value);

/**
 * Write RFFE control register.
 *
 * @param           dev         Device handle
 * @param[in]       len         Data buffer length
 * @param[in]       value       Value
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_rffe_control_write(struct bladerf *dev, uint32_t value);

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
int nios_legacy_rffe_fastlock_save(struct bladerf *dev, bool is_tx,
                                   uint8_t rffe_profile,
                                   uint16_t nios_profile);

/**
 * Write to the AD56X1 VCTCXO trim DAC.
 *
 * @param       dev         Device handle
 * @param[in]   value       Value
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_ad56x1_vctcxo_trim_dac_write(struct bladerf *dev,
                                             uint16_t value);

/**
 * Read the AD56X1 VCTCXO trim DAC.
 *
 * @param       dev         Device handle
 * @param[out]  value       Data
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_ad56x1_vctcxo_trim_dac_read(struct bladerf *dev,
                                            uint16_t *value);

/**
 * Write to the ADF400X.
 *
 * @param       dev         Device handle
 * @param[in]   addr        Address
 * @param[in]   data        Data
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_adf400x_write(struct bladerf *dev, uint8_t addr, uint32_t data);

/**
 * Read from the ADF400X.
 *
 * @param       dev         Device handle
 * @param[in]   addr        Address
 * @param[out]  data        Data
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_adf400x_read(struct bladerf *dev, uint8_t addr, uint32_t *data);

/**
 * Write to the VCTCXO trim DAC.
 *
 * @param       dev         Device handle
 * @param[in]   addr        Register (should be 0x08)
 * @param[in]   value       Value
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_vctcxo_trim_dac_write(struct bladerf *dev,
                                      uint8_t addr,
                                      uint16_t value);

/**
 * Read a IQ gain correction value
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[out]  value       On success, updated with phase correction value
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_get_iq_gain_correction(struct bladerf *dev,
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
int nios_legacy_get_iq_phase_correction(struct bladerf *dev,
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
int nios_legacy_set_iq_gain_correction(struct bladerf *dev,
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
int nios_legacy_set_iq_phase_correction(struct bladerf *dev,
                                        bladerf_channel ch,
                                        int16_t value);

/**
 * Write a value to the XB-200's ADF4351 synthesizer
 *
 * @param       dev         Device handle
 * @param[in]   value       Write value
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_xb200_synth_write(struct bladerf *dev, uint32_t value);

/**
 * Read from expanion GPIO
 *
 * @param       dev         Device handle
 * @param[out]  value       On success, updated with read value
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_expansion_gpio_read(struct bladerf *dev, uint32_t *val);

/**
 * Write to expansion GPIO
 *
 * @param       dev         Device handle
 * @param[in[   mask        Mask denoting bits to write
 * @param[in]   value       Value to write
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_expansion_gpio_write(struct bladerf *dev,
                                     uint32_t mask,
                                     uint32_t val);

/**
 * Read from expansion GPIO direction register
 *
 * @param       dev         Device handle
 * @param[out]  value       On success, updated with read value
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_expansion_gpio_dir_read(struct bladerf *dev, uint32_t *val);

/**
 * Write to expansion GPIO direction register
 *
 * @param       dev         Device handle
 * @param[in]   mask        Mask denoting bits to configure
 * @param[in]   output      '1' bit denotes an output, '0' bit denotes an input
 *
 * @return 0 on success, BLADERF_ERR_* code on error.
 */
int nios_legacy_expansion_gpio_dir_write(struct bladerf *dev,
                                         uint32_t mask,
                                         uint32_t outputs);

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
int nios_legacy_read_trigger(struct bladerf *dev,
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
int nios_legacy_write_trigger(struct bladerf *dev,
                              bladerf_channel ch,
                              bladerf_trigger_signal trigger,
                              uint8_t value);

#endif
