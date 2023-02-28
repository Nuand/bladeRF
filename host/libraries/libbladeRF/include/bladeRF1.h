/**
 * @file bladeRF1.h
 *
 * @brief bladeRF1-specific API
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
#ifndef BLADERF1_H_
#define BLADERF1_H_

/**
 * @defgroup BLADERF1 bladeRF1-specific API
 *
 * These functions are thread-safe.
 *
 * @{
 */

/**
 * @defgroup BLADERF1_CONSTANTS Constants (deprecated)
 *
 * \deprecated These constants are deprecated, and only apply to the original
 *             bladeRF (x40/x115). They are here for compatibility with legacy
 *             applications. Alternatives are noted below.
 *
 * @{
 */

/** Minimum sample rate, in Hz.
 *
 * \deprecated Use bladerf_get_sample_rate_range()
 */
#define BLADERF_SAMPLERATE_MIN 80000u

/**
 * Maximum recommended sample rate, in Hz.
 *
 * \deprecated Use bladerf_get_sample_rate_range()
 */
#define BLADERF_SAMPLERATE_REC_MAX 40000000u

/** Minimum bandwidth, in Hz
 *
 * \deprecated Use bladerf_get_bandwidth_range()
 */
#define BLADERF_BANDWIDTH_MIN 1500000u

/** Maximum bandwidth, in Hz
 *
 * \deprecated Use bladerf_get_bandwidth_range()
 */
#define BLADERF_BANDWIDTH_MAX 28000000u

/**
 * Minimum tunable frequency (with an XB-200 attached), in Hz.
 *
 * While this value is the lowest permitted, note that the components on the
 * XB-200 are only rated down to 50 MHz. Be aware that performance will likely
 * degrade as you tune to lower frequencies.
 *
 * \deprecated Call bladerf_expansion_attach(), then use
 *             bladerf_get_frequency_range() to get the frequency range.
 */
#define BLADERF_FREQUENCY_MIN_XB200 0u

/** Minimum tunable frequency (without an XB-200 attached), in Hz
 *
 * \deprecated Use bladerf_get_frequency_range()
 */
#define BLADERF_FREQUENCY_MIN 237500000u

/** Maximum tunable frequency, in Hz
 *
 * \deprecated Use bladerf_get_frequency_range()
 */
#define BLADERF_FREQUENCY_MAX 3800000000u

/** @} (End of BLADERF1_CONSTANTS) */

/**
 * @ingroup FN_IMAGE
 * @defgroup BLADERF_FLASH_CONSTANTS Flash image format constants
 *
 * \note These apply to both the bladeRF1 and bladeRF2, but they are still in
 *       bladeRF1.h for the time being.
 *
 * @{
 */

/** Byte address of FX3 firmware */
#define BLADERF_FLASH_ADDR_FIRMWARE 0x00000000

/** Length of firmware region of flash, in bytes */
#define BLADERF_FLASH_BYTE_LEN_FIRMWARE 0x00030000

/** Byte address of calibration data region */
#define BLADERF_FLASH_ADDR_CAL 0x00030000

/** Length of calibration data, in bytes */
#define BLADERF_FLASH_BYTE_LEN_CAL 0x100

/**
 * Byte address of of the autoloaded FPGA and associated metadata.
 *
 * The first page is allocated for metadata, and the FPGA bitstream resides
 * in the following pages.
 */
#define BLADERF_FLASH_ADDR_FPGA 0x00040000

/** @} (End of BLADERF_FLASH_CONSTANTS) */

/**
 * @defgroup FN_BLADERF1_GAIN Gain stages (deprecated)
 *
 * These functions provide control over the device's RX and TX gain stages.
 *
 * \deprecated Use bladerf_get_gain_range(), bladerf_set_gain(), and
 *             bladerf_get_gain() to control total system gain. For direct
 *             control of individual gain stages, use bladerf_get_gain_stages(),
 *             bladerf_get_gain_stage_range(), bladerf_set_gain_stage(), and
 *             bladerf_get_gain_stage().
 *
 * @{
 */

/**
 * In general, the gains should be incremented in the following order (and
 * decremented in the reverse order).
 *
 * <b>TX:</b> `TXVGA1`, `TXVGA2`
 *
 * <b>RX:</b> `LNA`, `RXVGA`, `RXVGA2`
 *
 */

/** Minimum RXVGA1 gain, in dB
 *
 * \deprecated Use bladerf_get_gain_stage_range()
 */
#define BLADERF_RXVGA1_GAIN_MIN 5

/** Maximum RXVGA1 gain, in dB
 *
 * \deprecated Use bladerf_get_gain_stage_range()
 */
#define BLADERF_RXVGA1_GAIN_MAX 30

/** Minimum RXVGA2 gain, in dB
 *
 * \deprecated Use bladerf_get_gain_stage_range()
 */
#define BLADERF_RXVGA2_GAIN_MIN 0

/** Maximum RXVGA2 gain, in dB
 *
 * \deprecated Use bladerf_get_gain_stage_range()
 */
#define BLADERF_RXVGA2_GAIN_MAX 30

/** Minimum TXVGA1 gain, in dB
 *
 * \deprecated Use bladerf_get_gain_stage_range()
 */
#define BLADERF_TXVGA1_GAIN_MIN (-35)

/** Maximum TXVGA1 gain, in dB
 *
 * \deprecated Use bladerf_get_gain_stage_range()
 */
#define BLADERF_TXVGA1_GAIN_MAX (-4)

/** Minimum TXVGA2 gain, in dB
 *
 * \deprecated Use bladerf_get_gain_stage_range()
 */
#define BLADERF_TXVGA2_GAIN_MIN 0

/** Maximum TXVGA2 gain, in dB
 *
 * \deprecated Use bladerf_get_gain_stage_range()
 */
#define BLADERF_TXVGA2_GAIN_MAX 25

/**
 * LNA gain options
 *
 * \deprecated Use bladerf_get_gain_stage_range()
 */
typedef enum {
    BLADERF_LNA_GAIN_UNKNOWN, /**< Invalid LNA gain */
    BLADERF_LNA_GAIN_BYPASS,  /**< LNA bypassed - 0dB gain */
    BLADERF_LNA_GAIN_MID,     /**< LNA Mid Gain (MAX-6dB) */
    BLADERF_LNA_GAIN_MAX      /**< LNA Max Gain */
} bladerf_lna_gain;

/**
 * Gain in dB of the LNA at mid setting
 *
 * \deprecated Use bladerf_get_gain_stage_range()
 */
#define BLADERF_LNA_GAIN_MID_DB 3

/**
 * Gain in db of the LNA at max setting
 *
 * \deprecated Use bladerf_get_gain_stage_range()
 */
#define BLADERF_LNA_GAIN_MAX_DB 6

/**
 * Set the PA gain in dB
 *
 * \deprecated Use either bladerf_set_gain() or bladerf_set_gain_stage().
 *
 * Values outside the range of
 * [ \ref BLADERF_TXVGA2_GAIN_MIN, \ref BLADERF_TXVGA2_GAIN_MAX ]
 * will be clamped.
 *
 * @param       dev         Device handle
 * @param[in]   gain        Desired gain
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_txvga2(struct bladerf *dev, int gain);

/**
 * Get the PA gain in dB
 *
 * \deprecated Use either bladerf_get_gain() or bladerf_get_gain_stage().
 *
 * @param       dev         Device handle
 * @param[out]  gain        Pointer to returned gain
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT int CALL_CONV bladerf_get_txvga2(struct bladerf *dev, int *gain);

/**
 * Set the post-LPF gain in dB
 *
 * \deprecated Use either bladerf_set_gain() or bladerf_set_gain_stage().
 *
 * Values outside the range of
 * [ \ref BLADERF_TXVGA1_GAIN_MIN, \ref BLADERF_TXVGA1_GAIN_MAX ]
 * will be clamped.
 *
 * @param       dev         Device handle
 * @param[in]   gain        Desired gain
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_txvga1(struct bladerf *dev, int gain);

/**
 * Get the post-LPF gain in dB
 *
 * \deprecated Use either bladerf_get_gain() or bladerf_get_gain_stage().
 *
 * @param       dev         Device handle
 * @param[out]  gain        Pointer to returned gain
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_txvga1(struct bladerf *dev, int *gain);

/**
 * Set the LNA gain
 *
 * \deprecated Use either bladerf_set_gain() or bladerf_set_gain_stage().
 *
 * @param       dev         Device handle
 * @param[in]   gain        Desired gain level
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_lna_gain(struct bladerf *dev, bladerf_lna_gain gain);

/**
 * Get the LNA gain
 *
 * \deprecated Use either bladerf_get_gain() or bladerf_get_gain_stage().
 *
 * @param       dev         Device handle
 * @param[out]  gain        Pointer to the set gain level
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_lna_gain(struct bladerf *dev, bladerf_lna_gain *gain);

/**
 * Set the pre-LPF VGA gain
 *
 * \deprecated Use either bladerf_set_gain() or bladerf_set_gain_stage().
 *
 * Values outside the range of
 * [ \ref BLADERF_RXVGA1_GAIN_MIN, \ref BLADERF_RXVGA1_GAIN_MAX ]
 * will be clamped.
 *
 * @param       dev         Device handle
 * @param[in]   gain        Desired gain
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_rxvga1(struct bladerf *dev, int gain);

/**
 * Get the pre-LPF VGA gain
 *
 * \deprecated Use either bladerf_get_gain() or bladerf_get_gain_stage().
 *
 * @param       dev         Device handle
 * @param[out]  gain        Pointer to the set gain level
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_rxvga1(struct bladerf *dev, int *gain);

/**
 * Set the post-LPF VGA gain
 *
 * \deprecated Use either bladerf_set_gain() or bladerf_set_gain_stage().
 *
 * Values outside the range of
 * [ \ref BLADERF_RXVGA2_GAIN_MIN, \ref BLADERF_RXVGA2_GAIN_MAX ]
 * will be clamped.
 *
 * @param       dev         Device handle
 * @param[in]   gain        Desired gain
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_rxvga2(struct bladerf *dev, int gain);

/**
 * Get the post-LPF VGA gain
 *
 * \deprecated Use either bladerf_get_gain() or bladerf_get_gain_stage().
 *
 * @param       dev         Device handle
 * @param[out]  gain        Pointer to the set gain level
 */
API_EXPORT
int CALL_CONV bladerf_get_rxvga2(struct bladerf *dev, int *gain);

/** @} (End of FN_BLADERF1_GAIN) */

/**
 * @defgroup FN_BLADERF1_SAMPLING_MUX Sampling Mux
 *
 * These functions provide control over internal and direct sampling modes of
 * the LMS6002D.
 *
 * These functions are thread-safe.
 *
 * @{
 */

/**
 * Sampling connection
 */
typedef enum {
    BLADERF_SAMPLING_UNKNOWN,  /**< Unable to determine connection type */
    BLADERF_SAMPLING_INTERNAL, /**< Sample from RX/TX connector */
    BLADERF_SAMPLING_EXTERNAL  /**< Sample from J60 or J61 */
} bladerf_sampling;

/**
 * Configure the sampling of the LMS6002D to be either internal or external.
 *
 * Internal sampling will read from the RXVGA2 driver internal to the chip.
 * External sampling will connect the ADC inputs to the external inputs for
 * direct sampling.
 *
 * @param       dev         Device handle
 * @param[in]   sampling    Sampling connection
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_sampling(struct bladerf *dev,
                                   bladerf_sampling sampling);

/**
 * Read the device's current state of RXVGA2 and ADC pin connection
 * to figure out which sampling mode it is currently configured in.
 *
 * @param       dev         Device handle
 * @param[out]  sampling    Sampling connection
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_sampling(struct bladerf *dev,
                                   bladerf_sampling *sampling);


/** @} (End of FN_BLADERF1_SAMPLING_MUX) */

/**
 * @defgroup FN_BLADERF1_LPF_BYPASS LPF Bypass
 *
 * These functions provide control over the LPF bypass mode of the LMS6002D.
 *
 * These functions are thread-safe.
 *
 * @{
 */

/**
 * Low-Pass Filter (LPF) mode
 */
typedef enum {
    BLADERF_LPF_NORMAL,   /**< LPF connected and enabled */
    BLADERF_LPF_BYPASSED, /**< LPF bypassed */
    BLADERF_LPF_DISABLED  /**< LPF disabled */
} bladerf_lpf_mode;

/**
 * Set the LMS LPF mode to bypass or disable it
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[in]   mode        Mode to be set
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_lpf_mode(struct bladerf *dev,
                                   bladerf_channel ch,
                                   bladerf_lpf_mode mode);

/**
 * Get the current mode of the LMS LPF
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[out]  mode        Current mode of the LPF
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_lpf_mode(struct bladerf *dev,
                                   bladerf_channel ch,
                                   bladerf_lpf_mode *mode);

/** @} (End of FN_BLADERF1_LPF_BYPASS) */

/**
 * @defgroup FN_SMB_CLOCK SMB clock port control
 *
 * The SMB clock port (J62) may be used to synchronize sampling on multiple
 * devices, or to generate an arbitrary clock output for a different device.
 *
 * For MIMO configurations, one device is the clock "master" and outputs its
 * 38.4 MHz reference on this port.  The clock "slave" devices configure the SMB
 * port as an input and expect to see this 38.4 MHz reference on this port. This
 * implies that the "master" must be configured first.
 *
 * Alternatively, this port may be used to generate an arbitrary clock signal
 * for use with other devices via the bladerf_set_smb_frequency() and
 * bladerf_set_rational_smb_frequency() functions.
 *
 * @warning <b>Do not</b> use these functions when operating an expansion board.
 * A different clock configuration is required for the XB devices which cannot
 * be used simultaneously with the SMB clock port.
 *
 * These functions are thread-safe.
 *
 * @{
 */

/**
 * Maximum output frequency on SMB connector, if no expansion board attached.
 */
#define BLADERF_SMB_FREQUENCY_MAX 200000000u

/**
 * Minimum output frequency on SMB connector, if no expansion board attached.
 */
#define BLADERF_SMB_FREQUENCY_MIN ((38400000u * 66u) / (32 * 567))


/**
 * SMB clock port mode of operation
 */
typedef enum {
    BLADERF_SMB_MODE_INVALID = -1, /**< Invalid selection */

    BLADERF_SMB_MODE_DISABLED, /**< Not in use. Device operates from its onboard
                                * clock and does not use J62.
                                */

    BLADERF_SMB_MODE_OUTPUT, /**< Device outputs a 38.4 MHz reference clock on
                              * J62. This may be used to drive another device
                              * that is configured with
                              * ::BLADERF_SMB_MODE_INPUT.
                              */

    BLADERF_SMB_MODE_INPUT, /**< Device configures J62 as an input and expects a
                             * 38.4 MHz reference to be available when this
                             * setting is applied.
                             */

    BLADERF_SMB_MODE_UNAVAILBLE, /**< SMB port is unavailable for use due to the
                                  * underlying clock being used elsewhere (e.g.,
                                  * for an expansion board).
                                  */

} bladerf_smb_mode;

/**
 * Set the current mode of operation of the SMB clock port
 *
 * In a MIMO configuration, one "master" device should first be configured to
 * output its reference clock to the slave devices via
 * `bladerf_set_smb_mode(dev, BLADERF_SMB_MODE_OUTPUT)`.
 *
 * Next, all "slave" devices should be configured to use the reference clock
 * provided on the SMB clock port (instead of using their on-board reference)
 * via `bladerf_set_smb_mode(dev, BLADERF_SMB_MODE_INPUT)`.
 *
 * @param       dev         Device handle
 * @param[in]   mode        Desired mode
 *
 * @return 0 on success, or a value from \ref RETCODES list on failure.
 */
API_EXPORT
int CALL_CONV bladerf_set_smb_mode(struct bladerf *dev, bladerf_smb_mode mode);

/**
 * Get the current mode of operation of the SMB clock port
 *
 * @param       dev         Device handle
 * @param[out]  mode        Desired mode
 *
 * @return 0 on success, or a value from \ref RETCODES list on failure.
 */
API_EXPORT
int CALL_CONV bladerf_get_smb_mode(struct bladerf *dev, bladerf_smb_mode *mode);

/**
 * Set the SMB clock port frequency in rational Hz
 *
 * @param       dev         Device handle
 * @param[in]   rate        Rational frequency
 * @param[out]  actual      If non-NULL, this is written with the actual
 *
 * The frequency must be between \ref BLADERF_SMB_FREQUENCY_MIN and
 * \ref BLADERF_SMB_FREQUENCY_MAX.
 *
 * This function inherently configures the SMB clock port as an output. Do not
 * call bladerf_set_smb_mode() with ::BLADERF_SMB_MODE_OUTPUT, as this will
 * reset the output frequency to the 38.4 MHz reference.
 *
 * @warning This clock should not be set if an expansion board is connected.
 *
 * @return 0 on success,
 *         BLADERF_ERR_INVAL for an invalid frequency,
 *         or a value from \ref RETCODES list on failure.
 */
API_EXPORT
int CALL_CONV
    bladerf_set_rational_smb_frequency(struct bladerf *dev,
                                       struct bladerf_rational_rate *rate,
                                       struct bladerf_rational_rate *actual);

/**
 * Set the SMB connector output frequency in Hz.
 * Use bladerf_set_rational_smb_frequency() for more arbitrary values.
 *
 * @param       dev         Device handle
 * @param[in]   rate        Frequency
 * @param[out]  actual      If non-NULL. this is written with the actual
 *                          frequency achieved.
 *
 * This function inherently configures the SMB clock port as an output. Do not
 * call bladerf_set_smb_mode() with ::BLADERF_SMB_MODE_OUTPUT, as this will
 * reset the output frequency to the 38.4 MHz reference.
 *
 * The frequency must be between \ref BLADERF_SMB_FREQUENCY_MIN and
 * \ref BLADERF_SMB_FREQUENCY_MAX.
 *
 * @warning This clock should not be set if an expansion board is connected.
 *
 * @return 0 on success,
 *         BLADERF_ERR_INVAL for an invalid frequency,
 *         or a value from \ref RETCODES list on other failures
 */
API_EXPORT
int CALL_CONV bladerf_set_smb_frequency(struct bladerf *dev,
                                        uint32_t rate,
                                        uint32_t *actual);

/**
 * Read the SMB connector output frequency in rational Hz
 *
 * @param       dev         Device handle
 * @param[out]  rate        Pointer to returned rational frequency
 *
 * @return 0 on success, value from \ref RETCODES list upon failure
 */
API_EXPORT
int CALL_CONV bladerf_get_rational_smb_frequency(
    struct bladerf *dev, struct bladerf_rational_rate *rate);

/**
 * Read the SMB connector output frequency in Hz
 *
 * @param       dev         Device handle
 * @param[out]  rate        Pointer to returned frequency
 *
 * @return 0 on success, value from \ref RETCODES list upon failure
 */
API_EXPORT
int CALL_CONV bladerf_get_smb_frequency(struct bladerf *dev,
                                        unsigned int *rate);

/** @} (End of FN_SMB_CLOCK) */

/**
 * @defgroup FN_EXP_IO Expansion I/O
 *
 * These definitions and functions provide high-level functionality for
 * manipulating pins on the bladeRF1 U74 Expansion Header, and the associated
 * mappings on expansion boards.
 *
 * These functions are thread-safe.
 *
 * @{
 */

/** Expansion pin GPIO number to bitmask */
#define BLADERF_XB_GPIO(n) (1 << (n - 1))

/** Specifies a pin to be an output */
#define BLADERF_XB_DIR_OUTPUT(pin) (pin)

/** Specifies a pin to be an input */
#define BLADERF_XB_DIR_INPUT(pin) (0)

/** Pin bitmask for Expansion GPIO 1 (U74 pin 11) */
#define BLADERF_XB_GPIO_01 BLADERF_XB_GPIO(1)

/** Pin bitmask for Expansion GPIO 2 (U74 pin 13) */
#define BLADERF_XB_GPIO_02 BLADERF_XB_GPIO(2)

/** Pin bitmask for Expansion GPIO 3 (U74 pin 17) */
#define BLADERF_XB_GPIO_03 BLADERF_XB_GPIO(3)

/** Pin bitmask for Expansion GPIO 4 (U74 pin 19) */
#define BLADERF_XB_GPIO_04 BLADERF_XB_GPIO(4)

/** Pin bitmask for Expansion GPIO 5 (U74 pin 23) */
#define BLADERF_XB_GPIO_05 BLADERF_XB_GPIO(5)

/** Pin bitmask for Expansion GPIO 6 (U74 pin 25) */
#define BLADERF_XB_GPIO_06 BLADERF_XB_GPIO(6)

/** Pin bitmask for Expansion GPIO 7 (U74 pin 29) */
#define BLADERF_XB_GPIO_07 BLADERF_XB_GPIO(7)

/** Pin bitmask for Expansion GPIO 8 (U74 pin 31) */
#define BLADERF_XB_GPIO_08 BLADERF_XB_GPIO(8)

/** Pin bitmask for Expansion GPIO 9 (U74 pin 35) */
#define BLADERF_XB_GPIO_09 BLADERF_XB_GPIO(9)

/** Pin bitmask for Expansion GPIO 10 (U74 pin 37) */
#define BLADERF_XB_GPIO_10 BLADERF_XB_GPIO(10)

/** Pin bitmask for Expansion GPIO 11 (U74 pin 41) */
#define BLADERF_XB_GPIO_11 BLADERF_XB_GPIO(11)

/** Pin bitmask for Expansion GPIO 12 (U74 pin 43) */
#define BLADERF_XB_GPIO_12 BLADERF_XB_GPIO(12)

/** Pin bitmask for Expansion GPIO 13 (U74 pin 47) */
#define BLADERF_XB_GPIO_13 BLADERF_XB_GPIO(13)

/** Pin bitmask for Expansion GPIO 14 (U74 pin 49) */
#define BLADERF_XB_GPIO_14 BLADERF_XB_GPIO(14)

/** Pin bitmask for Expansion GPIO 15 (U74 pin 53) */
#define BLADERF_XB_GPIO_15 BLADERF_XB_GPIO(15)

/** Pin bitmask for Expansion GPIO 16 (U74 pin 55) */
#define BLADERF_XB_GPIO_16 BLADERF_XB_GPIO(16)

/** Pin bitmask for Expansion GPIO 17 (U74 pin 12) */
#define BLADERF_XB_GPIO_17 BLADERF_XB_GPIO(17)

/** Pin bitmask for Expansion GPIO 18 (U74 pin 14) */
#define BLADERF_XB_GPIO_18 BLADERF_XB_GPIO(18)

/** Pin bitmask for Expansion GPIO 19 (U74 pin 18) */
#define BLADERF_XB_GPIO_19 BLADERF_XB_GPIO(19)

/** Pin bitmask for Expansion GPIO 20 (U74 pin 20) */
#define BLADERF_XB_GPIO_20 BLADERF_XB_GPIO(20)

/** Pin bitmask for Expansion GPIO 21 (U74 pin 24) */
#define BLADERF_XB_GPIO_21 BLADERF_XB_GPIO(21)

/** Pin bitmask for Expansion GPIO 22 (U74 pin 26) */
#define BLADERF_XB_GPIO_22 BLADERF_XB_GPIO(22)

/** Pin bitmask for Expansion GPIO 23 (U74 pin 30) */
#define BLADERF_XB_GPIO_23 BLADERF_XB_GPIO(23)

/** Pin bitmask for Expansion GPIO 24 (U74 pin 32) */
#define BLADERF_XB_GPIO_24 BLADERF_XB_GPIO(24)

/** Pin bitmask for Expansion GPIO 25 (U74 pin 36) */
#define BLADERF_XB_GPIO_25 BLADERF_XB_GPIO(25)

/** Pin bitmask for Expansion GPIO 26 (U74 pin 38) */
#define BLADERF_XB_GPIO_26 BLADERF_XB_GPIO(26)

/** Pin bitmask for Expansion GPIO 27 (U74 pin 42) */
#define BLADERF_XB_GPIO_27 BLADERF_XB_GPIO(27)

/** Pin bitmask for Expansion GPIO 28 (U74 pin 44) */
#define BLADERF_XB_GPIO_28 BLADERF_XB_GPIO(28)

/** Pin bitmask for Expansion GPIO 29 (U74 pin 48) */
#define BLADERF_XB_GPIO_29 BLADERF_XB_GPIO(29)

/** Pin bitmask for Expansion GPIO 30 (U74 pin 50) */
#define BLADERF_XB_GPIO_30 BLADERF_XB_GPIO(30)

/** Pin bitmask for Expansion GPIO 31 (U74 pin 54) */
#define BLADERF_XB_GPIO_31 BLADERF_XB_GPIO(31)

/** Pin bitmask for Expansion GPIO 32 (U74 pin 56) */
#define BLADERF_XB_GPIO_32 BLADERF_XB_GPIO(32)


/** Bitmask for XB-200 header J7, pin 1 */
#define BLADERF_XB200_PIN_J7_1 BLADERF_XB_GPIO_10

/** Bitmask for XB-200 header J7, pin 2 */
#define BLADERF_XB200_PIN_J7_2 BLADERF_XB_GPIO_11

/** Bitmask for XB-200 header J7, pin 5 */
#define BLADERF_XB200_PIN_J7_5 BLADERF_XB_GPIO_08

/** Bitmask for XB-200 header J7, pin 6 */
#define BLADERF_XB200_PIN_J7_6 BLADERF_XB_GPIO_09

/** Bitmask for XB-200 header J13, pin 1 */
#define BLADERF_XB200_PIN_J13_1 BLADERF_XB_GPIO_17

/** Bitmask for XB-200 header J13, pin 2 */
#define BLADERF_XB200_PIN_J13_2 BLADERF_XB_GPIO_18

/* XB-200 J13 Pin 6 is actually reserved for SPI */

/** Bitmask for XB-200 header J16, pin 1 */
#define BLADERF_XB200_PIN_J16_1 BLADERF_XB_GPIO_31

/** Bitmask for XB-200 header J16, pin 2 */
#define BLADERF_XB200_PIN_J16_2 BLADERF_XB_GPIO_32

/** Bitmask for XB-200 header J16, pin 3 */
#define BLADERF_XB200_PIN_J16_3 BLADERF_XB_GPIO_19

/** Bitmask for XB-200 header J16, pin 4 */
#define BLADERF_XB200_PIN_J16_4 BLADERF_XB_GPIO_20

/** Bitmask for XB-200 header J16, pin 5 */
#define BLADERF_XB200_PIN_J16_5 BLADERF_XB_GPIO_21

/** Bitmask for XB-200 header J16, pin 6 */
#define BLADERF_XB200_PIN_J16_6 BLADERF_XB_GPIO_24

/** Bitmask for XB-100 header J2, pin 3 */
#define BLADERF_XB100_PIN_J2_3 BLADERF_XB_GPIO_07

/** Bitmask for XB-100 header J2, pin 4 */
#define BLADERF_XB100_PIN_J2_4 BLADERF_XB_GPIO_08

/** Bitmask for XB-100 header J3, pin 3 */
#define BLADERF_XB100_PIN_J3_3 BLADERF_XB_GPIO_09

/** Bitmask for XB-100 header J3, pin 4 */
#define BLADERF_XB100_PIN_J3_4 BLADERF_XB_GPIO_10

/** Bitmask for XB-100 header J4, pin 3 */
#define BLADERF_XB100_PIN_J4_3 BLADERF_XB_GPIO_11

/** Bitmask for XB-100 header J4, pin 4 */
#define BLADERF_XB100_PIN_J4_4 BLADERF_XB_GPIO_12

/** Bitmask for XB-100 header J5, pin 3 */
#define BLADERF_XB100_PIN_J5_3 BLADERF_XB_GPIO_13

/** Bitmask for XB-100 header J5, pin 4 */
#define BLADERF_XB100_PIN_J5_4 BLADERF_XB_GPIO_14

/** Bitmask for XB-100 header J11, pin 2 */
#define BLADERF_XB100_PIN_J11_2 BLADERF_XB_GPIO_05

/** Bitmask for XB-100 header J11, pin 3 */
#define BLADERF_XB100_PIN_J11_3 BLADERF_XB_GPIO_04

/** Bitmask for XB-100 header J11, pin 4 */
#define BLADERF_XB100_PIN_J11_4 BLADERF_XB_GPIO_03

/** Bitmask for XB-100 header J11, pin 5 */
#define BLADERF_XB100_PIN_J11_5 BLADERF_XB_GPIO_06

/** Bitmask for XB-100 header J12, pin 2 */
#define BLADERF_XB100_PIN_J12_2 BLADERF_XB_GPIO_01

/*  XB-100 header J12, pins 3 and 4 are reserved for SPI */

/** Bitmask for XB-100 header J12, pin 5 */
#define BLADERF_XB100_PIN_J12_5 BLADERF_XB_GPIO_02

/** Bitmask for XB-100 LED_D1 (blue) */
#define BLADERF_XB100_LED_D1 BLADERF_XB_GPIO_24

/** Bitmask for XB-100 LED_D2 (blue) */
#define BLADERF_XB100_LED_D2 BLADERF_XB_GPIO_32

/** Bitmask for XB-100 LED_D3 (blue) */
#define BLADERF_XB100_LED_D3 BLADERF_XB_GPIO_30

/** Bitmask for XB-100 LED_D4 (red) */
#define BLADERF_XB100_LED_D4 BLADERF_XB_GPIO_28

/** Bitmask for XB-100 LED_D5 (red) */
#define BLADERF_XB100_LED_D5 BLADERF_XB_GPIO_23

/** Bitmask for XB-100 LED_D6 (red) */
#define BLADERF_XB100_LED_D6 BLADERF_XB_GPIO_25

/** Bitmask for XB-100 LED_D7 (green) */
#define BLADERF_XB100_LED_D7 BLADERF_XB_GPIO_31

/** Bitmask for XB-100 LED_D8 (green) */
#define BLADERF_XB100_LED_D8 BLADERF_XB_GPIO_29

/** Bitmask for XB-100 tricolor LED, red cathode */
#define BLADERF_XB100_TLED_RED BLADERF_XB_GPIO_22

/** Bitmask for XB-100 tricolor LED, green cathode */
#define BLADERF_XB100_TLED_GREEN BLADERF_XB_GPIO_21

/** Bitmask for XB-100 tricolor LED, blue cathode */
#define BLADERF_XB100_TLED_BLUE BLADERF_XB_GPIO_20

/** Bitmask for XB-100 DIP switch 1 */
#define BLADERF_XB100_DIP_SW1 BLADERF_XB_GPIO_27

/** Bitmask for XB-100 DIP switch 2 */
#define BLADERF_XB100_DIP_SW2 BLADERF_XB_GPIO_26

/** Bitmask for XB-100 DIP switch 3 */
#define BLADERF_XB100_DIP_SW3 BLADERF_XB_GPIO_16

/** Bitmask for XB-100 DIP switch 4 */
#define BLADERF_XB100_DIP_SW4 BLADERF_XB_GPIO_15

/** Bitmask for XB-100 button J6 */
#define BLADERF_XB100_BTN_J6 BLADERF_XB_GPIO_19

/** Bitmask for XB-100 button J7 */
#define BLADERF_XB100_BTN_J7 BLADERF_XB_GPIO_18

/** Bitmask for XB-100 button J8 */
#define BLADERF_XB100_BTN_J8 BLADERF_XB_GPIO_17

/* XB-100 buttons J9 and J10 are not mapped to the GPIO register,
 * but instead to reserved SPI pins. FPGA modifications are needed to
 * use these. */

/**
 * Read the state of expansion GPIO values
 *
 * @param       dev         Device handle
 * @param[out]  val         Value of GPIO pins
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_expansion_gpio_read(struct bladerf *dev, uint32_t *val);

/**
 * Write expansion GPIO pins.
 *
 * Callers should be sure to perform a read-modify-write sequence to avoid
 * accidentally clearing other GPIO bits that may be set by the library
 * internally.
 *
 * Consider using bladerf_expansion_gpio_masked_write() instead.
 *
 * @param       dev     Device handle
 * @param[in]   val     Data to write to GPIO pins
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_expansion_gpio_write(struct bladerf *dev, uint32_t val);

/**
 * Write values to the specified GPIO pins
 *
 * This function alleviates the need for the caller to perform a
 * read-modify-write sequence. The supplied mask is used by the FPGA to perform
 * the required RMW operation.
 *
 * @param       dev     Device handle
 * @param[in]   mask    Mask of pins to write
 * @param[in]   value   Value to write.
 *
 * For example, to set XB200 pins J16-1 and J16-2, and clear J16-4 and J16-5:
 *
 * @code{.c}
 *  const uint32_t pins_to_write =
 *      BLADERF_XB200_PIN_J16_1 |
 *      BLADERF_XB200_PIN_J16_2 |
 *      BLADERF_XB200_PIN_J16_3 |
 *      BLADERF_XB200_PIN_J16_4;
 *
 *  const uint32_t values_to_write =
 *      BLADERF_XB200_PIN_J16_1 |
 *      BLADERF_XB200_PIN_J16_2;
 *
 *  int status = bladerf_expansion_gpio_masked_write(dev,
 *                                                   pins_to_write,
 *                                                   values_to_write);
 * @endcode
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_expansion_gpio_masked_write(struct bladerf *dev,
                                                  uint32_t mask,
                                                  uint32_t value);

/**
 * Read the expansion GPIO direction register
 *
 * @param       dev         Device handle
 * @param[out]  outputs     Pins configured as outputs will be set to '1'.
 *                          Pins configured as inputs will be set to '0'.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_expansion_gpio_dir_read(struct bladerf *dev,
                                              uint32_t *outputs);

/**
 * Write to the expansion GPIO direction register.
 *
 * Callers should be sure to perform a read-modify-write sequence to avoid
 * accidentally clearing other GPIO bits that may be set by the library
 * internally.
 *
 * Consider using bladerf_expansion_gpio_dir_masked_write() instead.
 *
 * @param       dev         Device handle
 * @param[in]   outputs     Pins set to '1' will be configured as outputs.
 *                          Pins set to '0' will be configured as inputs.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_expansion_gpio_dir_write(struct bladerf *dev,
                                               uint32_t outputs);

/**
 * Configure the direction of the specified expansion GPIO pins
 *
 * This function alleviates the need for the caller to perform a
 * read-modify-write sequence. The supplied mask is used by the FPGA to perform
 * the required RMW operation.
 *
 * @param       dev         Device handle
 * @param[in]   mask        Bitmask of pins to configure
 * @param[in]   outputs     Pins set to '1' will be configured as outputs.
 *                          Pins set to '0' will be configured as inputs.
 *
 * For example, to configure XB200 pins J16-1 and J16-2 and pins J16-4 and J16-5
 * as inputs:
 *
 * @code{.c}
 *  const uint32_t pins_to_config =
 *      BLADERF_XB200_PIN_J16_1 |
 *      BLADERF_XB200_PIN_J16_2 |
 *      BLADERF_XB200_PIN_J16_3 |
 *      BLADERF_XB200_PIN_J16_4;
 *
 *  const uint32_t output_pins =
 *      BLADERF_XB200_PIN_J16_1 |
 *      BLADERF_XB200_PIN_J16_2;
 *
 *  int status = bladerf_expansion_gpio_masked_write(dev,
 *                                                   pins_to_config,
 *                                                   output_pins);
 * @endcode
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_expansion_gpio_dir_masked_write(struct bladerf *dev,
                                                      uint32_t mask,
                                                      uint32_t outputs);

/** @} (End of FN_EXP_IO) */

/**
 * @defgroup    FN_BLADERF1_XB   Expansion board support
 *
 * This group of functions provides the ability to control and configure
 * expansion boards such as the XB-100, XB-200, and XB-300.
 *
 * These functions are thread-safe.
 *
 * @{
 */

/**
 * XB-200 filter selection options
 */
typedef enum {
    /** 50-54 MHz (6 meter band) filterbank */
    BLADERF_XB200_50M = 0,

    /** 144-148 MHz (2 meter band) filterbank */
    BLADERF_XB200_144M,

    /**
     * 222-225 MHz (1.25 meter band) filterbank.
     *
     * Note that this filter option is technically wider, covering 206-235 MHz.
     */
    BLADERF_XB200_222M,

    /**
     * This option enables the RX/TX channel's custom filter bank path across
     * the associated FILT and FILT-ANT SMA connectors on the XB-200 board.
     *
     * For reception, it is often possible to simply connect the RXFILT and
     * RXFILT-ANT connectors with an SMA cable (effectively, "no filter"). This
     * allows for reception of signals outside of the frequency range of the
     * on-board filters, with some potential trade-off in signal quality.
     *
     * For transmission, <b>always</b> use an appropriate filter on the custom
     * filter path to avoid spurious emissions.
     *
     */
    BLADERF_XB200_CUSTOM,

    /**
     * When this option is selected, the other filter options are automatically
     * selected depending on the RX or TX channel's current frequency, based
     * upon the 1dB points of the on-board filters.  For frequencies outside
     * the range of the on-board filters, the custom path is selected.
     */
    BLADERF_XB200_AUTO_1DB,

    /**
     * When this option is selected, the other filter options are automatically
     * selected depending on the RX or TX channel's current frequency, based
     * upon the 3dB points of the on-board filters. For frequencies outside the
     * range of the on-board filters, the custom path is selected.
     */
    BLADERF_XB200_AUTO_3DB
} bladerf_xb200_filter;

/**
 * XB-200 signal paths
 */
typedef enum {
    BLADERF_XB200_BYPASS = 0, /**< Bypass the XB-200 mixer */
    BLADERF_XB200_MIX         /**< Pass signals through the XB-200 mixer */
} bladerf_xb200_path;

/**
 * XB-300 TRX setting
 */
typedef enum {
    BLADERF_XB300_TRX_INVAL = -1, /**< Invalid TRX selection */
    BLADERF_XB300_TRX_TX    = 0,  /**< TRX antenna operates as TX */
    BLADERF_XB300_TRX_RX,         /**< TRX antenna operates as RX */
    BLADERF_XB300_TRX_UNSET       /**< TRX antenna unset */
} bladerf_xb300_trx;

/**
 * XB-300 Amplifier selection
 */
typedef enum {
    BLADERF_XB300_AMP_INVAL = -1, /**< Invalid amplifier selection */
    BLADERF_XB300_AMP_PA    = 0,  /**< TX Power amplifier */
    BLADERF_XB300_AMP_LNA,        /**< RX LNA */
    BLADERF_XB300_AMP_PA_AUX      /**< Auxillary Power amplifier */
} bladerf_xb300_amplifier;

/**
 * Set XB-200 filterbank
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[in]   filter      XB200 filterbank
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_xb200_set_filterbank(struct bladerf *dev,
                                           bladerf_channel ch,
                                           bladerf_xb200_filter filter);

/**
 * Get current XB-200 filterbank
 *
 * @param        dev        Device handle
 * @param[in]    ch         Channel
 * @param[out]   filter     Pointer to filterbank, only updated if return
 *                          value is 0.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_xb200_get_filterbank(struct bladerf *dev,
                                           bladerf_channel ch,
                                           bladerf_xb200_filter *filter);

/**
 * Set XB-200 signal path
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[in]   path        Desired XB-200 signal path
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_xb200_set_path(struct bladerf *dev,
                                     bladerf_channel ch,
                                     bladerf_xb200_path path);

/**
 * Get current XB-200 signal path
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[out]  path        Pointer to XB200 signal path
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_xb200_get_path(struct bladerf *dev,
                                     bladerf_channel ch,
                                     bladerf_xb200_path *path);

/**
 * Configure the XB-300 TRX path
 *
 * @param       dev         Device handle
 * @param[in]   trx         Desired XB-300 TRX setting
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
API_EXPORT
int CALL_CONV bladerf_xb300_set_trx(struct bladerf *dev, bladerf_xb300_trx trx);

/**
 * Get the current XB-300 signal path
 *
 * @param       dev         Device handle
 * @param[out]  trx         XB300 TRX antenna setting
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_xb300_get_trx(struct bladerf *dev,
                                    bladerf_xb300_trx *trx);

/**
 * Enable or disable selected XB-300 amplifier
 *
 * @param       dev         Device handle
 * @param[in]   amp         XB-300 amplifier
 * @param[in]   enable      Set true to enable or false to disable
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_xb300_set_amplifier_enable(struct bladerf *dev,
                                                 bladerf_xb300_amplifier amp,
                                                 bool enable);
/**
 * Get state of selected XB-300 amplifier
 *
 * @param       dev         Device handle
 * @param[in]   amp         XB-300 amplifier
 * @param[out]  enable      Set true to enable or false to disable
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_xb300_get_amplifier_enable(struct bladerf *dev,
                                                 bladerf_xb300_amplifier amp,
                                                 bool *enable);
/**
 * Get current PA PDET output power in dBm
 *
 * @param       dev         Device handle
 * @param[out]  val         Output power in dBm
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_xb300_get_output_power(struct bladerf *dev, float *val);

/** @} (End of FN_BLADERF1_XB) */

/**
 * @defgroup FN_BLADERF1_DC_CAL DC Calibration
 *
 * These functions are thread-safe.
 *
 * @{
 */

/**
 * DC Calibration Modules
 */
typedef enum {
    BLADERF_DC_CAL_INVALID = -1,
    BLADERF_DC_CAL_LPF_TUNING,
    BLADERF_DC_CAL_TX_LPF,
    BLADERF_DC_CAL_RX_LPF,
    BLADERF_DC_CAL_RXVGA2
} bladerf_cal_module;

/**
 * Perform DC calibration
 *
 * @param       dev         Device handle
 * @param[in]   module      Module to calibrate
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_calibrate_dc(struct bladerf *dev,
                                   bladerf_cal_module module);

/** @} (End of FN_BLADERF1_DC_CAL) */

/**
 * @defgroup FN_BLADERF1_LOW_LEVEL Low-level accessors
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
 * Enable LMS receive
 *
 * @note This bit is set/cleared by bladerf_enable_module()
 */
#define BLADERF_GPIO_LMS_RX_ENABLE (1 << 1)

/**
 * Enable LMS transmit
 *
 * @note This bit is set/cleared by bladerf_enable_module()
 */
#define BLADERF_GPIO_LMS_TX_ENABLE (1 << 2)

/**
 * Switch to use TX low band (300MHz - 1.5GHz)
 *
 * @note This is set using bladerf_set_frequency().
 */
#define BLADERF_GPIO_TX_LB_ENABLE (2 << 3)

/**
 * Switch to use TX high band (1.5GHz - 3.8GHz)
 *
 * @note This is set using bladerf_set_frequency().
 */
#define BLADERF_GPIO_TX_HB_ENABLE (1 << 3)

/**
 * Counter mode enable
 *
 * Setting this bit to 1 instructs the FPGA to replace the (I, Q) pair in sample
 * data with an incrementing, little-endian, 32-bit counter value. A 0 in bit
 * specifies that sample data should be sent (as normally done).
 *
 * This feature is useful when debugging issues involving dropped samples.
 */
#define BLADERF_GPIO_COUNTER_ENABLE (1 << 9)

/**
 * Bit mask representing the rx mux selection
 *
 * @note These bits are set using bladerf_set_rx_mux()
 */
#define BLADERF_GPIO_RX_MUX_MASK (0x7 << BLADERF_GPIO_RX_MUX_SHIFT)

/**
 * Starting bit index of the RX mux values in FX3 <-> FPGA GPIO bank
 */
#define BLADERF_GPIO_RX_MUX_SHIFT 8

/**
 * Switch to use RX low band (300M - 1.5GHz)
 *
 * @note This is set using bladerf_set_frequency().
 */
#define BLADERF_GPIO_RX_LB_ENABLE (2 << 5)

/**
 * Switch to use RX high band (1.5GHz - 3.8GHz)
 *
 * @note This is set using bladerf_set_frequency().
 */
#define BLADERF_GPIO_RX_HB_ENABLE (1 << 5)

/**
 * This GPIO bit configures the FPGA to use smaller DMA transfers (256 cycles
 * instead of 512). This is required when the device is not connected at Super
 * Speed (i.e., when it is connected at High Speed).
 *
 * However, the caller need not set this in bladerf_config_gpio_write() calls.
 * The library will set this as needed; callers generally do not need to be
 * concerned with setting/clearing this bit.
 */
#define BLADERF_GPIO_FEATURE_SMALL_DMA_XFER (1 << 7)

/**
 * Enable Packet mode
 */
#define BLADERF_GPIO_PACKET (1 << 19)

/**
 * Enable 8bit sample mode
 */
#define BLADERF_GPIO_8BIT_MODE (1 << 20)

/**
 * AGC enable control bit
 *
 * @note This is set using bladerf_set_gain_mode().
 */
#define BLADERF_GPIO_AGC_ENABLE (1 << 18)

/**
 * Enable-bit for timestamp counter in the FPGA
 */
#define BLADERF_GPIO_TIMESTAMP (1 << 16)

/**
 * Timestamp 2x divider control.
 *
 * @note <b>Important</b>: This bit has no effect and is always enabled (1) in
 * FPGA versions >= v0.3.0.
 *
 * @note The remainder of the description of this bit is presented here for
 * historical purposes only. It is only relevant to FPGA versions <= v0.1.2.
 *
 * By default (value = 0), the sample counter is incremented with I and Q,
 * yielding two counts per sample.
 *
 * Set this bit to 1 to enable a 2x timestamp divider, effectively achieving 1
 * timestamp count per sample.
 * */
#define BLADERF_GPIO_TIMESTAMP_DIV2 (1 << 17)

/**
 * Packet capable core present bit.
 *
 * @note This is a read-only bit. The FPGA sets its value, and uses it to inform
 *  host that there is a core capable of using packets in the FPGA.
 */
#define BLADERF_GPIO_PACKET_CORE_PRESENT (1 << 28)

/**
 * Write value to VCTCXO trim DAC.
 *
 * \deprecated Use bladerf_trim_dac_write().
 *
 * This should not be used when the VCTCXO tamer is enabled.
 *
 * @param       dev     Device handle
 * @param[in]   val     Value to write to VCTCXO trim DAC
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_dac_write(struct bladerf *dev, uint16_t val);

/**
 * Read value from VCTCXO trim DAC.
 *
 * \deprecated Use bladerf_trim_dac_read().
 *
 * This is similar to bladerf_get_vctcxo_trim(), except that it returns the
 * current trim DAC value, as opposed to the calibration value read from flash.
 *
 * Use this if you are trying to query the value after having previously made
 * calls to bladerf_dac_write().
 *
 * @param       dev     Device handle
 * @param[out]  val     Value to read from VCTCXO trim DAC
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_dac_read(struct bladerf *dev, uint16_t *val);

/**
 * Read a Si5338 register
 *
 * @param       dev         Device handle
 * @param[in]   address     Si5338 register address
 * @param[out]  val         Register value
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_si5338_read(struct bladerf *dev,
                                  uint8_t address,
                                  uint8_t *val);

/**
 * Write a Si5338 register
 *
 * @param       dev         Device handle
 * @param[in]   address     Si5338 register address
 * @param[in]   val         Value to write to register
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_si5338_write(struct bladerf *dev,
                                   uint8_t address,
                                   uint8_t val);

/**
 * Read a LMS register
 *
 * @param       dev         Device handle
 * @param[in]   address     LMS register address
 * @param[out]  val         Register value
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_lms_read(struct bladerf *dev,
                               uint8_t address,
                               uint8_t *val);

/**
 * Write a LMS register
 *
 * @param       dev         Device handle
 * @param[in]   address     LMS register address
 * @param[in]   val         Value to write to register
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_lms_write(struct bladerf *dev,
                                uint8_t address,
                                uint8_t val);

/**
 * This structure is used to directly apply DC calibration register values to
 * the LMS, rather than use the values resulting from an auto-calibration.
 *
 * A value < 0 is used to denote that the specified value should not be written.
 * If a value is to be written, it will be truncated to 8-bits.
 */
struct bladerf_lms_dc_cals {
    int16_t lpf_tuning; /**< LPF tuning module */
    int16_t tx_lpf_i;   /**< TX LPF I filter */
    int16_t tx_lpf_q;   /**< TX LPF Q filter */
    int16_t rx_lpf_i;   /**< RX LPF I filter */
    int16_t rx_lpf_q;   /**< RX LPF Q filter */
    int16_t dc_ref;     /**< RX VGA2 DC reference module */
    int16_t rxvga2a_i;  /**< RX VGA2, I channel of first gain stage */
    int16_t rxvga2a_q;  /**< RX VGA2, Q channel of first gain stage */
    int16_t rxvga2b_i;  /**< RX VGA2, I channel of second gain stage */
    int16_t rxvga2b_q;  /**< RX VGA2, Q channel of second gain stage */
};

/**
 * Manually load values into LMS6002 DC calibration registers.
 *
 * This is generally intended for applying a set of known values resulting from
 * a previous run of the LMS autocalibrations.
 *
 * @param       dev        Device handle
 * @param[in]   dc_cals    Calibration values to load. Values set to <0 will
 *                          not be applied.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_lms_set_dc_cals(
    struct bladerf *dev, const struct bladerf_lms_dc_cals *dc_cals);

/**
 * Retrieve the current DC calibration values from the LMS6002
 *
 * @param       dev        Device handle
 * @param[out]  dc_cals    Populated with current values
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_lms_get_dc_cals(struct bladerf *dev,
                                      struct bladerf_lms_dc_cals *dc_cals);

/**
 * Write value to secondary XB SPI
 *
 * @param       dev     Device handle
 * @param[out]  val     Value to write to XB SPI
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_xb_spi_write(struct bladerf *dev, uint32_t val);

/** @} (End of FN_BLADERF1_LOW_LEVEL) */

/** @} (End of BLADERF1) */

#endif /* BLADERF1_H_ */
