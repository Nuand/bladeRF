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

/**
 * Read the temperature from the AD9361 RFIC
 *
 * @param       dev         Device handle
 * @param[out]  val         Temperature in degrees C
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_ad9361_temperature(struct bladerf *dev, float *val);

/**
 * Read the RSSI for the selected channel from the AD9361 RFIC
 *
 * @note  This is a relative value, not an absolute value. If an absolute
 *        value (e.g. in dBm) is desired, a calibration should be performed
 *        against a reference signal.
 *
 * @note  See `src/board/bladerf2/params.c` for the RSSI control parameters.
 *
 * Reference: AD9361 Reference Manual UG-570
 *
 * @param       dev         Device handle
 * @param       ch          Channel to query
 * @param[out]  pre_rssi    Preamble RSSI in dB (first calculated RSSI result)
 * @param[out]  sym_rssi    Symbol RSSI in dB (most recent RSSI result)
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_ad9361_get_rssi(struct bladerf *dev,
                                      bladerf_channel ch,
                                      int32_t *pre_rssi,
                                      int32_t *sym_rssi);

/** @} (End of FN_BLADERF2_LOW_LEVEL_AD9361) */

/**
 * @defgroup FN_BLADERF2_LOW_LEVEL_ADF4002 ADF4002 Phase Detector/Freq. Synth
 *
 * Reference:
 * http://www.analog.com/media/en/technical-documentation/data-sheets/ADF4002.pdf
 *
 * @{
 */

/**
 * Fetch the lock state of the ADF4002 Phase Detector/Frequency Synthesizer
 *
 * @param       dev         Device handle
 * @param[out]  locked      True if locked, False otherwise
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_adf4002_get_locked(struct bladerf *dev, bool *locked);

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
 * Get the valid range of frequencies for the ADF4002 reference clock input
 *
 * @param       dev         Device handle
 * @param[out]  range       Reference clock frequency range
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_adf4002_get_refclk_range(struct bladerf *dev,
                                               struct bladerf_range *range);

/**
 * Get the currently-configured frequency for the ADF4002 reference clock
 * input.
 *
 * @param       dev         Device handle
 * @param[out]  frequency   Reference clock frequency
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_adf4002_get_refclk(struct bladerf *dev,
                                         uint64_t *frequency);

/**
 * Set the expected frequency for the ADF4002 reference clock input.
 *
 * @param       dev         Device handle
 * @param[in]   frequency   Reference clock frequency
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_adf4002_set_refclk(struct bladerf *dev,
                                         uint64_t frequency);

/**
 * Read value from ADF4002 Phase Detector/Frequency Synthesizer
 *
 * The `address` is interpreted as the control bits (DB1 and DB0) used to write
 * to a specific latch.
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
 * The `address` is interpreted as the control bits (DB1 and DB0) used to write
 * to a specific latch.  These bits are masked out in `val`
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
 * @defgroup FN_BLADERF2_LOW_LEVEL_POWER_SOURCE TPS2115A Auto Power Multiplexer
 *
 * @{
 */

/**
 * Power sources
 */
typedef enum {
    BLADERF_UNKNOWN,    /**< Unknown; manual observation may be required */
    BLADERF_PS_DC,      /**< DC Barrel Plug */
    BLADERF_PS_USB_VBUS /**< USB Bus */
} bladerf_power_sources;

/**
 * Get the active power source reported by the TPS2115A
 *
 * Reference: http://www.ti.com/product/TPS2115A
 *
 * @param       dev     Device handle
 * @param[out]  val     Value read from TPS2115A
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_power_source(struct bladerf *dev,
                                       bladerf_power_sources *val);

/** @} (End of FN_BLADERF2_LOW_LEVEL_POWER_SOURCE) */

/**
 * @defgroup FN_BLADERF2_LOW_LEVEL_SI53304 Si53304 Clock Buffer
 *
 * @{
 */

/**
 * @defgroup FN_BLADERF2_LOW_LEVEL_SI53304_CLOCK_SELECT Clock input selection
 *
 * @{
 */

/**
 * Available clock sources
 */
typedef enum {
    CLOCK_SELECT_VCTCXO,  /**< Use onboard VCTCXO */
    CLOCK_SELECT_EXTERNAL /**< Use external clock input */
} bladerf_clock_select;

/**
 * Get the selected clock source
 *
 * Reference: https://www.silabs.com/documents/public/data-sheets/Si53304.pdf
 *
 * @param       dev     Device handle
 * @param[out]  sel     Clock input source currently in use
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_clock_select(struct bladerf *dev,
                                       bladerf_clock_select *sel);

/**
 * Set the clock source
 *
 * Reference: https://www.silabs.com/documents/public/data-sheets/Si53304.pdf
 *
 * @param       dev     Device handle
 * @param[in]   sel     Clock input source to use
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_clock_select(struct bladerf *dev,
                                       bladerf_clock_select sel);

/** @} (End of FN_BLADERF2_LOW_LEVEL_SI53304_CLOCK_SELECT) */

/**
 * @defgroup FN_BLADERF2_LOW_LEVEL_SI53304_CLOCK_OUTPUT Clock output control
 *
 * @{
 */

/**
 * Get the current state of the clock output
 *
 * @param       dev     Device handle
 * @param[out]  state   Clock output state
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_clock_output(struct bladerf *dev, bool *state);

/**
 * Set the clock output (enable/disable)
 *
 * @param       dev     Device handle
 * @param[in]   enable  Clock output enable
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_clock_output(struct bladerf *dev, bool enable);

/** @} (End of FN_BLADERF2_LOW_LEVEL_SI53304_CLOCK_OUTPUT) */

/** @} (End of FN_BLADERF2_LOW_LEVEL_SI53304) */

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

/**
 * @defgroup FN_BLADERF2_LOW_LEVEL_RF_SWITCHING RF Switching Control
 *
 * @{
 */

/**
 * RF switch configuration structure
 */
typedef struct {
    uint32_t tx1_rfic_port; /**< Active TX1 output from RFIC */
    uint32_t tx1_spdt_port; /**< RF switch configuration for the TX1 path */
    uint32_t tx2_rfic_port; /**< Active TX2 output from RFIC */
    uint32_t tx2_spdt_port; /**< RF switch configuration for the TX2 path */
    uint32_t rx1_rfic_port; /**< Active RX1 input to RFIC */
    uint32_t rx1_spdt_port; /**< RF switch configuration for the RX1 path */
    uint32_t rx2_rfic_port; /**< Active RX2 input to RFIC */
    uint32_t rx2_spdt_port; /**< RF switch configuration for the RX2 path */
} bladerf_rf_switch_config;

/**
 * Read the current RF switching configuration from the bladeRF hardware.
 *
 * Queries both the RFIC and the RF switch and passes back a
 * bladerf_rf_switch_config stucture.
 *
 * @param       dev     Device handle
 * @param[out]  config  Switch configuration struct
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_rf_switch_config(struct bladerf *dev,
                                           bladerf_rf_switch_config *config);

/** @} (End of FN_BLADERF2_LOW_LEVEL_RF_SWITCHING) */

/** @} (End of FN_BLADERF2_LOW_LEVEL) */

/** @} (End of BLADERF2) */

#endif /* BLADERF2_H_ */
