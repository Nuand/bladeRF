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

/** Marks an API routine to be made visible to dynamic loader  */
#if defined _WIN32 || defined _CYGWIN__
#   ifdef __GNUC__
#       define API_EXPORT __attribute__ ((dllexport))
#   else
#       define API_EXPORT __declspec(dllexport)
#   endif
#else
#   define API_EXPORT __attribute__ ((visibility ("default")))
#endif

/** This structure is an opaque device handle */
struct bladerf;

/** This opaque structure is used to keep track of stream information */
struct bladerf_stream;

/**
 * @defgroup    RETCODES    Error return codes
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
    BLADERF_FORMAT_SC16_Q12, /**< Signed, Complex 16-bit Q12.
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
    uint64_t timestamp;     /**< TODO Time in \<unit\> since \<origin\> */
};

/**
 * Sampling connection
 */
typedef enum {
    BLADERF_SAMPLING_UNKNOWN,
    BLADERF_SAMPLING_INTERNAL,
    BLADERF_SAMPLING_EXTERNAL
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
API_EXPORT int bladerf_get_device_list(struct bladerf_devinfo **devices);

/**
 * Free device list returned by bladerf_get_device_list()
 *
 * @param   devices     List of available devices
 */
API_EXPORT void bladerf_free_device_list(struct bladerf_devinfo *devices);

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
API_EXPORT int bladerf_open_with_devinfo(struct bladerf **device,
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
API_EXPORT int bladerf_open(struct bladerf **device,
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
API_EXPORT void bladerf_close(struct bladerf *device);

/** @} (End FN_INIT) */

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
API_EXPORT int bladerf_enable_module(struct bladerf *dev,
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
API_EXPORT int bladerf_set_loopback( struct bladerf *dev, bladerf_loopback l);


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
API_EXPORT int bladerf_set_sample_rate(struct bladerf *dev, bladerf_module module,
                                       unsigned int rate, unsigned int *actual);

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
API_EXPORT int bladerf_set_rational_sample_rate(struct bladerf *dev,
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
API_EXPORT int bladerf_set_sampling(struct bladerf *dev,
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
API_EXPORT int bladerf_get_sampling(struct bladerf *dev,
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
API_EXPORT int bladerf_get_sample_rate(struct bladerf *dev,
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
API_EXPORT int bladerf_get_rational_sample_rate(struct bladerf *dev,
                                                bladerf_module module,
                                                struct bladerf_rational_rate *rate);

/**
 * Set the PA gain in dB
 *
 * @param       dev         Device handle
 * @param       gain        Desired gain
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_set_txvga2(struct bladerf *dev, int gain);

/**
 * Get the PA gain in dB
 *
 * @param       dev         Device handle
 * @param       gain        Pointer to returned gain
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_get_txvga2(struct bladerf *dev, int *gain);

/**
 * Set the post-LPF gain in dB
 *
 * @param       dev         Device handle
 * @param       gain        Desired gain
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_set_txvga1(struct bladerf *dev, int gain);

/**
 * Get the post-LPF gain in dB
 *
 * @param       dev         Device handle
 * @param       gain        Pointer to returned gain
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_get_txvga1(struct bladerf *dev, int *gain);

/**
 * Set the LNA gain
 *
 * @param       dev         Device handle
 * @param       gain        Desired gain level
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_set_lna_gain(struct bladerf *dev, bladerf_lna_gain gain);

/**
 * Get the LNA gain
 *
 * @param       dev         Device handle
 * @param       gain        Pointer to the set gain level
 */
API_EXPORT int bladerf_get_lna_gain(struct bladerf *dev, bladerf_lna_gain *gain);

/**
 * Set the pre-LPF VGA gain
 *
 * @param       dev         Device handle
 * @param       gain        Desired gain
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_set_rxvga1(struct bladerf *dev, int gain);

/**
 * Get the pre-LPF VGA gain
 *
 * @param       dev         Device handle
 * @param       gain        Pointer to the set gain level
 */
API_EXPORT int bladerf_get_rxvga1(struct bladerf *dev, int *gain);

/**
 * Set the post-LPF VGA gain
 *
 * @param       dev         Device handle
 * @param       gain        Desired gain
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_set_rxvga2(struct bladerf *dev, int gain);

/**
 * Get the post-LPF VGA gain
 *
 * @param       dev         Device handle
 * @param       gain        Pointer to the set gain level
 */
API_EXPORT int bladerf_get_rxvga2(struct bladerf *dev, int *gain);

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
API_EXPORT int bladerf_set_bandwidth(struct bladerf *dev, bladerf_module module,
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
API_EXPORT int bladerf_get_bandwidth(struct bladerf *dev, bladerf_module module,
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
API_EXPORT int bladerf_set_lpf_mode(struct bladerf *dev, bladerf_module module,
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
API_EXPORT int bladerf_get_lpf_mode(struct bladerf *dev, bladerf_module module,
                                    bladerf_lpf_mode *mode);

/**
 * Select the appropriate band path given a frequency in Hz.
 *
 * @param       dev         Device handle
 * @param       module      Module to configure
 * @param       frequency   Tuned frequency
 */
API_EXPORT int bladerf_select_band(struct bladerf *dev, bladerf_module module,
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
API_EXPORT int bladerf_set_frequency(struct bladerf *dev,
                                     bladerf_module module,
                                     unsigned int frequency);

/**
 * Set module's frequency in Hz
 *
 * @param       dev         Device handle
 * @param       module      Module to configure
 * @param       frequency   Pointer to the returned frequency
 */
API_EXPORT int bladerf_get_frequency(struct bladerf *dev,
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
 * @param[in]   user_data        Caller-provided data that will be provided
 *                              in stream callbacks
 *
 *
 * @note  This call should be later followed by a call to
 *        bladerf_deinit_stream() to avoid memory leaks.
 *
 * @return 0 on success,
 *          value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_init_stream(struct bladerf_stream **stream,
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
API_EXPORT int bladerf_stream(struct bladerf_stream *stream,
                              bladerf_module module);

/**
 * Deinitialize and deallocate stream resources.
 *
 * @post   Stream is deallocated and may no longer be used.
 *
 * @param   stream      Stream to deinitialize. This function does nothin
 *                      if stream is NULL.
 */
API_EXPORT void bladerf_deinit_stream(struct bladerf_stream *stream);

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
 * @return number of samples sent on success,
 *          value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_tx(struct bladerf *dev, bladerf_format format,
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
 *
 * @return number of samples read or value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_rx(struct bladerf *dev, bladerf_format format,
                          void *samples, int num_samples,
                          struct bladerf_metadata *metadata);

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
API_EXPORT int bladerf_get_serial(struct bladerf *dev, char *serial);

/**
 * Query a device's VCTCXO calibration trim
 *
 * @param[in]   dev     Device handle
 * @param[out]  trim    Will be updated with the factory DAC trim value. If an
 *                      error occurs, no data will be written to this pointer.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_get_vctcxo_trim(struct bladerf *dev, uint16_t *trim);

/**
 * Query a device's FPGA size
 *
 * @param[in]   dev     Device handle
 * @param[out]  size    Will be updated with the onboard FPGA's size. If an
 *                      error occurs, no data will be written to this pointer.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_get_fpga_size(struct bladerf *dev,
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
API_EXPORT int bladerf_fw_version(struct bladerf *dev,
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
API_EXPORT int bladerf_is_fpga_configured(struct bladerf *dev);

/**
 * Query FPGA version
 *
 * @param[in]   dev         Device handle
 * @param[out]  version     Updated to contain firmware version
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_fpga_version(struct bladerf *dev,
                                    struct bladerf_version *version);

/**
 * Obtain device statistics
 *
 * @param[in]   dev     Device handle
 * @param[out]  stats   Current device statistics
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_stats(struct bladerf *dev, struct bladerf_stats *stats);

/**
 * Obtain the bus speed at which the device is operating
 *
 * @param       dev     Device handle
 * @return      speed   Device speed
 */
API_EXPORT bladerf_dev_speed bladerf_device_speed(struct bladerf *dev);

/** @} (End FN_INFO) */


/**
 * @defgroup FN_PROG    Device programming
 *
 * The flash chip used for storing the FX3 firmware on the BladeRF is a
 * MX25U3235 32Mbit flash with 4k sectors.
 *
 * The API for accessing the flash expects sector/page aligned byte
 * addresses. See the respective functions for details.
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


extern const unsigned int BLADERF_FLASH_ALIGNMENT_BYTE;
extern const unsigned int BLADERF_FLASH_ALIGNMENT_PAGE;
extern const unsigned int BLADERF_FLASH_ALIGNMENT_SECTOR;

/**
 * BladeRF flash image type
 */
typedef enum {
    BLADERF_IMAGE_TYPE_UNKNOWN = -1,
    BLADERF_IMAGE_TYPE_RAW,
    BLADERF_IMAGE_TYPE_FX3,
    BLADERF_IMAGE_TYPE_FPGA,
    BLADERF_IMAGE_TYPE_CALIBRATION,
} bladerf_image_type_t;

/**
 * Size of the magic signature at the beginning of bladeRF image files
 */
#define BLADERF_SIGNATURE_SIZE 16

typedef enum {
    BLADERF_IMAGE_META_INVALID = -1,
    /**
     * Address of flash data, `ent->data' is of type uint32_t in big endian
     * representation
     */
    BLADERF_IMAGE_META_ADDRESS,
    /**
     * Version of firmware image, `ent->data' is of type `struct
     * bladerf_image_meta_version'
     */
    BLADERF_IMAGE_META_VERSION,
    /**
     * Serial number of the bladeRF this flash image belongs onto, `ent->data'
     * is the serial number as a string and is of type uint8_t[33];
     */
    BLADERF_IMAGE_META_SERIAL,

    BLADERF_IMAGE_META_LAST,
} bladerf_image_meta_tag;

#define BLADERF_IMAGE_META_NENT 32

/**
 * BladeRF flash image metadata
 */
struct bladerf_image_meta {
    /**
     * Number of metadata entries
     */
    uint32_t n_entries;

    /**
     * Metadata
     */
    struct bladerf_image_meta_entry {
        int32_t tag;
        uint32_t len;
        char data[33];
    } entries[BLADERF_IMAGE_META_NENT];
};

/**
 * BladeRF flash image. The on disk format generated by the bladerf_image_write
 * function looks exactly like this struct and its sub structs. As arrays are
 * always prefixed by their length empty array elements are simply ommitted.
 */
struct bladerf_image {
    /**
     *Signature for detecting a flash image
     */
    char signature[BLADERF_SIGNATURE_SIZE];

    /**
     * Type of flash data
     */
    bladerf_image_type_t type;

    /**
     * Metadata about image
     */
    struct bladerf_image_meta meta;

    /**
     * Length of `data'
     */
    uint32_t len;
    /**
     * Data to be written or coming from flash memory
     */
    char data[BLADERF_FLASH_TOTAL_SIZE];

    /**
     * SHA256 checksum of the serialized flash image
     */
    char sha256[32];
};

/**
 * Minimum size of properly serialized image with no content
 */
#define BLADERF_IMAGE_MIN_SIZE                  \
    (BLADERF_SIGNATURE_SIZE                     \
     + sizeof(int32_t)  /* type */              \
     + sizeof(uint32_t) /* meta.n_entries */    \
     + sizeof(uint32_t) /* len */               \
     + 32)              /* sha256 */

struct bladerf_image_meta_version {
    uint8_t major, minor, patch;
};

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
API_EXPORT int bladerf_flash_firmware(struct bladerf *dev,
                                      const char *firmware);

/**
 * Flash FPGA image onto the device
 *
 * @param   dev         Device handle
 * @param   fpga_image  Full path to FPGA file
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_flash_fpga(struct bladerf *dev,
                                      const char *fpga_image);

/**
 * Erase sectors from FX3 flash device
 *
 * @note Unlike the bladerf_read_flash/bladerf_write_flash functions this
 *       function expects a BLADERF_FLASH_SECTOR_SIZE aligned address and
 *       length!
 *
 * @param   dev     Device handle
 * @param   addr    Page aligned byte address of the first sector to erase
 * @param   len     Number of bytes to erase, must be a multiple of
 *                  BLADERF_FLASH_SECTOR_SIZE
 *
 * @return Positive number of bytes erased on success, negative value from \ref
 *         RETCODES list on failure
 */
API_EXPORT int bladerf_erase_flash(struct bladerf *dev,
                                   uint32_t addr,
                                   uint32_t len);

/**
 * Read bytes from FX3 flash device
 *
 * @note Unline the `bladerf_erase_flash' function this function expects a
 *       BLADERF_FLASH_PAGE_SIZE aligned address and length!
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
API_EXPORT int bladerf_read_flash(struct bladerf *dev,
                                  uint32_t addr,
                                  uint8_t *buf,
                                  uint32_t len);

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
API_EXPORT int bladerf_read_flash_unaligned(struct bladerf *dev,
                                            uint32_t addr,
                                            uint8_t *pbuf,
                                            uint32_t len);

/**
 * Write bytes to FX3 flash device
 *
 * @note Unline the `bladerf_erase_flash' function this function expects a
 *       BLADERF_FLASH_PAGE_SIZE aligned address and length!
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
API_EXPORT int bladerf_write_flash(struct bladerf *dev,
                                   uint32_t addr,
                                   uint8_t *buf,
                                   uint32_t len);

/**
 * Program an unaligned region of flash memory (read/erase/write/verify).
 *
 * @note This function performs a full read/erase/write/verify cycle and the
 * aligned variants should be prefered whenever possible.
 *
 * @param   dev   Device handle
 * @param   addr  Unaligned byte address of destination
 * @param   pbuf  Data to write to flash
 * @param   len   Number of bytes to write. (No alignment requirement)
 *
 * @return Positive number of bytes written on success, negative value from \ref
 *         RETCODES list on failure
 */
API_EXPORT int bladerf_program_flash_unaligned(struct bladerf *dev,
                                               uint32_t addr,
                                               uint8_t *pbuf,
                                               uint32_t len);

/**
 * Create a calibration region from scratch.
 *
 * @param   fpga_size    Either "40" or "115" for x40 and x115 FPGA variant
 *                       respectivly
 * @param   vctcxo_trim  VCTCXO oscillator trim value.
 * @param   buf          Buffer to fill with the new cal region
 * @param   len          Size of `buf' in bytes
 *
 * @return 0 on success, negative value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_make_cal_region(char *fpga_size,
                                       uint16_t vctcxo_trim,
                                       char* buf, size_t len);


/**
 * Reset the device
 *
 * @note This also causes the device to reload its firmware
 *
 * @param   dev         Device handle
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_device_reset(struct bladerf *dev);

/**
 * Jump to FX3 bootloader
 *
 * @note This also causes the device to jump to the FX3 bootloader
 *
 * @param   dev         Device handle
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_jump_to_bootloader(struct bladerf *dev);

/**
 * Load device's FPGA
 *
 * @param   dev         Device handle
 * @param   fpga        Full path to FPGA bitstream
 *
 * @return 0 upon successfully, or a value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_load_fpga(struct bladerf *dev, const char *fpga);

/**
 * Fill a flash image with the parameters provided.
 *
 * @param[out]   img         Flash image
 * @param[in]    type        Type of the flash image
 * @param[in]    data        Image data
 * @param[in]    len         Length of `data'
 *
 * @return 0 upon success, or a value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_image_fill(struct bladerf_image *img,
                                  bladerf_image_type_t type,
                                  char *data,
                                  size_t len);

/**
 * Check if flash image file signature
 *
 * @param[in]   sig         Signature to check
 *
 * @return 0 if `sig' matches the BladeRF flash image signature, or a value from
 * \ref RETCODES list on failure
 */
API_EXPORT int bladerf_image_check_signature(char *sig);

/**
 * Probe a file for the existence of a flash image signature at the beginning
 *
 * @param[in]   file         File to probe for correct signature
 *
 * @return 0 if file's signature matches the BladeRF flash image signature, or a
 * value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_image_probe_file(char *file);

/**
 * Get string representation of metadata tag
 *
 * @param[in]      tag       Metadata tag to convert into a string
 *
 * @return a string representing `tag' (never returns NULL)
 */
API_EXPORT const char *bladerf_image_strmeta(bladerf_image_meta_tag tag);

/**
 * Add a metadata entry.
 *
 * @param[inout]   meta      Metadata list
 * @param[in]      tag       Metadata tag for item
 * @param[in]      data      Data to add, should be stored in host architecture
 *                           independent format. Big Endian is strongly recommended
 * @param[in]      len       Length of buffer at `data'
 *
 * @return 0 if successful, or a value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_image_meta_add(struct bladerf_image_meta *meta,
                                      bladerf_image_meta_tag tag,
                                      void *data,
                                      uint32_t len);

/**
 * Get a metadata entry by tag. If multiple entries exist for the given tag
 * returns the first entry found.
 *
 * @param[inout]   meta      Metadata list
 * @param[in]      tag       Metadata tag for item to search for
 * @param[out]     data      Address to write data to. If the data was converted to
 *                           a host architecture independent format when adding it
 *                           must be converted back to the host format before use.
 * @param[in]      len       Length of buffer at `data'
 *
 * @return 0 if successful, or a value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_image_meta_get(struct bladerf_image_meta *meta,
                                      int32_t tag,
                                      void *data,
                                      uint32_t len);

/**
 * Serialize an initialized flash image to a file.
 *
 * @param[in]    img         Flash image
 * @param[in]    file        File to write the flash image to
 *
 * @return 0 upon success, or a value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_image_write(struct bladerf_image *img, char* file);

/**
 * Deserialize a flash image from a file
 *
 * @param[out]   img         Flash image
 * @param[in]    file        File to write the flash image to
 *
 * @return 0 upon success, or a value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_image_read(struct bladerf_image *img, char* file);

/* @} (End of FN_PROG) */


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
API_EXPORT void bladerf_init_devinfo(struct bladerf_devinfo *info);

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
API_EXPORT int bladerf_get_devinfo(struct bladerf *dev,
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
API_EXPORT int bladerf_get_devinfo_from_str(const char *devstr,
                                            struct bladerf_devinfo *info);

/**
 * Test whether two device identifier information structures match, taking
 * wildcard values into account.
 */
API_EXPORT bool bladerf_devinfo_matches(const struct bladerf_devinfo *a,
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
API_EXPORT bool bladerf_devstr_matches(const char *dev_str,
                                       struct bladerf_devinfo *info);
/** @} (End of FN_DEVINFO) */


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
API_EXPORT const char * bladerf_strerror(int error);

/**
 * Get libbladeRF version information
 *
 * @param[out]  version     libbladeRF version information
 */
API_EXPORT void bladerf_version(struct bladerf_version *version);

/**
 * Sets the filter level for displayed log messages. Messages that are at
 * or above the specified log level will be written to the log output, while
 * messages with a lower log level will be suppressed. This function returns
 * the previous log level.
 *
 * @param   level       The new log level filter value
 */
API_EXPORT void bladerf_log_set_verbosity(bladerf_log_level level);

/** @} (End of FN_MISC) */

/**
 * @defgroup LOW_LEVEL Low level development and testing routines
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
API_EXPORT int bladerf_si5338_read(struct bladerf *dev,
                                   uint8_t address,
                                   uint8_t *val);

/**
 * Write a Si5338 register
 *
 * @param   dev         Device handle
 * @param   address     Si5338 register offset
 * @param   val         Data to write to register
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_si5338_write(struct bladerf *dev,
                                    uint8_t address,
                                    uint8_t val);

/**
 * Set frequency for TX clocks
 *
 * @param   dev         Device handle
 * @param   freq        Desired TX frequency in Hz
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_si5338_set_tx_freq(struct bladerf *dev, unsigned freq);

/**
 * Set frequency for RX clocks
 *
 * @param   dev         Device handle
 * @param   freq        Desired RX frequency in Hz
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_si5338_set_rx_freq(struct bladerf *dev,
                                          unsigned freq);


/**
 * Read a LMS register
 *
 * @param   dev         Device handle
 * @param   address     LMS register offset
 * @param   val         Pointer to variable the data should be read into
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_lms_read(struct bladerf *dev,
                                uint8_t address,
                                uint8_t *val);

/**
 * Write a LMS register
 *
 * @param   dev         Device handle
 * @param   address     LMS register offset
 * @param   val         Data to write to register
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_lms_write(struct bladerf *dev,
                                 uint8_t address,
                                 uint8_t val);

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
API_EXPORT int bladerf_config_gpio_read(struct bladerf *dev, uint32_t *val);

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
API_EXPORT int bladerf_config_gpio_write(struct bladerf *dev, uint32_t val);

/**
 * Write value to VCTCXO DAC
 *
 * @param   dev         Device handle
 * @param   val         Data to write to DAC register
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_dac_write(struct bladerf *dev, uint16_t val);

/**
 * Calibration routines
 *
 * @param   dev         Device handle
 * @param   module      Module to calibrate
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT int bladerf_calibrate_dc(struct bladerf *dev, bladerf_cal_module module);

/**
 * Set transfer timeout in milliseconds
 *
 * @param   dev         Device handle
 * @param   module      Module to adjust
 * @param   timeout     Timeout in milliseconds
 */
API_EXPORT void bladerf_set_transfer_timeout(struct bladerf *dev, bladerf_module module, int timeout);


/**
 * Get transfer timeout in milliseconds
 *
 * @param   dev         Device handle
 * @param   module      Module to adjust
 *
 * @return  Timeout in milliseconds
 */
int get_transfer_timeout(struct bladerf *dev, bladerf_module module);

/* @} (End of LOW_LEVEL) */

#ifdef __cplusplus
}
#endif

#endif /* BLADERF_H_ */
