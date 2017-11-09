/**
 * @file bladeRF2.h
 *
 * @brief bladeRF2-specific API
 *
 * Copyright (C) 2013-2017 Nuand LLC
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */
#ifndef BLADERF2_H_
#define BLADERF2_H_

/**
 * @defgroup BLADERF2 bladeRF2-specific API
 *
 * These functions are thread-safe.
 *
 * @{
 */

/**
 * @defgroup FN_BLADERF2_BIAS_TEE Bias Tee Control
 *
 * @{
 */

/**
 * Get current bias tee state
 *
 * @param       dev     Device handle
 * @param[in]   ch      Channel
 * @param[out]  enable  True if bias tee active, false otherwise
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_bias_tee(struct bladerf *dev,
                                   bladerf_channel ch,
                                   bool *enable);

/**
 * Get current bias tee state
 *
 * @param       dev     Device handle
 * @param[in]   ch      Channel
 * @param[in]   enable  True to activate bias tee, false to deactivate
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_bias_tee(struct bladerf *dev,
                                   bladerf_channel ch,
                                   bool enable);

/** @} (End of FN_BLADERF2_BIAS_TEE) */

/**
 * @defgroup FN_BLADERF2_LOW_LEVEL Low-level accessors
 *
 * In a most cases, higher-level routines should be used. These routines are
 * only intended to support development and testing.
 *
 * Use these routines with great care, and be sure to reference the relevant
 * schematics, data sheets, and source code (i.e., firmware and hdl).
 *
 * Be careful when mixing these calls with higher-level routines that manipulate
 * the same registers/settings.
 *
 * These functions are thread-safe.
 *
 * @{
 */

/**
 * @defgroup FN_BLADERF2_LOW_LEVEL_AD9361 AD9361 RFIC
 *
 * @{
 */

/**
 * Read an AD9361 register
 *
 * @param       dev         Device handle
 * @param[in]   address     AD9361 register address
 * @param[out]  val         Register value
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_ad9361_read(struct bladerf *dev,
                                  uint16_t address,
                                  uint8_t *val);
/**
 * Write an AD9361 register
 *
 * @param       dev         Device handle
 * @param[in]   address     AD9361 register address
 * @param[in]   val         Value to write to register
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_ad9361_write(struct bladerf *dev,
                                   uint16_t address,
                                   uint8_t val);

/** @} (End of FN_BLADERF2_LOW_LEVEL_AD9361) */

/**
 * @defgroup FN_BLADERF2_LOW_LEVEL_ADF4002 ADF4002 Phase Detector/Freq. Synth
 *
 * @{
 */

/**
 * Fetch the state of the ADF4002 Phase Detector/Frequency Synthesizer
 *
 * @param       dev         Device handle
 * @param[out]  enabled     True if enabled, False otherwise
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_adf4002_get_enable(struct bladerf *dev, bool *enabled);

/**
 * Enable the ADF4002 Phase Detector/Frequency Synthesizer
 *
 * Enabling this disables the AD5621 VCTCXO trimmer DAC, and vice versa.
 *
 * @param       dev         Device handle
 * @param[in]   enable      True to enable, False otherwise
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_adf4002_set_enable(struct bladerf *dev, bool enable);

/**
 * Read value from ADF4002 Phase Detector/Frequency Synthesizer
 *
 * Reference:
 * http://www.analog.com/media/en/technical-documentation/data-sheets/ADF4002.pdf
 *
 * The address is interpreted as the control bits (DB1 and DB0) used to write
 * to a specific latch.
 *
 * Note that val is shifted right by 2 bits, relative to the data sheet's latch
 * map. val(21 downto 0) = DB(23 downto 2)
 *
 * @param       dev         Device handle
 * @param[in]   address     ADF4002 latch address
 * @param[out]  val         Value to read from ADF4002
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_adf4002_read(struct bladerf *dev,
                                   uint8_t address,
                                   uint32_t *val);

/**
 * Write value to ADF4002 Phase Detector/Frequency Synthesizer
 *
 * Reference:
 * http://www.analog.com/media/en/technical-documentation/data-sheets/ADF4002.pdf
 *
 * The address is interpreted as the control bits (DB1 and DB0) used to write
 * to a specific latch.
 *
 * Note that val is shifted right by 2 bits, relative to the data sheet's latch
 * map. val(21 downto 0) = DB(23 downto 2)
 *
 * @param       dev         Device handle
 * @param[in]   address     ADF4002 latch address
 * @param[in]   val         Value to write to ADF4002
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_adf4002_write(struct bladerf *dev,
                                    uint8_t address,
                                    uint32_t val);

/** @} (End of FN_BLADERF2_LOW_LEVEL_ADF4002) */

/**
 * @defgroup FN_BLADERF2_LOW_LEVEL_INA219 INA219 Current/Power Monitor
 *
 * @{
 */

/**
 * Register identifiers for INA219
 */
typedef enum {
    BLADERF_INA219_CONFIGURATION, /**< Configuration register (uint16_t) */
    BLADERF_INA219_VOLTAGE_SHUNT, /**< Shunt voltage (float) */
    BLADERF_INA219_VOLTAGE_BUS,   /**< Bus voltage (float) */
    BLADERF_INA219_POWER,         /**< Load power (float) */
    BLADERF_INA219_CURRENT,       /**< Load current (float) */
    BLADERF_INA219_CALIBRATION,   /**< Calibration (uint16_t) */
} bladerf_ina219_register;

/**
 * Read value from INA219 Current/Power Monitor
 *
 * Reference: http://www.ti.com/product/INA219
 *
 * @param       dev     Device handle
 * @param[in]   reg     Register to read from
 * @param[out]  val     Value read from INA219
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_ina219_read(struct bladerf *dev,
                                  bladerf_ina219_register reg,
                                  void *val);

/** @} (End of FN_BLADERF2_LOW_LEVEL_INA219) */

/** @} (End of FN_BLADERF2_LOW_LEVEL) */

/** @} (End of BLADERF2) */

#endif /* BLADERF2_H_ */
