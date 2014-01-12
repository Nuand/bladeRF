/**
 * @file libbladeRF.h
 *
 * @brief bladeRF library
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef BLADERF_H_
#define BLADERF_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined _WIN32 || defined _CYGWIN__
#   include <Windows.h>
#   define CALL_CONV __cdecl
#   ifdef __GNUC__
#       define API_EXPORT __attribute__ ((dllexport))
#   else
#       define API_EXPORT __declspec(dllexport)
#   endif
#elif defined _DOXYGEN_ONLY_
    /** Marks an API routine to be made visible to the dynamic loader.
     *  This is OS and/or compiler-specific. */
#   define API_EXPORT
    /** Specifies calling convention, if necessary.
     *  This is OS and/or compiler-specific. */
#   define CALL_CONV
#else
#   define API_EXPORT __attribute__ ((visibility ("default")))
#   define CALL_CONV
#endif

/** This structure is an opaque device handle */
struct bladerf;

/** This opaque structure is used to keep track of stream information */
struct bladerf_stream;

/**
 * @defgroup    RETCODES    Error codes
 *
 * bladeRF library routines return negative values to indicate errors.
 * Values >= 0 are used to indicate success.
 *
 * @code
 *  int status = bladerf_set_txvga1(dev, 2);
 *
 *  if (status < 0)
 *      handle_error();
 * @endcode
 *
 * @{
 */
#define BLADERF_ERR_UNEXPECTED  (-1)  /**< An unexpected failure occurred */
#define BLADERF_ERR_RANGE       (-2)  /**< Provided parameter is out of range */
#define BLADERF_ERR_INVAL       (-3)  /**< Invalid operation/parameter */
#define BLADERF_ERR_MEM         (-4)  /**< Memory allocation error */
#define BLADERF_ERR_IO          (-5)  /**< File/Device I/O error */
#define BLADERF_ERR_TIMEOUT     (-6)  /**< Operation timed out */
#define BLADERF_ERR_NODEV       (-7)  /**< No device(s) available */
#define BLADERF_ERR_UNSUPPORTED (-8)  /**< Operation not supported */
#define BLADERF_ERR_MISALIGNED  (-9)  /**< Misaligned flash access */
#define BLADERF_ERR_CHECKSUM    (-10) /**< Invalid checksum */

/** @} (End RETCODES) */

/**
 * Backend by which the host communicates with the device
 */
typedef enum {
    BLADERF_BACKEND_ANY,    /**< "Don't Care" -- use any available backend */
    BLADERF_BACKEND_LINUX,  /**< Linux kernel driver */
    BLADERF_BACKEND_LIBUSB  /**< libusb */
} bladerf_backend;


/**
 * This enum describes the USB Speed at which the bladeRF is connected.
 * Speeds not listed here are not supported.
 */
typedef enum {
    BLADERF_DEVICE_SPEED_UNKNOWN,
    BLADERF_DEVICE_SPEED_HIGH,
    BLADERF_DEVICE_SPEED_SUPER,
} bladerf_dev_speed;

/** Length of device serial number string, including NUL-terminator */
#define BLADERF_SERIAL_LENGTH   33

/**
 * Information about a bladeRF attached to the system
 *
 * See the \ref FN_DEVINFO section for information on populating and comparing
 * these structures.
 */
struct bladerf_devinfo {
    bladerf_backend backend;    /**< Backend to use when connecting to device */
    char serial[BLADERF_SERIAL_LENGTH]; /**< Device serial number string */
    uint8_t usb_bus;            /**< Bus # device is attached to */
    uint8_t usb_addr;           /**< Device address on bus */
    unsigned int instance;      /**< Device instance or ID */
};

/**
 * Rational sample rate representation
 */
struct bladerf_rational_rate {
    uint64_t integer;           /**< Integer portion */
    uint64_t num;               /**< Numerator in fractional portion */
    uint64_t den;               /**< Denominator in fractional portion. This
                                     must be > 0. */
};

/**
 * Device statistics
 */
struct bladerf_stats {

    /** The number of times samples have been lost in the FPGA */
    uint64_t rx_overruns;

    /** The overall throughput of the device in samples/second */
    uint64_t rx_throughput;

    /**  Number of times samples have been too late to transmit to the FPGA */
    uint64_t tx_underruns;

    /** The overall throughput of the device in samples/second */
    uint64_t tx_throughput;
};

/**
 * Version structure for FPGA, firmware, libbladeRF, and associated utilities
 */
struct bladerf_version {
    uint16_t major;             /**< Major version */
    uint16_t minor;             /**< Minor version */
    uint16_t patch;             /**< Patch version */
    const char *describe;       /**< Version string with any additional suffix
                                 *   information.
                                 *
                                 *   @warning Do not attempt to modify or
                                 *            free() this string. */
};



/**
 * Sample format
 */
typedef enum {
    BLADERF_FORMAT_SC16_Q11, /**< Signed, Complex 16-bit Q11.
                               *  This is the native format of the DAC data.
                               *
                               *  Samples are interleaved IQ value pairs, where
                               *  each value in the pair is an int16_t. For each
                               *  value, the data in the lower bits. The upper
                               *  bits are reserved.
                               *
                               *  When using this format, note that buffers
                               *  must be at least
                               *       2 * num_samples * sizeof(int16_t)
                               *  bytes large
                               */
} bladerf_format;

/**
 * Reverse compatibility for the sample format misnomer fix
 *
 * @warning This is scheduled to be removed in the future.
 */
#define BLADERF_FORMAT_SC16_Q12 BLADERF_FORMAT_SC16_Q11

/**
 * FPGA device variant (size)
 */
typedef enum {
    BLADERF_FPGA_UNKNOWN = 0,   /**< Unable to determine FPGA variant */
    BLADERF_FPGA_40KLE = 40,    /**< 40 kLE FPGA */
    BLADERF_FPGA_115KLE = 115   /**< 115 kLE FPGA */
} bladerf_fpga_size;

/**
 * Sample metadata
 */
struct bladerf_metadata {
    uint32_t version;       /**< Metadata format version */
    uint64_t timestamp;     /**< Timestamp (TODO format TBD) */
};

/**
 * Sampling connection
 */
typedef enum {
    BLADERF_SAMPLING_UNKNOWN,  /**< Unable to determine connection type */
    BLADERF_SAMPLING_INTERNAL, /**< Sample from RX/TX connector */
    BLADERF_SAMPLING_EXTERNAL  /**< Sample from J60 or J61 */
} bladerf_sampling;

/**
 * LNA gain options
 */
typedef enum {
    BLADERF_LNA_GAIN_UNKNOWN,    /**< Invalid LNA gain */
    BLADERF_LNA_GAIN_BYPASS,     /**< LNA bypassed - 0dB gain */
    BLADERF_LNA_GAIN_MID,        /**< LNA Mid Gain (MAX-6dB) */
    BLADERF_LNA_GAIN_MAX         /**< LNA Max Gain */
} bladerf_lna_gain;

/**
 * LPF mode
 */
typedef enum {
    BLADERF_LPF_NORMAL,     /**< LPF connected and enabled */
    BLADERF_LPF_BYPASSED,   /**< LPF bypassed */
    BLADERF_LPF_DISABLED    /**< LPF disabled */
} bladerf_lpf_mode;

/**
 * Module selection for those which have both RX and TX constituents
 */
typedef enum
{
    BLADERF_MODULE_RX,  /**< Receive Module */
    BLADERF_MODULE_TX   /**< Transmit Module */
} bladerf_module;

/**
 * DC Calibration Modules
 */
typedef enum
{
    BLADERF_DC_CAL_LPF_TUNING,
    BLADERF_DC_CAL_TX_LPF,
    BLADERF_DC_CAL_RX_LPF,
    BLADERF_DC_CAL_RXVGA2
} bladerf_cal_module;

/**
 * Correction parameter
 */
typedef enum
{
    BLADERF_IQ_CORR_DC_I,
    BLADERF_IQ_CORR_DC_Q,
    BLADERF_IQ_CORR_PHASE,
    BLADERF_IQ_CORR_GAIN
} bladerf_correction;

/**
 * Transmit Loopback options
 */
typedef enum {
    BLADERF_LB_BB_LPF = 0,   /**< Baseband loopback enters before RX low-pass filter input */
    BLADERF_LB_BB_VGA2,      /**< Baseband loopback enters before RX VGA2 input */
    BLADERF_LB_BB_OP,        /**< Baseband loopback enters before RX ADC input */
    BLADERF_LB_RF_LNA_START, /**< Placeholder - DO NOT USE */
    BLADERF_LB_RF_LNA1,      /**< RF loopback enters at LNA1 (300MHz - 2.8GHz)*/
    BLADERF_LB_RF_LNA2,      /**< RF loopback enters at LNA2 (1.5GHz - 3.8GHz)*/
    BLADERF_LB_RF_LNA3,      /**< RF loopback enters at LNA3 (300MHz - 3.0GHz)*/
    BLADERF_LB_NONE          /**< Null loopback mode*/
} bladerf_loopback;

/**
 * Severity levels for logging functions
 */
typedef enum {
    BLADERF_LOG_LEVEL_VERBOSE,  /**< Verbose level logging */
    BLADERF_LOG_LEVEL_DEBUG,    /**< Debug level logging */
    BLADERF_LOG_LEVEL_INFO,     /**< Information level logging */
    BLADERF_LOG_LEVEL_WARNING,  /**< Warning level logging */
    BLADERF_LOG_LEVEL_ERROR,    /**< Error level logging */
    BLADERF_LOG_LEVEL_CRITICAL, /**< Fatal error level logging */
    BLADERF_LOG_LEVEL_SILENT    /**< No output */
} bladerf_log_level;

/**
 * For both RX and TX, the stream callback receives:
 * dev:             Device structure
 * stream:          The associated stream
 * metadata:        TBD
 * user_data:       User data provided when initializing stream
 *
 * <br>
 *
 * For TX callbacks:
 *  samples:        Pointer fo buffer of samples that was sent
 *  num_samples:    Number of sent in last transfer and to send in next transfer
 *
 *  Return value:   The user specifies the address of the next buffer to send
 *
 * For RX callbacks:
 *  samples:        Buffer filled with received data
 *  num_samples:    Number of samples received and size of next buffers
 *
 *  Return value:   The user specifies the next buffer to fill with RX data,
 *                  which should be num_samples in size.
 *
 */
typedef void *(*bladerf_stream_cb)(struct bladerf *dev,
                                   struct bladerf_stream *stream,
                                   struct bladerf_metadata *meta,
                                   void *samples,
                                   size_t num_samples,
                                   void *user_data);


/**
 * @defgroup FN_INIT    Initialization/deinitialization
 *
 * @{
 */

/**
 * Obtain a list of bladeRF devices attached to the system
 *
 * @param[out]  devices
 *
 * @return number of items in returned device list, or value from
 *         \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_device_list(struct bladerf_devinfo **devices);

/**
 * Free device list returned by bladerf_get_device_list()
 *
 * @param   devices     List of available devices
 */
API_EXPORT
void CALL_CONV bladerf_free_device_list(struct bladerf_devinfo *devices);

/**
 * Opens device specified by provided bladerf_devinfo structure
 *
 * @pre devinfo has been populated via a call to bladerf_get_device_list
 *
 * @param[out]  device      Update with device handle on success
 * @param[in]   devinfo     Device specification
 *
 * @return 0 on success, or value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_open_with_devinfo(struct bladerf **device,
                                        struct bladerf_devinfo *devinfo);

/**
 * Open specified device using a device identifier string
 *
 * The general form of the device identifier string is;
 * @code
 *      <backend>:[device=<bus>:<addr>] [instance=<n>] [serial=<serial>]
 * @endcode
 *
 * An empty ("") or NULL device identifier will result in the first
 * encountered device being opened (using the first discovered backend)
 *
 * The 'backend' describes the mechanism used to communicate with the device,
 * and may be one of the following:
 *   - libusb:  libusb (See libusb changelog notes for required version, given
 *   your OS and controller)
 *   - linux:   Linux Kernel Driver
 *
 * If no arguments are provided after the backend, the first encountered
 * device on the specified backend will be opened. Note that a backend is
 * required, if any arguments are to be provided.
 *
 * Next, any provided arguments are provide as used to find the desired device.
 * Be sure not to over constrain the search. Generally, only one of the above
 * is required -- providing all of these may over constrain the search for the
 * desired device (e.g., if a serial number matches, but not on the specified
 * bus and address.)
 *
 *   - device=\<bus\>:\<addr\>
 *      - Specifies USB bus and address. Decimal or hex prefixed by '0x' is
 *        permitted.
 *   - instance=\<n\>
 *      - Nth instance encountered, 0-indexed (libusb)
 *      - Device node N, such as /dev/bladerfN (linux)
 *   - serial=\<serial\>
 *      - Device's serial number.
 *
 * @param[out]  device             Update with device handle on success
 * @param[in]   device_identifier  Device identifier, formatted as described above
 *
 * @return 0 on success, or value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_open(struct bladerf **device,
                           const char *device_identifier);

/**
 * Close device
 *
 * @note    Failing to close a device may result in memory leaks.
 *
 * @post    device is deallocated and may no longer be used.
 *
 * @param   device  Device handle previously obtained by bladerf_open(). This
 *                  function does nothing if device is NULL.
 */
API_EXPORT
void CALL_CONV bladerf_close(struct bladerf *device);

/** @} (End FN_INIT) */

/**
 * @defgroup FN_DEVINFO Device identifier information functions
 * @{
 */

/**
 * Initialize a device identifier information structure to a "wildcard" state.
 * The values in each field will match any value for that field.
 *
 * Passing a bladerf_devinfo initialized with this function to
 * bladerf_open_with_devinfo() will match the first device found.
 */
API_EXPORT
void CALL_CONV bladerf_init_devinfo(struct bladerf_devinfo *info);

/**
 * Fill out a provided bladerf_devinfo structure, given an open device handle.
 *
 * @pre dev must be a valid device handle.
 *
 * @param[in]    dev     Device handle previously obtained with bladerf_open()
 * @param[out]   info    Device information populated by this function
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_devinfo(struct bladerf *dev,
                                  struct bladerf_devinfo *info);

/**
 * Populate a device identifier information structure using the provided
 * device identifier string.
 *
 * @param[in]   devstr  Device identifier string, formated as described
 *                      in the bladerf_open() documentation
 *
 * @param[out]  info    Upon success, this will be filled out according to the
 *                      provided device identifier string, with wildcards for
 *                      any fields that were not provided.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_devinfo_from_str(const char *devstr,
                                           struct bladerf_devinfo *info);

/**
 * Test whether two device identifier information structures match, taking
 * wildcard values into account.
 */
API_EXPORT
bool CALL_CONV bladerf_devinfo_matches(const struct bladerf_devinfo *a,
                                       const struct bladerf_devinfo *b);

/**
 * Test whether a provided device string matches a device described by
 * the provided bladerf_devinfo structure
 *
 * @param[in]   dev_str     Devices string, formated as described in the
 *                          the documentation of bladerf_open
 *
 * @param[in]   info        Device info to compare with
 *
 * @return  true upon a match, false otherwise
 */
API_EXPORT
bool CALL_CONV bladerf_devstr_matches(const char *dev_str,
                                      struct bladerf_devinfo *info);
/** @} (End of FN_DEVINFO) */


/**
 * @defgroup FN_CTRL    Device control and configuration
 *
 * @{
 */

/**
 * Enable or disable the specified RX/TX module
 *
 * @param       dev     Device handle
 * @param       m       Device module
 * @param       enable  true to enable, false to disable
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_enable_module(struct bladerf *dev,
                                    bladerf_module m, bool enable);

/**
 * Apply specified loopback mode
 *
 * @param       dev     Device handle
 * @param       l       Loopback mode. Note that LB_NONE disables the use
 *                      of loopback functionality.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_loopback(struct bladerf *dev,
                                   bladerf_loopback l);


/**
 * Configure the device's sample rate, in Hz.  Note this requires the sample
 * rate is an integer value of Hz.  Use bladerf_set_rational_sample_rate()
 * for more arbitrary values.
 *
 * @param[in]   dev         Device handle
 * @param[in]   module      Module to change
 * @param[in]   rate        Sample rate
 * @param[out]  actual      Actual sample rate
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_sample_rate(struct bladerf *dev,
                                      bladerf_module module,
                                      unsigned int rate,
                                      unsigned int *actual);

/**
 * Configure the device's sample rate as a rational fraction of Hz.
 * Sample rates are in the form of integer + num/denom.
 *
 * @param[in]   dev         Device handle
 * @param[in]   module      Module to change
 * @param[in]   rate        Rational sample rate
 * @param[out]  actual      Actual rational sample rate
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_rational_sample_rate(struct bladerf *dev,
                                               bladerf_module module,
                                               struct bladerf_rational_rate *rate,
                                               struct bladerf_rational_rate *actual);

/**
 * Configure the sampling of the LMS6002D to be either internal or
 * external.  Internal sampling will read from the RXVGA2 driver internal
 * to the chip.  External sampling will connect the ADC inputs to the
 * external inputs for direct sampling.
 *
 * @param[in]   dev         Device handle
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
 * @param[in]   dev         Device handle
 * @param[out]  sampling    Sampling connection
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_sampling(struct bladerf *dev,
                                   bladerf_sampling *sampling);

/**
 * Read the device's sample rate in Hz
 *
 * @param[in]   dev         Device handle
 * @param[in]   module      Module to query
 * @param[out]  rate        Pointer to returned sample rate
 *
 * @return 0 on success, value from \ref RETCODES list upon failure
 */
API_EXPORT
int CALL_CONV bladerf_get_sample_rate(struct bladerf *dev,
                                      bladerf_module module,
                                      unsigned int *rate);

/**
 * Read the device's sample rate in rational Hz
 *
 * @param[in]   dev         Device handle
 * @param[in]   module      Module to query
 * @param[out]  rate        Pointer to returned rational sample rate
 *
 * @return 0 on success, value from \ref RETCODES list upon failure
 */
API_EXPORT
int CALL_CONV bladerf_get_rational_sample_rate(struct bladerf *dev,
                                               bladerf_module module,
                                               struct bladerf_rational_rate *rate);

/**
 * Set the value of the specified configuration parameter
 *
 * @param   dev         Device handle
 * @param   module      Module to apply correction to
 * @param   corr        Correction type
 * @param   value       Value to apply
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int bladerf_set_correction(struct bladerf *dev, bladerf_module module,
                           bladerf_correction corr, int16_t value);

/**
 * Obtain the current value of the specified configuration parameter
 *
 * @param[in]   dev         Device handle
 * @param[in]   module      Module to retrieve correction information from
 * @param[in]   corr        Correction type
 * @param[out]  value       Current value
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int bladerf_get_correction(struct bladerf *dev, bladerf_module module,
                           bladerf_correction corr, int16_t *value);
/**
 * Set the PA gain in dB
 *
 * @param       dev         Device handle
 * @param       gain        Desired gain
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_txvga2(struct bladerf *dev, int gain);

/**
 * Get the PA gain in dB
 *
 * @param       dev         Device handle
 * @param       gain        Pointer to returned gain
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT int
CALL_CONV bladerf_get_txvga2(struct bladerf *dev, int *gain);

/**
 * Set the post-LPF gain in dB
 *
 * @param       dev         Device handle
 * @param       gain        Desired gain
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_txvga1(struct bladerf *dev, int gain);

/**
 * Get the post-LPF gain in dB
 *
 * @param       dev         Device handle
 * @param       gain        Pointer to returned gain
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_txvga1(struct bladerf *dev, int *gain);

/**
 * Set the LNA gain
 *
 * @param       dev         Device handle
 * @param       gain        Desired gain level
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_lna_gain(struct bladerf *dev, bladerf_lna_gain gain);

/**
 * Get the LNA gain
 *
 * @param       dev         Device handle
 * @param       gain        Pointer to the set gain level
 */
API_EXPORT
int CALL_CONV bladerf_get_lna_gain(struct bladerf *dev, bladerf_lna_gain *gain);

/**
 * Set the pre-LPF VGA gain
 *
 * @param       dev         Device handle
 * @param       gain        Desired gain
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_rxvga1(struct bladerf *dev, int gain);

/**
 * Get the pre-LPF VGA gain
 *
 * @param       dev         Device handle
 * @param       gain        Pointer to the set gain level
 */
API_EXPORT
int CALL_CONV bladerf_get_rxvga1(struct bladerf *dev, int *gain);

/**
 * Set the post-LPF VGA gain
 *
 * @param       dev         Device handle
 * @param       gain        Desired gain
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_rxvga2(struct bladerf *dev, int gain);

/**
 * Get the post-LPF VGA gain
 *
 * @param       dev         Device handle
 * @param       gain        Pointer to the set gain level
 */
API_EXPORT
int CALL_CONV bladerf_get_rxvga2(struct bladerf *dev, int *gain);

/**
 * Set the bandwidth to specified value in Hz
 *
 * @param       dev                 Device handle
 * @param       module              Module for bandwidth request
 * @param       bandwidth           Desired bandwidth
 * @param       actual              If non-NULL, written with the actual
 *                                  bandwidth that the device was able to
 *                                  achieve
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_bandwidth(struct bladerf *dev, bladerf_module module,
                                    unsigned int bandwidth,
                                    unsigned int *actual);

/**
 * Get the bandwidth of the LMS LPF
 *
 * @param       dev                 Device Handle
 * @param       module              Module for bandwidth request
 * @param       bandwidth           Actual bandwidth in Hz
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_bandwidth(struct bladerf *dev, bladerf_module module,
                                    unsigned int *bandwidth);

/**
 * Set the LMS LPF mode to bypass or disable it
 *
 * @param       dev         Device handle
 * @param       module      Module for mode request
 * @param       mode        Mode to be set
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_lpf_mode(struct bladerf *dev, bladerf_module module,
                                   bladerf_lpf_mode mode);

/**
 * Get the current mode of the LMS LPF
 *
 * @param       dev         Device handle
 * @param       module      Module for mode request
 * @param       mode        Current mode of the LPF
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_lpf_mode(struct bladerf *dev, bladerf_module module,
                                   bladerf_lpf_mode *mode);

/**
 * Select the appropriate band path given a frequency in Hz.
 *
 * @param       dev         Device handle
 * @param       module      Module to configure
 * @param       frequency   Tuned frequency
 */
API_EXPORT
int CALL_CONV bladerf_select_band(struct bladerf *dev, bladerf_module module,
                                  unsigned int frequency);

/**
 * Set module's frequency in Hz.
 *
 * @param       dev         Device handle
 * @param       module      Module to configure
 * @param       frequency   Desired frequency
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_frequency(struct bladerf *dev,
                                    bladerf_module module,
                                    unsigned int frequency);

/**
 * Set module's frequency in Hz
 *
 * @param       dev         Device handle
 * @param       module      Module to configure
 * @param       frequency   Pointer to the returned frequency
 */
API_EXPORT
int CALL_CONV bladerf_get_frequency(struct bladerf *dev,
                                    bladerf_module module,
                                    unsigned int *frequency);

/** @} (End of FN_CTRL) */


/**
 * @defgroup FN_DATA    Data transmission and reception
 *
 * @{
 */


/**
 * Initialize a stream for use with asynchronous routines
 *
 * @param[out]  stream          Upon success, this will be updated to contain
 *                              a stream handle (i.e., address)
 *
 * @param[in]   dev             Device to associate with the stream
 *
 * @param[in]   callback        Callback routine to handle asynchronous events
 *
 * @param[out]  buffers         This will be updated to point to a dynamically
 *                              allocated array of buffer pointers.
 *
 * @param[in]   num_buffers     Number of buffers to allocate and return
 *
 * @param[in]   format          Sample data format
 *
 * @param[in]   num_samples     Number of samples in each buffer of samples.
 *                              Note that the physical size of the buffer
 *                              is a function of this and the format parameter.
 *
 * @param[in]   num_transfers   Maximum number of transfers that may be
 *                              in-flight simultaneous. This must be <= the
 *                              number of buffers.
 *
 * @param[in]   user_data       Caller-provided data that will be provided
 *                              in stream callbacks
 *
 *
 * @note  This call should be later followed by a call to
 *        bladerf_deinit_stream() to avoid memory leaks.
 *
 * @return 0 on success,
 *          value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_init_stream(struct bladerf_stream **stream,
                                  struct bladerf *dev,
                                  bladerf_stream_cb callback,
                                  void ***buffers,
                                  size_t num_buffers,
                                  bladerf_format format,
                                  size_t num_samples,
                                  size_t num_transfers,
                                  void *user_data);

/**
 * Begin running  a stream. This call will block until the steam completes.
 *
 * Only 1 RX stream and 1 TX stream may be running at a time. Attempting to
 * call bladerf_stream() with more than one stream per module will yield
 * unexpected (and most likely undesirable) results.
 *
 * When running a full-duplex configuration with two threads (e.g,
 * one thread calling bladerf_stream() for TX, and another for RX), stream
 * callbacks may be executed in either thread. Therefore, the caller is
 * responsible for ensuring that his or her callbacks are thread-safe. For the
 * same reason, it is highly recommended that callbacks do not block.
 *
 * When starting a TX stream, an initial set of callbacks will be immediately
 * invoked. The caller must ensure that there are at *more than* T buffers
 * filled before calling bladerf_stream(..., BLADERF_MODULE_TX), where T is the
 * num_transfers value provided to bladerf_init_stream(), to avoid an underrun.
 *
 * @param   stream  A stream handle that has been successfully been initialized
 *                  via bladerf_init_stream()
 *
 * @param   module  Module to perform streaming with
 *
 * @return  0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_stream(struct bladerf_stream *stream,
                             bladerf_module module);

/**
 * Deinitialize and deallocate stream resources.
 *
 * @post   Stream is deallocated and may no longer be used.
 *
 * @param   stream      Stream to deinitialize. This function does nothin
 *                      if stream is NULL.
 */
API_EXPORT
void CALL_CONV bladerf_deinit_stream(struct bladerf_stream *stream);

/**
 * Transmit IQ samples
 *
 * @param[in]   dev         Device handle
 * @param[in]   format      Format of the provided samples
 * @param[in]   samples     Array of samples
 * @param[in]   num_samples Number of samples to write
 * @param[in]   metadata    Sample metadata. This parameter is currently
 *                          unused.
 *
 * @note When using the libusb backend, this function will likely be too slow
 *       for mid to high sample rates. For anything other than slow sample
 *       rates, the bladerf_stream() function is better choice.
 *
 * @warning This function is scheduled to be removed and replaced by a
 *          synchronous companion library that utilizes the async interface
 *
 * @return number of samples sent on success,
 *          value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_tx(struct bladerf *dev, bladerf_format format,
                         void *samples, int num_samples,
                         struct bladerf_metadata *metadata);

/**
 * Receive IQ samples
 *
 * @param[in]   dev         Device handle
 * @param[in]   format      The data format that the received data should be in.
 *                          The caller is responsible for ensuring the provided
 *                          sample buffer is sufficiently large.
 *
 * @param[out]  samples     Buffer to store samples in
 * @param[in]   num_samples Number of samples to read
 * @param[out]  metadata    Sample metadata. This parameter is currently
 *                          unused.
 *
 * @note When using the libusb backend, this function will likely be too slow
 *       for mid to high sample rates. For anything other than slow sample
 *       rates, the bladerf_stream() function is better choice.
 *
 * @warning This function is scheduled to be removed and replaced by a
 *          synchronous companion library that utilizes the async interface
 *
 * @return number of samples read or value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_rx(struct bladerf *dev, bladerf_format format,
                         void *samples, int num_samples,
                         struct bladerf_metadata *metadata);

/**
 * Set transfer timeout in milliseconds
 *
 * @param   dev         Device handle
 * @param   module      Module to adjust
 * @param   timeout     Timeout in milliseconds
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_transfer_timeout(struct bladerf *dev,
                                            bladerf_module module,
                                            unsigned int timeout);


/**
 * Get transfer timeout in milliseconds
 *
 * @param[in]   dev         Device handle
 * @param[in]   module      Module to adjust
 * @param[out]  timeout     On success, updated with current transfer
 *                          timeout value. Undefined on failure.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_transfer_timeout(struct bladerf *dev,
                                           bladerf_module module,
                                           unsigned int *timeout);

/** @} (End of FN_DATA) */



/**
 * @defgroup FN_INFO    Device info
 *
 * @{
 */

/**
 * Query a device's serial number
 *
 * @param[in]   dev     Device handle
 * @param[out]  serial  Will be updated with serial number. If an error occurs,
 *                      no data will be written to this pointer.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_serial(struct bladerf *dev, char *serial);

/**
 * Query a device's VCTCXO calibration trim
 *
 * @param[in]   dev     Device handle
 * @param[out]  trim    Will be updated with the factory DAC trim value. If an
 *                      error occurs, no data will be written to this pointer.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_vctcxo_trim(struct bladerf *dev, uint16_t *trim);

/**
 * Query a device's FPGA size
 *
 * @param[in]   dev     Device handle
 * @param[out]  size    Will be updated with the onboard FPGA's size. If an
 *                      error occurs, no data will be written to this pointer.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_fpga_size(struct bladerf *dev,
                                    bladerf_fpga_size *size);

/**
 * Query firmware version
 *
 * @param[in]   dev         Device handle
 * @param[out]  version     Updated to contain firmware version
 *
 * @return 0 on success, value from \ref RETCODES list upon failing to retrieve
 *         this information from the device.
 */
API_EXPORT
int CALL_CONV bladerf_fw_version(struct bladerf *dev,
                                 struct bladerf_version *version);

/**
 * Check FPGA configuration status
 *
 * @param   dev     Device handle
 *
 * @return  1 if FPGA is configured,
 *          0 if it is not,
 *          and value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_is_fpga_configured(struct bladerf *dev);

/**
 * Query FPGA version
 *
 * @param[in]   dev         Device handle
 * @param[out]  version     Updated to contain firmware version
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_fpga_version(struct bladerf *dev,
                                   struct bladerf_version *version);

/**
 * Obtain device statistics
 *
 * @param[in]   dev     Device handle
 * @param[out]  stats   Current device statistics
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_stats(struct bladerf *dev, struct bladerf_stats *stats);

/**
 * Obtain the bus speed at which the device is operating
 *
 * @param       dev     Device handle
 * @return      speed   Device speed
 */
API_EXPORT
bladerf_dev_speed CALL_CONV bladerf_device_speed(struct bladerf *dev);

/** @} (End FN_INFO) */


/**
 * @defgroup FN_PROG  Device loading and programming
 *
 * @{
 */

/**
 * Flash firmware onto the device
 *
 * @note This will require a power cycle to take effect
 *
 * @param   dev         Device handle
 * @param   firmware    Full path to firmware file
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_flash_firmware(struct bladerf *dev,
                                     const char *firmware);

/**
 * Load device's FPGA
 *
 * @param   dev         Device handle
 * @param   fpga        Full path to FPGA bitstream
 *
 * @return 0 upon successfully, or a value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_load_fpga(struct bladerf *dev, const char *fpga);

/**
 * Flash FPGA image onto the device
 *
 * @param   dev         Device handle
 * @param   fpga_image  Full path to FPGA file
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_flash_fpga(struct bladerf *dev,
                                 const char *fpga_image);

/**
 * Reset the device, causing it to reload its firmware from flash
 *
 * @param   dev         Device handle
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_device_reset(struct bladerf *dev);

/**
 * Jump to FX3 bootloader
 *
 * @note This also causes the device to jump to the FX3 bootloader
 *
 * @param   dev         Device handle
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_jump_to_bootloader(struct bladerf *dev);

/* @} (End of FN_PROG) */


/**
 * @defgroup FN_MISC Miscellaneous
 * @{
 */

/**
 * Obtain a textual description of a value from the \ref RETCODES list
 *
 * @warning Do not attempt to modify the returned string.
 *
 * @param   error   Error value to look up
 * @return  Error string
 */
API_EXPORT
const char * CALL_CONV bladerf_strerror(int error);

/**
 * Get libbladeRF version information
 *
 * @param[out]  version     libbladeRF version information
 */
API_EXPORT
void CALL_CONV bladerf_version(struct bladerf_version *version);

/**
 * Sets the filter level for displayed log messages. Messages that are at
 * or above the specified log level will be written to the log output, while
 * messages with a lower log level will be suppressed. This function returns
 * the previous log level.
 *
 * @param   level       The new log level filter value
 */
API_EXPORT
void CALL_CONV bladerf_log_set_verbosity(bladerf_log_level level);

/** @} (End of FN_MISC) */

/**
 * @defgroup FN_IMAGE Flash image format
 *
 * This section contains a file format and associated routines for storing
 * and loading flash contents with metadata
 *
 * @{
 */

/** Type of data stored in a flash image */
typedef enum {
    BLADERF_IMAGE_TYPE_INVALID = -1,    /** Used to denote invalid value */
    BLADERF_IMAGE_TYPE_RAW,             /** Misc. raw data */
    BLADERF_IMAGE_TYPE_FIRMWARE,        /** Firmware data */
    BLADERF_IMAGE_TYPE_FPGA_40KLE,      /** FPGA bitstream for 40 KLE device */
    BLADERF_IMAGE_TYPE_FPGA_115KLE,     /** FPGA bitstream for 115  KLE device */
    BLADERF_IMAGE_TYPE_CALIBRATION,     /** Calibration data */
} bladerf_image_type;

/**
 * Size of the magic signature at the beginning of bladeRF image files
 */
#define BLADERF_IMAGE_MAGIC_LEN 7

/**
 * Size of bladeRF flash image checksum
 */
#define BLADERF_IMAGE_CHECKSUM_LEN 32

/**
 * Size of reserved region of flash image
 */
#define BLADERF_IMAGE_RESERVED_LEN 128

/**
 * Image format for backing up and restoring bladeRF flash contents
 *
 * The on disk format generated by the bladerf_image_write function is a
 * serialized version of this structure and its contents. When written to disk,
 * values are CALL_CONVerted to big-endian byte order, for ease of reading in a hex
 * editor.
 */
struct bladerf_image {

    /**
     * Magic value used to identify image file format.
     *
     * Note that an extra character is added to store a NUL-terminator,
     * to allow this field to be printed. This NUL-terminator is *NOT*
     * written in the serialized image.
     */
    char magic[BLADERF_IMAGE_MAGIC_LEN + 1];

    /**
     * SHA256 checksum of the flash image. This is computed over the entire
     * image, with this field filled with 0's.
     */
    uint8_t checksum[BLADERF_IMAGE_CHECKSUM_LEN];

    /**
     * Image format version. Only the major, minor, and patch fields are
     * written to the disk; the describe field is not used. The version is
     * serialized as: [major | minor | patch]
     */
    struct bladerf_version version;

    /** UTC image timestamp, in seconds since the Unix Epoch */
    uint64_t timestamp;

    /**
     * Serial number of the device that the image was obtained from. This
     * field should be all '\0' if irrelevant.
     *
     * Note that an extra character is added to store a NUL-terminator,
     * to allow this field to be printed. This NUL-terminator is *NOT*
     * written in the serialized image.
     */
    char serial[BLADERF_SERIAL_LENGTH + 1];

    /**
     * Reserved for future metadata. Should be 0's.
     */
    char reserved[BLADERF_IMAGE_RESERVED_LEN];

    /**
     * Type of data contained in the image. Serialized as a uint32_t.
     */
    bladerf_image_type type;

    /**
     * Address of the flash data in this image. A value of 0xffffffff
     * implies that this field is left unspecified (i.e., "don't care").
     */
    uint32_t address;

    /** Length of the data contained in the image */
    uint32_t length;

    /** Image data */
    uint8_t *data;
};

/**
 * Allocate and initialize an image structure.
 *
 * This following bladerf_image fields are populated: `magic`, `version`,
 * `timestamp`, `type`, `address`, and `length`
 *
 * The following bladerf_image fields are zeroed out:  `checksum`, `serial`, and
 * `reserved`,
 *
 * If the `length` parameter is not 0, the bladerf_image `data` field will be
 * dynamically allocated. Otherwise, `data` will be set to NULL.
 *
 * @note A non-zero `lenth` should be use only with bladerf_image_write();
 * bladerf_image_read() allocates and sets `data` based upon size of the image
 * contents, and does not attempt to free() the `data` field before setting it.
 *
 * The `address` and `length` fields should be set 0 when reading an image from
 * a file.
 *
 * @return Pointer to allocated and initialized structure on success,
 *         NULL on memory allocation failure
 */
API_EXPORT
struct bladerf_image * CALL_CONV bladerf_alloc_image(bladerf_image_type type,
                                                     uint32_t address,
                                                     uint32_t length);

/**
 * Create a flash image initialized to contain a calibration data region.
 * This is intended to be used in conjunction with bladerf_image_write(),
 * or a write of the image's `data` field to flash.
 *
 * @param   fpga_size    Target FPGA size
 * @param   vctcxo_trim  VCTCXO oscillator trim value.
 *
 * @return Pointer to allocated and initialized structure on success,
 *         NULL on memory allocation failure
 */
API_EXPORT
struct bladerf_image * CALL_CONV bladerf_alloc_cal_image(bladerf_fpga_size fpga_size,
                                                         uint16_t vctcxo_trim);

/**
 * Free a bladerf_image previously obtained via bladerf_alloc_image.
 * If the bladerf_image's `data` field is non-NULL, it will be freed.
 */
API_EXPORT
void CALL_CONV bladerf_free_image(struct bladerf_image *image);


/**
 * Write a flash image to a file.
 *
 * This function will fill in the checksum field before writing the contents to
 * the specified file. The user-supplied contents of this field are ignored.
 *
 * @pre   `image` has been initialized using bladerf_image_init()
 * @post `image->checksum` will be populated if this function succeeds
 *
 * @param[in]    image       Flash image
 * @param[in]    file        File to write the flash image to
 *
 * @return 0 upon success, or a value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_image_write(struct bladerf_image *image,
                                  const char *file);

/**
 * Read flash image from a file.
 *
 * @param[out]   image      Flash image structure to populate.
 *
 * @param[in]    file       File to read image from.
 *
 * @pre  The `image` parameter has been obtained via a call to
 *       bladerf_alloc_image(), with a `length` of 0.
 *
 * @post The `image` fields will be populated upon success, overwriting
 *       any previous values.
 *
 * @note The contents of the `image` paramater should not be used if this
 *       function fails.
 *
 *
 * @return 0 upon success,<br>
 *         BLADERF_ERR_CHECKSUM upon detecting a checksum mismatch,<br>
 *         BLADERF_ERR_INVAL if any image fields are invalid,<br>
 *         BLADERF_ERR_IO on a file I/O error,<br>
 *         or a value from \ref RETCODES list on any other failure<br>
 */
API_EXPORT
int CALL_CONV bladerf_image_read(struct bladerf_image *image, const char *file);

/** @} (End of FN_IMAGE) */



/**
 * @defgroup LOW_LEVEL Low-level development and testing routines
 *
 * In a most cases, higher-level routines should be used. These routines are
 * only intended to support development and testing.   Treat these routines as
 * if they may disappear in future revision of the API; do not depend on them
 * for any long-term software.
 *
 * Use these routines with great care, and be sure to reference the relevant
 * schematics, datasheets, and source code (e.g., firmware and hdl).
 *
 * Be careful when mixing these calls with higher-level routines that manipulate
 * the same registers/settings.
 *
 *
 * @{
 */

/**
 * Read a Si5338 register
 *
 * @param   dev         Device handle
 * @param   address     Si5338 register offset
 * @param   val         Pointer to variable the data should be read into
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_si5338_read(struct bladerf *dev,
                                  uint8_t address, uint8_t *val);

/**
 * Write a Si5338 register
 *
 * @param   dev         Device handle
 * @param   address     Si5338 register offset
 * @param   val         Data to write to register
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_si5338_write(struct bladerf *dev,
                                   uint8_t address, uint8_t val);

/**
 * Set frequency for TX clocks
 *
 * @param   dev         Device handle
 * @param   freq        Desired TX frequency in Hz
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_si5338_set_tx_freq(struct bladerf *dev, unsigned freq);

/**
 * Set frequency for RX clocks
 *
 * @param   dev         Device handle
 * @param   freq        Desired RX frequency in Hz
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_si5338_set_rx_freq(struct bladerf *dev, unsigned freq);


/**
 * Read a LMS register
 *
 * @param   dev         Device handle
 * @param   address     LMS register offset
 * @param   val         Pointer to variable the data should be read into
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_lms_read(struct bladerf *dev,
                               uint8_t address, uint8_t *val);

/**
 * Write a LMS register
 *
 * @param   dev         Device handle
 * @param   address     LMS register offset
 * @param   val         Data to write to register
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_lms_write(struct bladerf *dev,
                                uint8_t address, uint8_t val);

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
#define BLADERF_GPIO_LMS_TX_ENABLE  (1 << 2)

/**
 * Switch to use TX low band (300MHz - 1.5GHz)
 *
 * @note This is set using bladerf_set_frequency().
 */
#define BLADERF_GPIO_TX_LB_ENABLE   (2 << 3)

/**
 * Switch to use TX high band (1.5GHz - 3.8GHz)
 *
 * @note This is set using bladerf_set_frequency().
 */
#define BLADERF_GPIO_TX_HB_ENABLE   (1 << 3)

/**
 * Switch to use RX low band (300M - 1.5GHz)
 *
 * @note THis is set using bladerf_set_frequency().
 */
#define BLADERF_GPIO_RX_LB_ENABLE   (2 << 5)

/**
 * Switch to use RX high band (1.5GHz - 3.8GHz)
 *
 * @note This is set using bladerf_set_frequency().
 */
#define BLADERF_GPIO_RX_HB_ENABLE   (1 << 5)

/**
 * This GPIO bit configures the FPGA to use smaller DMA
 * transfers (256 cycles instead of 512). This is required
 * when the device is not connected at Super Speed (i.e., when
 * it is connected at High Speed).
 *
 * However, the caller need not set this in gpio_set() calls.
 * The library will set this as needed; callers generally
 * do not need to be concerned with setting/clearing this bit.
 */
#define BLADERF_GPIO_FEATURE_SMALL_DMA_XFER (1 << 7)

/**
 * Read a configuration GPIO register
 *
 * @param   dev         Device handle
 * @param   val         Pointer to variable the data should be read into
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_config_gpio_read(struct bladerf *dev, uint32_t *val);

/**
 * Write a configuration GPIO register. Callers should be sure to perform a
 * read-modify-write sequence to avoid accidentally clearing other
 * GPIO bits that may be set by the library internally.
 *
 * @param   dev         Device handle
 * @param   val         Data to write to GPIO register
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_config_gpio_write(struct bladerf *dev, uint32_t val);


/**
 * Write value to VCTCXO DAC
 *
 * @param   dev         Device handle
 * @param   val         Data to write to DAC register
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_dac_write(struct bladerf *dev, uint16_t val);

/**
 * Calibration routines
 *
 * @param   dev         Device handle
 * @param   module      Module to calibrate
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_calibrate_dc(struct bladerf *dev,
                                   bladerf_cal_module module);

/* @} (End of LOW_LEVEL) */

/**
 * @defgroup FN_FLASH  Low-level flash routines
 *
 * These routines provide the ability to manipulate the device's SPI flash.
 * Most users will find no reason to use these, as higher-level functions
 * perform flash accesses under the hood.
 *
 * Be sure to review the following page and the associated flash datasheet
 * before using these functions:
 *
 *   https://github.com/nuand/bladeRF/wiki/FX3-Firmware#spi-flash-layout
 *
 * @{
 */
#define BLADERF_FLASH_PAGE_BITS   (8)  /**< 256bytes == 2^8  bytes */
#define BLADERF_FLASH_SECTOR_BITS (16) /**< 64KiB    == 2^16 bytes */
#define BLADERF_FLASH_SIZE_BITS   (22) /**< 32Mbit   == 2^22 bytes */

/** Total size of bladeRF SPI flash */
#define BLADERF_FLASH_TOTAL_SIZE  (1<<BLADERF_FLASH_SIZE_BITS)

/** SPI flash page size */
#define BLADERF_FLASH_PAGE_SIZE   (1<<BLADERF_FLASH_PAGE_BITS)

/** SPI flash sector size */
#define BLADERF_FLASH_SECTOR_SIZE (1<<BLADERF_FLASH_SECTOR_BITS)

/** Size of the SPI flash, in bytes */
#define BLADERF_FLASH_NUM_BYTES BLADERF_FLASH_TOTAL_SIZE

/** Size of the SPI flash, in pages */
#define BLADERF_FLASH_NUM_PAGES \
    (BLADERF_FLASH_TOTAL_SIZE / BLADERF_FLASH_PAGE_SIZE)

/** Size of the SPI flash, in sectors */
#define BLADERF_FLASH_NUM_SECTORS \
    (BLADERF_FLASH_TOTAL_SIZE / BLADERF_FLASH_SECTOR_SIZE)

/** Address of firmware in flash */
#define BLADERF_FLASH_ADDR_FIRMWARE     0x00000000

/** Length of firmware region in flash*/
#define BLADERF_FLASH_LEN_FIRMWARE      0x00030000

/** Address of calibration data in flash */
#define BLADERF_FLASH_ADDR_CALIBRATION  0x00030000

/** Length of calibration data region */
#define BLADERF_FLASH_LEN_CALIBRATION   0x100

/** Address of FPGA metadata */
#define BLADERF_FLASH_ADDR_FPGA_META    0x00040000

/** Length of FPGA metadata */
#define BLADERF_FLASH_LEN_FPGA_META     0x100

/** Address of FPGA bitstream for autoloading */
#define BLADERF_FLASH_ADDR_FPGA         0x00040100

/** Length of of FPGA bistream */
#define BLADERF_FLASH_LEN_FPGA          0x003BFF00

/**
 * Erase sectors from FX3 flash device
 *
 * @note Unlike the bladerf_read_flash/bladerf_write_flash functions this
 *       function expects a BLADERF_FLASH_SECTOR_SIZE aligned address and
 *       length.
 *
 * @param   dev     Device handle
 * @param   addr    Page aligned byte address of the first sector to erase
 * @param   len     Number of bytes to erase, must be a multiple of
 *                  BLADERF_FLASH_SECTOR_SIZE
 *
 * @return Positive number of bytes erased on success, negative value from \ref
 *         RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_erase_flash(struct bladerf *dev, uint32_t addr,
                                  uint32_t len);

/**
 * Read bytes from FX3 flash device
 *
 * @note Unline the `bladerf_erase_flash' function this function expects a
 *       BLADERF_FLASH_PAGE_SIZE aligned address and length.
 *
 * @param   dev   Device handle
 * @param   addr  Page aligned byte address of the first page to read
 * @param   buf   Buffer to read into, must be at least `len' bytes long
 * @param   len   Number of bytes to read, must be a multiple of
 *                BLADERF_FLASH_PAGE_SIZE
 *
 * @return Positive number of bytes read on success, negative value from \ref
 *         RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_read_flash(struct bladerf *dev, uint32_t addr,
                                 uint8_t *buf, uint32_t len);

/**
 * Read an unaligned region of flash memory
 *
 * @param   dev   Device handle
 * @param   addr  Unaligned byte address of first byte to read
 * @param   pbuf  Buffer to read into, must be at least `len' bytes long
 * @param   len   Number of bytes to write. (No alignment requirement)
 *
 * @return Positive number of bytes read on success, negative value from \ref
 *         RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_read_flash_unaligned(struct bladerf *dev, uint32_t addr,
                                           uint8_t *pbuf, uint32_t len);

/**
 * Write bytes to FX3 flash device
 *
 * @note Unline the `bladerf_erase_flash' function this function expects a
 *       BLADERF_FLASH_PAGE_SIZE aligned address and length.
 *
 * @param   dev   Device handle
 * @param   addr  Page aligned byte address of the first page to write
 * @param   buf   Data to write to flash
 * @param   len   Number of bytes to write, must be a multiple of
 *                BLADERF_FLASH_PAGE_SIZE
 *
 * @return Positive number of bytes written on success, negative value from \ref
 *         RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_write_flash(struct bladerf *dev, uint32_t addr,
                                  uint8_t *buf, uint32_t len);

/**
 * Program an unaligned region of flash memory (read/erase/write/verify).
 *
 * @note This function performs a full read/erase/write/verify cycle and the
 * aligned variants should be prefered whenever possible.
 *
 * @param   dev   Device handle
 * @param   addr  Unaligned byte address of destination
 * @param   buf   Data to write to flash
 * @param   len   Number of bytes to write. (No alignment requirement)
 *
 * @return Positive number of bytes written on success, negative value from \ref
 *         RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_program_flash_unaligned(struct bladerf *dev,
                                              uint32_t addr,
                                              uint8_t *buf,
                                              uint32_t len);

/* @} (End of FN_FLASH) */


#ifdef __cplusplus
}
#endif

#endif /* BLADERF_H_ */
