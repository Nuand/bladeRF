/**
 * @file liblms.h
 *
 * @brief LMS6002D library
 */

#ifndef LMS_H_
#define LMS_H_

#include <libbladeRF.h>

#define kHz(x) (x*1000)             /**< Convenience for kHz */
#define MHz(x) (x*1000000)          /**< Convenience for MHz */
#define GHz(x) (x*1000000000)       /**< Convenience for GHz */

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
} lms_bw_t;


/**
 * LNA options
 */
typedef enum {
    LNA_NONE,   /**< Disable all LNAs */
    LNA_1,      /**< Enable LNA1 (300MHz - 2.8GHz) */
    LNA_2,      /**< Enable LNA2 (1.5GHz - 3.8GHz) */
    LNA_3       /**< Enable LNA3 (300MHz - 3.0GHz) */
} lms_lna_t;


/**
 * TX loopback modes
 */
typedef enum {
    TXLB_BB,        /**< TX Baseband Loopback */
    TXLB_RF         /**< TX RF Loopback */
} lms_txlb_t;

/**
 * PA Selection
 */
typedef enum {
    PA_AUX,         /**< AUX PA Enable - used for RF loopback modes */
    PA_1,           /**< PA1 Enable */
    PA_2,           /**< PA2 Enable */
    PA_ALL          /**< ALL PA's - used for DISABLE only.  Cannot be used for ENABLE */
} lms_pa_t;

/**
 * LMS6002D Transceiver configuration
 */
struct lms_xcvr_config {
    uint32_t tx_freq_hz;                   /**< Transmit frequency in Hz */
    uint32_t rx_freq_hz;                   /**< Receive frequency in Hz */
    bladerf_loopback loopback_mode;     /**< Loopback Mode */
    lms_lna_t lna;                         /**< LNA Selection */
    lms_pa_t pa;                           /**< PA Selection */
    lms_bw_t tx_bw;                        /**< Transmit Bandwidth */
    lms_bw_t rx_bw;                        /**< Receive Bandwidth */
};

/**
 * Convert an integer to a bandwidth selection.
 * If the actual bandwidth is not available, the closest
 * bandwidth greater than the requested bandwidth is selected
 * unless the requested bandwidth is greater than the maximum
 * allowable bandwidth.  Sorry. :(
 *
 * @param[in]   req     Requested bandwidth
 *
 * @return closest bandwidth
 */
lms_bw_t lms_uint2bw(unsigned int req);

/**
 * Convert a bandwidth seletion to an unsigned int.
 *
 * @param[in]   bw      Bandwidth enumeration
 *
 * @return bandwidth as an unsigned integer
 */
unsigned int lms_bw2uint(lms_bw_t bw);

/**
 * Select the bandwidth of the low-pass filter
 *
 * @param[in]   dev     Device handle
 * @param[in]   mod     Module to change
 * @param[in]   bw      Low-pass bandwidth selection
 */
void lms_lpf_enable(struct bladerf *dev, bladerf_module mod, lms_bw_t bw);

/**
 * Set the LPF mode
 * 
 * @param[in]   dev     Device handle
 * @param[in]   mod     Module to change
 * @param[in]   mode    Mode to set to
 */
void lms_lpf_set_mode(struct bladerf *dev, bladerf_module mod,
                      bladerf_lpf_mode mode);

/**
 * Get the LPF mode
 * 
 * @param[in]   dev     Device handle
 * @param[in]   mod     Module to change
 * @param[out]  mode    Current LPF mode
 */
void lms_lpf_get_mode(struct bladerf *dev, bladerf_module mod,
                      bladerf_lpf_mode *mode);

/**
 * Get the bandwidth for the selected module
 * TODO: Should this be returned as a pointer passed in?
 *
 * @param[in]   dev     Device handle
 * @param[in]   mod     Module to read
 *
 * @return the current bandwidth of the module
 */
lms_bw_t lms_get_bandwidth(struct bladerf *dev, bladerf_module mod);

/**
 * Enable dithering on PLL in the module to help reduce any fractional spurs
 * which might be occurring.
 *
 * @param[in]   dev     Device handle
 * @param[in]   mod     Module to change
 * @param[in]   nbits   Number of bits to dither (1 to 8)
 */
void lms_dither_enable(struct bladerf *dev, bladerf_module mod, uint8_t nbits);

/**
 * Disable dithering on PLL in the module.
 *
 * @param[in]   dev     Device handle
 * @param[in]   mod     Module to change
 */
void lms_dither_disable(struct bladerf *dev, bladerf_module mod);

/**
 * Perform a soft reset of the LMS6002D device
 *
 * @param[in]   dev     Device handle
 */
void lms_soft_reset(struct bladerf *dev);

/**
 * Set the gain of the LNA
 *
 * The LNA gain can be one of three values: bypassed (0dB gain),
 * mid (MAX-6dB) and max.
 *
 * @param[in]   dev     Device handle
 * @param[in]   gain    Bypass, mid or max gain
 */
void lms_lna_set_gain(struct bladerf *dev, bladerf_lna_gain gain);

/**
 * Get the gain of the LNA
 *
 * @param[in]   dev     Device handle
 * @param[out]  gain    Bypass, mid or max gain
 */
void lms_lna_get_gain(struct bladerf *dev, bladerf_lna_gain *gain);

/**
 * Select which LNA to enable
 *
 * LNA1 frequency range is from 300MHz to 2.8GHz
 * LNA2 frequency range is from 1.5GHz to 3.8GHz
 * LNA3 frequency range is from 300MHz to 3.0GHz
 *
 * @param[in]   dev     Device handle
 * @param[in]   lna     LNA to enable
 */
void lms_lna_select(struct bladerf *dev, lms_lna_t lna);

/**
 * Disable RXVGA1
 *
 * @param[in]   dev     Device handle
 */
void lms_rxvga1_disable(struct bladerf *dev);

/**
 * Enable RXVGA1
 *
 * @param[in]   dev     Device handle
 */
void lms_rxvga1_enable(struct bladerf *dev);

/**
 * Set the RXVGA1 mixer gain in weird units.
 *
 * Gain range is complicated and non-linear.
 * If 120 -> mixer gain = 30dB
 * If 102 -> mixer gain = 19dB
 * If 2   -> mixer gain = 5dB
 *
 * Allowable range is 0 to 120.
 *
 * @param[in]   dev     Device handle
 * @param[in]   gain    Gain value
 */
void lms_rxvga1_set_gain(struct bladerf *dev, uint8_t gain);

/**
 * Get the RXVGA1 mixer gain in weird units.
 *
 * Gain range is complicated and non-linear.
 * If 120 -> mixer gain = 30dB
 * If 102 -> mixer gain = 19dB
 * If 2   -> mixer gain = 5dB
 *
 * @param[in]   dev     Device handle
 * @param[out]  gain    Gain valye
 */
void lms_rxvga1_get_gain(struct bladerf *dev, uint8_t *gain);

/**
 * Disable RXVGA2
 *
 * @param[in]   dev     Device handle
 */
void lms_rxvga2_disable(struct bladerf *dev);

/**
 * Set the gain on RXVGA2 in dB.
 *
 * The range of gain values is from 0dB to 60dB.
 * Anything above 30dB is not recommended as a gain setting.
 *
 * @param[in]   dev     Device handle
 * @param[in]   gain    Gain in dB (range: 0 to 60, >30 not recommended)
 */
void lms_rxvga2_set_gain(struct bladerf *dev, uint8_t gain);

/**
 * Get the gain on RXVGA2 in dB.
 *
 * @param[in]   dev     Device handle
 * @param[out]  gain    Gain in dB (range: 0 to 60)
 */
void lms_rxvga2_get_gain(struct bladerf *dev, uint8_t *gain);

/**
 * Set the gain in dB and enable RXVGA2.
 *
 * The range of gain values is from 0db to 60dB.
 * Anything above 30dB is not recommended as a gain setting.
 *
 * @param[in]   dev     Device handle
 * @param[in]   gain    Gain in dB (range: 0 to 60, >30 not recommended)
 */
void lms_rxvga2_enable(struct bladerf *dev, uint8_t gain);

/**
 * Set the gain in dB of TXVGA2.
 *
 * The range of gain values is from 0dB to 25dB.
 * Anything above 25 will be clamped at 25.
 *
 * @param[in]   dev     Device handle
 * @param[in]   gain    Gain in dB (range: 0 to 25)
 */
void lms_txvga2_set_gain(struct bladerf *dev, uint8_t gain);

/**
 * Get the gain in dB of TXVGA2.
 *
 * @param[in]   dev     Device handle
 * @param[out]  gain    Gain in dB (range: 0 to 25)
 */
void lms_txvga2_get_gain(struct bladerf *dev, uint8_t *gain);

/**
 * Set the gain in dB of TXVGA1.
 *
 * The range of gain values is from -35dB to -4dB.
 *
 * @param[in]   dev     Device handle
 * @param[in]   gain    Gain in dB (range: -4 to -35)
 */
void lms_txvga1_set_gain(struct bladerf *dev, int8_t gain);

/**
 * Get the gain in dB of TXVGA1.
 *
 * The range of gain values is from -35dB to -4dB.
 *
 * @param[in]   dev     Device handle
 * @param[in]   gain    Gain in dB (range: -4 to -35)
 */
void lms_txvga1_get_gain(struct bladerf *dev, int8_t *gain);

/**
 * Enable PA.
 * NOTE: PA_ALL is NOT valid for enabling, only for disabling.
 *
 * @param[in]   dev     Device handle
 * @param[in]   pa      PA to enable
 */
void lms_pa_enable(struct bladerf *dev, lms_pa_t pa);

/**
 * Disable PA.
 * NOTE: PA_ALL is valid to disable all the PA's in the system
 *
 * @param[in]   dev     Device handle
 * @param[in]   pa      PA to disable
 */
void lms_pa_disable(struct bladerf *dev, lms_pa_t pa);

/**
 * Enable the peak detectors.  This is used as a baseband feedback
 * to the system during transmit for calibration purposes.
 *
 * NOTE: You cannot actively receive RF when the peak detectors are enabled.
 *
 * @param[in]   dev     Device handle
 */
void lms_peakdetect_enable(struct bladerf *dev);

/**
 * Disable the RF peak detectors.
 *
 * @param[in]   dev     Device handle
 */
void lms_peakdetect_disable(struct bladerf *dev);

/**
 * Enable or disable the RF front end.
 *
 * @param[in]   dev     Device handle
 * @param[in]   module  Module to enable or disable
 * @param[in]   enable  Enable or disable boolean flag
 */
void lms_enable_rffe(struct bladerf *dev, bladerf_module module, bool enable);

/**
 * Enable TX loopback
 * TODO: Does this need to be exposed like this?
 *
 * @param[in]   dev     Device handle
 * @param[in]   mode    Loopback mode (baseband or RF)
 */
void lms_tx_loopback_enable(struct bladerf *dev, lms_txlb_t mode);

/**
 * Disable TX loopback
 * TODO: Can this be more elegant? Does this need to be exposed like this?
 *
 * @param[in]   dev     Device handle
 * @param[in]   mode    Current loopback mode
 */
void lms_tx_loopback_disable(struct bladerf *dev, lms_txlb_t mode);

/**
 * Enable loopback of the TX system to the RX system in the mode given.
 *
 * @param[in]   dev     Device handle
 * @param[in]   mode    Loopback mode
 */
void lms_loopback_enable(struct bladerf *dev, bladerf_loopback mode);

/**
 * Figure out what loopback mode we're in (if any at all!)
 *
 * @param[in]   dev     Device handle
 *
 * @return the loopback mode the LMS6002D is currently in, if any.
 */
bladerf_loopback lms_get_loopback_mode(struct bladerf *dev);

/**
 * Disable loopback mode.
 * NOTE: You must choose which LNA to hook up and what bandwidth you want as well.
 *
 * @param[in]   dev     Device handle
 * @param[in]   lna     LNA to enable
 * @param[in]   bw      Bandwidth to set
 */
void lms_loopback_disable(struct bladerf *dev, lms_lna_t lna, lms_bw_t bw);

/**
 * Top level power down of the LMS6002D
 *
 * @param[in]   dev     Device handle
 */
void lms_power_down(struct bladerf *dev);

/**
 * Enable the PLL of a module.
 *
 * @param[in]   dev     Device handle
 * @param[in]   mod     Module PLL to enable
 */
void lms_pll_enable(struct bladerf *dev, bladerf_module mod);

/**
 * Disable the PLL of a module.
 *
 * @param[in]   dev     Device handle
 * @param[in]   mod     Module PLL to disable
 */
void lms_pll_disable(struct bladerf *dev, bladerf_module mod);

/**
 * Enable the RX subsystem
 *
 * @param[in]   dev     Device handle
 */
void lms_rx_enable(struct bladerf *dev);

/**
 * Disable the RX subsystem
 *
 * @param[in]   dev     Device handle
 */
void lms_rx_disable(struct bladerf *dev);

/**
 * Enable the TX subsystem
 *
 * @param[in]   dev     Device handle
 */
void lms_tx_enable(struct bladerf *dev);

/**
 * Disable the TX subsystem
 *
 * @param[in]   dev     Device handle
 */
void lms_tx_disable(struct bladerf *dev);

/**
 * Converts a frequency structure into the final frequency in Hz
 *
 * @param[in]   freq    Frequency structure to print out
 * @returns The closest frequency in Hz that it's tuned to
 */
uint32_t lms_frequency_to_hz(struct lms_freq *f);

/**
 * Pretty print a frequency structure
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
 */
void lms_get_frequency(struct bladerf *dev, bladerf_module mod,
                       struct lms_freq *freq);

/**
 * Set the frequency of a module in Hz
 *
 * @param[in]   dev     Device handle
 * @param[in]   mod     Module to change
 * @param[in]   freq    Frequency in Hz to tune
 */
void lms_set_frequency(struct bladerf *dev,
                       bladerf_module mod, uint32_t freq);

/**
 * Read back every register from the LMS6002D device.
 *
 * NOTE: This is mainy used for debug purposes.
 *
 * @param[in]   dev     Device handle
 */
void lms_dump_registers(struct bladerf *dev);

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
 * Initialize and calibrate the low pass filters
 *
 * @param[in]   dev     Device handle
 */
void lms_lpf_init(struct bladerf *dev);

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

#ifdef __cplusplus
}
#endif

#endif /* LMS_H_ */
