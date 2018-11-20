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
 * In most cases, higher-level routines should be used. These routines are only
 * intended to support development and testing.
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
 * @defgroup FN_BLADERF2_LOW_LEVEL_RFIC RF Integrated Circuit
 *
 * @{
 */

/**
 * Read a RFIC register
 *
 * @param       dev         Device handle
 * @param[in]   address     Register address
 * @param[out]  val         Register value
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_rfic_register(struct bladerf *dev,
                                        uint16_t address,
                                        uint8_t *val);
/**
 * Write a RFIC register
 *
 * @param       dev         Device handle
 * @param[in]   address     Register address
 * @param[in]   val         Value to write to register
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_rfic_register(struct bladerf *dev,
                                        uint16_t address,
                                        uint8_t val);

/**
 * Read the temperature from the RFIC
 *
 * @param       dev         Device handle
 * @param[out]  val         Temperature in degrees C
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_rfic_temperature(struct bladerf *dev, float *val);

/**
 * Read the RSSI for the selected channel from the RFIC
 *
 * @note  This is a relative value, not an absolute value. If an absolute
 *        value (e.g. in dBm) is desired, a calibration should be performed
 *        against a reference signal.
 *
 * @note  See `fpga_common/src/ad936x_params.c` for the RSSI control parameters.
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
int CALL_CONV bladerf_get_rfic_rssi(struct bladerf *dev,
                                    bladerf_channel ch,
                                    int32_t *pre_rssi,
                                    int32_t *sym_rssi);

/**
 * Read the CTRL_OUT pins from the RFIC
 *
 * @note  See AD9361 Reference Manual UG-570's "Control Output" chapter for
 *        complete information about this feature.
 *
 * @see   bladerf_set_rfic_register()
 *
 * @param      dev       Device handle
 * @param[out] ctrl_out  Pointer for storing the retrieved value
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_rfic_ctrl_out(struct bladerf *dev, uint8_t *ctrl_out);

/**
 * RFIC RX FIR filter choices
 */
typedef enum {
    BLADERF_RFIC_RXFIR_BYPASS = 0, /**< No filter */
    BLADERF_RFIC_RXFIR_CUSTOM,     /**< Custom FIR filter (currently unused) */
    BLADERF_RFIC_RXFIR_DEC1,       /**< Decimate by 1 (default) */
    BLADERF_RFIC_RXFIR_DEC2,       /**< Decimate by 2 */
    BLADERF_RFIC_RXFIR_DEC4,       /**< Decimate by 4 */
} bladerf_rfic_rxfir;

/** Default RFIC RX FIR filter */
#define BLADERF_RFIC_RXFIR_DEFAULT BLADERF_RFIC_RXFIR_DEC1

/**
 * RFIC TX FIR filter choices
 */
typedef enum {
    BLADERF_RFIC_TXFIR_BYPASS = 0, /**< No filter (default) */
    BLADERF_RFIC_TXFIR_CUSTOM,     /**< Custom FIR filter (currently unused) */
    BLADERF_RFIC_TXFIR_INT1,       /**< Interpolate by 1 */
    BLADERF_RFIC_TXFIR_INT2,       /**< Interpolate by 2 */
    BLADERF_RFIC_TXFIR_INT4,       /**< Interpolate by 4 */
} bladerf_rfic_txfir;

/** Default RFIC TX FIR filter */
#define BLADERF_RFIC_TXFIR_DEFAULT BLADERF_RFIC_TXFIR_BYPASS

/**
 * Get the current status of the RX FIR filter on the RFIC.
 *
 * @param   dev     Device handle
 * @param   rxfir   RX FIR selection
 *
 * @note  See `fpga_common/src/ad936x_params.c` for FIR parameters.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_rfic_rx_fir(struct bladerf *dev,
                                      bladerf_rfic_rxfir *rxfir);

/**
 * Set the RX FIR filter on the RFIC.
 *
 * @param   dev     Device handle
 * @param   rxfir   RX FIR selection
 *
 * @note  See `fpga_common/src/ad936x_params.c` for FIR parameters.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_rfic_rx_fir(struct bladerf *dev,
                                      bladerf_rfic_rxfir rxfir);

/**
 * Get the current status of the TX FIR filter on the RFIC.
 *
 * @param   dev     Device handle
 * @param   txfir   TX FIR selection
 *
 * @note  See `fpga_common/src/ad936x_params.c` for FIR parameters.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_rfic_tx_fir(struct bladerf *dev,
                                      bladerf_rfic_txfir *txfir);

/**
 * Set the TX FIR filter on the RFIC.
 *
 * @param   dev     Device handle
 * @param   txfir   TX FIR selection
 *
 * @note  See `fpga_common/src/ad936x_params.c` for FIR parameters.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_rfic_tx_fir(struct bladerf *dev,
                                      bladerf_rfic_txfir txfir);

/** @} (End of FN_BLADERF2_LOW_LEVEL_RFIC) */

/**
 * @defgroup FN_BLADERF2_LOW_LEVEL_PLL Phase Detector/Freq. Synth Control
 *
 * Reference:
 * http://www.analog.com/media/en/technical-documentation/data-sheets/ADF4002.pdf
 *
 * @{
 */

/**
 * Fetch the lock state of the Phase Detector/Frequency Synthesizer
 *
 * @param       dev         Device handle
 * @param[out]  locked      True if locked, False otherwise
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_pll_lock_state(struct bladerf *dev, bool *locked);

/**
 * Fetch the state of the Phase Detector/Frequency Synthesizer
 *
 * @param       dev         Device handle
 * @param[out]  enabled     True if enabled, False otherwise
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_pll_enable(struct bladerf *dev, bool *enabled);

/**
 * Enable the Phase Detector/Frequency Synthesizer
 *
 * Enabling this disables the VCTCXO trimmer DAC, and vice versa.
 *
 * @param       dev         Device handle
 * @param[in]   enable      True to enable, False otherwise
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_pll_enable(struct bladerf *dev, bool enable);

/**
 * Get the valid range of frequencies for the reference clock input
 *
 * @param       dev         Device handle
 * @param[out]  range       Reference clock frequency range
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_pll_refclk_range(struct bladerf *dev,
                                           const struct bladerf_range **range);

/**
 * Get the currently-configured frequency for the reference clock
 * input.
 *
 * @param       dev         Device handle
 * @param[out]  frequency   Reference clock frequency
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_pll_refclk(struct bladerf *dev, uint64_t *frequency);

/**
 * Set the expected frequency for the reference clock input.
 *
 * @param       dev         Device handle
 * @param[in]   frequency   Reference clock frequency
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_pll_refclk(struct bladerf *dev, uint64_t frequency);

/**
 * Read value from Phase Detector/Frequency Synthesizer
 *
 * The `address` is interpreted as the control bits (DB1 and DB0) used to write
 * to a specific latch.
 *
 * @param       dev         Device handle
 * @param[in]   address     Latch address
 * @param[out]  val         Value to read from
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_pll_register(struct bladerf *dev,
                                       uint8_t address,
                                       uint32_t *val);

/**
 * Write value to Phase Detector/Frequency Synthesizer
 *
 * The `address` is interpreted as the control bits (DB1 and DB0) used to write
 * to a specific latch.  These bits are masked out in `val`
 *
 * @param       dev         Device handle
 * @param[in]   address     Latch address
 * @param[in]   val         Value to write to
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_pll_register(struct bladerf *dev,
                                       uint8_t address,
                                       uint32_t val);

/** @} (End of FN_BLADERF2_LOW_LEVEL_PLL) */

/**
 * @defgroup FN_BLADERF2_LOW_LEVEL_POWER_SOURCE Power Multiplexer
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
 * Get the active power source reported by the power multiplexer
 *
 * Reference: http://www.ti.com/product/TPS2115A
 *
 * @param       dev     Device handle
 * @param[out]  val     Value read from power multiplexer
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_power_source(struct bladerf *dev,
                                       bladerf_power_sources *val);

/** @} (End of FN_BLADERF2_LOW_LEVEL_POWER_SOURCE) */

/**
 * @defgroup FN_BLADERF2_LOW_LEVEL_CLOCK_BUFFER Clock Buffer
 *
 * @{
 */

/**
 * @defgroup FN_BLADERF2_LOW_LEVEL_CLOCK_BUFFER_SELECT Clock input selection
 *
 * @{
 */

/**
 * Available clock sources
 */
typedef enum {
    CLOCK_SELECT_ONBOARD, /**< Use onboard VCTCXO */
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

/** @} (End of FN_BLADERF2_LOW_LEVEL_CLOCK_BUFFER_SELECT) */

/**
 * @defgroup FN_BLADERF2_LOW_LEVEL_CLOCK_BUFFER_OUTPUT Clock output control
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

/** @} (End of FN_BLADERF2_LOW_LEVEL_CLOCK_BUFFER_OUTPUT) */

/** @} (End of FN_BLADERF2_LOW_LEVEL_CLOCK_BUFFER) */

/**
 * @defgroup FN_BLADERF2_LOW_LEVEL_PMIC Power Monitoring
 *
 * @{
 */

/**
 * Register identifiers for PMIC
 */
typedef enum {
    BLADERF_PMIC_CONFIGURATION, /**< Configuration register (uint16_t) */
    BLADERF_PMIC_VOLTAGE_SHUNT, /**< Shunt voltage (float) */
    BLADERF_PMIC_VOLTAGE_BUS,   /**< Bus voltage (float) */
    BLADERF_PMIC_POWER,         /**< Load power (float) */
    BLADERF_PMIC_CURRENT,       /**< Load current (float) */
    BLADERF_PMIC_CALIBRATION,   /**< Calibration (uint16_t) */
} bladerf_pmic_register;

/**
 * Read value from Power Monitor IC
 *
 * Reference: http://www.ti.com/product/INA219
 *
 * @param       dev     Device handle
 * @param[in]   reg     Register to read from
 * @param[out]  val     Value read from PMIC
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_pmic_register(struct bladerf *dev,
                                        bladerf_pmic_register reg,
                                        void *val);

/** @} (End of FN_BLADERF2_LOW_LEVEL_PMIC) */

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
