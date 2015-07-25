/**
 * @file libbladeRF.h
 *
 * @brief bladeRF library
 *
 * Copyright (C) 2013-2015 Nuand LLC
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
#ifndef BLADERF_H_
#define BLADERF_H_

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#else
/* stdbool.h is not applicable for C++ programs, as the language inherently
 * provides the bool type.
 *
 * Users of Visual Studio 2012 and earlier will need to supply a stdbool.h
 * implementation, as it is not included with the toolchain. One is provided
 * with the bladeRF source code. Visual Studio 2013 onward supplies this header.
 */
#include <stdbool.h>
#endif

#if defined _WIN32 || defined __CYGWIN__
#   include <windows.h>
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
#define BLADERF_ERR_NO_FILE     (-11) /**< File not found */
#define BLADERF_ERR_UPDATE_FPGA (-12) /**< An FPGA update is required */
#define BLADERF_ERR_UPDATE_FW   (-13) /**< A firmware update is requied */
#define BLADERF_ERR_TIME_PAST   (-14) /**< Requested timestamp is in the past */
#define BLADERF_ERR_QUEUE_FULL  (-15) /**< Could not enqueue data into
                                       *   full queue */
#define BLADERF_ERR_FPGA_OP     (-16) /**< An FPGA operation reported failure */

/** @} (End RETCODES) */

/**
 * @defgroup FN_INIT    Initialization/deinitialization
 *
 * The functions in this section provide the ability query available devices,
 * initialize them, and deinitialize them. They are not guaranteed to be
 * thread-safe; the caller is responsible for ensuring they are executed
 * atomically.
 *
 * @{
 */

/** This structure is an opaque device handle */
struct bladerf;


/**
 * Backend by which the host communicates with the device
 */
typedef enum {
    BLADERF_BACKEND_ANY,    /**< "Don't Care" -- use any available backend */
    BLADERF_BACKEND_LINUX,  /**< Linux kernel driver */
    BLADERF_BACKEND_LIBUSB, /**< libusb */
    BLADERF_BACKEND_CYPRESS, /**< CyAPI */
    BLADERF_BACKEND_DUMMY = 100, /**< Dummy used for development purposes */
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
 * This function is generally preferred over bladerf_open() when a device
 * identifier string is not already provided.
 *
 * The most common uses of this function are to:
 *  - Open a device based upon the results of bladerf_get_device_list()
 *  - Open a specific device based upon its serial number
 *
 * Below is an example of how to use this function to open a device with a
 * specific serial number:
 *
 * @snippet open_via_serial.c example_snippet
 *
 * @param[out]  device      Update with device handle on success
 * @param[in]   devinfo     Device specification. If NULL, any available
 *                          device will be opened.
 *
 * @return 0 on success, or value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_open_with_devinfo(struct bladerf **device,
                                        struct bladerf_devinfo *devinfo);

/**
 * Open specified device using a device identifier string. See
 * bladerf_open_with_devinfo() if a device identifier string is not readily
 * available.
 *
 * The general form of the device identifier string is;
 * @code{.txt}
 *      <backend>:[device=<bus>:<addr>] [instance=<n>] [serial=<serial>]
 * @endcode
 *
 * An empty ("") or NULL device identifier will result in the first
 * encountered device being opened (using the first discovered backend)
 *
 * The 'backend' describes the mechanism used to communicate with the device,
 * and may be one of the following:
 *   - *:       Any available backend
 *   - libusb:  libusb (See libusb changelog notes for required version, given
 *   your OS and controller)
 *   - cypress: Cypress CyUSB/CyAPI backend (Windows only)
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
 *      - Nth instance encountered, 0-indexed
 *   - serial=\<serial\>
 *      - Device's serial number.
 *
 * Below is an example of how to open a device with a specific serial
 * number, using any avaiable backend supported by libbladeRF:
 *
 * @code {.c}
 * struct bladerf *dev;
 * int status = bladerf_open(&dev, "*:serial=f12ce1037830a1b27f3ceeba1f521413");
 * if (status != 0) {
 *      fprintf(stderr, "Unable to open device: %s\n",
 *              bladerf_strerror(status));
 *      return status;
 * }
 * @endcode
 *
 * @param[out]  device             Update with device handle on success
 * @param[in]   device_identifier  Device identifier, formatted as described
 *                                 above
 *
 * @return 0 on success, or value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_open(struct bladerf **device,
                           const char *device_identifier);

/**
 * Close device
 *
 * @note    Failing to close a device will result in memory leaks.
 *
 * @post    device is deallocated and may no longer be used.
 *
 * @param   device  Device handle previously obtained by bladerf_open(). This
 *                  function does nothing if device is NULL.
 */
API_EXPORT
void CALL_CONV bladerf_close(struct bladerf *device);

/**
 * Enable or disable USB device reset operation upon opening a device for
 * future bladerf_open() and bladerf_open_with_devinfo() calls.
 *
 * This operation has been found to be necessary on Linux-based systems for
 * some USB 3.0 controllers on Linux.
 *
 * This <b>does not</b> reset the state of the device in terms of its frequency,
 * gain, samplerate, etc. settings.
 *
 * @param   enabled     Set true to enable the use of the USB device reset,
 *                      and false otherwise.
 */
API_EXPORT
void bladerf_set_usb_reset_on_open(bool enabled);

/** @} (End FN_INIT) */

/**
 * @defgroup FN_DEVINFO Device identifier information functions
 *
 * As the functions in this section do not operate on a device, there are no
 * internal thread-safety concerns. The caller only needs to ensure the
 * function parameters are not modified while these functions are executing.
 *
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
 * This function is thread-safe.
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

/**
 * Retrieve the backend string associated with the specified
 * backend enumeration value.
 *
 * @warning Do not attempt to modify or free() the returned string.
 *
 * @return A string that can used to specify the `backend` portion of a device
 *         identifier string. (See bladerf_open().)
 */
API_EXPORT
const char * CALL_CONV bladerf_backend_str(bladerf_backend backend);


/** @} (End of FN_DEVINFO) */


/**
 * @defgroup FN_CTRL    Device control and configuration
 *
 * This section provides functions pertaining to accessing, controlling, and
 * configuring various device options and parameters.
 *
 * @{
 */

/** Minimum RXVGA1 gain, in dB */
#define BLADERF_RXVGA1_GAIN_MIN     5

/** Maximum RXVGA1 gain, in dB */
#define BLADERF_RXVGA1_GAIN_MAX     30

/** Minimum RXVGA2 gain, in dB */
#define BLADERF_RXVGA2_GAIN_MIN     0

/** Maximum RXVGA2 gain, in dB */
#define BLADERF_RXVGA2_GAIN_MAX     30

/** Minimum TXVGA1 gain, in dB */
#define BLADERF_TXVGA1_GAIN_MIN     (-35)

/** Maximum TXVGA1 gain, in dB */
#define BLADERF_TXVGA1_GAIN_MAX     (-4)

/** Minimum TXVGA2 gain, in dB */
#define BLADERF_TXVGA2_GAIN_MIN     0

/** Maximum TXVGA2 gain, in dB */
#define BLADERF_TXVGA2_GAIN_MAX     25

/** Minimum sample rate, in Hz */
#define BLADERF_SAMPLERATE_MIN      80000u

/** Maximum recommended sample rate, in Hz */
#define BLADERF_SAMPLERATE_REC_MAX  40000000u

/** Minimum bandwidth, in Hz */
#define BLADERF_BANDWIDTH_MIN       1500000u

/** Maximum bandwidth, in Hz */
#define BLADERF_BANDWIDTH_MAX       28000000u

/** Minimum tunable frequency (without an XB-200 attached), in Hz */
#define BLADERF_FREQUENCY_MIN       237500000u

/**
 * Minimum tunable frequency (with an XB-200 attached), in HZ.
 *
 * While this value is the lowest permitted, note that the components on the
 * XB-200 are only rated down to 50 MHz. Be aware that performance will likely
 * degrade as you tune to lower frequencies.
 */
#define BLADERF_FREQUENCY_MIN_XB200 0u

/** Maximum tunable frequency, in Hz */
#define BLADERF_FREQUENCY_MAX       3800000000u

/**
 * Specifies that scheduled retune should occur immediately when using
 * bladerf_schedule_retune().
 */
#define BLADERF_RETUNE_NOW  0

/**
 * Frequency tuning modes
 *
 * BLADERF_TUNING_MODE_HOST is the default if either of the following conditions
 * are true:
 *   - libbladeRF < v1.3.0
 *   - FPGA       < v0.2.0
 *
 * BLADERF_TUNING_MODE_FPGA is the default if both of the following conditions
 * are true:
 *  - libbladeRF >= v1.3.0
 *  - FPGA       >= v0.2.0
 *
 * The default mode can be overridden by setting a BLADERF_DEFAULT_TUNING_MODE
 * environment variable to "host" or "fpga". Overriding this value with a mode
 * not supported by the FPGA will result in failures or unexpected behavior.
 */
typedef enum {
    /** Indicates an invalid mode is set */
    BLADERF_TUNING_MODE_INVALID = -1,

    /**
     * Perform tuning algorithm on the host. This is slower, but provides
     * easier accessiblity to diagnostic information.
     */
    BLADERF_TUNING_MODE_HOST,

    /**
     * Perform tuning algorithm on the FPGA for faster tuning.
     *
     */
    BLADERF_TUNING_MODE_FPGA,
} bladerf_tuning_mode;

/**
 * Loopback options
 */
typedef enum {

    /**
     * Firmware loopback inside of the FX3
     */
    BLADERF_LB_FIRMWARE = 1,

    /**
     * Baseband loopback. TXLPF output is connected to the RXVGA2 input.
     */
    BLADERF_LB_BB_TXLPF_RXVGA2,

    /**
     * Baseband loopback. TXVGA1 output is connected to the RXVGA2 input.
     */
    BLADERF_LB_BB_TXVGA1_RXVGA2,

    /**
     * Baseband loopback. TXLPF output is connected to the RXLPF input.
     */
    BLADERF_LB_BB_TXLPF_RXLPF,

    /**
     * Baseband loopback. TXVGA1 output is connected to RXLPF input.
     */
    BLADERF_LB_BB_TXVGA1_RXLPF,

    /**
     * RF loopback. The TXMIX output, through the AUX PA, is connected to the
     * output of LNA1.
     */
    BLADERF_LB_RF_LNA1,


    /**
     * RF loopback. The TXMIX output, through the AUX PA, is connected to the
     * output of LNA2.
     */
    BLADERF_LB_RF_LNA2,

    /**
     * RF loopback. The TXMIX output, through the AUX PA, is connected to the
     * output of LNA3.
     */
    BLADERF_LB_RF_LNA3,

    /**
     * Disables loopback and returns to normal operation.
     */
    BLADERF_LB_NONE

} bladerf_loopback;


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

#define BLADERF_LNA_GAIN_MID_DB    3 /**< Gain in dB of the LNA at mid setting */
#define BLADERF_LNA_GAIN_MAX_DB    6 /**< Gain in db of the LNA at max setting */

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
 * Expansion boards
 */
typedef enum {
    BLADERF_XB_NONE = 0,    /**< No expansion boards attached */
    BLADERF_XB_100,         /**< XB-100 GPIO expansion board.
                             *   This device is not yet supported in
                             *   libbladeRF, and is here as a placeholder
                             *   for future support. */
    BLADERF_XB_200          /**< XB-200 Transverter board */
} bladerf_xb;

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
     * This option enables the RX/TX module's custom filter bank path across the
     * associated FILT and FILT-ANT SMA connectors on the XB-200 board.
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
     * selected depending on the RX or TX module's current frequency, based upon
     * the 1dB points of the on-board filters.  For frequencies outside the
     * range of the on-board filters, the custom path is selected.
     */
    BLADERF_XB200_AUTO_1DB,

    /**
     * When this option is selected, the other filter options are automatically
     * selected depending on the RX or TX module's current frequency, based upon
     * the 3dB points of the on-board filters.  For frequencies outside the
     * range of the on-board filters, the custom path is selected.
     */
    BLADERF_XB200_AUTO_3DB
} bladerf_xb200_filter;

/**
 * XB-200 signal paths
 */
typedef enum {
    BLADERF_XB200_BYPASS = 0,   /**< Bypass the XB-200 mixer */
    BLADERF_XB200_MIX           /**< Pass signals through the XB-200 mixer */
} bladerf_xb200_path;

/**
 * Quick Re-tune parameters. Note that these parameters, which are associated
 * with LMS6002D register values, are sensitive to changes in the operating
 * environment (e.g., temperature).
 *
 * This structure should be filled in via bladerf_get_quick_tune().
 */
struct bladerf_quick_tune {
    uint8_t freqsel;    /**< Choice of VCO and VCO division factor */
    uint8_t vcocap;     /**< VCOCAP value */
    uint16_t nint;      /**< Integer portion of LO frequency value */
    uint32_t nfrac;     /**< Fractional portion of LO frequency value */
    uint8_t  flags;     /**< Flag bits used internally by libbladeRF */
};

/**
 * DC Calibration Modules
 */
typedef enum {
    BLADERF_DC_CAL_LPF_TUNING,
    BLADERF_DC_CAL_TX_LPF,
    BLADERF_DC_CAL_RX_LPF,
    BLADERF_DC_CAL_RXVGA2
} bladerf_cal_module;

/**
 * Correction parameter selection
 *
 * These values specify the correction parameter to modify or query when
 * calling bladerf_set_correction() or bladerf_get_correction(). Note that the
 * meaning of the `value` parameter to these functions depends upon the
 * correction parameter.
 *
 */
typedef enum {
    /**
     * Adjusts the in-phase DC offset via controls provided by the LMS6002D
     * front end. Valid values are [-2048, 2048], which are scaled to the
     * available control bits in the LMS device.
     */
    BLADERF_CORR_LMS_DCOFF_I,

    /**
     * Adjusts the quadrature DC offset via controls provided the LMS6002D
     * front end. Valid values are [-2048, 2048], which are scaled to the
     * available control bits.
     */
    BLADERF_CORR_LMS_DCOFF_Q,

    /**
     * Adjusts FPGA-based phase correction of [-10, 10] degrees, via a provided
     * count value of [-4096, 4096].
     */
    BLADERF_CORR_FPGA_PHASE,

    /**
     * Adjusts FPGA-based gain correction of [0.0, 2.0], via provided
     * values in the range of [-4096, 4096], where a value of 0 corresponds to
     * a gain of 1.0.
     */
    BLADERF_CORR_FPGA_GAIN
} bladerf_correction;

/**
 * Enable or disable the specified RX/TX module.
 *
 * When a synchronous stream is associated with the specified module, this
 * will shut down the underlying asynchronous stream when `enable` = false.
 *
 * When transmitting samples with the sync interface, be sure to provide ample
 * time for TX samples reach the FPGA and be transmitted before calling this
 * function with `enable` = false.
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
 * @param       l       Loopback mode. Note that BLADERF_LB_NONE disables the
 *                      use of loopback functionality.
 *
 * @note Loopback modes should only be enabled or disabled while the RX and TX
 *       modules are both disabled (and therefore, when no samples are being
 *       actively streamed). Otherwise, unexpected behavior may occur.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_loopback(struct bladerf *dev, bladerf_loopback l);


/**
 * Get current loopback mode
 *
 * @param[in]   dev     Device handle
 * @param[out]  l       Current loopback mode
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_loopback(struct bladerf *dev, bladerf_loopback *l);

/**
 * Configure the device's sample rate, in Hz.  Note this requires the sample
 * rate is an integer value of Hz.  Use bladerf_set_rational_sample_rate()
 * for more arbitrary values.
 *
 * The sample rate must be greater than or equal to \ref BLADERF_SAMPLERATE_MIN.
 * Values above \ref BLADERF_SAMPLERATE_REC_MAX are allowed, but not
 * recommended. Setting the sample rates higher than recommended max may yield
 * errors and unexpected results.
 *
 * @param[in]   dev         Device handle
 * @param[in]   module      Module to change
 * @param[in]   rate        Sample rate
 * @param[out]  actual      If non-NULL. this is written with the actual
 *                          sample rate achieved.
 *
 * @return 0 on success,
 *         BLADERF_ERR_INVAL for an invalid sample rate,
 *         or a value from \ref RETCODES list on other failures
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
 * @param[out]  actual      If non-NULL, this is written with the actual
 *                          rational sample rate achieved.
 *
 * The sample rate must be greater than or equal to \ref BLADERF_SAMPLERATE_MIN.
 * Values above \ref BLADERF_SAMPLERATE_REC_MAX are allowed, but not
 * recommended. Setting the sample rates higher than recommended max may yield
 * errors and unexpected results.
 *
 * @return 0 on success,
 *         BLADERF_ERR_INVAL for an invalid sample rate,
 *         or a value from \ref RETCODES list on other failures
 */
API_EXPORT
int CALL_CONV bladerf_set_rational_sample_rate(
                                        struct bladerf *dev,
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
int CALL_CONV bladerf_get_rational_sample_rate(
                                        struct bladerf *dev,
                                        bladerf_module module,
                                        struct bladerf_rational_rate *rate);

/**
 * Set the value of the specified configuration parameter
 *
 * See the ::bladerf_correction description for the valid ranges of the
 * `value` parameter.
 *
 * @param   dev         Device handle
 * @param   module      Module to apply correction to
 * @param   corr        Correction type
 * @param   value       Value to apply
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_correction(struct bladerf *dev, bladerf_module module,
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
int CALL_CONV bladerf_get_correction(struct bladerf *dev, bladerf_module module,
                                     bladerf_correction corr, int16_t *value);

/**
 * Set the PA gain in dB
 *
 * Values outside the range of
 * [ \ref BLADERF_TXVGA2_GAIN_MIN, \ref BLADERF_TXVGA2_GAIN_MAX ]
 * will be clamped.
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
 * Values outside the range of
 * [ \ref BLADERF_TXVGA1_GAIN_MIN, \ref BLADERF_TXVGA1_GAIN_MAX ]
 * will be clamped.
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
 * Values outside the range of
 * [ \ref BLADERF_RXVGA1_GAIN_MIN, \ref BLADERF_RXVGA1_GAIN_MAX ]
 * will be clamped.
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
 * Values outside the range of
 * [ \ref BLADERF_RXVGA2_GAIN_MIN, \ref BLADERF_RXVGA2_GAIN_MAX ]
 * will be clamped.
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
 * Set combined gain values
 *
 * This function computes the optimal LNA, RXVGA1, and RVGA2 gains for a
 * requested amount of RX gain, and computes the optimal TXVGA1 and TXVGA2 gains
 * for a requested amount of TX gain.
 *
 * Values outside the valid gain range will be clipped.
 *
 * @param       dev         Device handle
 * @param       mod         Module
 * @param       gain        Desired gain
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_gain(struct bladerf *dev, bladerf_module mod, int gain);

/**
 * Set the bandwidth of the LMS LPF to specified value in Hz
 *
 * The underlying device is capable of a discrete set of bandwidth values. The
 * caller should check the `actual` parameter to determine which of these
 * discrete bandwidth values is actually used for the requested bandwidth.
 *
 * Values outside the range of
 * [ \ref BLADERF_BANDWIDTH_MIN, \ref BLADERF_BANDWIDTH_MAX ]
 * will be clamped.
 *
 * @param[in]   dev                 Device handle
 * @param[in]   module              Module for bandwidth request
 * @param[in]   bandwidth           Desired bandwidth
 * @param[out]  actual              If non-NULL, written with the actual
 *                                  bandwidth that the device was able to
 *                                  achieve.
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
 * The high band (LNA2 and PA2) is used for `frequency` >= 1.5 GHz. Otherwise,
 * The low band (LNA1 and PA1) is used.
 *
 * Frequency values outside the range of
 * [ \ref BLADERF_FREQUENCY_MIN, \ref BLADERF_FREQUENCY_MAX ]
 * will be clamped.
 *
 * @param       dev         Device handle
 * @param       module      Module to configure
 * @param       frequency   Tuned frequency
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_select_band(struct bladerf *dev, bladerf_module module,
                                  unsigned int frequency);

/**
 * Set module's frequency in Hz.
 *
 * Values outside the range of
 * [ \ref BLADERF_FREQUENCY_MIN, \ref BLADERF_FREQUENCY_MAX ]
 * will be clamped.
 *
 * This calls bladerf_select_band() internally.
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
 * Schedule a frequency retune to occur at specified sample timestamp value.
 *
 * @pre bladerf_sync_config() must have been called with the
 *      BLADERF_FORMAT_SC16_Q11_META format for the associated module in order
 *      to enable timestamps. (The timestamped metadata format must be enabled
 *      in order to use this function.)
 *
 * @param       dev             Device handle
 *
 * @param       module          Module to retune
 *
 * @param       timestamp       Module's sample timestamp to perform the retune
 *                              operation. If this value is in the past, the
 *                              retune will occur immediately. To perform the
 *                              retune immediately, specify BLADERF_RETUNE_NOW.
 *
 * @param       frequency       Desired frequency, in Hz.
 *
 * @param       quick_tune      If NULL, the `frequency` parameter will be used.
 *                              If non-NULL, the provided "quick retune" values
 *                              will be applied to the transceiver to tune it
 *                              according to a previous state retrieved via
 *                              bladerf_get_quick_tune().
 *
 *
 * @return 0 on success, value from \ref RETCODES list on failure. If the
 *         underlying queue of scheduled retune requests becomes full,
 *         BLADERF_ERR_QUEUE_FULL will be returned. In this case, it should be
 *         possible to schedule a retune after the timestamp of one of the
 *         earlier requests occurs.
 */
API_EXPORT
int CALL_CONV bladerf_schedule_retune(struct bladerf *dev,
                                      bladerf_module module,
                                      uint64_t timestamp,
                                      unsigned int frequency,
                                      struct bladerf_quick_tune *quick_tune);

/**
 * Cancel all pending scheduled retune operations for the specified module.
 *
 * This will be done automatically during bladerf_close() to ensure that
 * previously queued retunes do not continue to occur after closing and then
 * later re-opening a device.
 *
 * @param   dev     Device handle
 * @param   module  Module to cancel pending operations on
 *
 * @return 0 on success, value from \ref RETCODES list on failure.
 */
API_EXPORT
int CALL_CONV bladerf_cancel_scheduled_retunes(struct bladerf *dev,
                                               bladerf_module module);

/**
 * Get module's current frequency in Hz
 *
 * @param       dev         Device handle
 * @param       module      Module to configure
 * @param       frequency   Pointer to the returned frequency
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_frequency(struct bladerf *dev,
                                    bladerf_module module,
                                    unsigned int *frequency);

/**
 * Fetch parameters used to tune the transceiver to the current frequency for
 * use with bladerf_schedule_retune() to perform a "quick retune."
 *
 * This allows for a faster retune, with a potential trade off of
 * increased phase noise.  Note that these parameters are sensitive to
 * changes in the operating environment, and should be "refreshed" if planning
 * to use the "quick retune" functionality over a long period of time.
 *
 * @pre bladerf_set_frequency() or bladerf_schedule_retune() have previously
 *      been used to retune to the desired frequency.
 *
 * @param[in]   dev         Device handle
 * @param[in]   module      Module to query
 * @param[out]  quick_tune  Quick retune parameters
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_quick_tune(struct bladerf *dev,
                                     bladerf_module module,
                                     struct bladerf_quick_tune *quick_tune);

/**
 * Set the device's tuning mode
 *
 * @param       dev         Device handle
 * @param       mode        Desired tuning mode. Note that the available modes
 *                          depends on the FPGA version.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_tuning_mode(struct bladerf *dev,
                                      bladerf_tuning_mode mode);

/**
 * Attach and enable an expansion board's features
 *
 * @param       dev         Device handle
 * @param       xb          Expansion board
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_expansion_attach(struct bladerf *dev, bladerf_xb xb);

/**
 * Determine which expansion board is attached
 *
 * @param       dev         Device handle
 * @param       xb          Expansion board
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_expansion_get_attached(struct bladerf *dev, bladerf_xb *xb);

/**
 * Set XB-200 filterbank
 *
 * @param       dev         Device handle
 * @param       mod         Module
 * @param       filter      XB200 filterbank
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_xb200_set_filterbank(struct bladerf *dev,
                                           bladerf_module mod,
                                           bladerf_xb200_filter filter);

/**
 * Get current XB-200 filterbank
 *
 * @param[in]    dev        Device handle
 * @param[in]    module     Module to query
 * @param[out]   filter     Pointer to filterbank, only updated if return
 *                          value is 0.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_xb200_get_filterbank(struct bladerf *dev,
                                           bladerf_module module,
                                           bladerf_xb200_filter *filter);

/**
 * Set XB-200 signal path
 *
 * @param       dev         Device handle
 * @param       module      Module to configure
 * @param       path        Desired XB-200 signal path
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_xb200_set_path(struct bladerf *dev,
                                     bladerf_module module,
                                     bladerf_xb200_path path);

/**
 * Get current XB-200 signal path
 *
 * @param       dev         Device handle
 * @param       module      Module to query
 * @param       path        Pointer to XB200 signal path
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_xb200_get_path(struct bladerf *dev,
                                     bladerf_module module,
                                     bladerf_xb200_path *path);

/** @} (End of FN_CTRL) */

/**
 * @defgroup FMT_META   Sample Formats and Metadata
 *
 * This section defines the available sample formats and metadata flags.
 *
 * @{
 */

/**
 * Sample format
 */
typedef enum {
    /**
     * Signed, Complex 16-bit Q11. This is the native format of the DAC data.
     *
     * Values in the range [-2048, 2048) are used to represent [-1.0, 1.0).
     * Note that the lower bound here is inclusive, and the upper bound is
     * exclusive. Ensure that provided samples stay within [-2048, 2047].
     *
     * Samples consist of interleaved IQ value pairs, with I being the first
     * value in the pair. Each value in the pair is a right-aligned,
     * little-endian int16_t. The FPGA ensures that these values are
     * sign-extended.
     *
     * When using this format the minimum required buffer size, in bytes, is:
     * <pre>
     *   buffer_size_min = [ 2 * num_samples * sizeof(int16_t) ]
     * </pre>
     *
     * For example, to hold 2048 samples, a buffer must be at least 8192 bytes
     * large.
     */
    BLADERF_FORMAT_SC16_Q11,

    /**
     * This format is the same as the ::BLADERF_FORMAT_SC16_Q11 format, except the
     * first 4 samples (16 bytes) in every block of 1024 samples are replaced
     * with metadata, organized as follows, with all fields being little endian
     * byte order:
     *
     * <pre>
     *  0x00 [uint32_t:  Reserved]
     *  0x04 [uint64_t:  64-bit Timestamp]
     *  0x0c [uint32_t:  BLADERF_META_FLAG_* flags]
     * </pre>
     *
     * When using the bladerf_sync_rx() and bladerf_sync_tx() functions,
     * this detail is transparent to caller. These functions take care of
     * packing/unpacking the metadata into/from the data, via the
     * bladerf_metadata structure.
     *
     * Currently, when using the asynchronous data transfer interface, the user
     * is responsible for manually packing/unpacking this metadata into/from
     * their sample data.
     */
    BLADERF_FORMAT_SC16_Q11_META,
} bladerf_format;

/*
 * Metadata status bits
 *
 * These are used in conjunction with the bladerf_metadata structure's
 * `status` field.
 */

/**
 * A sample overrun has occurred. This indicates that either the host
 * (more likely) or the FPGA is not keeping up with the incoming samples
 */
#define BLADERF_META_STATUS_OVERRUN  (1 << 0)

/**
 * A sample underrun has occurred. This generally only occurrs on the TX module
 * when the FPGA is starved of samples.
 *
 * @note libbladeRF does not report this status. It is here for future use.
 */
#define BLADERF_META_STATUS_UNDERRUN (1 << 1)



/*
 * Metadata flags
 *
 * These are used in conjunction with the bladerf_metadata structure's
 * `flags` field.
 */

/**
 * Mark the associated buffer as the start of a burst transmission.
 * This is only used for the bladerf_sync_tx() call.
 *
 * When using this flag, the bladerf_metadata::timestamp field should contain
 * the timestamp at which samples should be sent.
 *
 * Between specifying the ::BLADERF_META_FLAG_TX_BURST_START and
 * ::BLADERF_META_FLAG_TX_BURST_END flags, there is no need for the user to the
 * bladerf_metadata::timestamp field because the library will ensure the
 * correct value is used, based upon the timestamp initially provided and
 * the number of samples that have been sent.
 */
#define BLADERF_META_FLAG_TX_BURST_START   (1 << 0)

/**
 * Mark the associated buffer as the end of a burst transmission. This will
 * flush the remainder of the sync interface's current working buffer and
 * enqueue samples into the hardware's transmit FIFO.
 *
 * As of libbladeRF v1.3.0, it is no longer necessary for the API user to
 * ensure that the final 3 samples of a burst are 0+0j. libbladeRF now ensures
 * this hardware requirement (driven by the LMS6002D's pre-DAC register stages)
 * is upheld.
 *
 * Specifying this flag and flushing the sync interface's working buffer implies
 * that the next timestamp that can be transmitted is the current timestamp plus
 * the duration of the burst that this flag is ending <b>and</b> the remaining
 * length of the remaining buffer that is flushed. (The buffer size, in this
 * case, is the `buffer_size` value passed to the previous bladerf_sync_config()
 * call.)
 *
 * Rather than attempting to keep track of the number of samples sent with
 * respect to buffer sizes, it is easiest to always assume 1 buffer's worth of
 * time is required between bursts. In this case "buffer" refers to the
 * `buffer_size` parameter provided to bladerf_sync_config().) If this is too
 * much time, consider using the ::BLADERF_META_FLAG_TX_UPDATE_TIMESTAMP
 * flag.
 *
 * This is only used for the bladerf_sync_tx() call. It is ignored by the
 * bladerf_sync_rx() call.
 *
 */
#define BLADERF_META_FLAG_TX_BURST_END     (1 << 1)

/**
 * Use this flag in conjunction with ::BLADERF_META_FLAG_TX_BURST_START to
 * indicate that the burst should be transmitted as soon as possible, as opposed
 * to waiting for a specific timestamp.
 *
 * When this flag is used, there is no need to set the
 * bladerf_metadata::timestamp field.
 */
#define BLADERF_META_FLAG_TX_NOW           (1 << 2)

/**
 * Use this flag within a burst (i.e., between the use of
 * ::BLADERF_META_FLAG_TX_BURST_START and ::BLADERF_META_FLAG_TX_BURST_END) to
 * specify that bladerf_sync_tx() should read the bladerf_metadata::timestamp
 * field and zero-pad samples up to the specified timestamp. The provided
 * samples will then be transmitted at that timestamp.
 *
 * Use this flag when potentially flushing an entire buffer via the
 * ::BLADERF_META_FLAG_TX_BURST_END would yield an unacceptably large gap in
 * the transmitted samples.
 *
 * In some applications where a transmitter is constantly transmitting
 * with extremely small gaps (less than a buffer), users may end up using a
 * single ::BLADERF_META_FLAG_TX_BURST_START, and then numerous calls to
 * bladerf_sync_tx() with the ::BLADERF_META_FLAG_TX_UPDATE_TIMESTAMP
 * flag set.  The ::BLADERF_META_FLAG_TX_BURST_END would only be used to end
 * the stream when shutting down.
 */
#define BLADERF_META_FLAG_TX_UPDATE_TIMESTAMP (1 << 3)

/**
 * This flag indicates that calls to bladerf_sync_rx should return any available
 * samples, rather than wait until the timestamp indicated in the
 * bladerf_metadata timestamp field.
 */
#define BLADERF_META_FLAG_RX_NOW           (1 << 31)

/**
 * Sample metadata
 *
 * This structure is used in conjunction with the ::BLADERF_FORMAT_SC16_Q11_META
 * format to TX scheduled bursts or retrieve timestamp information about
 * received samples.
 */
struct bladerf_metadata {

    /**
     * Free-running FPGA counter that monotonically increases at the
     * sample rate of the associated module. */
    uint64_t timestamp;

    /**
     * Input bit field to control the behavior of the call that the metadata
     * structure is passed to. API calls read this field from the provided
     * data structure, and do not modify it.
     *
     * Valid flags include
     *  ::BLADERF_META_FLAG_TX_BURST_START, ::BLADERF_META_FLAG_TX_BURST_END,
     *  ::BLADERF_META_FLAG_TX_NOW, ::BLADERF_META_FLAG_TX_UPDATE_TIMESTAMP,
     *  and ::BLADERF_META_FLAG_RX_NOW
     *
     */
    uint32_t flags;

    /**
     * Output bit field to denoting the status of transmissions/receptions. API
     * calls will write this field.
     *
     * Possible status flags include ::BLADERF_META_STATUS_OVERRUN and
     * ::BLADERF_META_STATUS_UNDERRUN;
     *
     */
    uint32_t status;

    /**
     * This output parameter is updated to reflect the actual number of
     * contiguous samples that have been populated in an RX buffer during
     * a bladerf_sync_rx() call.
     *
     * This will not be equal to the requested count in the event of a
     * discontinuity (i.e., when the status field has the
     * ::BLADERF_META_STATUS_OVERRUN flag set). When an overrun occurs, it is
     * important not to read past the number of samples specified by this
     * value, as the remaining contents of the buffer are undefined.
     *
     * This parameter is not currently used by bladerf_sync_tx().
     */
    unsigned int actual_count;

    /**
     * Reserved for future use. This is not used by any functions.
     * It is recommended that users zero out this field.
     */
    uint8_t reserved[32];
};


/** @} (End of FMT_META) */


/**
 * @defgroup FN_DATA_ASYNC    Asynchronous data transmission and reception
 *
 * This interface gives the API user full control over the stream and buffer
 * management, at the cost of added complexity.
 *
 * New users are recommended to first evaluate the \ref FN_DATA_SYNC interface,
 * and to only use this interface if the former is found to not yield suitable
 * performance.
 *
 * @{
 */

/**
 * Use this as a return value in callbacks or as the buffer parameter to
 * bladerf_submit_stream_buffer() to shutdown a stream.
 */
#define BLADERF_STREAM_SHUTDOWN (NULL)

/**
 * Use this value in a stream callback to indicate that no buffer is being
 * provided. In this case, buffers are expected to be provided via
 * bladerf_submit_stream_buffer().
 */
#define BLADERF_STREAM_NO_DATA  ((void*)(-1))

/** This opaque structure is used to keep track of stream information */
struct bladerf_stream;

/**
 * This typedef represents a callback function that is executed in response to
 * this interface's asynchronous events.
 *
 * Stream callbacks <b>must not</b> block or perform long-running operations.
 * Otherwise, timeouts may occur. If this cannot be guaranteed, consider
 * returning BLADERF_STREAM_NO_DATA in callbacks and later submit a buffer using
 * bladerf_submit_stream_buffer(). However, callbacks should always take a
 * single approach of returning buffers <b>or</b> returning
 * BLADERF_STREAM_NO_DATA and submitting buffers later -- <b>but not both</b>.
 *
 * When running in a full-duplex mode of operation with simultaneous TX and RX
 * stream threads, be aware that one module's callback may occur in the context
 * of another module's thread. The API user is responsible for ensuring their
 * callbacks are thread safe. For example, when managing access to sample
 * buffers, the caller must ensure that if one thread is processing samples in a
 * buffer, that this buffer is not returned via the callback's return value.
 *
 * As of libbladeRF v0.15.0, is guaranteed that only one callback from a module
 * will occur at a time. (i.e., a second TX callback will not fire while one is
 * currently being handled.)  To achieve this, while a callback is executing, a
 * per-stream lock is held. It is important to consider this when thinking about
 * the order of lock acquisitions both in the callbacks, and the code
 * surrounding bladerf_submit_stream_buffer().
 *
 * <b>Note:</b>Do not call bladerf_submit_stream_buffer() from a callback.
 *
 * For both RX and TX, the stream callback receives:
 *  - dev:          Device structure
 *  - stream:       The associated stream
 *  - metadata:     For future support - do not attempt to read/write this
 *                  in the current library implementation.
 *  - user_data:    User data provided when initializing stream
 *
 * For TX callbacks:
 *  - samples:      Pointer to buffer of samples that was sent
 *  - num_samples:  Number of sent in last transfer and to send in next transfer
 *  - Return value: The user specifies the address of the next buffer to send,
 *                  BLADERF_STREAM_SHUTDOWN, or BLADERF_STREAM_NO_DATA.
 *
 * For RX callbacks:
 *  - samples:          Buffer filled with received data
 *  - num_samples:      Number of samples received and size of next buffers
 *  - Return value:     The user specifies the next buffer to fill with RX data,
 *                      which should be `num_samples` in size,
 *                      BLADERF_STREAM_SHUTDOWN, or BLADERF_STREAM_NO_DATA.
 *
 */
typedef void *(*bladerf_stream_cb)(struct bladerf *dev,
                                   struct bladerf_stream *stream,
                                   struct bladerf_metadata *meta,
                                   void *samples,
                                   size_t num_samples,
                                   void *user_data);

/**
 * Initialize a stream for use with asynchronous routines.
 *
 * This function will internally allocate data buffers, which will be provided
 * to the API user in callback functions.
 *
 * The `buffers` output parameter populates a pointer to the list of allocated
 * buffers. This allows the API user to implement a buffer management scheme to
 * best suit his or her specific use case.
 *
 * Generally, one will want to set the `buffers` parameter to a value larger
 * than the `num_transfers` parameter, and keep track of which buffers are
 * currently "in-flight", versus those available for use.
 *
 * For example, for a transmit stream, modulated data can be actively written
 * into free buffers while transfers of other buffers are occurring. Once a
 * buffer has been filled with data, it can be marked 'in-flight' and be
 * returned in a successive callback to transmit.
 *
 * The choice of values for the `num_transfers` and `buffer_size` should be
 * made based upon the desired samplerate, and the stream timeout value
 * specified via bladerf_set_stream_timeout(), which defaults to 1 second.
 *
 * For a given sample rate, the below relationship must be upheld to transmit or
 * receive data without timeouts or dropped data.
 *
 * @f[
 * Sample\ Rate > \frac{\#\ Transfers}{Timeout} \times Buffer\ Size
 * @f]
 *
 * ...where Sample Rate is in samples per second, and Timeout is in seconds.
 *
 * To account for general system overhead, it is recommended to multiply the
 * righthand side by 1.1 to 1.25.
 *
 * While increasing the number of buffers available provides additional
 * elasticity, be aware that it also increases latency.
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
 * @param[in]   num_buffers     Number of buffers to allocate and return. This
 *                              value must >= the `num_transfers` parameter.
 *
 * @param[in]   format          Sample data format
 *
 * @param[in]   samples_per_buffer  Size of allocated buffers, in units of
 *                                  samples Note that the physical size of the
 *                                  buffer is a function of this and the format
 *                                  parameter.
 *
 * @param[in]   num_transfers   Maximum number of transfers that may be
 *                              in-flight simultaneously. This must be <= the
 *                              `num_buffers` parameter.
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
                                  size_t samples_per_buffer,
                                  size_t num_transfers,
                                  void *user_data);

/**
 * Begin running a stream. This call will block until the steam completes.
 *
 * Only 1 RX stream and 1 TX stream may be running at a time. Attempting to
 * call bladerf_stream() with more than one stream per module will yield
 * unexpected (and most likely undesirable) results. See the ::bladerf_stream_cb
 * description for additional thread-safety caveats.
 *
 * @pre This function should be preceded by a call to bladerf_enable_module()
 *      to enable the associated RX or TX module before attempting to use
 *      it to stream data.
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
 * Submit a buffer to a stream from outside of a stream callback function.
 * Use this only when returning BLADERF_STREAM_NO_DATA from callbacks. <b>Do
 * not</b> use this function if the associated callback functions will be
 * returning buffers for submission.
 *
 * This call may block if the device is not ready to submit a buffer for
 * transfer. Use the `timeout_ms` to place an upper limit on the time this
 * function can block.
 *
 * To safely submit buffers from outside the stream callback flow, this function
 * internally acquires a per-stream lock (the same one that is held during the
 * execution of a stream callback). Therefore, it is important to be aware of
 * locks that may be held while making this call, especially those acquired
 * during execution of the associated stream callback function. (i.e., be wary
 * of the order of lock acquisitions, including the internal per-stream lock.)
 *
 * @param   stream      Stream to submit buffer to
 * @param   buffer      Buffer to fill (RX) or containing data (TX). This buffer
 *                      is assumed to be the size specified in the associated
 *                      bladerf_init_stream() call.
 * @param   timeout_ms  Milliseconds to timeout in, if this call blocks. 0
 *                      implies an "infinite" wait.
 *
 * @return  0 on success, BLADERF_ERR_TIMEOUT upon a timeout, or a value from
 * \ref RETCODES list on other failures
 */
API_EXPORT
int CALL_CONV bladerf_submit_stream_buffer(struct bladerf_stream *stream,
                                           void *buffer,
                                           unsigned int timeout_ms);

/**
 * Deinitialize and deallocate stream resources.
 *
 * @pre    Stream is no longer being used (via bladerf_submit_stream_buffer() or
 *          bladerf_stream() calls.)
 * @post   Stream is deallocated and may no longer be used.
 *
 * @param   stream      Stream to deinitialize. This function does nothing
 *                      if stream is NULL.
 */
API_EXPORT
void CALL_CONV bladerf_deinit_stream(struct bladerf_stream *stream);

/**
 * Set stream transfer timeout in milliseconds
 *
 * @param   dev         Device handle
 * @param   module      Module to adjust
 * @param   timeout     Timeout in milliseconds
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_stream_timeout(struct bladerf *dev,
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
int CALL_CONV bladerf_get_stream_timeout(struct bladerf *dev,
                                         bladerf_module module,
                                         unsigned int *timeout);

/** @} (End of FN_DATA_ASYNC) */

/**
 * @defgroup FN_DATA_SYNC  Synchronous data transmission and reception
 *
 * This group of functions presents synchronous, blocking calls (with optional
 * timeouts) for transmitting and receiving samples.
 *
 * The synchronous interface is built atop the asynchronous interface, and is
 * generally less complex and easier to work with.  It alleviates the need to
 * explicitly spawn threads (it is done under the hood) and manually manage
 * sample buffers.
 *
 * Under the hood, this interface spawns worker threads to handle an
 * asynchronous stream and perform thread-safe buffer management.
 *
 * These functions are thread-safe.
 *
 * The following pages provide additional information and example usage:
 *  <ul>
 *      <li>
 *          <a class="el" href="sync_no_meta.html">
 *              Synchronous Interface: Basic usage without metadata
 *          </a>
 *      </li>
 *      <li>
 *          <a class="el" href="sync_rx_meta.html">
 *             Synchronous Interface: RX with metadata
 *          </a>
 *      </li>
 *      <li>
            <a class="el" href="sync_tx_meta_bursts.html">
 *             Synchronous Interface: TX with metadata
 *          </a>
 *      </li>
 *  </ul>
 *
 * @{
 */

/**
 * (Re)Configure a device for synchronous transmission or reception
 *
 * This function sets up the device for the specified format and initializes
 * the underlying asynchronous stream parameters
 *
 * This function does not call bladerf_enable_module(). The API user is
 * responsible for enabling/disable modules when desired.
 *
 * Note that (re)configuring ::BLADERF_MODULE_TX does not affect the
 * ::BLADERF_MODULE_RX modules, and vice versa. This call configures each module
 * independently.
 *
 * Memory allocated by this function will be deallocated when bladerf_close()
 * is called.
 *
 * See the bladerf_init_stream() documentation for information on determining
 * appropriate values for `buffers_size`, `num_transfers`, and `stream_timeout`.
 * The `num_buffers` parameter should generally be increased as the amount of
 * work done between bladerf_sync_rx() or bladerf_sync_tx() calls increases.
 *
 * @param   dev             Device to configure
 *
 * @param   module          Module to use with synchronous interface
 *
 * @param   format          Format to use in synchronous data transfers
 *
 * @param   num_buffers     The number of buffers to use in the underlying
 *                          data stream. This must be greater than the
 *                          `num_xfers` parameter.
 *
 * @param   buffer_size     The size of the underlying stream buffers, in
 *                          samples. This value must be a multiple of 1024.
 *                          Note that samples are only transferred when a buffer
 *                          of this size is filled.
 *
 * @param   num_transfers   The number of active USB transfers that may be
 *                          in-flight at any given time. If unsure of what
 *                          to use here, try values of 4, 8, or 16.
 *
 * @param   stream_timeout  Timeout (milliseconds) for transfers in the
 *                          underlying data stream.
 *
 * @return 0 on success,
 *         BLADERF_ERR_UNSUPPORTED if libbladeRF is not built with support
 *         for this functionality,
 *         or a value from \ref RETCODES list on failures.
 */
API_EXPORT
int CALL_CONV bladerf_sync_config(struct bladerf *dev,
                                  bladerf_module module,
                                  bladerf_format format,
                                  unsigned int num_buffers,
                                  unsigned int buffer_size,
                                  unsigned int num_transfers,
                                  unsigned int stream_timeout);

/**
 * Transmit IQ samples.
 *
 * Under the hood, this call starts up an underlying asynchronous stream as
 * needed. This stream can be stopped by disabling the TX module. (See
 * bladerf_enable_module for more details.)
 *
 * Samples will only be sent to the FPGA when a buffer have been filled. The
 * number of samples required to fill a buffer corresponds to the `buffer_size`
 * parameter passed to bladerf_sync_config().
 *
 * @param[in]   dev         Device handle
 *
 * @param[in]   samples     Array of samples
 *
 * @param[in]   num_samples Number of samples to write
 *
 * @param[in]   metadata    Sample metadata. This must be provided when using
 *                          the ::BLADERF_FORMAT_SC16_Q11_META format, but may
 *                          be NULL when the interface is configured for
 *                          the ::BLADERF_FORMAT_SC16_Q11 format.
 *
 * @param[in]   timeout_ms  Timeout (milliseconds) for this call to complete.
 *                          Zero implies "infinite."
 *
 * @pre A bladerf_sync_config() call has been to configure the device for
 *      synchronous data transfer.
 *
 * @pre A call to bladerf_enable_module() should be made before attempting to
 *      transmit samples. Failing to do this may result in timeouts and other
 *      errors.
 *
 * @return 0 on success,
 *         BLADERF_ERR_UNSUPPORTED if libbladeRF is not built with support
 *         for this functionality,
 *         or a value from \ref RETCODES list on failures.
 */
API_EXPORT
int CALL_CONV bladerf_sync_tx(struct bladerf *dev,
                              void *samples, unsigned int num_samples,
                              struct bladerf_metadata *metadata,
                              unsigned int timeout_ms);

/**
 * Receive IQ samples.
 *
 * Under the hood, this call starts up an underlying asynchronous stream as
 * needed. This stream can be stopped by disabling the RX module. (See
 * bladerf_enable_module for more details.)
 *
 * @param[in]   dev         Device handle
 *
 * @param[out]  samples     Buffer to store samples in. The caller is
 *                          responsible for ensuring this buffer is sufficiently
 *                          large for the number of samples requested,
 *                          considering the size of the sample format being
 *                          used.
 *
 * @param[in]   num_samples Number of samples to read
 *
 * @param[out]  metadata    Sample metadata. This must be provided when using
 *                          the ::BLADERF_FORMAT_SC16_Q11_META format, but may
 *                          be NULL when the interface is configured for
 *                          the ::BLADERF_FORMAT_SC16_Q11 format.
 *
 * @param[in]   timeout_ms  Timeout (milliseconds) for this call to complete.
 *                          Zero implies "infinite."
 *
 * @pre A bladerf_sync_config() call has been to configure the device for
 *      synchronous data transfer.
 *
 * @pre A call to bladerf_enable_module() should be made before attempting to
 *      receive samples. Failing to do this may result in timeouts and other
 *      errors.
 *
 *
 * @return 0 on success,
 *         BLADERF_ERR_UNSUPPORTED if libbladeRF is not built with support
 *         for this functionality,
 *         or a value from \ref RETCODES list on failures.
 */
API_EXPORT
int CALL_CONV bladerf_sync_rx(struct bladerf *dev,
                              void *samples, unsigned int num_samples,
                              struct bladerf_metadata *metadata,
                              unsigned int timeout_ms);


/** @} (End of FN_DATA_SYNC) */

/**
 * @defgroup FN_INFO    Device info
 *
 * These functions provide the ability to query various pieces of information
 * from an attached device. They are thread-safe.
 *
 * @{
 */

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
 * FPGA device variant (size)
 */
typedef enum {
    BLADERF_FPGA_UNKNOWN = 0,   /**< Unable to determine FPGA variant */
    BLADERF_FPGA_40KLE = 40,    /**< 40 kLE FPGA */
    BLADERF_FPGA_115KLE = 115   /**< 115 kLE FPGA */
} bladerf_fpga_size;


/**
 * Query a device's serial number
 *
 * @param[in]   dev     Device handle
 * @param[out]  serial  This user-supplied buffer, which <b>must be at least
 *                      ::BLADERF_SERIAL_LENGTH bytes</b>, will be updated to
 *                      contain a NUL-terminated serial number string. If an
 *                      error occurs (as indicated by a non-zero return value),
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
 * @param[out]  size    Will be updated with the on-board FPGA's size. If an
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
 * These functions provide the ability to load and program devices
 * on the bladeRF board. They are thread-safe.
 *
 * @{
 */

/**
 * Write FX3 firmware to the bladeRF's SPI flash
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
 * Load device's FPGA. Note that this FPGA configuration will be reset
 * at the next power cycle.
 *
 * @param   dev         Device handle
 * @param   fpga        Full path to FPGA bitstream
 *
 * @return 0 upon successfully, or a value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_load_fpga(struct bladerf *dev, const char *fpga);

/**
 * Write the provided FPGA image to the bladeRF's SPI flash and enable FPGA
 * loading from SPI flash at power on (also referred to within this project as
 * FPGA "autoloading").
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
 * Erase the FPGA region of SPI flash, effectively disabling FPGA autoloading
 *
 * @param   dev         Device handle
 */
API_EXPORT
int CALL_CONV bladerf_erase_stored_fpga(struct bladerf *dev);

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
 * @param   dev         Device handle
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_jump_to_bootloader(struct bladerf *dev);

/** @} (End of FN_PROG) */


/**
 * @defgroup FN_MISC Miscellaneous
 * @{
 */

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
 * Sets the filter level for displayed log messages. Messages that are at or
 * above the specified log level will be printed, while messages with a lower
 * log level will be suppressed.
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
 * and loading flash contents with metadata.
 *
 * @{
 */

/** Type of data stored in a flash image */
typedef enum {
    BLADERF_IMAGE_TYPE_INVALID = -1,  /**< Used to denote invalid value */
    BLADERF_IMAGE_TYPE_RAW,           /**< Misc. raw data */
    BLADERF_IMAGE_TYPE_FIRMWARE,      /**< Firmware data */
    BLADERF_IMAGE_TYPE_FPGA_40KLE,    /**< FPGA bitstream for 40 KLE device */
    BLADERF_IMAGE_TYPE_FPGA_115KLE,   /**< FPGA bitstream for 115  KLE device */
    BLADERF_IMAGE_TYPE_CALIBRATION,   /**< Board calibration */
    BLADERF_IMAGE_TYPE_RX_DC_CAL,     /**< RX DC offset calibration table */
    BLADERF_IMAGE_TYPE_TX_DC_CAL,     /**< TX DC offset calibration table */
    BLADERF_IMAGE_TYPE_RX_IQ_CAL,     /**< RX IQ balance calibration table */
    BLADERF_IMAGE_TYPE_TX_IQ_CAL,     /**< TX IQ balance calibration table */
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
 * values are converted to big-endian byte order, for ease of reading in a hex
 * editor.
 *
 * When creating and using a bladerf_image of type BLADERF_IMAGE_TYPE_RAW,
 * the address and length fields must be erase-block aligned.
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
     * The +1 here is actually extraneous; BLADERF_SERIAL_LENGTH already
     * accounts for a NUL terminator. However, this is left here to avoid
     * breaking backwards compatibility.
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
 *         NULL on memory allocation failure or invalid address/length.
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
struct bladerf_image * CALL_CONV bladerf_alloc_cal_image(
                                                bladerf_fpga_size fpga_size,
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
 * @pre   `image` has been initialized using bladerf_alloc_image()
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
 * This structure is used to directly apply DC calibration register values to
 * the LMS, rather than use the values resulting from an auto-calibration.
 *
 * A value < 0 is used to denote that the specified value should not
 * be written.  If a value is to be written, it will be truncated to 8-bits.
 */
struct bladerf_lms_dc_cals
{
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
 * Manually load values into LMS6002 DC calibration registers.
 *
 * This is generally intended for applying a set of known values resulting from
 * a previous run of the LMS autocalibrations.
 *
 * @param   dev        Device handle
 * @param   dc_cals    Calibration values to load. Values set to <0 will
 *                     not be applied.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_lms_set_dc_cals(struct bladerf *dev,
                                     const struct bladerf_lms_dc_cals *dc_cals);

/**
 * Retrieve the current DC calibration values from the LMS6002
 *
 * @param[in]   dev        Device handle
 * @param[out]  dc_cals    Populated with current values
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_lms_get_dc_cals(struct bladerf *dev,
                                      struct bladerf_lms_dc_cals *dc_cals);

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
 * Counter mode enable
 *
 * Setting this bit to 1 instructs the FPGA to replace the (I, Q) pair in
 * sample data with an incrementing, little-endian, 32-bit counter value. A
 * 0 in bit specifies that sample data should be sent (as normally done).
 *
 * This feature is useful when debugging issues involving dropped samples.
 */
#define BLADERF_GPIO_COUNTER_ENABLE (1 << 9)

/**
 * Switch to use RX low band (300M - 1.5GHz)
 *
 * @note This is set using bladerf_set_frequency().
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
 * However, the caller need not set this in bladerf_config_gpio_write() calls.
 * The library will set this as needed; callers generally
 * do not need to be concerned with setting/clearing this bit.
 */
#define BLADERF_GPIO_FEATURE_SMALL_DMA_XFER (1 << 7)

/**
 * Enable-bit for timestamp counter in the FPGA
 */
#define BLADERF_GPIO_TIMESTAMP      (1 << 16)

/**
 * Timestamp 2x divider control
 *
 * By default (value = 0), the sample counter is incremented with I and Q,
 * yielding two counts per sample.
 *
 * Set this bit to 1 to enable a 2x timestamp divider, effectively
 * achieving 1 timestamp count per sample.
 * */
#define BLADERF_GPIO_TIMESTAMP_DIV2 (1 << 17)

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
 * Read a expansion GPIO register
 *
 * @param   dev         Device handle
 * @param   val         Pointer to variable the data should be read into
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_expansion_gpio_read(struct bladerf *dev, uint32_t *val);

/**
 * Write a expansion GPIO register. Callers should be sure to perform a
 * read-modify-write sequence to avoid accidentally clearing other
 * GPIO bits that may be set by the library internally.
 *
 * @param   dev         Device handle
 * @param   val         Data to write to GPIO register
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_expansion_gpio_write(struct bladerf *dev, uint32_t val);

/**
 * Read a expansion GPIO direction register
 *
 * @param   dev         Device handle
 * @param   val         Pointer to variable the data should be read into
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_expansion_gpio_dir_read(struct bladerf *dev,
                                              uint32_t *val);

/**
 * Write a expansion GPIO direction register. Callers should be sure to perform
 * a read-modify-write sequence to avoid accidentally clearing other
 * GPIO bits that may be set by the library internally.
 *
 * @param   dev         Device handle
 * @param   val         Data to write to GPIO register
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_expansion_gpio_dir_write(struct bladerf *dev,
                                               uint32_t val);

/**
 * Retrieve the current timestamp counter value from the FPGA
 *
 * @param   dev         Device handle
 * @param   mod         Module to perform streaming with
 * @param   value       Pointer to variable the data should be read into
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_timestamp(struct bladerf *dev, bladerf_module mod,
                                    uint64_t *value);

/**
 * Write value to VCTCXO trim DAC
 *
 * @param   dev         Device handle
 * @param   val         Value to write to VCTCXO trim DAC
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_dac_write(struct bladerf *dev, uint16_t val);

/**
 * Read value from VCTCXO trim DAC.
 *
 * This is similar to bladerf_get_vctcxo_trim(), except that it returns the
 * current trim DAC value, as opposed to the calibration value read from
 * flash.
 *
 * Use this if you are trying to query the value after having previously
 * made calls to bladerf_dac_write().
 *
 * @param[in]   dev     Device handle
 * @param[out]  val     Value to read from VCTCXO trim DAC
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_dac_read(struct bladerf *dev, uint16_t *val);

/**
 * Write value to secondary XB SPI
 *
 * @param   dev         Device handle
 * @param   val         Data to write to XB SPI
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_xb_spi_write(struct bladerf *dev, uint32_t val);


/**
 * Perform DC calibration
 *
 * @param   dev         Device handle
 * @param   module      Module to calibrate
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_calibrate_dc(struct bladerf *dev,
                                   bladerf_cal_module module);

/** @} (End of LOW_LEVEL) */

/**
 * @defgroup FN_FLASH  Low-level flash routines
 *
 * These routines provide the ability to manipulate the device's SPI flash.
 * Most users will find no reason to use these, as higher-level functions
 * perform flash accesses under the hood.
 *
 * These routines are not recommended for use other than testing, development,
 * and significant customization of the bladeRF platform (which would require
 * firmware and libbladeRF changes).
 *
 * Use of these functions requires an understanding of the underlying SPI
 * flash device, and the bladeRF's flash memory map. Be sure to review the
 * following page and the associated flash datasheet before using these
 * functions:
 *   https://github.com/nuand/bladeRF/wiki/FX3-Firmware#spi-flash-layout
 *
 * These functions are thread-safe.
 *
 * @{
 */

/** Total size of bladeRF SPI flash, in bytes */
#define BLADERF_FLASH_TOTAL_SIZE  (4 * 1024 * 1024)

/** SPI flash page size, in bytes */
#define BLADERF_FLASH_PAGE_SIZE   256

/** SPI flash 64KiB erase block size, in bytes */
#define BLADERF_FLASH_EB_SIZE     (64 * 1024)

/** Size of the SPI flash, in pages */
#define BLADERF_FLASH_NUM_PAGES \
    (BLADERF_FLASH_TOTAL_SIZE / BLADERF_FLASH_PAGE_SIZE)

/** Size of the SPI flash, in 64KiB erase blocks */
#define BLADERF_FLASH_NUM_EBS \
    (BLADERF_FLASH_TOTAL_SIZE / BLADERF_FLASH_EB_SIZE)



/** Convert byte addresses to units of flash pages */
#define BLADERF_FLASH_TO_PAGES(bytes) ((bytes) / BLADERF_FLASH_PAGE_SIZE)

/** Convert byte addresses to units of flash erase blocks */
#define BLADERF_FLASH_TO_EB(bytes)    ((bytes) / BLADERF_FLASH_EB_SIZE)



/** Byte address of FX3 firmware */
#define BLADERF_FLASH_ADDR_FIRMWARE     0x00000000

/** Flash page where FX3 firmware starts */
#define BLADERF_FLASH_PAGE_FIRMWARE \
            (BLADERF_FLASH_TO_PAGES(BLADERF_FLASH_ADDR_FIRMWARE))

/** Flash erase block where FX3 firmware starts */
#define BLADERF_FLASH_EB_FIRMWARE \
            (BLADERF_FLASH_TO_EB(BLADERF_FLASH_ADDR_FIRMWARE))

/** Length of firmware region of flash, in bytes */
#define BLADERF_FLASH_BYTE_LEN_FIRMWARE 0x00030000

/** Length of firmware region of flash, in pages */
#define BLADERF_FLASH_PAGE_LEN_FIRMWARE \
            (BLADERF_FLASH_TO_PAGES(BLADERF_FLASH_BYTE_LEN_FIRMWARE))

/** Length of firmware region of flash, in erase blocks */
#define BLADERF_FLASH_EB_LEN_FIRMWARE \
            (BLADERF_FLASH_TO_EB(BLADERF_FLASH_BYTE_LEN_FIRMWARE))


/** Byte address of calibration data region */
#define BLADERF_FLASH_ADDR_CAL  0x00030000

/** Flash page where calibration data starts */
#define BLADERF_FLASH_PAGE_CAL  (BLADERF_FLASH_TO_PAGES(BLADERF_FLASH_ADDR_CAL))

/** Flash erase block where calibration data starts */
#define BLADERF_FLASH_EB_CAL    (BLADERF_FLASH_TO_EB(BLADERF_FLASH_ADDR_CAL))

/** Length of calibration data, in bytes */
#define BLADERF_FLASH_BYTE_LEN_CAL 0x100

/** Length of calibration data, in pages */
#define BLADERF_FLASH_PAGE_LEN_CAL \
            (BLADERF_FLASH_TO_PAGES(FLASH_BYTE_LEN_CAL))

/**
 * Length of calibration data, in erase blocks. This is a special case,
 * as the entire remainder of the erase block is reserved for future calibration
 * data use. When updating calibration data, the whole block will be erased,
 * even though the current firmware only uses one page of it. */
#define BLADERF_FLASH_EB_LEN_CAL 1


/**
 * Byte address of of the autoloaded FPGA and associated metadata.
 * The first page is allocated for metadata, and the FPGA bitstream resides
 * in the following pages.
 */
#define BLADERF_FLASH_ADDR_FPGA 0x00040000

/** Flash page where FPGA metadata and bitstream start */
#define BLADERF_FLASH_PAGE_FPGA \
            (BLADERF_FLASH_TO_PAGES(BLADERF_FLASH_ADDR_FPGA))

/** Flash erase block where FPGA metadata and bitstream start */
#define BLADERF_FLASH_EB_FPGA \
            (BLADERF_FLASH_TO_EB(BLADERF_FLASH_ADDR_FPGA))

/** Length of entire FPGA region, including both metadata and bitstream. */
#define BLADERF_FLASH_BYTE_LEN_FPGA 0x00370000

/** Length of entire FPGA region, in units of erase blocks */
#define BLADERF_FLASH_EB_LEN_FPGA \
            (BLADERF_FLASH_TO_EB(BLADERF_FLASH_BYTE_LEN_FPGA))

/**
 * Erase regions of the bladeRF's SPI flash
 *
 * This function operates in units of 64KiB erase blocks
 *
 * @param   dev             Device handle
 * @param   erase_block     Erase block to start erasing at
 * @param   count           Number of blocks to erase.
 *
 * @return 0 on success, or BLADERF_ERR_INVAL on an invalid `erase_block` or
 *         `count` value, or a value from \ref RETCODES list on other failures
 */
API_EXPORT
int CALL_CONV bladerf_erase_flash(struct bladerf *dev,
                                  uint32_t erase_block, uint32_t count);

/**
 * Read data from the bladeRF's SPI flash
 *
 * This function operates in units of 256-byte pages.
 *
 * @param   dev     Device handle
 * @param   buf     Buffer to read data into. Must be
 *                  `count` * BLADERF_FLASH_PAGE_SIZE bytes or larger.
 *
 * @param   page    Page to begin reading from
 * @param   count   Number of pages to read
 *
 * @return 0 on success, or BLADERF_ERR_INVAL on an invalid `page` or
 *         `count` value, or a value from \ref RETCODES list on other failures.
 */
API_EXPORT
int CALL_CONV bladerf_read_flash(struct bladerf *dev, uint8_t *buf,
                                 uint32_t page, uint32_t count);

/**
 * Write data to the bladeRF's SPI flash device
 *
 * @param   dev   Device handle
 * @param   buf   Data to write to flash
 *
 * @param   page  Page to begin writing at
 * @param   count Number of pages to write
 *
 * @return 0 on success, or BLADERF_ERR_INVAL on an invalid `page` or
 *         `count` value, or a value from \ref RETCODES list on other failures.
 */
API_EXPORT
int CALL_CONV bladerf_write_flash(struct bladerf *dev, const uint8_t *buf,
                                  uint32_t page, uint32_t count);

/** @} (End of FN_FLASH) */

/**
 * @defgroup FN_BOOTLOADER  Bootloader Recovery
 *
 * These functions provide the ability to identify devices enumerating as an
 * FX3 bootloader, download firmware to RAM, and then execute the firmware.
 *
 * Care should be taken to ensure that devices operated on are indeed a bladeRF,
 * as opposed to another FX3-based device running in bootloader mode.
 * @{
 */

/**
 * Get a list of devices that are running the FX3 bootloader.
 *
 * After obtaining this list, identify the device that you would like to load
 * firmware onto. Save the bus and address values so that you can provide them
 * to bladerf_load_fw_from_bootloader(), and then free this list via
 * bladerf_free_device_list().
 *
 * @param[out]   list    Upon finding devices, this will be updated to point
 *                       to a list of bladerf_devinfo structures that
 *                       describe the identified devices.
 *
 * @return Number of items populated in `list`,
 *         or an error value from the \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_bootloader_list(struct bladerf_devinfo **list);

/**
 * Download firmware to the specified device that is enumarating an FX3
 * bootloader, and begin executing the firmware from RAM.
 *
 * Note that this function <b>does not</b> write the firmware to SPI flash.
 * If this is desired, open the newly enumerated device with bladerf_open() and
 * use bladerf_flash_firmware().
 *
 * @param device_identifier   Device identifier string describing the
 *                            backend to use via the
 *                              `<backend>:device=<bus>:<addr>` syntax.
 *                            If this is NULL, the backend, bus, and addr
 *                            arguments will be used instead.
 *
 * @param backend             Backend to use. This is only used if
 *                              device_identifier is NULL.
 *
 * @param bus                 Bus number the device is located on. This is only
 *                              used if device_identifier is NULL.
 *
 * @param addr                Bus address the device is located on. This is only
 *                              used if device_identifier is NULL.
 *
 * @param file                Filename of the firmware image to boot
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 *
 *
 */
API_EXPORT
int CALL_CONV bladerf_load_fw_from_bootloader(const char *device_identifier,
                                              bladerf_backend backend,
                                              uint8_t bus, uint8_t addr,
                                              const char *file);

/** @} (End of FN_BOOTLOADER) */

#ifdef __cplusplus
}
#endif

#endif /* BLADERF_H_ */
