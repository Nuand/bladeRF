/**
 * @file libbladeRF.h
 *
 * @brief bladeRF library
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
#ifndef LIBBLADERF_H_
#define LIBBLADERF_H_

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * @ingroup FN_LIBRARY_VERSION
 *
 * libbladeRF API version
 *
 * As of libbladeRF v1.5.0, this macro is defined to assist with feature
 * detection. Generally, this will be used as follows:
 *
 * @code
 * #if defined(LIBBLADERF_API_VERSION) && (LIBBLADERF_API_VERSION >= 0x01050000)
 *      // ...  Use features added in libbladeRF v1.5.0 ...
 * #endif
 * @endcode
 *
 * This value is defined as follows:
 *      `(major << 24) | (minor << 16) | (patch << 8) | (reserved << 0)`
 *
 * The reserved field may be used at a later date to denote additions between
 * releases. It will be set to zero when not used.
 *
 * This value is intended to track the values returned by bladerf_version().
 * Fields are updated per the scheme defined here:
 *
 *  https://github.com/Nuand/bladeRF/blob/master/doc/development/versioning.md
 */
#define LIBBLADERF_API_VERSION (0x02050000)

#ifdef __cplusplus
extern "C" {
#else
/**
 * stdbool.h is not applicable for C++ programs, as the language inherently
 * provides the bool type.
 *
 * Users of Visual Studio 2012 and earlier will need to supply a stdbool.h
 * implementation, as it is not included with the toolchain. One is provided
 * with the bladeRF source code. Visual Studio 2013 onward supplies this header.
 */
#include <stdbool.h>
#endif

// clang-format off
#if defined _WIN32 || defined __CYGWIN__
#   include <windows.h>
#   define CALL_CONV __cdecl
#   ifdef __GNUC__
#       define API_EXPORT __attribute__ ((dllexport))
#   else
#       define API_EXPORT __declspec(dllexport)
#   endif
#elif defined _DOXYGEN_ONLY_ || defined MATLAB_LINUX_THUNK_BUILD_
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
// clang-format on

/**
 * @defgroup FN_INIT    Initialization
 *
 * The functions in this section provide the ability query and inspect available
 * devices, initialize them, and deinitialize them.
 *
 * See the \link boilerplate.html Device configuration boilerplate\endlink
 * page for an overview on how to open and configure a device.
 *
 * These functions are thread-safe.
 *
 * @{
 */

/** This structure is an opaque device handle */
struct bladerf;

/**
 * Backend by which the host communicates with the device
 */
typedef enum {
    BLADERF_BACKEND_ANY,         /**< "Don't Care" -- use any available
                                  *   backend */
    BLADERF_BACKEND_LINUX,       /**< Linux kernel driver */
    BLADERF_BACKEND_LIBUSB,      /**< libusb */
    BLADERF_BACKEND_CYPRESS,     /**< CyAPI */
    BLADERF_BACKEND_DUMMY = 100, /**< Dummy used for development purposes */
} bladerf_backend;

/** Length of device description string, including NUL-terminator */
#define BLADERF_DESCRIPTION_LENGTH 33

/** Length of device serial number string, including NUL-terminator */
#define BLADERF_SERIAL_LENGTH 33

/**
 * Information about a bladeRF attached to the system
 */
struct bladerf_devinfo {
    bladerf_backend backend;            /**< Backend to use when connecting to
                                         *   device */
    char serial[BLADERF_SERIAL_LENGTH]; /**< Device serial number string */
    uint8_t usb_bus;                    /**< Bus # device is attached to */
    uint8_t usb_addr;                   /**< Device address on bus */
    unsigned int instance;              /**< Device instance or ID */

    /** Manufacturer description string */
    char manufacturer[BLADERF_DESCRIPTION_LENGTH];
    char product[BLADERF_DESCRIPTION_LENGTH]; /**< Product description string */
};

/**
 * Information about a bladeRF attached to the system
 */
struct bladerf_backendinfo {
    int handle_count;                   /**< Backend handle count */
    void *handle;                       /**< Backend handle for device */
    int lock_count;                     /**< Backend lock count */
    void *lock;                         /**< Backend lock for device */
};

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
 * @note Failing to close a device will result in memory leaks.
 *
 * @post `device` is deallocated and may no longer be used.
 *
 * @param   device  Device handle previously obtained by bladerf_open(). This
 *                  function does nothing if device is NULL.
 */
API_EXPORT
void CALL_CONV bladerf_close(struct bladerf *device);

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
 * @param[inout]    devices     List of available devices
 */
API_EXPORT
void CALL_CONV bladerf_free_device_list(struct bladerf_devinfo *devices);

/**
 * Initialize a device identifier information structure to a "wildcard" state.
 *
 * The values in each field will match any value for that field.
 *
 * @note Passing a bladerf_devinfo initialized with this function to
 *       bladerf_open_with_devinfo() will match the first device found.
 */
API_EXPORT
void CALL_CONV bladerf_init_devinfo(struct bladerf_devinfo *info);

/**
 * Fill out a provided bladerf_devinfo structure, given an open device handle.
 *
 * @pre `dev` must be a valid device handle.
 *
 * @param        dev     Device handle previously obtained with bladerf_open()
 * @param[out]   info    Device information populated by this function
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_devinfo(struct bladerf *dev,
                                  struct bladerf_devinfo *info);


/**
 * Fill out a provided bladerf_backendinfo structure, given an open device handle.
 *
 * @pre `dev` must be a valid device handle.
 *
 * @param        dev     Device handle previously obtained with bladerf_open()
 * @param[out]   info    Backend information populated by this function
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_backendinfo(struct bladerf *dev,
                                  struct bladerf_backendinfo *info);
/**
 * Populate a device identifier information structure using the provided
 * device identifier string.
 *
 * @param[in]   devstr  Device identifier string, formated as described
 *                      in the bladerf_open() documentation
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
 *
 * @param[in]   a   the first bladerf_devinfo struct
 * @param[in]   b   the second bladerf_devinfo struct
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
 * @return A string that can used to specify the `backend` portion of a device
 *         identifier string. (See bladerf_open().)
 */
API_EXPORT
const char *CALL_CONV bladerf_backend_str(bladerf_backend backend);

/**
 * Enable or disable USB device reset operation upon opening a device for
 * future bladerf_open() and bladerf_open_with_devinfo() calls.
 *
 * This operation has been found to be necessary on Linux-based systems for
 * some USB 3.0 controllers on Linux.
 *
 * This <b>does not</b> reset the state of the device in terms of its frequency,
 * gain, sample rate, etc. settings.
 *
 * @param[in]   enabled     Set true to enable the use of the USB device reset,
 *                          and false otherwise.
 */
API_EXPORT
void CALL_CONV bladerf_set_usb_reset_on_open(bool enabled);

/** @} (End FN_INIT) */

/**
 * @defgroup FN_INFO    Device properties
 *
 * These functions provide the ability to query various pieces of information
 * from an attached device.
 *
 * These functions are thread-safe.
 *
 * @{
 */

/**
 * Range structure
 */
struct bladerf_range {
    int64_t min;  /**< Minimum value */
    int64_t max;  /**< Maximum value */
    int64_t step; /**< Step of value */
    float scale;  /**< Unit scale */
};

/**
 * Serial number structure
 */
struct bladerf_serial {
    char serial[BLADERF_SERIAL_LENGTH]; /**< Device serial number string */
};

/**
 * Version structure for FPGA, firmware, libbladeRF, and associated utilities
 */
struct bladerf_version {
    uint16_t major;       /**< Major version */
    uint16_t minor;       /**< Minor version */
    uint16_t patch;       /**< Patch version */
    const char *describe; /**< Version string with any additional suffix
                           *   information.
                           *
                           *   @warning Do not attempt to modify or free()
                           *            this string. */
};

/**
 * FPGA device variant (size)
 */
typedef enum {
    BLADERF_FPGA_UNKNOWN = 0,   /**< Unable to determine FPGA variant */
    BLADERF_FPGA_40KLE   = 40,  /**< 40 kLE FPGA */
    BLADERF_FPGA_115KLE  = 115, /**< 115 kLE FPGA */
    BLADERF_FPGA_A4      = 49,  /**< 49 kLE FPGA (A4) */
    BLADERF_FPGA_A5      = 77,  /**< 77 kLE FPGA (A5) */
    BLADERF_FPGA_A9      = 301  /**< 301 kLE FPGA (A9) */
} bladerf_fpga_size;

/**
 * This enum describes the USB Speed at which the bladeRF is connected.
 * Speeds not listed here are not supported.
 */
typedef enum {
    BLADERF_DEVICE_SPEED_UNKNOWN,
    BLADERF_DEVICE_SPEED_HIGH,
    BLADERF_DEVICE_SPEED_SUPER
} bladerf_dev_speed;

/**
 * FPGA configuration source
 *
 * Note: the numbering of this enum must match NuandFpgaConfigSource in
 * firmware_common/bladeRF.h
 */
typedef enum {
    BLADERF_FPGA_SOURCE_UNKNOWN = 0, /**< Uninitialized/invalid */
    BLADERF_FPGA_SOURCE_FLASH   = 1, /**< Last FPGA load was from flash */
    BLADERF_FPGA_SOURCE_HOST    = 2  /**< Last FPGA load was from host */
} bladerf_fpga_source;

/**
 * Query a device's serial number (deprecated)
 *
 * @param       dev     Device handle
 * @param[out]  serial  This user-supplied buffer, which <b>must be at least
 *                      ::BLADERF_SERIAL_LENGTH bytes</b>, will be updated to
 *                      contain a NUL-terminated serial number string. If an
 *                      error occurs (as indicated by a non-zero return value),
 *                      no data will be written to this buffer.
 *
 * @deprecated New code should use ::bladerf_get_serial_struct instead.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_serial(struct bladerf *dev, char *serial);

/**
 * Query a device's serial number
 *
 * @param       dev     Device handle
 * @param[out]  serial  Pointer to a bladerf_serial structure, which will be
 *                      populated with a `serial` string on success.
 *
 * Example code:
 *
 * @code
 *   struct bladerf_serial sn;
 *
 *   status = bladerf_get_serial_struct(dev, &sn);
 *   if (status < 0) {
 *       // error handling here
 *   }
 *
 *   printf("Serial number: %s\n", sn.serial);
 * @endcode
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_serial_struct(struct bladerf *dev,
                                        struct bladerf_serial *serial);

/**
 * Query a device's FPGA size
 *
 * @param       dev     Device handle
 * @param[out]  size    Will be updated with the on-board FPGA's size. If an
 *                      error occurs, no data will be written to this pointer.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_fpga_size(struct bladerf *dev,
                                    bladerf_fpga_size *size);

/**
 * Query a device's expected FPGA bitstream length, in bytes
 *
 * @param       dev     Device handle
 * @param[out]  size    Will be updated with expected bitstream length. If an
 *                      error occurs, no data will be written to this pointer.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_fpga_bytes(struct bladerf *dev, size_t *size);

/**
 * Query a device's Flash size
 *
 * @param       dev      Device handle
 * @param[out]  size     Will be updated with the size of the onboard flash,
 *                       in bytes. If an error occurs, no data will be written
 *                       to this pointer.
 * @param[out]  is_guess True if the flash size is a guess (using FPGA size).
 *                       False if the flash ID was queried and its size
 *                       was successfully decoded.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_flash_size(struct bladerf *dev,
                                     uint32_t *size,
                                     bool *is_guess);

/**
 * Query firmware version
 *
 * @param       dev         Device handle
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
 * @param       dev         Device handle
 *
 * @return 1 if FPGA is configured, 0 if it is not,
 *         and value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_is_fpga_configured(struct bladerf *dev);

/**
 * Query FPGA version
 *
 * @param       dev         Device handle
 * @param[out]  version     Updated to contain firmware version
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_fpga_version(struct bladerf *dev,
                                   struct bladerf_version *version);

/**
 * Query FPGA configuration source
 *
 * Determine whether the FPGA image was loaded from flash, or if it was
 * loaded from the host, by asking the firmware for the last-known FPGA
 * configuration source.
 *
 * @param       dev     Device handle
 * @param[out]  source  Source of the configuration
 *
 * @return 0 on success, ::BLADERF_ERR_UNSUPPORTED if the
 * BLADERF_CAP_FW_FPGA_SOURCE capability is not present, value from \ref
 * RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_fpga_source(struct bladerf *dev,
                                      bladerf_fpga_source *source);

/**
 * Obtain the bus speed at which the device is operating
 *
 * @param       dev     Device handle
 *
 * @return Device speed enumeration
 */
API_EXPORT
bladerf_dev_speed CALL_CONV bladerf_device_speed(struct bladerf *dev);

/**
 * Get the board name
 *
 * @param       dev     Device handle
 *
 * @return Pointer to C string with the board's model name, either `bladerf1`
 *         for a bladeRF x40/x115, or `bladerf2` for a bladeRF Micro.
 */
API_EXPORT
const char *CALL_CONV bladerf_get_board_name(struct bladerf *dev);

/** @} (End FN_INFO) */

/**
 * @defgroup FN_CHANNEL Channel control
 *
 * The RX and TX channels are independently configurable. As such, many
 * libbladeRF functions require a ::bladerf_channel parameter to specify the
 * desired channel.
 *
 * These functions are thread-safe.
 *
 * @{
 */

/**
 * Channel type
 *
 * Example usage:
 *
 * @code{.c}
 * // RX Channel 0
 * bladerf_channel ch = BLADERF_CHANNEL_RX(0);
 *
 * // RX Channel 1
 * bladerf_channel ch = BLADERF_CHANNEL_RX(1);
 *
 * // TX Channel 0
 * bladerf_channel ch = BLADERF_CHANNEL_TX(0);
 *
 * // TX Channel 1
 * bladerf_channel ch = BLADERF_CHANNEL_TX(1);
 * @endcode
 */
typedef int bladerf_channel;

/**
 * RX Channel Macro
 *
 * Example usage:
 *
 * @code{.c}
 * // RX Channel 0
 * bladerf_channel ch = BLADERF_CHANNEL_RX(0);
 *
 * // RX Channel 1
 * bladerf_channel ch = BLADERF_CHANNEL_RX(1);
 * @endcode
 */
#define BLADERF_CHANNEL_RX(ch) (bladerf_channel)(((ch) << 1) | 0x0)

/**
 * TX Channel Macro
 *
 * Example usage:
 *
 * @code{.c}
 * // TX Channel 0
 * bladerf_channel ch = BLADERF_CHANNEL_TX(0);
 *
 * // TX Channel 1
 * bladerf_channel ch = BLADERF_CHANNEL_TX(1);
 * @endcode
 */
#define BLADERF_CHANNEL_TX(ch) (bladerf_channel)(((ch) << 1) | 0x1)

/**
 * Invalid channel
 */
#define BLADERF_CHANNEL_INVALID (bladerf_channel)(-1)

/** @cond IGNORE */
#define BLADERF_DIRECTION_MASK (0x1)
/** @endcond */

/** @cond IGNORE */
/* Backwards compatible mapping to `bladerf_module`. */
typedef bladerf_channel bladerf_module;
#define BLADERF_MODULE_INVALID BLADERF_CHANNEL_INVALID
#define BLADERF_MODULE_RX BLADERF_CHANNEL_RX(0)
#define BLADERF_MODULE_TX BLADERF_CHANNEL_TX(0)
/** @endcond */

/**
 * Convenience macro: true if argument is a TX channel
 */
#define BLADERF_CHANNEL_IS_TX(ch) (ch & BLADERF_TX)

/**
 * Stream direction
 */
typedef enum {
    BLADERF_RX = 0, /**< Receive direction */
    BLADERF_TX = 1, /**< Transmit direction */
} bladerf_direction;

/**
 * Stream channel layout
 */
typedef enum {
    BLADERF_RX_X1 = 0, /**< x1 RX (SISO) */
    BLADERF_TX_X1 = 1, /**< x1 TX (SISO) */
    BLADERF_RX_X2 = 2, /**< x2 RX (MIMO) */
    BLADERF_TX_X2 = 3, /**< x2 TX (MIMO) */
} bladerf_channel_layout;

/**
 * Get the number of RX or TX channels supported by the given device
 *
 * @param       dev     Device handle
 * @param[in]   dir     Stream direction
 *
 * @return Number of channels
 */
API_EXPORT
size_t CALL_CONV bladerf_get_channel_count(struct bladerf *dev,
                                           bladerf_direction dir);

/**
 * @defgroup FN_GAIN    Gain
 *
 * These functions provide control over the device's RX and TX gain stages.
 *
 * These functions are thread-safe.
 *
 * @{
 */

/**
 * Gain value, in decibels (dB)
 *
 * May be positive or negative.
 */
typedef int bladerf_gain;

/**
 * Gain control modes
 *
 * In general, the default mode is automatic gain control. This will
 * continuously adjust the gain to maximize dynamic range and minimize clipping.
 *
 * @note Implementers are encouraged to simply present a boolean choice between
 *       "AGC On" (::BLADERF_GAIN_DEFAULT) and "AGC Off" (::BLADERF_GAIN_MGC).
 *       The remaining choices are for advanced use cases.
 */
typedef enum {
    /** Device-specific default (automatic, when available)
     *
     * On the bladeRF x40 and x115 with FPGA versions >= v0.7.0, this is
     * automatic gain control.
     *
     * On the bladeRF 2.0 Micro, this is BLADERF_GAIN_SLOWATTACK_AGC with
     * reasonable default settings.
     */
    BLADERF_GAIN_DEFAULT,

    /** Manual gain control
     *
     * Available on all bladeRF models.
     */
    BLADERF_GAIN_MGC,

    /** Automatic gain control, fast attack (advanced)
     *
     * Only available on the bladeRF 2.0 Micro. This is an advanced option, and
     * typically requires additional configuration for ideal performance.
     */
    BLADERF_GAIN_FASTATTACK_AGC,

    /** Automatic gain control, slow attack (advanced)
     *
     * Only available on the bladeRF 2.0 Micro. This is an advanced option, and
     * typically requires additional configuration for ideal performance.
     */
    BLADERF_GAIN_SLOWATTACK_AGC,

    /** Automatic gain control, hybrid attack (advanced)
     *
     * Only available on the bladeRF 2.0 Micro. This is an advanced option, and
     * typically requires additional configuration for ideal performance.
     */
    BLADERF_GAIN_HYBRID_AGC,
} bladerf_gain_mode;

/** Default AGC mode (for backwards compatibility with libbladeRF 1.x) */
#define BLADERF_GAIN_AUTOMATIC BLADERF_GAIN_DEFAULT
/** Manual gain control (for backwards compatibility with libbladeRF 1.x) */
#define BLADERF_GAIN_MANUAL BLADERF_GAIN_MGC

/**
 * Mapping between C string description of gain modes and bladerf_gain_mode
 */
struct bladerf_gain_modes {
    const char *name;       /**< Name of gain mode */
    bladerf_gain_mode mode; /**< Gain mode enumeration */
};

/**
 * Set overall system gain
 *
 * This sets an overall system gain, optimally proportioning the gain between
 * multiple gain stages if applicable.
 *
 * @see Use bladerf_get_gain_range() to determine the range of system gain.
 *
 * On receive channels, 60 dB is the maximum gain level.
 *
 * On transmit channels, 60 dB is defined as approximately 0 dBm. Note that
 * this is not a calibrated value, and the actual output power will vary based
 * on a multitude of factors.
 *
 * @todo The gain ranges are not shifted to account for external accessories,
 *       such as amplifiers and LNAs.
 *
 * @note Values outside the valid gain range will be clamped.
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[in]   gain        Desired gain, in dB
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_gain(struct bladerf *dev,
                               bladerf_channel ch,
                               bladerf_gain gain);

/**
 * Get overall system gain
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[out]  gain        Gain, in dB
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_gain(struct bladerf *dev,
                               bladerf_channel ch,
                               bladerf_gain *gain);

/**
 * Set gain control mode
 *
 * Sets the mode for hardware AGC. Not all channels or boards will support
 * all possible values (e.g. transmit channels); invalid combinations will
 * return ::BLADERF_ERR_UNSUPPORTED.
 *
 * The special value of ::BLADERF_GAIN_DEFAULT will return hardware AGC to
 * its default value at initialization.
 *
 * @see bladerf_gain_mode for implementation guidance
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[in]   mode        Desired gain mode
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_gain_mode(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bladerf_gain_mode mode);

/**
 * Get gain control mode
 *
 * Gets the current mode for hardware AGC. If the channel or board does not
 * meaningfully have a gain mode (e.g. transmit channels), mode will be
 * set to ::BLADERF_GAIN_DEFAULT and `0` will be returned.
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[out]  mode        Gain mode
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_gain_mode(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bladerf_gain_mode *mode);

/**
 * Get available gain control modes
 *
 * Populates `modes` with a pointer to an array of structs containing the
 * supported gain modes.
 *
 * This function may be called with `NULL` for `modes` to determine the number
 * of gain modes supported.
 *
 * @see bladerf_gain_mode for implementation guidance
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[out]  modes       Supported gain modes
 *
 * @return Number of gain modes on success, value from \ref RETCODES list on
 *         failure
 */
API_EXPORT
int CALL_CONV bladerf_get_gain_modes(struct bladerf *dev,
                                     bladerf_channel ch,
                                     const struct bladerf_gain_modes **modes);

/**
 * Get range of overall system gain
 *
 * @note This may vary depending on the configured frequency, so it should be
 *       checked after setting the desired frequency.
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[out]  range       Gain range
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_gain_range(struct bladerf *dev,
                                     bladerf_channel ch,
                                     const struct bladerf_range **range);

/**
 * Set the gain for a specific gain stage
 *
 * @note Values outside the valid gain range will be clipped.
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[in]   stage       Gain stage name
 * @param[in]   gain        Desired gain
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_gain_stage(struct bladerf *dev,
                                     bladerf_channel ch,
                                     const char *stage,
                                     bladerf_gain gain);

/**
 * Set the gain for a specific gain stage
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[in]   stage       Gain stage name
 * @param[out]  gain        Gain
 *
 * Note that, in some cases, gain may be negative (e.g. transmit channels).
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_gain_stage(struct bladerf *dev,
                                     bladerf_channel ch,
                                     const char *stage,
                                     bladerf_gain *gain);

/**
 * Get gain range of a specific gain stage
 *
 * @note This may vary depending on the configured frequency, so it should be
 *       checked after setting the desired frequency.
 *
 * This function may be called with `NULL` for `range` to test if a given gain
 * range exists.
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[in]   stage       Gain stage name
 * @param[out]  range       Gain range
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_gain_stage_range(struct bladerf *dev,
                                           bladerf_channel ch,
                                           const char *stage,
                                           const struct bladerf_range **range);

/**
 * Get a list of available gain stages
 *
 * This function may be called with `NULL` for `stages`, or 0 for `count`, to
 * determine the number of gain stages.
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[out]  stages      Gain stage names
 * @param[out]  count       Number to populate
 *
 * @return Number of gain stages on success, value from \ref RETCODES list on
 *         failure
 */
API_EXPORT
int CALL_CONV bladerf_get_gain_stages(struct bladerf *dev,
                                      bladerf_channel ch,
                                      const char **stages,
                                      size_t count);

/** @} (End of FN_GAIN) */

/**
 * @defgroup FN_SAMPLING Sample rate
 *
 * This section presents functionality pertaining to configuring the
 * sample rate and mode of the device's RX and TX channels.
 *
 * These functions are thread-safe.
 *
 * @{
 */

/**
 * Sample rate, in samples per second (sps)
 */
typedef unsigned int bladerf_sample_rate;

/**
 * Rational sample rate representation
 *
 * Sample rates are in the form of
 * @f[
 *  rate = integer + \frac{num}{den}
 * @f]
 */
struct bladerf_rational_rate {
    uint64_t integer; /**< Integer portion */
    uint64_t num;     /**< Numerator in fractional portion */
    uint64_t den;     /**< Denominator in fractional portion. This must be
                       *   greater than 0. */
};

/**
 * Configure the channel's sample rate to the specified rate in Hz.
 *
 * @note This requires the sample rate is an integer value of Hz.  Use
 *       bladerf_set_rational_sample_rate() for more arbitrary values.
 *
 * @see Use bladerf_get_sample_rate_range() to determine the range of supported
 *      sample rates.
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[in]   rate        Sample rate
 * @param[out]  actual      If non-NULL, this is written with the actual
 *                          sample rate achieved.
 *
 * @return 0 on success, value from \ref RETCODES list upon failure
 */
API_EXPORT
int CALL_CONV bladerf_set_sample_rate(struct bladerf *dev,
                                      bladerf_channel ch,
                                      bladerf_sample_rate rate,
                                      bladerf_sample_rate *actual);

/**
 * Configure the channel's sample rate as a rational fraction of Hz.
 *
 * @see Use bladerf_get_sample_rate_range() to determine the range of supported
 *      sample rates.
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel to change
 * @param[in]   rate        Rational sample rate
 * @param[out]  actual      If non-NULL, this is written with the actual
 *                          rational sample rate achieved.
 *
 * @return 0 on success, value from \ref RETCODES list upon failure
 */
API_EXPORT
int CALL_CONV
    bladerf_set_rational_sample_rate(struct bladerf *dev,
                                     bladerf_channel ch,
                                     struct bladerf_rational_rate *rate,
                                     struct bladerf_rational_rate *actual);
/**
 * Get the channel's current sample rate in Hz
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[out]  rate        Current sample rate
 *
 * @return 0 on success, value from \ref RETCODES list upon failure
 */
API_EXPORT
int CALL_CONV bladerf_get_sample_rate(struct bladerf *dev,
                                      bladerf_channel ch,
                                      bladerf_sample_rate *rate);

/**
 * Get the channel's supported range of sample rates
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[out]  range       Sample rate range
 *
 * @return 0 on success, value from \ref RETCODES list upon failure
 */
API_EXPORT
int CALL_CONV bladerf_get_sample_rate_range(struct bladerf *dev,
                                            bladerf_channel ch,
                                            const struct bladerf_range **range);

/**
 * Get the channel's sample rate in rational Hz
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[out]  rate        Current rational sample rate
 *
 * @return 0 on success, value from \ref RETCODES list upon failure
 */
API_EXPORT
int CALL_CONV
    bladerf_get_rational_sample_rate(struct bladerf *dev,
                                     bladerf_channel ch,
                                     struct bladerf_rational_rate *rate);

/** @} (End of FN_SAMPLING) */

/**
 * @defgroup FN_BANDWIDTH Bandwidth
 *
 * This section defines functionality for configuring a channel's bandwidth. In
 * most cases, one should define the bandwidth to be less than the sample rate
 * to minimize the impact of aliasing.
 *
 * These functions are thread-safe.
 *
 * @{
 */

/**
 * Bandwidth, in hertz (Hz)
 */
typedef unsigned int bladerf_bandwidth;

/**
 * Set the bandwidth of the channel to the specified value in Hz
 *
 * The underlying device is capable of a discrete set of bandwidth values. The
 * caller should check the `actual` parameter to determine which of these
 * discrete bandwidth values is actually used for the requested bandwidth.
 *
 * @see Use bladerf_get_bandwidth_range() to determine the range of supported
 *      bandwidths.
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[in]   bandwidth   Desired bandwidth
 * @param[out]  actual      If non-NULL, written with the actual bandwidth that
 *                          the device was able to achieve.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_bandwidth(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bladerf_bandwidth bandwidth,
                                    bladerf_bandwidth *actual);

/**
 * Get the bandwidth of the channel
 *
 * @param       dev         Device Handle
 * @param[in]   ch          Channel
 * @param[out]  bandwidth   Actual bandwidth in Hz
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_bandwidth(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bladerf_bandwidth *bandwidth);

/**
 * Get the supported range of bandwidths for a channel
 *
 * @param       dev         Device Handle
 * @param[in]   ch          Channel
 * @param[out]  range       Bandwidth range
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_bandwidth_range(struct bladerf *dev,
                                          bladerf_channel ch,
                                          const struct bladerf_range **range);

/** @} (End of FN_BANDWIDTH) */

/**
 * @defgroup FN_TUNING  Frequency
 *
 * These functions provide the ability to tune the RX and TX channels.
 *
 * See \link tuning.html this page\endlink for more detailed information about
 * how the API performs this tuning, and for example code snippets.
 *
 * These functions are thread-safe.
 *
 * @{
 */

/**
 * RF center frequency, in hertz (Hz)
 *
 * @see Format macros for fprintf() and fscanf(): `BLADERF_PRIuFREQ`,
 * `BLADERF_PRIxFREQ`, `BLADERF_SCNuFREQ`, `BLADERF_SCNxFREQ`
 *
 * @remark Prior to libbladeRF 2.0.0, frequencies were specified as
 *         `unsigned int`.
 */
typedef uint64_t bladerf_frequency;

/** printf format for frequencies in unsigned decimal */
#define BLADERF_PRIuFREQ PRIu64
/** printf format for frequencies in hexadecimal */
#define BLADERF_PRIxFREQ PRIx64
/** scanf format for frequencies in unsigned decimal */
#define BLADERF_SCNuFREQ SCNu64
/** scanf format for frequencies in hexadecimal */
#define BLADERF_SCNxFREQ SCNx64

/**
 * Select the appropriate band path given a frequency in Hz.
 *
 * @note Most API users will not need to use this function, as
 *       bladerf_set_frequency() calls this internally after tuning the device.
 *
 * The high band is used for `frequency` above 1.5 GHz on bladeRF1 and above
 * 3.0 GHz on bladeRF2. Otherwise, the low band is used.
 *
 * @see bladerf_get_frequency_range() to determine the range of supported
 *      frequencies.
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[in]   frequency   Tuned frequency
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_select_band(struct bladerf *dev,
                                  bladerf_channel ch,
                                  bladerf_frequency frequency);

/**
 * Set channel's frequency in Hz.
 *
 * @note On the bladeRF1 platform, it is recommended to keep the RX and TX
 *       frequencies at least 1 MHz apart, and to digitally mix on the RX side
 *       if reception closer to the TX frequency is required.
 *
 * @note On the bladeRF2, there is one oscillator for all RX channels and one
 *       oscillator for all TX channels. Therefore, changing one channel will
 *       change the frequency of all channels in that direction.
 *
 * This function calls bladerf_select_band() internally, and performs all
 * other tasks required to prepare the channel for the given frequency.
 *
 * @see bladerf_get_frequency_range() to determine the range of supported
 *      frequencies.
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[in]   frequency   Desired frequency
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_frequency(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bladerf_frequency frequency);
/**
 * Get channel's current frequency in Hz
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[out]  frequency   Current frequency
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_frequency(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bladerf_frequency *frequency);

/**
 * Get the supported range of frequencies for a channel
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[out]  range       Frequency range
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_frequency_range(struct bladerf *dev,
                                          bladerf_channel ch,
                                          const struct bladerf_range **range);

/** @} (End of FN_TUNING) */

/**
 * @defgroup FN_LOOPBACK Internal loopback
 *
 * The bladeRF provides a variety of loopback modes to aid in development and
 * testing.
 *
 * In general, the digital or baseband loopback modes provide the most "ideal"
 * operating conditions, while the internal RF loopback modes introduce more of
 * the typical nonidealities of analog systems.
 *
 * These functions are thread-safe.
 *
 * @{
 */

/**
 * Loopback options
 */
typedef enum {
    /** Disables loopback and returns to normal operation. */
    BLADERF_LB_NONE = 0,

    /** Firmware loopback inside of the FX3 */
    BLADERF_LB_FIRMWARE,

    /** Baseband loopback. TXLPF output is connected to the RXVGA2 input. */
    BLADERF_LB_BB_TXLPF_RXVGA2,

    /** Baseband loopback. TXVGA1 output is connected to the RXVGA2 input. */
    BLADERF_LB_BB_TXVGA1_RXVGA2,

    /** Baseband loopback. TXLPF output is connected to the RXLPF input. */
    BLADERF_LB_BB_TXLPF_RXLPF,

    /** Baseband loopback. TXVGA1 output is connected to RXLPF input. */
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

    /** RFIC digital loopback (built-in self-test) */
    BLADERF_LB_RFIC_BIST,
} bladerf_loopback;

/**
 * Mapping of human-readable names to loopback modes
 */
struct bladerf_loopback_modes {
    const char *name;      /**< Name of loopback mode */
    bladerf_loopback mode; /**< Loopback mode enumeration */
};

/**
 * Get loopback modes
 *
 * Populates `modes` with a pointer to an array of structs containing the
 * supported loopback modes.
 *
 * This function may be called with `NULL` for `modes` to determine the number
 * of loopback modes supported.
 *
 * @param       dev         Device handle
 * @param[out]  modes       Supported loopback modes
 *
 * @return Number of loopback modes on success, value from \ref RETCODES list
 *         on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_loopback_modes(
    struct bladerf *dev, const struct bladerf_loopback_modes **modes);

/**
 * Test if a given loopback mode is supported on this device.
 *
 * @param       dev         Device handle
 * @param[in]   mode        bladerf_loopback enum to check
 *
 * @return true if supported, false if not (or on error)
 */
API_EXPORT bool CALL_CONV bladerf_is_loopback_mode_supported(
    struct bladerf *dev, bladerf_loopback mode);

/**
 * Apply specified loopback mode
 *
 * @note Loopback modes should only be enabled or disabled while the RX and TX
 *       channels are both disabled (and therefore, when no samples are being
 *       actively streamed). Otherwise, unexpected behavior may occur.
 *
 * @param       dev     Device handle
 * @param[in]   lb      Loopback mode. Note that BLADERF_LB_NONE disables the
 *                      use of loopback functionality.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_loopback(struct bladerf *dev, bladerf_loopback lb);

/**
 * Get current loopback mode
 *
 * @param       dev     Device handle
 * @param[out]  lb      Current loopback mode
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_loopback(struct bladerf *dev, bladerf_loopback *lb);

/** @} (End of FN_LOOPBACK) */

/**
 * @defgroup FN_TRIG   Triggers
 *
 * Trigger functionality introduced in bladeRF FPGA v0.6.0 allows TX and/or RX
 * samples to be gated via a trigger signal. This allows multiple devices to
 * synchronize their TX/RX operations upon the reception of a trigger event.
 *
 * The set of functions presented in this section of the API provides control
 * over this triggering functionality. It is intended that these functions be
 * used \b prior to starting sample streams. Attempting to use these functions
 * while streaming may yield undefined and undesirable behavior.
 *
 * These functions are thread-safe.
 *
 * For devices running at the same sample rate, the trigger event should
 * achieve synchronization within +/- 1 sample on each device in the chain.
 *
 * @note As of FPGA v0.6.0, `mini_exp[1]` has been allocated as the trigger
 *       signal. However, this API section is designed to allow future signals
 *       to be added, including users' software and hardware customizations.
 *
 * @note <b>Important</b>: Ensure that you disarm triggers <b>before</b>
 *       stopping sample streams (i.e., calling bladerf_enable_module() with
 *       `enable = false`).  Otherwise, the operation of shutting down streams
 *       will block for the entire duration of the stream timeout (or infinitely
 *       if the timeouts were set to 0).
 *
 * These functions are thread-safe.
 *
 * The standard usage of these functions is shown below. This example assumes:
 *
 *  - The two devices are connected such they share a common ground and their
 *      `mini_exp[1]` pins are connected. `mini_exp[1]` is J71-4 on bladeRF
 *      x40/x115, and J51-1 on bladeRF xA4/xA5/xA9.
 *
 *  - Both devices are already configured to utilize a common clock signal via
 *      the external SMB connection.  Generally, this will consist of one device
 *      to outputting its reference clock via the SMB clock port, and
 *      configuring the other device(s) to use the SMB clock port as a reference
 *      clock input. This may be achieved using the bladerf_set_smb_mode()
 *      function, found in the \ref FN_SMB_CLOCK section.
 *
 *
 * @code{.c}
 *
 * int status;
 * bladerf_channel channel = BLADERF_CHANNEL_RX(0);
 * bladerf_trigger_signal signal = BLADERF_TRIGGER_J71_4;
 *
 * // Allocate and initialize a bladerf_trigger structure for each
 * // trigger in the system.
 * struct bladerf_trigger trig_master, trig_slave;
 *
 * status = bladerf_trigger_init(dev_master, channel, signal, &trig_master);
 * if (status == 0) {
 *     trig_master.role = BLADERF_TRIGGER_ROLE_MASTER;
 * } else {
 *     goto handle_error;
 * }
 *
 * status = bladerf_trigger_init(dev_slave1, channel, signal, &trig_slave);
 * if (status == 0) {
 *     master_rx.role = BLADERF_TRIGGER_ROLE_SLAVE;
 * } else {
 *     goto handle_error;
 * }
 *
 * // Arm the triggering functionality on each device
 * status = bladerf_trigger_arm(dev_master, &trig_master, true, 0, 0);
 * if (status != 0) {
 *     goto handle_error;
 * }
 *
 * status = bladerf_trigger_arm(dev_slave, &trig_slave, true, 0, 0);
 * if (status != 0) {
 *     goto handle_error;
 * }
 *
 * // Call bladerf_sync_config() and bladerf_sync_rx() on each device.
 * // Ensure the timeout parameters used are long enough to accommodate
 * // the expected time until the trigger will be fired.
 * status = start_rx_streams(dev_master, dev_slave);
 * if (status != 0) {
 *     goto handle_error;
 * }
 *
 * // Fire the trigger signal
 * status = bladerf_trigger_fire(dev_master, &trig_master);
 * if (status != 0) {
 *     goto handle_error;
 * }
 *
 * // Handle RX signals and then shut down streams.
 * // Synchronized samples should now be reaching the host following the
 * // reception of the external trigger signal.
 * status = handle_rx_operations(dev_master, dev_slave);
 * if (status != 0) {
 *     goto handle_error;
 * }
 *
 * // Disable triggering on all devices to restore normal RX operation
 * trig_master.role = BLADERF_TRIGGER_ROLE_DISABLED;
 * status = bladerf_trigger_arm(dev_master, &trig_master, false, 0, 0);
 * if (status != 0) {
 *     goto handle_error;
 * }
 *
 * trig_slave.role  = BLADERF_TRIGGER_ROLE_DISABLED;
 * status = bladerf_trigger_arm(dev_master, &trig_slave, false, 0, 0);
 * if (status != 0) {
 *     goto handle_error;
 * }
 *
 * @endcode
 *
 * @{
 */

/**
 * This value denotes the role of a device in a trigger chain.
 */
typedef enum {
    /** Invalid role selection */
    BLADERF_TRIGGER_ROLE_INVALID = -1,

    /**
     * Triggering functionality is disabled on this device. Samples are not
     * gated and the trigger signal is an input.
     */
    BLADERF_TRIGGER_ROLE_DISABLED,

    /**
     * This device is the trigger master. Its trigger signal will be an output
     * and this device will determine when all devices shall trigger.
     */
    BLADERF_TRIGGER_ROLE_MASTER,

    /**
     * This device is the trigger slave. This device's trigger signal will be an
     * input and this devices will wait for the master's trigger signal
     * assertion.
     */
    BLADERF_TRIGGER_ROLE_SLAVE,
} bladerf_trigger_role;

/**
 * Trigger signal selection
 *
 * This selects pin or signal used for the trigger.
 *
 * @note ::BLADERF_TRIGGER_J71_4, ::BLADERF_TRIGGER_J51_1, and
 *       ::BLADERF_TRIGGER_MINI_EXP_1 are the only valid options as of FPGA
 *       v0.6.0. All three values have the same behavior and may be used
 *       interchangably.
 *
 * The `BLADERF_TRIGGER_USER_*` values have been added to allow users to modify
 * both hardware and software implementations to add custom triggers, while
 * maintaining libbladeRF API compatibility. Official bladeRF releases will
 * not utilize these user signal IDs.
 */
typedef enum {
    BLADERF_TRIGGER_INVALID = -1, /**< Invalid selection */
    BLADERF_TRIGGER_J71_4,        /**< J71 pin 4, mini_exp[1] on x40/x115 */
    BLADERF_TRIGGER_J51_1,        /**< J51 pin 1, mini_exp[1] on xA4/xA5/xA9 */
    BLADERF_TRIGGER_MINI_EXP_1,   /**< mini_exp[1], hardware-independent */

    BLADERF_TRIGGER_USER_0 = 128, /**< Reserved for user SW/HW customizations */
    BLADERF_TRIGGER_USER_1,       /**< Reserved for user SW/HW customizations */
    BLADERF_TRIGGER_USER_2,       /**< Reserved for user SW/HW customizations */
    BLADERF_TRIGGER_USER_3,       /**< Reserved for user SW/HW customizations */
    BLADERF_TRIGGER_USER_4,       /**< Reserved for user SW/HW customizations */
    BLADERF_TRIGGER_USER_5,       /**< Reserved for user SW/HW customizations */
    BLADERF_TRIGGER_USER_6,       /**< Reserved for user SW/HW customizations */
    BLADERF_TRIGGER_USER_7,       /**< Reserved for user SW/HW customizations */
} bladerf_trigger_signal;

/**
 * Trigger configuration
 *
 * It is <b>highly recommended</b> to keep a 1:1 relationship between triggers
 * in the physical setup and instances of this structure. (i.e., do not re-use
 * and change the same bladerf_trigger) for multiple triggers.)
 */
struct bladerf_trigger {
    bladerf_channel channel;       /**< RX/TX channel associated with trigger */
    bladerf_trigger_role role;     /**< Role of the device in a trigger chain */
    bladerf_trigger_signal signal; /**< Pin or signal being used */
    uint64_t options;              /**< Reserved field for future options. This
                                    *   is unused and should be set to 0. */
};

/**
 * Initialize a bladerf_trigger structure based upon the current configuration
 * of the specified trigger signal.
 *
 * While it is possible to simply declare and manually fill in a bladerf_trigger
 * structure, it is recommended to use this function to retrieve the current
 * `role` and `options` values.
 *
 * @param       dev         Device to query
 * @param[in]   ch          Channel
 * @param[in]   signal      Trigger signal to query
 * @param[out]  trigger     Updated to describe trigger
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_trigger_init(struct bladerf *dev,
                                   bladerf_channel ch,
                                   bladerf_trigger_signal signal,
                                   struct bladerf_trigger *trigger);

/**
 * Configure and (dis)arm a trigger on the specified device.
 *
 * @note If trigger->role is set to ::BLADERF_TRIGGER_ROLE_DISABLED, this will
 *       inherently disarm an armed trigger and clear any fire requests,
 *       regardless of the value of `arm`.
 *
 * @param       dev     Device to configure
 * @param[in]   trigger Trigger configure
 * @param[in]   arm     (Re)Arm trigger if true, disarm if false
 * @param[in]   resv1   Reserved for future use. Set to 0.
 * @param[in]   resv2   Reserved for future use. Set to 0.
 *
 * @warning Configuring two devices in the trigger chain (or both RX and TX on a
 *          single device) as masters can damage the associated FPGA pins, as
 *          this would cause contention over the trigger signal. <b>Ensure only
 *          one device in the chain is configured as the master!</b>
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_trigger_arm(struct bladerf *dev,
                                  const struct bladerf_trigger *trigger,
                                  bool arm,
                                  uint64_t resv1,
                                  uint64_t resv2);

/**
 * Fire a trigger event.
 *
 * Calling this functiona with a trigger whose role is anything other than
 * ::BLADERF_TRIGGER_REG_MASTER will yield a BLADERF_ERR_INVAL return value.
 *
 * @param       dev         Device handle
 * @param[in]   trigger     Trigger to assert
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_trigger_fire(struct bladerf *dev,
                                   const struct bladerf_trigger *trigger);

/**
 * Query the fire request status of a master trigger
 *
 * @param       dev             Device handle
 * @param[in]   trigger         Trigger to query
 * @param[out]  is_armed        Set to true if the trigger is armed, and false
 *                              otherwise. May be NULL.
 * @param[out]  has_fired       Set to true if the trigger has fired, and false
 *                              otherwise. May be NULL.
 * @param[out]  fire_requested  Only applicable to a trigger master.
 *                              Set to true if a fire request has been
 *                              previously submitted. May be NULL.
 * @param[out]  resv1           Reserved for future use.
 *                              This field is written as 0 if not set to NULL.
 * @param[out]  resv2           Reserved for future use.
 *                              This field is written as 0 if not set to NULL.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_trigger_state(struct bladerf *dev,
                                    const struct bladerf_trigger *trigger,
                                    bool *is_armed,
                                    bool *has_fired,
                                    bool *fire_requested,
                                    uint64_t *resv1,
                                    uint64_t *resv2);

/** @} (End of FN_TRIG) */

/**
 * @defgroup FN_RECEIVE_MUX Receive Mux
 *
 * These functions are thread-safe.
 *
 * @{
 */

/**
 * RX Mux modes
 *
 * These values describe the source of samples to the RX FIFOs in the FPGA.
 * They map directly to rx_mux_mode_t inside the FPGA's source code.
 */
typedef enum {
    /** Invalid RX Mux mode selection */
    BLADERF_RX_MUX_INVALID = -1,

    /** Read baseband samples. This is the default mode of operation. */
    BLADERF_RX_MUX_BASEBAND = 0x0,

    /**
     * Read samples from 12 bit counters.
     *
     * The I channel counts up while the Q channel counts down.
     */
    BLADERF_RX_MUX_12BIT_COUNTER = 0x1,

    /**
     * Read samples from a 32 bit up-counter.
     *
     * I and Q form a little-endian value.
     */
    BLADERF_RX_MUX_32BIT_COUNTER = 0x2,

    /* RX_MUX setting 0x3 is reserved for future use */

    /** Read samples from the baseband TX input to the FPGA (from the host) */
    BLADERF_RX_MUX_DIGITAL_LOOPBACK = 0x4,
} bladerf_rx_mux;

/** @cond IGNORE */
/* Backwards compatible mapping for `bladerf_rx_mux`. */
#define BLADERF_RX_MUX_BASEBAND_LMS BLADERF_RX_MUX_BASEBAND
/** @endcond */

/**
 * Set the current RX Mux mode
 *
 * @param       dev     Device handle
 * @param[in]   mux     Mux mode.
 *
 * @returns 0 on success, value from \ref RETCODES list on failure.
 */
API_EXPORT
int CALL_CONV bladerf_set_rx_mux(struct bladerf *dev, bladerf_rx_mux mux);

/**
 * Gets the current RX Mux mode
 *
 * @param       dev     Device handle
 * @param[out]  mode    Current RX Mux mode
 *
 * @returns 0 on success, value from \ref RETCODES list on failure.
 */
API_EXPORT
int CALL_CONV bladerf_get_rx_mux(struct bladerf *dev, bladerf_rx_mux *mode);

/** @} (End of FN_RECEIVE_MUX) */

/**
 * @defgroup FN_SCHEDULED_TUNING Scheduled Tuning
 *
 * These functions are thread-safe.
 *
 * @{
 */

/**
 * @ingroup STREAMING
 *
 * Timestamp, in ticks
 *
 * A channel's timestamp typically increments at the sample rate.
 *
 * @see Format macros for fprintf() and fscanf(): `BLADERF_PRIuTS`,
 * `BLADERF_PRIxTS`, `BLADERF_SCNuTS`, `BLADERF_SCNxTS`
 */
typedef uint64_t bladerf_timestamp;

/**
 * Specifies that scheduled retune should occur immediately when using
 * bladerf_schedule_retune().
 */
#define BLADERF_RETUNE_NOW (bladerf_timestamp)0

/**
 * Quick Re-tune parameters.
 *
 * @note These parameters, which are associated with the RFIC's register values,
 *       are sensitive to changes in the operating environment (e.g.,
 *       temperature).
 *
 * This structure should be filled in via bladerf_get_quick_tune().
 */
struct bladerf_quick_tune {
    union {
        /* bladeRF1 quick tune parameters */
        struct {
            uint8_t freqsel; /**< Choice of VCO and VCO division factor */
            uint8_t vcocap;  /**< VCOCAP value */
            uint16_t nint;   /**< Integer portion of LO frequency value */
            uint32_t nfrac;  /**< Fractional portion of LO frequency value */
            uint8_t flags;   /**< Flag bits used internally by libbladeRF */
            uint8_t xb_gpio;   /**< Flag bits used to configure XB */
        };
        /* bladeRF2 quick tune parameters */
        struct {
            uint16_t nios_profile; /**< Profile number in Nios */
            uint8_t rffe_profile;  /**< Profile number in RFFE */
            uint8_t port;          /**< RFFE port settings */
            uint8_t spdt;          /**< External SPDT settings */
        };
    };
};

/**
 * Schedule a frequency retune to occur at specified sample timestamp value.
 *
 * @pre bladerf_sync_config() must have been called with the
 *      \ref BLADERF_FORMAT_SC16_Q11_META format for the associated channel in
 *      order to enable timestamps. (The timestamped metadata format must be
 *      enabled in order to use this function.)
 *
 * @param       dev             Device handle
 * @param[in]   ch              Channel
 * @param[in]   timestamp       Channel's sample timestamp to perform the
 *                              retune operation. If this value is in the past,
 *                              the retune will occur immediately. To perform
 *                              the retune immediately, specify
 *                              ::BLADERF_RETUNE_NOW.
 * @param[in]   frequency       Desired frequency, in Hz.
 * @param[in]   quick_tune      If non-NULL, the provided "quick retune" values
 *                              will be applied to the transceiver to tune it
 *                              according to a previous state retrieved via
 *                              bladerf_get_quick_tune().
 *
 * @return 0 on success, value from \ref RETCODES list on failure.
 *
 * @note If the underlying queue of scheduled retune requests becomes full, \ref
 *       BLADERF_ERR_QUEUE_FULL will be returned. In this case, it should be
 *       possible to schedule a retune after the timestamp of one of the earlier
 *       requests occurs.
 */
API_EXPORT
int CALL_CONV bladerf_schedule_retune(struct bladerf *dev,
                                      bladerf_channel ch,
                                      bladerf_timestamp timestamp,
                                      bladerf_frequency frequency,
                                      struct bladerf_quick_tune *quick_tune);

/**
 * Cancel all pending scheduled retune operations for the specified channel.
 *
 * This will be done automatically during bladerf_close() to ensure that
 * previously queued retunes do not continue to occur after closing and then
 * later re-opening a device.
 *
 * @param       dev     Device handle
 * @param[in]   ch      Channel
 *
 * @return 0 on success, value from \ref RETCODES list on failure.
 */
API_EXPORT
int CALL_CONV bladerf_cancel_scheduled_retunes(struct bladerf *dev,
                                               bladerf_channel ch);

/**
 * Fetch parameters used to tune the transceiver to the current frequency for
 * use with bladerf_schedule_retune() to perform a "quick retune."
 *
 * This allows for a faster retune, with a potential trade off of increased
 * phase noise.
 *
 * @note These parameters are sensitive to changes in the operating environment,
 *       and should be "refreshed" if planning to use the "quick retune"
 *       functionality over a long period of time.
 *
 * @pre bladerf_set_frequency() or bladerf_schedule_retune() have previously
 *      been used to retune to the desired frequency.
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[out]  quick_tune  Quick retune parameters
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_quick_tune(struct bladerf *dev,
                                     bladerf_channel ch,
                                     struct bladerf_quick_tune *quick_tune);

/** @} (End of FN_SCHEDULED_TUNING) */

/**
 * @defgroup FN_CORR    Correction
 *
 * This group provides routines for applying manual offset, gain, and phase
 * corrections.
 *
 * These functions are thread-safe.
 *
 * @{
 */

/**
 * Correction value, in arbitrary units
 *
 * @see ::bladerf_correction
 * @see bladerf_get_correction()
 * @see bladerf_set_correction()
 */
typedef int16_t bladerf_correction_value;

/**
 * Correction parameter selection
 *
 * These values specify the correction parameter to modify or query when calling
 * bladerf_set_correction() or bladerf_get_correction(). Note that the meaning
 * of the `value` parameter to these functions depends upon the correction
 * parameter.
 *
 */
typedef enum {
    /**
     * Adjusts the in-phase DC offset. Valid values are [-2048, 2048], which are
     * scaled to the available control bits.
     */
    BLADERF_CORR_DCOFF_I,

    /**
     * Adjusts the quadrature DC offset. Valid values are [-2048, 2048], which
     * are scaled to the available control bits.
     */
    BLADERF_CORR_DCOFF_Q,

    /**
     * Adjusts phase correction of [-10, 10] degrees, via a provided count value
     * of [-4096, 4096].
     */
    BLADERF_CORR_PHASE,

    /**
     * Adjusts gain correction value in [-1.0, 1.0], via provided values in the
     * range of [-4096, 4096].
     */
    BLADERF_CORR_GAIN
} bladerf_correction;

/** @cond IGNORE */
/* Backwards compatible mapping to `bladerf_correction`. */
#define BLADERF_CORR_LMS_DCOFF_I BLADERF_CORR_DCOFF_I
#define BLADERF_CORR_LMS_DCOFF_Q BLADERF_CORR_DCOFF_Q
#define BLADERF_CORR_FPGA_PHASE BLADERF_CORR_PHASE
#define BLADERF_CORR_FPGA_GAIN BLADERF_CORR_GAIN
/** @endcond */

/**
 * Set the value of the specified configuration parameter
 *
 * @see The ::bladerf_correction description for the valid ranges of the `value`
 *      parameter.
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[in]   corr        Correction type
 * @param[in]   value       Value to apply
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_correction(struct bladerf *dev,
                                     bladerf_channel ch,
                                     bladerf_correction corr,
                                     bladerf_correction_value value);

/**
 * Obtain the current value of the specified configuration parameter
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[in]   corr        Correction type
 * @param[out]  value       Current value
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_correction(struct bladerf *dev,
                                     bladerf_channel ch,
                                     bladerf_correction corr,
                                     bladerf_correction_value *value);

/** @} (End of FN_CORR) */

/** @} (End of FN_CHANNEL) */

/**
 * @defgroup STREAMING Streaming
 *
 * This section defines the streaming APIs.
 *
 * @{
 */

/** printf format for timestamps in unsigned decimal */
#define BLADERF_PRIuTS PRIu64
/** printf format for timestamps in hexadecimal */
#define BLADERF_PRIxTS PRIx64
/** scanf format for timestamps in unsigned decimal */
#define BLADERF_SCNuTS SCNu64
/** scanf format for timestamps in hexadecimal */
#define BLADERF_SCNxTS SCNx64

/**
 * @defgroup STREAMING_FORMAT Formats
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
     * <pre>
     *  .--------------.--------------.
     *  | Bits 31...16 | Bits 15...0  |
     *  +--------------+--------------+
     *  |   Q[15..0]   |   I[15..0]   |
     *  `--------------`--------------`
     * </pre>
     *
     * When using this format the minimum required buffer size, in bytes, is:
     *
     * \f$
     *  buffer\_size\_min = (2 \times num\_samples \times num\_channels \times
     *                      sizeof(int16\_t))
     * \f$
     *
     * For example, to hold 2048 samples for one channel, a buffer must be at
     * least 8192 bytes large.
     *
     * When a multi-channel ::bladerf_channel_layout is selected, samples
     * will be interleaved per channel. For example, with ::BLADERF_RX_X2
     * or ::BLADERF_TX_X2 (x2 MIMO), the buffer is structured like:
     *
     * <pre>
     *  .-------------.--------------.--------------.------------------.
     *  | Byte offset | Bits 31...16 | Bits 15...0  |    Description   |
     *  +-------------+--------------+--------------+------------------+
     *  |    0x00     |     Q0[0]    |     I0[0]    |  Ch 0, sample 0  |
     *  |    0x04     |     Q1[0]    |     I1[0]    |  Ch 1, sample 0  |
     *  |    0x08     |     Q0[1]    |     I0[1]    |  Ch 0, sample 1  |
     *  |    0x0c     |     Q1[1]    |     I1[1]    |  Ch 1, sample 1  |
     *  |    ...      |      ...     |      ...     |        ...       |
     *  |    0xxx     |     Q0[n]    |     I0[n]    |  Ch 0, sample n  |
     *  |    0xxx     |     Q1[n]    |     I1[n]    |  Ch 1, sample n  |
     *  `-------------`--------------`--------------`------------------`
     * </pre>
     *
     * Per the `buffer_size_min` formula above, 2048 samples for two channels
     * will generate 4096 total samples, and require at least 16384 bytes.
     *
     * Implementors may use the interleaved buffers directly, or may use
     * bladerf_deinterleave_stream_buffer() / bladerf_interleave_stream_buffer()
     * if contiguous blocks of samples are desired.
     */
    BLADERF_FORMAT_SC16_Q11,

    /**
     * This format is the same as the ::BLADERF_FORMAT_SC16_Q11 format, except
     * the first 4 samples in every <i>block*</i> of samples are replaced with
     * metadata organized as follows. All fields are little-endian byte order.
     *
     * <pre>
     *  .-------------.------------.----------------------------------.
     *  | Byte offset |   Type     | Description                      |
     *  +-------------+------------+----------------------------------+
     *  |    0x00     | uint16_t   | Reserved                         |
     *  |    0x02     |  uint8_t   | Stream flags                     |
     *  |    0x03     |  uint8_t   | Meta version ID                  |
     *  |    0x04     | uint64_t   | 64-bit Timestamp                 |
     *  |    0x0c     | uint32_t   | BLADERF_META_FLAG_* flags        |
     *  |  0x10..end  |            | Payload                          |
     *  `-------------`------------`----------------------------------`
     * </pre>
     *
     * For IQ sample meta mode, the Meta version ID and Stream flags should
     * currently be set to values 0x00 and 0x00, respectively.
     *
     * <i>*</i>The number of samples in a <i>block</i> is dependent upon
     * the USB speed being used:
     *  - USB 2.0 Hi-Speed: 256 samples
     *  - USB 3.0 SuperSpeed: 512 samples
     *
     * When using the bladerf_sync_rx() and bladerf_sync_tx() functions, the
     * above details are entirely transparent; the caller need not be concerned
     * with these details. These functions take care of packing/unpacking the
     * metadata into/from the underlying stream and convey this information
     * through the ::bladerf_metadata structure.
     *
     * However, when using the \ref FN_STREAMING_ASYNC interface, the user is
     * responsible for manually packing/unpacking the above metadata into/from
     * their samples.
     *
     * @see STREAMING_FORMAT_METADATA
     * @see The `src/streaming/metadata.h` header in the libbladeRF codebase.
     */
    BLADERF_FORMAT_SC16_Q11_META,

    /**
     * This format is for exchanging packets containing digital payloads with
     * the FPGA. A packet is generall a digital payload, that the FPGA then
     * processes to either modulate, demodulate, filter, etc.
     *
     * All fields are little-endian byte order.
     *
     * <pre>
     *  .-------------.------------.----------------------------------.
     *  | Byte offset |   Type     | Description                      |
     *  +-------------+------------+----------------------------------+
     *  |    0x00     | uint16_t   | Packet length (in 32bit DWORDs)  |
     *  |    0x02     |  uint8_t   | Packet flags                     |
     *  |    0x03     |  uint8_t   | Packet core ID                   |
     *  |    0x04     | uint64_t   | 64-bit Timestamp                 |
     *  |    0x0c     | uint32_t   | BLADERF_META_FLAG_* flags        |
     *  |  0x10..end  |            | Payload                          |
     *  `-------------`------------`----------------------------------`
     * </pre>
     *
     * A target core (for example a modem) must be specified when calling the
     * bladerf_sync_rx() and bladerf_sync_tx() functions.
     *
     * When in packet mode, lengths for all functions and data formats are
     * expressed in number of 32-bit DWORDs. As an example, a 12 byte packet
     * is considered to be 3 32-bit DWORDs long.
     *
     * This packet format does not send or receive raw IQ samples. The digital
     * payloads contain configurations, and digital payloads that are specific
     * to the digital core to which they are addressed. It is the FPGA core
     * that should generate, interpret, and process the digital payloads.
     *
     * With the exception of packet lenghts, no difference should exist between
     * USB 2.0 Hi-Speed or USB 3.0 SuperSpeed for packets for this streaming
     * format.
     *
     * @see STREAMING_FORMAT_METADATA
     * @see The `src/streaming/metadata.h` header in the libbladeRF codebase.
     */
    BLADERF_FORMAT_PACKET_META,

    /**
     * Signed, Complex 8-bit Q8. This is the native format of the DAC data.
     *
     * Values in the range [-128, 128) are used to represent [-1.0, 1.0).
     * Note that the lower bound here is inclusive, and the upper bound is
     * exclusive. Ensure that provided samples stay within [-128, 127].
     *
     * Samples consist of interleaved IQ value pairs, with I being the first
     * value in the pair. Each value in the pair is a right-aligned,
     * little-endian int16_t. The FPGA ensures that these values are
     * sign-extended.
     *
     * <pre>
     *  .--------------.--------------.
     *  | Bits 15...8  | Bits  7...0  |
     *  +--------------+--------------+
     *  |    Q[7..0]   |    I[7..0]   |
     *  `--------------`--------------`
     * </pre>
     *
     * When using this format the minimum required buffer size, in bytes, is:
     *
     * \f$
     *  buffer\_size\_min = (2 \times num\_samples \times num\_channels \times
     *                      sizeof(int8\_t))
     * \f$
     *
     * For example, to hold 2048 samples for one channel, a buffer must be at
     * least 4096 bytes large.
     *
     * When a multi-channel ::bladerf_channel_layout is selected, samples
     * will be interleaved per channel. For example, with ::BLADERF_RX_X2
     * or ::BLADERF_TX_X2 (x2 MIMO), the buffer is structured like:
     *
     * <pre>
     *  .-------------.--------------.--------------.------------------.
     *  | Byte offset | Bits 15...8  | Bits  7...0  |    Description   |
     *  +-------------+--------------+--------------+------------------+
     *  |    0x00     |     Q0[0]    |     I0[0]    |  Ch 0, sample 0  |
     *  |    0x02     |     Q1[0]    |     I1[0]    |  Ch 1, sample 0  |
     *  |    0x04     |     Q0[1]    |     I0[1]    |  Ch 0, sample 1  |
     *  |    0x06     |     Q1[1]    |     I1[1]    |  Ch 1, sample 1  |
     *  |    ...      |      ...     |      ...     |        ...       |
     *  |    0xxx     |     Q0[n]    |     I0[n]    |  Ch 0, sample n  |
     *  |    0xxx     |     Q1[n]    |     I1[n]    |  Ch 1, sample n  |
     *  `-------------`--------------`--------------`------------------`
     * </pre>
     *
     * Per the `buffer_size_min` formula above, 2048 samples for two channels
     * will generate 4096 total samples, and require at least 8192 bytes.
     *
     * Implementors may use the interleaved buffers directly, or may use
     * bladerf_deinterleave_stream_buffer() / bladerf_interleave_stream_buffer()
     * if contiguous blocks of samples are desired.
     */
    BLADERF_FORMAT_SC8_Q7,

    /**
     * This format is the same as the ::BLADERF_FORMAT_SC8_Q7 format, except
     * the first 4 samples in every <i>block*</i> of samples are replaced with
     * metadata organized as follows. All fields are little-endian byte order.
     *
     * <pre>
     *  .-------------.------------.----------------------------------.
     *  | Byte offset |   Type     | Description                      |
     *  +-------------+------------+----------------------------------+
     *  |    0x00     | uint16_t   | Reserved                         |
     *  |    0x02     |  uint8_t   | Stream flags                     |
     *  |    0x03     |  uint8_t   | Meta version ID                  |
     *  |    0x04     | uint64_t   | 64-bit Timestamp                 |
     *  |    0x0c     | uint32_t   | BLADERF_META_FLAG_* flags        |
     *  |  0x10..end  |            | Payload                          |
     *  `-------------`------------`----------------------------------`
     * </pre>
     *
     * For IQ sample meta mode, the Meta version ID and Stream flags should
     * currently be set to values 0x00 and 0x00, respectively.
     *
     * <i>*</i>The number of samples in a <i>block</i> is dependent upon
     * the USB speed being used:
     *  - USB 2.0 Hi-Speed: 256 samples
     *  - USB 3.0 SuperSpeed: 512 samples
     *
     * When using the bladerf_sync_rx() and bladerf_sync_tx() functions, the
     * above details are entirely transparent; the caller need not be concerned
     * with these details. These functions take care of packing/unpacking the
     * metadata into/from the underlying stream and convey this information
     * through the ::bladerf_metadata structure.
     *
     * However, when using the \ref FN_STREAMING_ASYNC interface, the user is
     * responsible for manually packing/unpacking the above metadata into/from
     * their samples.
     *
     * @see STREAMING_FORMAT_METADATA
     * @see The `src/streaming/metadata.h` header in the libbladeRF codebase.
     */
    BLADERF_FORMAT_SC8_Q7_META,
} bladerf_format;

/**
 * @defgroup STREAMING_FORMAT_METADATA Metadata structure and flags
 *
 * @{
 */

/*
 * Metadata status bits
 *
 * These are used in conjunction with the bladerf_metadata structure's `status`
 * field.
 */

/**
 * A sample overrun has occurred.
 *
 * This indicates that either the host (more likely) or the FPGA is not keeping
 * up with the incoming samples.
 */
#define BLADERF_META_STATUS_OVERRUN (1 << 0)

/**
 * A sample underrun has occurred.
 *
 * This generally only occurs on the TX channel when the FPGA is starved of
 * samples.
 *
 * @note libbladeRF does not report this status. It is here for future use.
 */
#define BLADERF_META_STATUS_UNDERRUN (1 << 1)

/*
 * Metadata flags
 *
 * These are used in conjunction with the bladerf_metadata structure's `flags`
 * field.
 */

/**
 * Mark the associated buffer as the start of a burst transmission.
 *
 * @note This is only used for the bladerf_sync_tx() call.
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
#define BLADERF_META_FLAG_TX_BURST_START (1 << 0)

/**
 * Mark the associated buffer as the end of a burst transmission. This will
 * flush the remainder of the sync interface's current working buffer and
 * enqueue samples into the hardware's transmit FIFO.
 *
 * As of libbladeRF v1.3.0, it is no longer necessary for the API user to ensure
 * that the final 3 samples of a burst are \f$0 + 0 j\f$. libbladeRF now ensures
 * this hardware requirement is upheld.
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
 * @note This is only used for the bladerf_sync_tx() call. It is ignored by the
 *       bladerf_sync_rx() call.
 */
#define BLADERF_META_FLAG_TX_BURST_END (1 << 1)

/**
 * Use this flag in conjunction with ::BLADERF_META_FLAG_TX_BURST_START to
 * indicate that the burst should be transmitted as soon as possible, as opposed
 * to waiting for a specific timestamp.
 *
 * When this flag is used, there is no need to set the
 * bladerf_metadata::timestamp field.
 */
#define BLADERF_META_FLAG_TX_NOW (1 << 2)

/**
 * Use this flag within a burst (i.e., between the use of
 * ::BLADERF_META_FLAG_TX_BURST_START and ::BLADERF_META_FLAG_TX_BURST_END) to
 * specify that bladerf_sync_tx() should read the bladerf_metadata::timestamp
 * field and zero-pad samples up to the specified timestamp. The provided
 * samples will then be transmitted at that timestamp.
 *
 * Use this flag when potentially flushing an entire buffer via the
 * ::BLADERF_META_FLAG_TX_BURST_END would yield an unacceptably large gap in the
 * transmitted samples.
 *
 * In some applications where a transmitter is constantly transmitting with
 * extremely small gaps (less than a buffer), users may end up using a single
 * ::BLADERF_META_FLAG_TX_BURST_START, and then numerous calls to
 * bladerf_sync_tx() with the ::BLADERF_META_FLAG_TX_UPDATE_TIMESTAMP flag set.
 * The ::BLADERF_META_FLAG_TX_BURST_END would only be used to end the stream
 * when shutting down.
 */
#define BLADERF_META_FLAG_TX_UPDATE_TIMESTAMP (1 << 3)

/**
 * This flag indicates that calls to bladerf_sync_rx should return any available
 * samples, rather than wait until the timestamp indicated in the
 * bladerf_metadata timestamp field.
 */
#define BLADERF_META_FLAG_RX_NOW (1 << 31)

/**
 * This flag is asserted in bladerf_metadata.status by the hardware when an
 * underflow is detected in the sample buffering system on the device.
 */
#define BLADERF_META_FLAG_RX_HW_UNDERFLOW (1 << 0)

/**
 * This flag is asserted in bladerf_metadata.status by the hardware if mini
 * expansion IO pin 1 is asserted.
 */
#define BLADERF_META_FLAG_RX_HW_MINIEXP1 (1 << 16)

/**
 * This flag is asserted in bladerf_metadata.status by the hardware if mini
 * expansion IO pin 2 is asserted.
 */
#define BLADERF_META_FLAG_RX_HW_MINIEXP2 (1 << 17)

/**
 * Sample metadata
 *
 * This structure is used in conjunction with the ::BLADERF_FORMAT_SC16_Q11_META
 * format to TX scheduled bursts or retrieve timestamp information about
 * received samples.
 */
struct bladerf_metadata {
    /**
     * Free-running FPGA counter that monotonically increases at the sample rate
     * of the associated channel.
     */
    bladerf_timestamp timestamp;

    /**
     * Input bit field to control the behavior of the call that the metadata
     * structure is passed to. API calls read this field from the provided data
     * structure, and do not modify it.
     *
     * Valid flags include
     *  ::BLADERF_META_FLAG_TX_BURST_START,
     *  ::BLADERF_META_FLAG_TX_BURST_END,
     *  ::BLADERF_META_FLAG_TX_NOW,
     *  ::BLADERF_META_FLAG_TX_UPDATE_TIMESTAMP, and
     *  ::BLADERF_META_FLAG_RX_NOW
     */
    uint32_t flags;

    /**
     * Output bit field to denoting the status of transmissions/receptions. API
     * calls will write this field.
     *
     * Possible status flags include ::BLADERF_META_STATUS_OVERRUN and
     * ::BLADERF_META_STATUS_UNDERRUN.
     */
    uint32_t status;

    /**
     * This output parameter is updated to reflect the actual number of
     * contiguous samples that have been populated in an RX buffer during a
     * bladerf_sync_rx() call.
     *
     * This will not be equal to the requested count in the event of a
     * discontinuity (i.e., when the status field has the
     * ::BLADERF_META_STATUS_OVERRUN flag set). When an overrun occurs, it is
     * important not to read past the number of samples specified by this value,
     * as the remaining contents of the buffer are undefined.
     *
     * @note This parameter is not currently used by bladerf_sync_tx().
     */
    unsigned int actual_count;

    /**
     * Reserved for future use. This is not used by any functions. It is
     * recommended that users zero out this field.
     */
    uint8_t reserved[32];
};

/** @} (End of STREAMING_FORMAT_METADATA) */

/**
 * Interleaves contiguous blocks of samples in preparation for MIMO TX.
 *
 * Given a `buffer` loaded with data as such:
 *
 * <pre>
 *  .-------------------.--------------.--------------.------------------.
 *  |    Byte offset    | Bits 31...16 | Bits 15...0  |    Description   |
 *  +-------------------+--------------+--------------+------------------+
 *  |  0x00 + 0*chsize  |     Q0[0]    |     I0[0]    |  Ch 0, sample 0  |
 *  |  0x04 + 0*chsize  |     Q0[1]    |     I0[1]    |  Ch 0, sample 1  |
 *  |  0x08 + 0*chsize  |     Q0[2]    |     I0[2]    |  Ch 0, sample 2  |
 *  |  0x0c + 0*chsize  |     Q0[3]    |     I0[3]    |  Ch 0, sample 3  |
 *  |        ...        |      ...     |      ...     |        ...       |
 *  |  0x00 + 1*chsize  |     Q1[0]    |     I1[0]    |  Ch 1, sample 0  |
 *  |  0x04 + 1*chsize  |     Q1[1]    |     I1[1]    |  Ch 1, sample 1  |
 *  |  0x08 + 1*chsize  |     Q1[2]    |     I1[2]    |  Ch 1, sample 2  |
 *  |  0x0c + 1*chsize  |     Q1[3]    |     I1[3]    |  Ch 1, sample 3  |
 *  |        ...        |      ...     |      ...     |        ...       |
 *  `-------------------`--------------`--------------`------------------`
 * </pre>
 *
 * where \f$chsize = \frac{sizeof(buffer)}{num\_channels}\f$.
 *
 * This function interleaves the samples in the manner described by the
 * ::BLADERF_FORMAT_SC16_Q11 format, in place. Each channel must have
 * \f$buffer\_size / num\_channels\f$ samples, and they must be concatenated in
 * order.
 *
 * If the ::BLADERF_FORMAT_SC16_Q11_META format is specified, the first 16 bytes
 * will skipped.
 *
 * This function's inverse is bladerf_deinterleave_stream_buffer().
 *
 * @param[in]   layout        Stream direction and layout
 * @param[in]   format        Data format to use
 * @param[in]   buffer_size   The size of the buffer, in samples. Note that this
 *                            is the entire buffer, not just a single channel.
 * @param       samples       Buffer to process. The user is responsible for
 *                            ensuring this buffer contains exactly
 *                            `buffer_size` samples.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_interleave_stream_buffer(bladerf_channel_layout layout,
                                               bladerf_format format,
                                               unsigned int buffer_size,
                                               void *samples);

/**
 * Deinterleaves samples into contiguous blocks after MIMO RX.
 *
 * This function deinterleaves a multi-channel interleaved buffer, as described
 * by the ::BLADERF_FORMAT_SC16_Q11 format. The output is in the format
 * described as the input to this function's inverse,
 * bladerf_interleave_stream_buffer().
 *
 * If the ::BLADERF_FORMAT_SC16_Q11_META format is specified, the first 16 bytes
 * will skipped.
 *
 * @param[in]   layout          Stream direction and layout
 * @param[in]   format          Data format to use
 * @param[in]   buffer_size     The size of the buffer, in samples. Note that
 *                              this is the entire buffer, not just a single
 *                              channel.
 * @param       samples         Buffer to process. The user is responsible for
 *                              ensuring this buffer contains exactly
 *                              `buffer_size` samples.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_deinterleave_stream_buffer(bladerf_channel_layout layout,
                                                 bladerf_format format,
                                                 unsigned int buffer_size,
                                                 void *samples);

/** @} (End of STREAMING_FORMAT) */

/**
 * Enable or disable the RF front end of the specified direction.
 *
 * RF front ends must always be enabled prior to streaming samples on the
 * associated interface.
 *
 * When a synchronous stream is associated with the specified channel, this will
 * shut down the underlying asynchronous stream when `enable` = false.
 *
 * When transmitting samples, be sure to provide ample time for TX samples reach
 * the RF front-end before calling this function with `enable` = false. (This
 * can be achieved easily when using metadata, as shown on
 * \link sync_tx_meta_bursts.html this page\endlink.)
 *
 * @param       dev     Device handle
 * @param[in]   ch      Channel
 * @param[in]   enable  true to enable, false to disable
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_enable_module(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bool enable);

/**
 * Retrieve the specified stream's current timestamp counter value from the
 * FPGA.
 *
 * This function is only intended to be used to retrieve a coarse estimate of
 * the current timestamp when starting up a stream. It <b>should not</b> be used
 * as a means to accurately retrieve the current timestamp of individual samples
 * within a running stream. The reasons for this are:
 *  - The timestamp counter will have advanced during the time that the captured
 *      value is propagated back from the FPGA to the host
 *  - The value retrieved in this manner is not tightly-coupled with
 *      specific sample positions in the stream.
 *
 * When actively receiving a sample stream, instead use the
 * ::bladerf_metadata::timestamp field (provided when using the
 * ::BLADERF_FORMAT_SC16_Q11_META format) to retrieve the timestamp value
 * associated with a block of samples. See the \link sync_rx_meta.html RX with
 * metadata\endlink page for examples of this.
 *
 * An example use-case of this function is to schedule an initial TX burst in a
 * set of bursts:
 *
 *  - Configure and start a TX stream using the ::BLADERF_FORMAT_SC16_Q11_META
 *      format.
 *  - Retrieve timestamp \f$T\f$, a coarse estimate the TX's current timestamp
 *      via this function.
 *  - Schedule the first burst, \f$F\f$ to occur in the future: \f$F = T + N\f$.
 *      Generally, adding \f$N\f$ in tens to low hundreds of milliseconds is
 *      sufficient to account for timestamp retrieval overhead and stream
 *      startup.
 *  - Schedule additional bursts relative to the first burst \f$F\f$.
 *
 * Examples of the above are shown on the \link sync_tx_meta_bursts.html TX
 * with metadata\endlink page.
 *
 * @param       dev         Device handle
 * @param[in]   dir         Stream direction
 * @param[out]  timestamp   Coarse timestamp value
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_timestamp(struct bladerf *dev,
                                    bladerf_direction dir,
                                    bladerf_timestamp *timestamp);

/**
 * @defgroup FN_STREAMING_SYNC  Synchronous API
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
 *
 *  - \link sync_no_meta.html Basic usage without metadata\endlink
 *  - \link sync_rx_meta.html Synchronous RX with metadata\endlink
 *  - \link sync_tx_meta_bursts.html Synchronous TX with metadata\endlink
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
 * responsible for enabling/disable streams when desired.
 *
 * Note that (re)configuring the TX direction does not affect the RX direction,
 * and vice versa. This call configures each direction independently.
 *
 * Memory allocated by this function will be deallocated when bladerf_close()
 * is called.
 *
 * @see The bladerf_init_stream() documentation for information on determining
 *      appropriate values for `buffers_size`, `num_transfers`, and
 *      `stream_timeout`.
 *
 * @note The `num_buffers` parameter should generally be increased as the amount
 *       of work done between bladerf_sync_rx() or bladerf_sync_tx() calls
 *       increases.
 *
 * @param       dev             Device to configure
 * @param[in]   layout          Stream direction and layout
 * @param[in]   format          Format to use in synchronous data transfers
 * @param[in]   num_buffers     The number of buffers to use in the underlying
 *                              data stream. This must be greater than the
 *                              `num_xfers` parameter.
 * @param[in]   buffer_size     The size of the underlying stream buffers, in
 *                              samples. This value must be a multiple of 1024.
 *                              Note that samples are only transferred when a
 *                              buffer of this size is filled.
 * @param[in]   num_transfers   The number of active USB transfers that may be
 *                              in-flight at any given time. If unsure of what
 *                              to use here, try values of 4, 8, or 16.
 * @param[in]   stream_timeout  Timeout (milliseconds) for transfers in the
 *                              underlying data stream.
 *
 * @return 0 on success,
 *         ::BLADERF_ERR_UNSUPPORTED if libbladeRF is not built with support
 *         for this functionality,
 *         or a value from \ref RETCODES list on failures.
 */
API_EXPORT
int CALL_CONV bladerf_sync_config(struct bladerf *dev,
                                  bladerf_channel_layout layout,
                                  bladerf_format format,
                                  unsigned int num_buffers,
                                  unsigned int buffer_size,
                                  unsigned int num_transfers,
                                  unsigned int stream_timeout);

/**
 * Transmit IQ samples.
 *
 * Under the hood, this call starts up an underlying asynchronous stream as
 * needed. This stream can be stopped by disabling the TX channel. (See
 * bladerf_enable_module for more details.)
 *
 * Samples will only be sent to the FPGA when a buffer have been filled. The
 * number of samples required to fill a buffer corresponds to the `buffer_size`
 * parameter passed to bladerf_sync_config().
 *
 * @pre A bladerf_sync_config() call has been to configure the device for
 *      synchronous data transfer.
 *
 * @note A call to bladerf_enable_module() should be made before attempting to
 *       transmit samples. Failing to do this may result in timeouts and other
 *       errors.
 *
 * @param       dev         Device handle
 * @param[in]   samples     Array of samples
 * @param[in]   num_samples Number of samples to write
 * @param[in]   metadata    Sample metadata. This must be provided when using
 *                          the ::BLADERF_FORMAT_SC16_Q11_META format, but may
 *                          be NULL when the interface is configured for
 *                          the ::BLADERF_FORMAT_SC16_Q11 format.
 * @param[in]   timeout_ms  Timeout (milliseconds) for this call to complete.
 *                          Zero implies "infinite."
 *
 * @return 0 on success,
 *         ::BLADERF_ERR_UNSUPPORTED if libbladeRF is not built with support
 *         for this functionality,
 *         or a value from \ref RETCODES list on failures.
 */
API_EXPORT
int CALL_CONV bladerf_sync_tx(struct bladerf *dev,
                              const void *samples,
                              unsigned int num_samples,
                              struct bladerf_metadata *metadata,
                              unsigned int timeout_ms);

/**
 * Receive IQ samples.
 *
 * Under the hood, this call starts up an underlying asynchronous stream as
 * needed. This stream can be stopped by disabling the RX channel. (See
 * bladerf_enable_module for more details.)
 *
 * @pre A bladerf_sync_config() call has been to configure the device for
 *      synchronous data transfer.
 *
 * @note A call to bladerf_enable_module() should be made before attempting to
 *       receive samples. Failing to do this may result in timeouts and other
 *       errors.
 *
 * @param       dev         Device handle
 * @param[out]  samples     Buffer to store samples in. The caller is
 *                          responsible for ensuring this buffer is sufficiently
 *                          large for the number of samples requested,
 *                          considering the size of the sample format being
 *                          used.
 * @param[in]   num_samples Number of samples to read
 * @param[out]  metadata    Sample metadata. This must be provided when using
 *                          the ::BLADERF_FORMAT_SC16_Q11_META format, but may
 *                          be NULL when the interface is configured for
 *                          the ::BLADERF_FORMAT_SC16_Q11 format.
 * @param[in]   timeout_ms  Timeout (milliseconds) for this call to complete.
 *                          Zero implies "infinite."
 *
 * @return 0 on success,
 *         ::BLADERF_ERR_UNSUPPORTED if libbladeRF is not built with support
 *         for this functionality,
 *         or a value from \ref RETCODES list on failures.
 */
API_EXPORT
int CALL_CONV bladerf_sync_rx(struct bladerf *dev,
                              void *samples,
                              unsigned int num_samples,
                              struct bladerf_metadata *metadata,
                              unsigned int timeout_ms);


/** @} (End of FN_STREAMING_SYNC) */

/**
 * @defgroup FN_STREAMING_ASYNC    Asynchronous API
 *
 * This interface gives the API user full control over the stream and buffer
 * management, at the cost of added complexity.
 *
 * @note New users are recommended to first evaluate the \ref FN_STREAMING_SYNC
 *       interface, and to only use this interface if the former is found to not
 *       yield suitable performance.
 *
 * These functions are either thread-safe or may be used in a thread-safe
 * manner (per the details noted in the function description).
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
#define BLADERF_STREAM_NO_DATA ((void *)(-1))

/** This opaque structure is used to keep track of stream information */
struct bladerf_stream;

/**
 * This typedef represents a callback function that is executed in response to
 * this interface's asynchronous events.
 *
 * Stream callbacks <b>must not</b> block or perform long-running operations.
 * Otherwise, timeouts may occur. If this cannot be guaranteed, consider
 * returning ::BLADERF_STREAM_NO_DATA in callbacks and later submit a buffer
 * using bladerf_submit_stream_buffer(). However, callbacks should always take
 * a single approach of returning buffers <b>or</b> returning
 * ::BLADERF_STREAM_NO_DATA and submitting buffers later -- <b>but not both</b>.
 *
 * When running in a full-duplex mode of operation with simultaneous TX and RX
 * stream threads, be aware that one stream's callback may occur in the context
 * of another stream's thread. The API user is responsible for ensuring their
 * callbacks are thread safe. For example, when managing access to sample
 * buffers, the caller must ensure that if one thread is processing samples in a
 * buffer, that this buffer is not returned via the callback's return value.
 *
 * As of libbladeRF v0.15.0, is guaranteed that only one callback from a stream
 * will occur at a time. (i.e., a second TX callback will not fire while one is
 * currently being handled.)  To achieve this, while a callback is executing, a
 * per-stream lock is held. It is important to consider this when thinking about
 * the order of lock acquisitions both in the callbacks, and the code
 * surrounding bladerf_submit_stream_buffer().
 *
 * @note Do not call bladerf_submit_stream_buffer() from a callback.
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
 *                  ::BLADERF_STREAM_SHUTDOWN, or ::BLADERF_STREAM_NO_DATA.
 *
 * For RX callbacks:
 *  - samples:          Buffer filled with received data
 *  - num_samples:      Number of samples received and size of next buffers
 *  - Return value:     The user specifies the next buffer to fill with RX data,
 *                      which should be `num_samples` in size,
 *                      ::BLADERF_STREAM_SHUTDOWN, or ::BLADERF_STREAM_NO_DATA.
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
 * @param       dev             Device to associate with the stream
 * @param[in]   callback        Callback routine to handle asynchronous events
 * @param[out]  buffers         This will be updated to point to a dynamically
 *                              allocated array of buffer pointers.
 * @param[in]   num_buffers     Number of buffers to allocate and return. This
 *                              value must >= the `num_transfers` parameter.
 * @param[in]   format          Sample data format
 * @param[in]   samples_per_buffer  Size of allocated buffers, in units of
 *                                  samples Note that the physical size of the
 *                                  buffer is a function of this and the format
 *                                  parameter.
 * @param[in]   num_transfers   Maximum number of transfers that may be
 *                              in-flight simultaneously. This must be <= the
 *                              `num_buffers` parameter.
 * @param[in]   user_data       Caller-provided data that will be provided
 *                              in stream callbacks
 *
 * @note  This call should be later followed by a call to
 *        bladerf_deinit_stream() to avoid memory leaks.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
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
 * Begin running a stream. This call will block until the stream completes.
 *
 * Only 1 RX stream and 1 TX stream may be running at a time. Attempting to
 * call bladerf_stream() with more than one stream will yield unexpected (and
 * most likely undesirable) results.
 *
 * @note See the ::bladerf_stream_cb description for additional thread-safety
 *       caveats.
 *
 * @pre This function should be preceded by a call to bladerf_enable_module()
 *      to enable the associated RX or TX directions before attempting to use
 *      it to stream data.
 *
 * @param      stream   A stream handle that has been successfully been
 *                      initialized via bladerf_init_stream()
 * @param[in]  layout   Stream direction and channel layout
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_stream(struct bladerf_stream *stream,
                             bladerf_channel_layout layout);

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
 * @param           stream      Stream to submit buffer to
 * @param[inout]    buffer      Buffer to fill (RX) or containing data (TX).
 *                              This buffer is assumed to be the size specified
 *                              in the associated bladerf_init_stream() call.
 * @param[in]       timeout_ms  Milliseconds to timeout in, if this call blocks.
 *                              0 implies an "infinite" wait.
 *
 * @return 0 on success, ::BLADERF_ERR_TIMEOUT upon a timeout, or a value from
 *         \ref RETCODES list on other failures
 */
API_EXPORT
int CALL_CONV bladerf_submit_stream_buffer(struct bladerf_stream *stream,
                                           void *buffer,
                                           unsigned int timeout_ms);

/**
 * This is a non-blocking variant of bladerf_submit_stream_buffer(). All of the
 * caveats and important notes from bladerf_submit_stream_buffer() apply.
 *
 * In the event that this call would need to block in order to submit a buffer,
 * it returns BLADERF_ERR_WOULD_BLOCK. In this case, the caller could either
 * wait and try again or defer buffer submission to the asynchronous callback.
 *
 * @param           stream      Stream to submit buffer to
 * @param[inout]    buffer      Buffer to fill (RX) or containing data (TX).
 *                              This buffer is assumed to be the size specified
 *                              in the associated bladerf_init_stream() call.
 *
 * @return  0 on success, ::BLADERF_ERR_WOULD_BLOCK if the call would have to
 *          block to succeed, or another value from \ref RETCODES upon other
 *          failure
 */
API_EXPORT
int CALL_CONV bladerf_submit_stream_buffer_nb(struct bladerf_stream *stream,
                                              void *buffer);


/**
 * Deinitialize and deallocate stream resources.
 *
 * @pre  Stream is no longer being used (via bladerf_submit_stream_buffer() or
 *       bladerf_stream() calls.)
 *
 * @post Stream is deallocated and may no longer be used.
 *
 * @param   stream  Stream to deinitialize. This function does nothing if
 *                  stream is `NULL`.
 */
API_EXPORT
void CALL_CONV bladerf_deinit_stream(struct bladerf_stream *stream);

/**
 * Set stream transfer timeout in milliseconds
 *
 * @param       dev         Device handle
 * @param[in]   dir         Stream direction
 * @param[in]   timeout     Timeout in milliseconds
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_stream_timeout(struct bladerf *dev,
                                         bladerf_direction dir,
                                         unsigned int timeout);

/**
 * Get transfer timeout in milliseconds
 *
 * @param       dev         Device handle
 * @param[in]   dir         Stream direction
 * @param[out]  timeout     On success, updated with current transfer
 *                          timeout value. Undefined on failure.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_stream_timeout(struct bladerf *dev,
                                         bladerf_direction dir,
                                         unsigned int *timeout);

/** @} (End of FN_STREAMING_ASYNC) */

/** @} (End of STREAMING) */

/**
 * @defgroup FN_PROG  Firmware and FPGA
 *
 * These functions provide the ability to load and program devices on the
 * bladeRF board.
 *
 * Care should be taken with bootloader recovery functions to ensure that
 * devices operated on are indeed a bladeRF, as opposed to another FX3-based
 * device running in bootloader mode.
 *
 * These functions are thread-safe.
 *
 * @{
 */

/**
 * Write FX3 firmware to the bladeRF's SPI flash
 *
 * @note This will require a power cycle to take effect
 *
 * @param       dev         Device handle
 * @param[in]   firmware    Full path to firmware file
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_flash_firmware(struct bladerf *dev, const char *firmware);

/**
 * Load device's FPGA.
 *
 * @note This FPGA configuration will be reset at the next power cycle.
 *
 * @param       dev         Device handle
 * @param[in]   fpga        Full path to FPGA bitstream
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
 * @param       dev         Device handle
 * @param[in]   fpga_image  Full path to FPGA file
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_flash_fpga(struct bladerf *dev, const char *fpga_image);

/**
 * Erase the FPGA region of SPI flash, effectively disabling FPGA autoloading
 *
 * @param   dev         Device handle
 *
 * @return 0 on success, value from \ref RETCODES list on failure
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
 * Read firmware log data and write it to the specified file
 *
 * @param       dev         Device to read firmware log from
 * @param[in]   filename    Filename to write log information to. If set to
 *                          `NULL`, log data will be printed to stdout.
 *
 * @return 0 upon success, or a value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_fw_log(struct bladerf *dev, const char *filename);

/**
 * Clear out a firmware signature word in flash and jump to FX3 bootloader.
 *
 * The device will continue to boot into the FX3 bootloader across power cycles
 * until new firmware is written to the device.
 *
 * @param   dev         Device handle
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_jump_to_bootloader(struct bladerf *dev);

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
 * @note This function <b>does not</b> write the firmware to SPI flash. If this
 * is desired, open the newly enumerated device with bladerf_open() and use
 * bladerf_flash_firmware().
 *
 * @param[in]   device_identifier   Device identifier string describing the
 *                                  backend to use via the
 *                                  `<backend>:device=<bus>:<addr>` syntax.  If
 *                                  this is NULL, the backend, bus, and addr
 *                                  arguments will be used instead.
 * @param[in]   backend             Backend to use. This is only used if
 *                                  device_identifier is `NULL`.
 * @param[in]   bus                 Bus number the device is located on. This
 *                                  is only used if device_identifier is `NULL`.
 * @param[in]   addr                Bus address the device is located on. This
 *                                  is only used if device_identifier is `NULL`.
 * @param[in]   file                Filename of the firmware image to boot
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_load_fw_from_bootloader(const char *device_identifier,
                                              bladerf_backend backend,
                                              uint8_t bus,
                                              uint8_t addr,
                                              const char *file);

/** @} (End of FN_PROG) */

/**
 * @defgroup FN_IMAGE Flash image format
 *
 * This section contains a file format and associated routines for storing
 * and loading flash contents with metadata.
 *
 * These functions are thread-safe.
 *
 * @{
 */

/** Type of data stored in a flash image */
typedef enum {
    BLADERF_IMAGE_TYPE_INVALID = -1, /**< Used to denote invalid value */
    BLADERF_IMAGE_TYPE_RAW,          /**< Misc. raw data */
    BLADERF_IMAGE_TYPE_FIRMWARE,     /**< Firmware data */
    BLADERF_IMAGE_TYPE_FPGA_40KLE,   /**< FPGA bitstream for 40 KLE device */
    BLADERF_IMAGE_TYPE_FPGA_115KLE,  /**< FPGA bitstream for 115  KLE device */
    BLADERF_IMAGE_TYPE_FPGA_A4,      /**< FPGA bitstream for A4 device */
    BLADERF_IMAGE_TYPE_FPGA_A9,      /**< FPGA bitstream for A9 device */
    BLADERF_IMAGE_TYPE_CALIBRATION,  /**< Board calibration */
    BLADERF_IMAGE_TYPE_RX_DC_CAL,    /**< RX DC offset calibration table */
    BLADERF_IMAGE_TYPE_TX_DC_CAL,    /**< TX DC offset calibration table */
    BLADERF_IMAGE_TYPE_RX_IQ_CAL,    /**< RX IQ balance calibration table */
    BLADERF_IMAGE_TYPE_TX_IQ_CAL,    /**< TX IQ balance calibration table */
    BLADERF_IMAGE_TYPE_FPGA_A5,      /**< FPGA bitstream for A5 device */
} bladerf_image_type;

/** Size of the magic signature at the beginning of bladeRF image files */
#define BLADERF_IMAGE_MAGIC_LEN 7

/** Size of bladeRF flash image checksum */
#define BLADERF_IMAGE_CHECKSUM_LEN 32

/** Size of reserved region of flash image */
#define BLADERF_IMAGE_RESERVED_LEN 128

/**
 * Image format for backing up and restoring bladeRF flash contents
 *
 * The on disk format generated by the bladerf_image_write function is a
 * serialized version of this structure and its contents. When written to disk,
 * values are converted to big-endian byte order, for ease of reading in a hex
 * editor.
 *
 * When creating and using a bladerf_image of type ::BLADERF_IMAGE_TYPE_RAW,
 * the address and length fields must be erase-block aligned.
 */
struct bladerf_image {
    /**
     * Magic value used to identify image file format.
     *
     * Note that an extra character is added to store a `NUL`-terminator,
     * to allow this field to be printed. This `NUL`-terminator is *NOT*
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
     * The +1 here is actually extraneous; ::BLADERF_SERIAL_LENGTH already
     * accounts for a `NUL` terminator. However, this is left here to avoid
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
     * Address of the flash data in this image. A value of `0xffffffff`
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
 * The following bladerf_image fields are zeroed out: `checksum`, `serial`, and
 * `reserved`
 *
 * If the `length` parameter is not 0, the ::bladerf_image `data` field will be
 * dynamically allocated. Otherwise, `data` will be set to NULL.
 *
 * @note A non-zero `length` should be use only with bladerf_image_write();
 *       bladerf_image_read() allocates and sets `data` based upon size of the
 *       image contents, and does not attempt to free() the `data` field before
 *       setting it.
 *
 * The `address` and `length` fields should be set 0 when reading an image from
 * a file.
 *
 * @return Pointer to allocated and initialized structure on success,
 *         `NULL` on memory allocation failure or invalid address/length.
 */
API_EXPORT
struct bladerf_image *CALL_CONV bladerf_alloc_image(struct bladerf *dev,
                                                    bladerf_image_type type,
                                                    uint32_t address,
                                                    uint32_t length);

/**
 * Create a flash image initialized to contain a calibration data region.
 *
 * This is intended to be used in conjunction with bladerf_image_write(), or a
 * write of the image's `data` field to flash.
 *
 * @param[in]   dev          Device handle
 * @param[in]   fpga_size    Target FPGA size
 * @param[in]   vctcxo_trim  VCTCXO oscillator trim value.
 *
 * @return Pointer to allocated and initialized structure on success,
 *         `NULL` on memory allocation failure
 */
API_EXPORT
struct bladerf_image *CALL_CONV bladerf_alloc_cal_image(
    struct bladerf *dev, bladerf_fpga_size fpga_size, uint16_t vctcxo_trim);

/**
 * Free a bladerf_image previously obtained via bladerf_alloc_image.
 *
 * If the bladerf_image's `data` field is non-`NULL`, it will be freed.
 *
 * @param[inout]    image   Flash image
 */
API_EXPORT
void CALL_CONV bladerf_free_image(struct bladerf_image *image);

/**
 * Write a flash image to a file.
 *
 * This function will fill in the checksum field before writing the contents to
 * the specified file. The user-supplied contents of this field are ignored.
 *
 * @pre  `image` has been initialized using bladerf_alloc_image()
 * @post `image->checksum` will be populated if this function succeeds
 *
 * @param[in]    dev         Device handle
 * @param[in]    image       Flash image
 * @param[in]    file        File to write the flash image to
 *
 * @return 0 upon success, or a value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_image_write(struct bladerf *dev,
                                  struct bladerf_image *image,
                                  const char *file);

/**
 * Read flash image from a file.
 *
 * @pre  The `image` parameter has been obtained via a call to
 *       bladerf_alloc_image(), with a `length` of 0.
 *
 * @post The `image` fields will be populated upon success, overwriting any
 *       previous values.
 *
 * @note The contents of the `image` parameter should not be used if this
 *       function fails.
 *
 * @param[out]   image      Flash image structure to populate.
 * @param[in]    file       File to read image from.
 *
 * @return 0 upon success,
 *         ::BLADERF_ERR_CHECKSUM upon detecting a checksum mismatch,
 *         ::BLADERF_ERR_INVAL if any image fields are invalid,
 *         ::BLADERF_ERR_IO on a file I/O error,
 *         or a value from \ref RETCODES list on any other failure
 */
API_EXPORT
int CALL_CONV bladerf_image_read(struct bladerf_image *image, const char *file);

/** @} (End of FN_IMAGE) */

/**
 * @defgroup FN_LOW_LEVEL Low-level Functions
 *
 * This section defines low-level APIs.
 *
 * @{
 */

/**
 * @defgroup FN_VCTCXO_TAMER VCTCXO Tamer Mode
 *
 * This group provides routines for controlling the VTCTXO tamer.
 *
 * These functions are thread-safe.
 *
 * @{
 */

/**
 * VCTCXO Tamer mode selection
 *
 * These values control the use of header J71 pin 1 for taming the
 * on-board VCTCXO to improve or sustain frequency accuracy.
 *
 * When supplying input into the VCTCXO tamer, a 1.8V signal must be provided.
 *
 * @warning IMPORTANT: Exceeding 1.8V on J71-1 can damage the associated FPGA
 *          I/O bank. Ensure that you provide only a 1.8V signal!
 */
typedef enum {
    /** Denotes an invalid selection or state */
    BLADERF_VCTCXO_TAMER_INVALID = -1,

    /** Do not attempt to tame the VCTCXO with an input source. */
    BLADERF_VCTCXO_TAMER_DISABLED = 0,

    /** Use a 1 pps input source to tame the VCTCXO. */
    BLADERF_VCTCXO_TAMER_1_PPS = 1,

    /** Use a 10 MHz input source to tame the VCTCXO. */
    BLADERF_VCTCXO_TAMER_10_MHZ = 2
} bladerf_vctcxo_tamer_mode;

/**
 * Set the VCTCXO tamer mode.
 *
 * @param       dev         Device handle
 * @param[in]   mode        VCTCXO taming mode
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_vctcxo_tamer_mode(struct bladerf *dev,
                                            bladerf_vctcxo_tamer_mode mode);

/**
 * Get the current VCTCXO tamer mode
 *
 * @param           dev         Device handle
 * @param[out]      mode        Current VCTCXO taming mode or
 *                              ::BLADERF_VCTCXO_TAMER_INVALID if a failure
 *                              occurs.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_vctcxo_tamer_mode(struct bladerf *dev,
                                            bladerf_vctcxo_tamer_mode *mode);

/** @} (End of FN_VCTCXO_TAMER) */

/**
 * @defgroup FN_VCTCXO_TRIM_DAC VCTCXO Trim DAC
 *
 * These functions provide the ability to manipulate the VCTCXO Trim DAC.
 *
 * These functions are thread-safe.
 *
 * @{
 */

/**
 * Query a device's VCTCXO calibration trim
 *
 * @param       dev     Device handle
 * @param[out]  trim    VCTCXO calibration trim
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_vctcxo_trim(struct bladerf *dev, uint16_t *trim);

/**
 * Write value to VCTCXO trim DAC.
 *
 * @note This should not be used when the VCTCXO tamer is enabled.
 *
 * @param       dev     Device handle
 * @param[in]   val     Desired VCTCXO trim DAC value
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_trim_dac_write(struct bladerf *dev, uint16_t val);

/**
 * Read value from VCTCXO trim DAC.
 *
 * This is similar to bladerf_get_vctcxo_trim(), except that it returns the
 * current trim DAC value, as opposed to the calibration value read from flash.
 *
 * Use this if you are trying to query the value after having previously made
 * calls to bladerf_trim_dac_write().
 *
 * @param       dev     Device handle
 * @param[out]  val     Current VCTCXO trim DAC value
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_trim_dac_read(struct bladerf *dev, uint16_t *val);

/** @} (End of FN_VCTCXO_TRIM_DAC) */

/**
 * @defgroup FN_TUNING_MODE Tuning Mode
 *
 * These functions provide the ability to select the tuning mode.
 *
 * These functions are thread-safe.
 *
 * @{
 */

/**
 * Frequency tuning modes
 *
 * ::BLADERF_TUNING_MODE_HOST is the default if either of the following
 * conditions are true:
 *   - libbladeRF < v1.3.0
 *   - FPGA       < v0.2.0
 *
 * ::BLADERF_TUNING_MODE_FPGA is the default if both of the following
 * conditions are true:
 *  - libbladeRF >= v1.3.0
 *  - FPGA       >= v0.2.0
 *
 * The default mode can be overridden by setting a BLADERF_DEFAULT_TUNING_MODE
 * environment variable to `host` or `fpga`.
 *
 * @note Overriding this value with a mode not supported by the FPGA will result
 *       in failures or unexpected behavior.
 */
typedef enum {
    /** Indicates an invalid mode is set */
    BLADERF_TUNING_MODE_INVALID = -1,

    /**
     * Perform tuning algorithm on the host. This is slower, but provides
     * easier accessiblity to diagnostic information.
     */
    BLADERF_TUNING_MODE_HOST,

    /** Perform tuning algorithm on the FPGA for faster tuning. */
    BLADERF_TUNING_MODE_FPGA,
} bladerf_tuning_mode;

/**
 * Set the device's tuning mode
 *
 * @param       dev         Device handle
 * @param[in]   mode        Desired tuning mode. Note that the available modes
 *                          depends on the FPGA version.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_tuning_mode(struct bladerf *dev,
                                      bladerf_tuning_mode mode);

/**
 * Get the device's current tuning mode
 *
 * @param       dev         Device handle
 * @param[in]   mode        Tuning mode
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_tuning_mode(struct bladerf *dev,
                                      bladerf_tuning_mode *mode);

/** @} (End of FN_TUNING_MODE) */

/**
 * @defgroup FN_TRIGGER_CONTROL Trigger Control
 *
 * These functions provide the ability to read and write the trigger control
 * registers.
 *
 * These functions are thread-safe.
 *
 * @{
 */

/**
 * Trigger control register "Arm" bit
 *
 * This bit arms (i.e., enables) the trigger controller when set to 1. Samples
 * will be gated until the "Fire" bit has been asserted.
 *
 * A 0 in this bit disables the trigger controller. Samples will continue to
 * flow as they normally do in this state.
 */
#define BLADERF_TRIGGER_REG_ARM ((uint8_t)(1 << 0))

/**
 * Trigger control register "Fire" bit
 *
 * For a master, this bit causes a trigger to be sent to all slave devices. Once
 * this trigger is received (the master "receives" it immediately as well),
 * devices begin streaming samples.
 *
 * This bit has no effect on slave devices.
 */
#define BLADERF_TRIGGER_REG_FIRE ((uint8_t)(1 << 1))

/**
 * Trigger control register "Master" bit
 *
 * Selects whether the device is a trigger master (1) or trigger slave (0). The
 * trigger master drives the trigger signal as an output.
 *
 * Slave devices configure the trigger signal as an input.
 */
#define BLADERF_TRIGGER_REG_MASTER ((uint8_t)(1 << 2))

/**
 * Trigger control registers "line" bit
 *
 * This is a read-only register bit that denotes the current state of the the
 * trigger signal.
 */
#define BLADERF_TRIGGER_REG_LINE ((uint8_t)(1 << 3))

/**
 * Read trigger control register
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[in]   signal      Trigger signal (control register) to read from
 * @param[out]  val         Pointer to variable that register is read into See
 *                          the BLADERF_TRIGGER_REG_* macros for the meaning of
 *                          each bit.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_read_trigger(struct bladerf *dev,
                                   bladerf_channel ch,
                                   bladerf_trigger_signal signal,
                                   uint8_t *val);

/**
 * Write trigger control register
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[in]   signal      Trigger signal to configure
 * @param[in]   val         Data to write into the trigger control register.
 *                          See the BLADERF_TRIGGER_REG_* macros for options.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_write_trigger(struct bladerf *dev,
                                    bladerf_channel ch,
                                    bladerf_trigger_signal signal,
                                    uint8_t val);

/** @} (End of FN_TRIGGER_CONTROL) */

/**
 * @defgroup FN_WISHBONE_MASTER Wishbone bus master
 *
 * These functions provide the ability to read and write to the wishbone peripheral bus,
 * which is reserved for modem
 *
 * These functions are thread-safe.
 *
 * @{
 */

/**
 * Read a specific Wishbone Master address
 *
 * @param       dev     Device handle
 * @param       addr    Wishbone Master address
 * @param[out]  data    Wishbone Master data
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_wishbone_master_read(struct bladerf *dev, uint32_t addr, uint32_t *data);

/**
 * Write value to a specific Wishbone Master address
 *
 *
 * @param       dev     Device handle
 * @param       addr    Wishbone Master address
 * @param       data    Wishbone Master data
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_wishbone_master_write(struct bladerf *dev, uint32_t addr, uint32_t val);

/** @} (End of FN_WISHBONE_MASTER) */


/**
 * @defgroup FN_CONFIG_GPIO Configuration GPIO
 *
 * These functions provide the ability to read and write the configuration
 * GPIO.
 *
 * These functions are thread-safe.
 *
 * @{
 */

/**
 * Read the configuration GPIO register.
 *
 * @param       dev     Device handle
 * @param[out]  val     Current configuration GPIO value
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_config_gpio_read(struct bladerf *dev, uint32_t *val);

/**
 * Write the configuration GPIO register.
 *
 * @note Callers should be sure to perform a read-modify-write sequence to
 *       avoid accidentally clearing other GPIO bits that may be set by the
 *       library internally.
 *
 * @param       dev     Device handle
 * @param[out]  val     Desired configuration GPIO value
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_config_gpio_write(struct bladerf *dev, uint32_t val);

/** @} (End of FN_CONFIG_GPIO) */

/**
 * @defgroup FN_SPI_FLASH SPI Flash
 *
 * These functions provide the ability to erase, read, and write the SPI flash.
 *
 * @warning Use of SPI flash functions requires an understanding of the
 *          underlying SPI flash device, and the bladeRF's flash memory map. Be
 *          sure to review the following page and the associated flash datasheet
 *          before using these functions:
 *      https://github.com/nuand/bladeRF/wiki/FX3-Firmware#spi-flash-layout
 *
 * These functions are thread-safe.
 *
 * @{
 */

/**
 * Erase regions of the bladeRF's SPI flash
 *
 * @note This function operates in units of 64 KiB erase blocks
 * @note Not recommended for new designs. Consider using the
 *       `bladerf_erase_flash_bytes()` function instead. It will perform the
 *       necessary conversion from bytes to pages based on the specific
 *       flash architecture found on the board.
 *
 * @param       dev             Device handle
 * @param[in]   erase_block     Erase block from which to start erasing
 * @param[in]   count           Number of blocks to erase
 *
 * @return 0 on success,
 *         or ::BLADERF_ERR_INVAL on an invalid `erase_block` or `count` value,
 *         or a value from \ref RETCODES list on other failures
 */
API_EXPORT
int CALL_CONV bladerf_erase_flash(struct bladerf *dev,
                                  uint32_t erase_block,
                                  uint32_t count);

/**
 * Erase regions of the bladeRF's SPI flash
 *
 * @note This function operates in units of bytes
 *
 * @param       dev             Device handle
 * @param[in]   address         Address at which to start erasing
 * @param[in]   length          Number of bytes to erase
 *
 * @return 0 on success,
 *         or ::BLADERF_ERR_INVAL on an invalid `address` or `length` value,
 *         or a value from \ref RETCODES list on other failures
 */
API_EXPORT
int CALL_CONV bladerf_erase_flash_bytes(struct bladerf *dev,
                                        uint32_t address,
                                        uint32_t length);

/**
 * Read data from the bladeRF's SPI flash
 *
 * @note This function operates in units of flash pages.
 * @note Not recommended for new designs. Consider using the
 *       `bladerf_read_flash_bytes()` function instead. It will perform the
 *       necessary conversion from bytes to pages based on the specific
 *       flash architecture found on the board.
 *
 * @param       dev     Device handle
 * @param[in]   buf     Buffer to read data into. Must be `count` *
 *                      flash-page-size bytes or larger.
 * @param[in]   page    Page to begin reading from
 * @param[in]   count   Number of pages to read
 *
 * @return 0 on success,
 *         or ::BLADERF_ERR_INVAL on an invalid `page` or `count` value,
 *         or a value from \ref RETCODES list on other failures.
 */
API_EXPORT
int CALL_CONV bladerf_read_flash(struct bladerf *dev,
                                 uint8_t *buf,
                                 uint32_t page,
                                 uint32_t count);

/**
 * Read data from the bladeRF's SPI flash
 *
 * @note This function operates in units of bytes.
 *
 * @param       dev     Device handle
 * @param[in]   buf     Buffer to read data into. Must be `bytes`
 *                      bytes or larger.
 * @param[in]   address Address to begin reading from
 * @param[in]   bytes   Number of bytes to read
 *
 * @return 0 on success,
 *         or ::BLADERF_ERR_INVAL on an invalid `address` or `bytes` value,
 *         or a value from \ref RETCODES list on other failures.
 */
API_EXPORT
int CALL_CONV bladerf_read_flash_bytes(struct bladerf *dev,
                                       uint8_t *buf,
                                       uint32_t address,
                                       uint32_t bytes);

/**
 * Write data to the bladeRF's SPI flash device
 *
 * @note This function operates in units of flash pages.
 * @note Not recommended for new designs. Consider using the
 *       `bladerf_write_flash_bytes()` function instead. It will perform the
 *       necessary conversion from bytes to pages based on the specific
 *       flash architecture found on the board.
 *
 * @param       dev     Device handle
 * @param[in]   buf     Data to write to flash
 * @param[in]   page    Page to begin writing at
 * @param[in]   count   Number of pages to write
 *
 * @return 0 on success,
 *         or ::BLADERF_ERR_INVAL on an invalid `page` or `count` value,
 *         or a value from \ref RETCODES list on other failures.
 */
API_EXPORT
int CALL_CONV bladerf_write_flash(struct bladerf *dev,
                                  const uint8_t *buf,
                                  uint32_t page,
                                  uint32_t count);

/**
 * Write data to the bladeRF's SPI flash device
 *
 * @note This function operates in units of bytes.
 *
 * @param       dev     Device handle
 * @param[in]   buf     Data to write to flash
 * @param[in]   address Address to begin writing at
 * @param[in]   length  Number of bytes to write
 *
 * @return 0 on success,
 *         or ::BLADERF_ERR_INVAL on an invalid `address` or `length` value,
 *         or a value from \ref RETCODES list on other failures.
 */
API_EXPORT
int CALL_CONV bladerf_write_flash_bytes(struct bladerf *dev,
                                        const uint8_t *buf,
                                        uint32_t address,
                                        uint32_t length);

/**
 * Lock the bladeRF's OTP
 *
 * @param       dev     Device handle
 *
 * @return 0 on success,
 *         or ::BLADERF_ERR_INVAL on an invalid `page` or `count` value,
 *         or a value from \ref RETCODES list on other failures.
 */

API_EXPORT
int CALL_CONV bladerf_lock_otp(struct bladerf *dev);

/**
 * Read data from the bladeRF's SPI flash OTP
 *
 * @note This function operates solely on the first 256 byte page of the OTP
 *
 * @param       dev     Device handle
 * @param[in]   buf     Buffer to read OTP data into
 *
 * @return 0 on success,
 *         or ::BLADERF_ERR_INVAL on an invalid `page` or `count` value,
 *         or a value from \ref RETCODES list on other failures.
 */
API_EXPORT
int CALL_CONV bladerf_read_otp(struct bladerf *dev,
                                 uint8_t *buf);

/**
 * Write data to the bladeRF's SPI flash OTP device
 *
 * @note This function operates solely on the first 256 byte page of the OTP
 *
 * @param       dev     Device handle
 * @param[in]   buf     Data to write to OTP
 *
 * @return 0 on success,
 *         or ::BLADERF_ERR_INVAL on an invalid `page` or `count` value,
 *         or a value from \ref RETCODES list on other failures.
 */
API_EXPORT
int CALL_CONV bladerf_write_otp(struct bladerf *dev,
                                  uint8_t *buf);

/** @} (End of FN_SPI_FLASH) */

/**
 * @defgroup FN_RF_PORTS RF Ports
 *
 * These functions provide the ability to select various RF ports for RX and TX
 * channels. This is normally handled automatically.
 *
 * These functions are thread-safe.
 *
 * @{
 */

/**
 * Set the RF port
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[in]   port        RF port name
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_set_rf_port(struct bladerf *dev,
                                  bladerf_channel ch,
                                  const char *port);

/**
 * Get the RF port
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[out]  port        RF port name
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_rf_port(struct bladerf *dev,
                                  bladerf_channel ch,
                                  const char **port);

/**
 * Get available RF ports
 *
 * This function may be called with `NULL` for `ports`, or 0 for `count`, to
 * determine the number of RF ports.
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[out]  ports       RF port names
 * @param[out]  count       Number to populate
 *
 * @return Number of RF ports on success, value from \ref RETCODES list on
 *         failure
 */
API_EXPORT
int CALL_CONV bladerf_get_rf_ports(struct bladerf *dev,
                                   bladerf_channel ch,
                                   const char **ports,
                                   unsigned int count);

/** @} (End of FN_RF_PORTS) */

/** @} (End of FN_LOW_LEVEL) */

/**
 * @defgroup    FN_SF Features
 *
 * This group of functions provides the ability to set features available
 * to bladeRF devices.
 *
 * @{
 */

/**
 * Feature Set
 */
typedef enum {
    BLADERF_FEATURE_DEFAULT = 0,   /**< No feature enabled */
    BLADERF_FEATURE_OVERSAMPLE     /**< Enforces AD9361 OC and 8bit mode */
} bladerf_feature;

/**
 * Enables a feature.
 *
 * @param       dev         Device handle
 * @param[out]  feature     Feature
 * @param[in]   enable  true to enable, false to disable
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_enable_feature(struct bladerf *dev,
                                  bladerf_feature feature,
                                  bool enable);

/**
 * Gets currently enabled feature.
 *
 * @param       dev         Device handle
 * @param[out]  feature     Feature
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_get_feature(struct bladerf *dev,
                                  bladerf_feature* feature);

/** @} (End of FN_SF) */

/**
 * @defgroup    FN_XB   Expansion board support
 *
 * This group of functions provides the ability to attach and detach expansion
 * boards.
 *
 * In general, one should call bladerf_expansion_attach() immediately after
 * opening the device.
 *
 * @note Hotplug and expansion board removal is not supported. It is expected
 *       that the expansion boards are attached at power-on and remain attached
 *       until power is removed.
 *
 * These functions are thread-safe.
 *
 * @{
 */

/**
 * Expansion boards
 */
typedef enum {
    BLADERF_XB_NONE = 0, /**< No expansion boards attached */
    BLADERF_XB_100,      /**< XB-100 GPIO expansion board.
                          *   This device is not yet supported in
                          *   libbladeRF, and is here as a placeholder
                          *   for future support. */
    BLADERF_XB_200,      /**< XB-200 Transverter board */
    BLADERF_XB_300       /**< XB-300 Amplifier board */
} bladerf_xb;

/**
 * Attach and enable an expansion board's features
 *
 * @param       dev         Device handle
 * @param[in]   xb          Expansion board
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_expansion_attach(struct bladerf *dev, bladerf_xb xb);

/**
 * Determine which expansion board is attached
 *
 * @param       dev         Device handle
 * @param[out]  xb          Expansion board
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
API_EXPORT
int CALL_CONV bladerf_expansion_get_attached(struct bladerf *dev,
                                             bladerf_xb *xb);

/** @} (End of FN_XB) */

/**
 * @defgroup FN_LOGGING Logging
 *
 * This section contains various helper/utility functions for library logging
 * facilities.
 *
 * These functions are thread-safe.
 *
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
 * Sets the filter level for displayed log messages.
 *
 * Messages that are at or above the specified log level will be printed, while
 * messages with a lower log level will be suppressed.
 *
 * @param[in]   level       The new log level filter value
 */
API_EXPORT
void CALL_CONV bladerf_log_set_verbosity(bladerf_log_level level);

/** @} (End of FN_LOGGING) */

/**
 * @defgroup FN_LIBRARY_VERSION Library version
 *
 * @{
 */

/**
 * Get libbladeRF version information
 *
 * @param[out]  version     libbladeRF version information
 */
API_EXPORT
void CALL_CONV bladerf_version(struct bladerf_version *version);

/** @} (End of FN_LIBRARY_VERSION) */

/**
 * @defgroup    RETCODES    Error codes
 *
 * bladeRF library routines return negative values to indicate errors.
 * Values >= 0 are used to indicate success.
 *
 * @code
 *  int status = bladerf_set_gain(dev, BLADERF_CHANNEL_RX(0), 2);
 *
 *  if (status < 0) {
 *      handle_error();
 *  }
 * @endcode
 *
 * @{
 */
// clang-format off
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
#define BLADERF_ERR_PERMISSION  (-17) /**< Insufficient permissions for the
                                       *   requested operation */
#define BLADERF_ERR_WOULD_BLOCK (-18) /**< Operation would block, but has been
                                       *   requested to be non-blocking. This
                                       *   indicates to a caller that it may
                                       *   need to retry the operation later.
                                       */
#define BLADERF_ERR_NOT_INIT    (-19) /**< Device insufficiently initialized
                                       *   for operation */
// clang-format on

/**
 * Obtain a textual description of a value from the \ref RETCODES list
 *
 * @param[in]   error   Error value to look up
 *
 * @return  Error string
 */
API_EXPORT
const char *CALL_CONV bladerf_strerror(int error);

/** @} (End RETCODES) */

#include <bladeRF1.h>
#include <bladeRF2.h>

#ifdef __cplusplus
}
#endif

#endif /* LIBBLADERF_H_ */
