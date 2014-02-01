/**
 * @file liblms.h
 *
 * @brief LMS6002D support
 *
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2013 Nuand LLC
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
#ifndef LMS_H_
#define LMS_H_

#include <libbladeRF.h>

/**
 * Information about the frequency calculation for the LMS6002D PLL
 * Calculation taken from the LMS6002D Programming and Calibration Guide
 * version 1.1r1.
 */
struct lms_freq {
    uint8_t     x;         /**< VCO division ratio */
    uint16_t    nint;      /**< Integer portion of f_LO given f_REF */
    uint32_t    nfrac;     /**< Fractional portion of f_LO given nint and f_REF */
    uint8_t     freqsel;   /**< Choice of VCO and dision ratio */
    uint32_t    reference; /**< Reference frequency going to the LMS6002D */
};

/**
 * Internal low-pass filter bandwidth selection
 */
typedef enum {
    BW_28MHz,       /**< 28MHz bandwidth, 14MHz LPF */
    BW_20MHz,       /**< 20MHz bandwidth, 10MHz LPF */
    BW_14MHz,       /**< 14MHz bandwidth, 7MHz LPF */
    BW_12MHz,       /**< 12MHz bandwidth, 6MHz LPF */
    BW_10MHz,       /**< 10MHz bandwidth, 5MHz LPF */
    BW_8p75MHz,     /**< 8.75MHz bandwidth, 4.375MHz LPF */
    BW_7MHz,        /**< 7MHz bandwidth, 3.5MHz LPF */
    BW_6MHz,        /**< 6MHz bandwidth, 3MHz LPF */
    BW_5p5MHz,      /**< 5.5MHz bandwidth, 2.75MHz LPF */
    BW_5MHz,        /**< 5MHz bandwidth, 2.5MHz LPF */
    BW_3p84MHz,     /**< 3.84MHz bandwidth, 1.92MHz LPF */
    BW_3MHz,        /**< 3MHz bandwidth, 1.5MHz LPF */
    BW_2p75MHz,     /**< 2.75MHz bandwidth, 1.375MHz LPF */
    BW_2p5MHz,      /**< 2.5MHz bandwidth, 1.25MHz LPF */
    BW_1p75MHz,     /**< 1.75MHz bandwidth, 0.875MHz LPF */
    BW_1p5MHz,      /**< 1.5MHz bandwidth, 0.75MHz LPF */
} lms_bw;


/**
 * LNA options
 */
typedef enum {
    LNA_NONE,   /**< Disable all LNAs */
    LNA_1,      /**< Enable LNA1 (300MHz - 2.8GHz) */
    LNA_2,      /**< Enable LNA2 (1.5GHz - 3.8GHz) */
    LNA_3       /**< Enable LNA3 (Unused on the bladeRF) */
} lms_lna;


/**
 * Loopback paths
 */
typedef enum {
    LBP_BB,        /**< Baseband loopback path */
    LBP_RF         /**< RF Loopback path */
} lms_lbp;

/**
 * PA Selection
 */
typedef enum {
    PA_AUX,         /**< AUX PA Enable (for RF Loopback) */
    PA_1,           /**< PA1 Enable (300MHz - 2.8GHz) */
    PA_2,           /**< PA2 Enable (1.5GHz - 3.8GHz) */
    PA_NONE,        /**< All PAs disabled */
} lms_pa;

/**
 * LMS6002D Transceiver configuration
 */
struct lms_xcvr_config {
    uint32_t tx_freq_hz;                /**< Transmit frequency in Hz */
    uint32_t rx_freq_hz;                /**< Receive frequency in Hz */
    bladerf_loopback loopback_mode;     /**< Loopback Mode */
    lms_lna lna;                        /**< LNA Selection */
    lms_pa pa;                          /**< PA Selection */
    lms_bw tx_bw;                       /**< Transmit Bandwidth */
    lms_bw rx_bw;                       /**< Receive Bandwidth */
};

/**
 * Convert an integer to a bandwidth selection.
 * If the actual bandwidth is not available, the closest
 * bandwidth greater than the requested bandwidth is selected.
 * If the provide value is greater than the maximum available bandwidth, the
 * maximum available bandiwidth is returned.
 *
 * @param[in]   req     Requested bandwidth
 *
 * @return closest bandwidth
 */
lms_bw lms_uint2bw(unsigned int req);

/**
 * Convert a bandwidth seletion to an unsigned int.
 *
 * @param[in]   bw      Bandwidth enumeration
 *
 * @return bandwidth as an unsigned integer
 */
unsigned int lms_bw2uint(lms_bw bw);

/**
 * Enable or disable the low-pass filter on the specified module
 *
 * @param[in]   dev     Device handle
 * @param[in]   mod     Module to change
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_lpf_enable(struct bladerf *dev, bladerf_module mod, bool enable);

/**
 * Set the LPF mode
 *
 * @param[in]   dev     Device handle
 * @param[in]   mod     Module to change
 * @param[in]   mode    Mode to set to
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_lpf_set_mode(struct bladerf *dev, bladerf_module mod,
                     bladerf_lpf_mode mode);

/**
 * Get the LPF mode
 *
 * @param[in]   dev     Device handle
 * @param[in]   mod     Module to change
 * @param[out]  mode    Current LPF mode
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_lpf_get_mode(struct bladerf *dev, bladerf_module mod,
                     bladerf_lpf_mode *mode);

/**
 * Set the bandwidth for the specified module
 *
 * @param[in]   dev     Device handle
 * @param[in]   mod     Module to set bandwidth for
 * @param[in]   bw      Desired bandwidth
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_set_bandwidth(struct bladerf *dev, bladerf_module mod, lms_bw bw);

/**
 * Get the bandwidth for the specified module
 *
 * @param[in]   dev     Device handle
 * @param[in]   mod     Module to read
 * @param[out]  bw      Current bandwidth
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_get_bandwidth(struct bladerf *dev, bladerf_module mod, lms_bw *bw);

/**
 * Enable dithering on PLL in the module to help reduce any fractional spurs
 * which might be occurring.
 *
 * @param[in]   dev     Device handle
 * @param[in]   mod     Module to change
 * @param[in]   nbits   Number of bits to dither (1 to 8). Ignored when
 *                      disabling dithering.
 * @param[in]   enable  Set to `true` to enable, `false` to disable
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_dither_enable(struct bladerf *dev, bladerf_module mod,
                      uint8_t nbits, bool enable);

/**
 * Perform a soft reset of the LMS6002D device
 *
 * @param[in]   dev     Device handle
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_soft_reset(struct bladerf *dev);

/**
 * Set the gain of the LNA
 *
 * The LNA gain can be one of three values: bypassed (0dB gain),
 * mid (MAX-6dB) and max.
 *
 * @param[in]   dev     Device handle
 * @param[in]   gain    Bypass, mid or max gain
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_lna_set_gain(struct bladerf *dev, bladerf_lna_gain gain);

/**
 * Get the gain of the LNA
 *
 * @param[in]   dev     Device handle
 * @param[out]  gain    Bypass, mid or max gain
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_lna_get_gain(struct bladerf *dev, bladerf_lna_gain *gain);

/**
 * Select which LNA to enable
 *
 * LNA1 frequency range is from 300MHz to 2.8GHz
 * LNA2 frequency range is from 1.5GHz to 3.8GHz
 * LNA3 frequency range is from 300MHz to 3.0GHz
 *
 * @param[in]   dev     Device handle
 * @param[in]   lna     LNA to enable
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_select_lna(struct bladerf *dev, lms_lna lna);

/**
 * Enable or disable RXVGA1
 *
 * @param[in]   dev     Device handle
 * @param[in]   enable  Set to `true` to enable, `false` to disable
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_rxvga1_enable(struct bladerf *dev, bool enable);

/**
 * Set the gain value of RXVGA1 (in dB)
 *
 * @param[in]   dev     Device handle
 * @param[in]   gain    Gain value
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_rxvga1_set_gain(struct bladerf *dev, int gain);

/**
 * Get the RXVGA1 gain value (in dB)
 *
 * @param[in]   dev     Device handle
 * @param[out]  gain    Gain value
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_rxvga1_get_gain(struct bladerf *dev, int *gain);

/**
 * Set the gain in dB and enable RXVGA2, or disable RXVGA2
 *
 * The range of gain values is from 0db to 60dB.
 * Anything above 30dB is not recommended as a gain setting.
 *
 * @param[in]   dev     Device handle
 * @param[in]   enable  Set to `true` to enable, `false` to disable
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_rxvga2_enable(struct bladerf *dev, bool enable);

/**
 * Set the gain on RXVGA2 in dB.
 *
 * The range of gain values is from 0dB to 60dB.
 * Anything above 30dB is not recommended as a gain setting.
 *
 * @param[in]   dev     Device handle
 * @param[in]   gain    Gain in dB (range: 0 to 30)
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_rxvga2_set_gain(struct bladerf *dev, int gain);

/**
 * Get the gain on RXVGA2 in dB.
 *
 * @param[in]   dev     Device handle
 * @param[out]  gain    Gain in dB (range: 0 to 30)
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_rxvga2_get_gain(struct bladerf *dev, int *gain);

/**
 * Set the gain in dB of TXVGA2.
 *
 * The range of gain values is from 0dB to 25dB.
 * Anything above 25 will be clamped at 25.
 *
 * @param[in]   dev     Device handle
 * @param[in]   gain    Gain in dB (range: 0 to 25). Out of range values will
 *                      be clamped.
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_txvga2_set_gain(struct bladerf *dev, int gain);

/**
 * Get the gain in dB of TXVGA2.
 *
 * @param[in]   dev     Device handle
 * @param[out]  gain    Gain in dB (range: 0 to 25)
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_txvga2_get_gain(struct bladerf *dev, int *gain);

/**
 * Set the gain in dB of TXVGA1.
 *
 * The range of gain values is from -35dB to -4dB.
 *
 * @param[in]   dev     Device handle
 * @param[in]   gain    Gain in dB (range: -4 to -35). Out of range values
 *                      will be clamped.
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_txvga1_set_gain(struct bladerf *dev, int gain);

/**
 * Get the gain in dB of TXVGA1.
 *
 * The range of gain values is from -35dB to -4dB.
 *
 * @param[in]   dev     Device handle
 * @param[out]  gain    Gain in dB (range: -4 to -35)
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_txvga1_get_gain(struct bladerf *dev, int *gain);

/**
 * Enable or disable a PA
 *
 * @note PA_ALL is NOT valid for enabling, only for disabling.
 *
 * @param[in]   dev     Device handle
 * @param[in]   pa      PA to enable
 * @param[in]   enable  Set to `true` to enable, `false` to disable
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_pa_enable(struct bladerf *dev, lms_pa pa, bool enable);

/**
 * Enable or disable  the peak detectors.
 *
 * This is used as a baseband feedback to the system during transmit for
 * calibration purposes.
 *
 * @note You cannot actively receive RF when the peak detectors are enabled.
 *
 * @param[in]   dev     Device handle
 * @param[in]   enable  Set to `true` to enable, `false` to disable
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_peakdetect_enable(struct bladerf *dev, bool enable);

/**
 * Enable or disable the RF front end.
 *
 * @param[in]   dev     Device handle
 * @param[in]   module  Module to enable or disable
 * @param[in]   enable  Set to `true` to enable, `false` to disable
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_enable_rffe(struct bladerf *dev, bladerf_module module, bool enable);

/**
 * Configure TX -> RX loopback mode
 *
 * @param[in]   dev     Device handle
 * @param[in]   mode    Loopback mode. USE BLADERF_LB_NONE to disable
 *                      loopback functionality.
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_set_loopback_mode(struct bladerf *dev, bladerf_loopback mode);

/**
 * Figure out what loopback mode we're in.
 *
 * @param[in]   dev     Device handle
 * @param[out]  mode    Current loopback mode, or BLADERF_LB_NONE if
 *                      loopback is not enabled.
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_get_loopback_mode(struct bladerf *dev, bladerf_loopback *mode);

/**
 * Top level power down of the LMS6002D
 *
 * @param[in]   dev     Device handle
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_power_down(struct bladerf *dev);

/**
 * Enable or disable the PLL of a module.
 *
 * @param[in]   dev     Device handle
 * @param[in]   mod     Module PLL to enable
 * @param[in]   enable  Set to `true` to enable, `false` to disable
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_pll_enable(struct bladerf *dev, bladerf_module mod, bool enable);

/**
 * Enable or disable the RX subsystem
 *
 * @param[in]   dev     Device handle
 * @param[in]   enable  Set to `true` to enable, `false` to disable
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_rx_enable(struct bladerf *dev, bool enable);

/**
 * Enable or disable the TX subsystem
 *
 * @param[in]   dev     Device handle
 * @param[in]   enable  Set to `true` to enable, `false` to disable
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_tx_enable(struct bladerf *dev, bool enable);

/**
 * Converts a frequency structure into the final frequency in Hz
 *
 * @param[in]   f   Frequency structure to convert
 * @returns  The closest frequency in Hz that `f` can be converted to
 */
uint32_t lms_frequency_to_hz(struct lms_freq *f);

/**
 * Pretty print a frequency structure
 *
 * @note This is intended only for debug purposes. The log level must
 *       be set to DEBUG for this output to be made visible.
 *
 * @param[in]   freq    Frequency structure to print out
 */
void lms_print_frequency(struct lms_freq *freq);

/**
 * Get the frequency structure of the module
 *
 * @param[in]   dev     Device handle
 * @param[in]   mod     Module to change
 * @param[out]  freq    LMS frequency structure detailing VCO settings
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_get_frequency(struct bladerf *dev, bladerf_module mod,
                      struct lms_freq *freq);

/**
 * Set the frequency of a module in Hz
 *
 * @param[in]   dev     Device handle
 * @param[in]   mod     Module to change
 * @param[in]   freq    Frequency in Hz to tune
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_set_frequency(struct bladerf *dev,
                      bladerf_module mod, uint32_t freq);

/**
 * Read back every register from the LMS6002D device.
 *
 * @note This is intended only for debug purposes.
 *
 * @param[in]   dev     Device handle
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int lms_dump_registers(struct bladerf *dev);

/**
 * Calibrate the DC offset value for RX and TX modules for the
 * direct conversion receiver.
 *
 * @param[in]   dev     Device handle
 * @param[in]   module  Module to calibrate
 *
 * @return 0 on success, -1 on failure.
 */
int lms_calibrate_dc(struct bladerf *dev, bladerf_cal_module module);

/**
 * Initialize and configure the LMS6002D given the transceiver
 * configuration passed in.
 *
 * @param[in]   dev     Device handle
 * @param[in]   config  Transceiver configuration
 *
 * @return 0 on success, -1 on failure.
 */
int lms_config_init(struct bladerf *dev, struct lms_xcvr_config *config);

/**
 * Select the appropriate band fore the specified frequency
 *
 * @note This is band selection is specific to how the bladeRF is connected
 *       to the LNA and PA blocks.
 *
 * @param[in]   dev     Device handle
 * @param[in]   module  Module to configure
 * @parma[in]   freq    Frequency
 *
 * @return 0 on succes, BLADERF_ERR_* value on failure
 */
int lms_select_band(struct bladerf *dev, bladerf_module module,
                    unsigned int freq);

#endif /* LMS_H_ */
