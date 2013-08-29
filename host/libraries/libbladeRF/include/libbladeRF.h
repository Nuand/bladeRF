/**
 * @file libbladeRF.h
 *
 * @brief bladeRF library
 */
#ifndef BLADERF_H_
#define BLADERF_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
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

/** @} (End RETCODES) */

/**
 * Backend by which the host communicates with the device
 */
typedef enum {
    BLADERF_BACKEND_ANY,    /**< "Don't Care" -- use any available backend */
    BLADERF_BACKEND_LINUX,  /**< Linux kernel driver */
    BLADERF_BACKEND_LIBUSB  /**< libusb */
} bladerf_backend;

/* Length of device serial number string, including NUL-terminator */
#define BLADERF_SERIAL_LENGTH   33

/**
 * Information about a bladeRF attached to the system
 */
struct bladerf_devinfo {
    bladerf_backend backend;    /**< Backend to use when connecting to device */
    char serial[BLADERF_SERIAL_LENGTH]; /**< Device serial number string */
    uint8_t usb_bus;            /**< Bus # device is attached to */
    uint8_t usb_addr;           /**< Device address on bus */
    unsigned int instance;      /**< Device instance or ID */
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
 * Module selection for those which have both RX and TX constituents
 */
typedef enum
{
    BLADERF_MODULE_RX,  /**< Receive Module */
    BLADERF_MODULE_TX   /**< Transmit Module */
} bladerf_module;

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
 int bladerf_get_device_list(struct bladerf_devinfo **devices);

/**
 * Free device list returned by bladerf_get_device_list()
 *
 * @param   devices     List of available devices
 */
void bladerf_free_device_list(struct bladerf_devinfo *devices);


/**
 * Get the device path if a bladeRF device
 *
 * @param[in]   dev     Device handle
 *
 * @returns A pointer to the device path string, or NULL on error
 */
char * bladerf_dev_path(struct bladerf *dev);

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
int bladerf_open_with_devinfo(struct bladerf **device,
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
 *      - Nth instance encountered (libusb)
 *      - Device node N, such as /dev/bladerfN (linux)
 *   - serial=\<serial\>
 *      - Device's serial number.
 *
 * @param[out]  device             Update with device handle on success
 * @param[in]   device_identifier  Device identifier, formatted as described above
 *
 * @return 0 on success, or value from \ref RETCODES list on failure
 */
int bladerf_open(struct bladerf **device, const char *device_identifier);

/**
 * Close device
 *
 * @note    Failing to close a device may result in memory leaks.
 *
 * @post    device is deallocated and may no longer be used.
 *
 * @param   device  Device handle previously obtained by bladerf_open()
 */
void bladerf_close(struct bladerf *device);

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
int bladerf_enable_module(struct bladerf *dev,
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
int bladerf_set_loopback( struct bladerf *dev, bladerf_loopback l);


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
int bladerf_set_sample_rate(struct bladerf *dev, bladerf_module module,
                            unsigned int rate, unsigned int *actual);

/**
 * Configure the device's sample rate as a rational fraction of Hz.
 * Sample rates are in the form of integer + num/denom.
 * TODO: Should this be the only way we set values, and num=0 and denom=1
 * for integer portions?
 *
 * @param[in]   dev         Device handle
 * @param[in]   module      Module to change
 * @param[in]   integer     Integer portion of the equation integer + num/denom
 * @param[in]   num         Numerator of rational fractional part
 * @param[in]   denom       Denominator of rational fractional part
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
int bladerf_set_rational_sample_rate(struct bladerf *dev,
                                     bladerf_module module,
                                     unsigned int integer, unsigned int num,
                                     unsigned int denom);

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
int bladerf_set_sampling(struct bladerf *dev, bladerf_sampling sampling);

/**
 * Read the device's current state of RXVGA2 and ADC pin connection
 * to figure out which sampling mode it is currently configured in.
 *
 * @param[in]   dev         Device handle
 * @param[out]  sampling    Sampling connection
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
int bladerf_get_sampling(struct bladerf *dev, bladerf_sampling *sampling);

/**
 * Read the device's sample rate in Hz
 *
 * @param[in]   dev         Device handle
 * @param[in]   module      Module to query
 * @param[out]  rate        Pointer to returned sample rate
 *
 * @return 0 on success, value from \ref RETCODES list upon failure
 */
int bladerf_get_sample_rate(struct bladerf *dev, bladerf_module module,
                            unsigned int *rate);

/**
 * Set the PA gain in dB
 *
 * @param       dev         Device handle
 * @param       gain        Desired gain
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
int bladerf_set_txvga2(struct bladerf *dev, int gain);

/**
 * Get the PA gain in dB
 *
 * @param       dev         Device handle
 * @param       gain        Pointer to returned gain
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
int bladerf_get_txvga2(struct bladerf *dev, int *gain);

/**
 * Set the post-LPF gain in dB
 *
 * @param       dev         Device handle
 * @param       gain        Desired gain
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
int bladerf_set_txvga1(struct bladerf *dev, int gain);

/**
 * Get the post-LPF gain in dB
 *
 * @param       dev         Device handle
 * @param       gain        Pointer to returned gain
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
int bladerf_get_txvga1(struct bladerf *dev, int *gain);

/**
 * Set the LNA gain
 *
 * @param       dev         Device handle
 * @param       gain        Desired gain level
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
int bladerf_set_lna_gain(struct bladerf *dev, bladerf_lna_gain gain);

/**
 * Get the LNA gain
 *
 * @param       dev         Device handle
 * @param       gain        Pointer to the set gain level
 */
int bladerf_get_lna_gain(struct bladerf *dev, bladerf_lna_gain *gain);

/**
 * Set the pre-LPF VGA gain
 *
 * @param       dev         Device handle
 * @param       gain        Desired gain
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
int bladerf_set_rxvga1(struct bladerf *dev, int gain);

/**
 * Get the pre-LPF VGA gain
 *
 * @param       dev         Device handle
 * @param       gain        Pointer to the set gain level
 */
int bladerf_get_rxvga1(struct bladerf *dev, int *gain);

/**
 * Set the post-LPF VGA gain
 *
 * @param       dev         Device handle
 * @param       gain        Desired gain
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
int bladerf_set_rxvga2(struct bladerf *dev, int gain);

/**
 * Get the post-LPF VGA gain
 *
 * @param       dev         Device handle
 * @param       gain        Pointer to the set gain level
 */
int bladerf_get_rxvga2(struct bladerf *dev, int *gain);

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
int bladerf_set_bandwidth(struct bladerf *dev, bladerf_module module,
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
int bladerf_get_bandwidth(struct bladerf *dev, bladerf_module module,
                            unsigned int *bandwidth);

/**
 * Select the appropriate band path given a frequency in Hz.
 *
 * @param       dev         Device handle
 * @param       module      Module to configure
 * @param       frequency   Tuned frequency
 */
int bladerf_select_band(struct bladerf *dev, bladerf_module module,
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
int bladerf_set_frequency(struct bladerf *dev,
                            bladerf_module module, unsigned int frequency);

/**
 * Set module's frequency in Hz
 *
 * @param       dev         Device handle
 * @param       module      Module to configure
 * @param       frequency   Pointer to the returned frequency
 */
int bladerf_get_frequency(struct bladerf *dev,
                            bladerf_module module, unsigned int *frequency);

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
int bladerf_init_stream(struct bladerf_stream **stream,
                        struct bladerf *dev,
                        bladerf_stream_cb callback,
                        void ***buffers,
                        size_t num_buffers,
                        bladerf_format format,
                        size_t num_samples,
                        size_t num_transfers,
                        void *user_data);

/**
 * Deinitialize and deallocate stream resources
 */
void bladerf_deinit_stream(struct bladerf_stream *stream);

/**
 *
 * @pre The provided bladerf_stream must have had its callback,
 *      samples_per_buffer, buffers_per_stream, and (optional) data fields
 *      initialized prior to this call
 */
int bladerf_stream(struct bladerf *dev, bladerf_module module,
                   bladerf_format format, struct bladerf_stream *stream);

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
int bladerf_tx(struct bladerf *dev, bladerf_format format,
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
int bladerf_rx(struct bladerf *dev, bladerf_format format,
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
int bladerf_get_serial(struct bladerf *dev, char *serial);

/**
 * Query a device's VCTCXO calibration trim
 *
 * @param[in]   dev     Device handle
 * @param[out]  trim    Will be updated with the factory DAC trim value. If an
 *                      error occurs, no data will be written to this pointer.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
int bladerf_get_vctcxo_trim(struct bladerf *dev, uint16_t *trim);

/**
 * Query a device's FPGA size
 *
 * @param[in]   dev     Device handle
 * @param[out]  size    Will be updated with the onboard FPGA's size. If an
 *                      error occurs, no data will be written to this pointer.
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
int bladerf_get_fpga_size(struct bladerf *dev, bladerf_fpga_size *size);

/**
 * Query firmware version
 *
 * @param[in]   dev     Device handle
 * @param[out]  major   Firmware major version
 * @param[out]  minor   Firmware minor version
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
int bladerf_get_fw_version(struct bladerf *dev,
                            unsigned int *major, unsigned int *minor);

/**
 * Check FPGA configuration status
 *
 * @param   dev     Device handle
 *
 * @return  1 if FPGA is configured,
 *          0 if it is not,
 *          and value from \ref RETCODES list on failure
 */
int bladerf_is_fpga_configured(struct bladerf *dev);

/**
 * Query FPGA version
 *
 * @param[in]   dev     Device handle
 * @param[out]  major   FPGA major version
 * @param[out]  minor   FPGA minor version
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
int bladerf_get_fpga_version(struct bladerf *dev,
                                unsigned int *major, unsigned int *minor);

/**
 * Obtain device statistics
 *
 * @param[in]   dev     Device handle
 * @param[out]  stats   Current device statistics
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
int bladerf_stats(struct bladerf *dev, struct bladerf_stats *stats);

/** @} (End FN_INFO) */


/**
 * @defgroup FN_PROG    Device programming
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
int bladerf_flash_firmware(struct bladerf *dev, const char *firmware);

/**
 * Reset the device
 *
 * @note This also causes the device to reload its firmware
 *
 * @param   dev         Device handle
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
int bladerf_device_reset(struct bladerf *dev);

/**
 * Load device's FPGA
 *
 * @param   dev         Device handle
 * @param   fpga        Full path to FPGA bitstream
 *
 * @return 0 upon successfully,
 *         1 if FPGA is already loaded,
 *         or a value from \ref RETCODES list on failure
 */
int bladerf_load_fpga(struct bladerf *dev, const char *fpga);


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
const char * bladerf_strerror(int error);

/**
 * Get libbladeRF version information.
 *
 * The parameters are optional and may be set to NULL if not needed
 *
 * param[out] major     Major version
 * param[out] minor     Minor version
 * param[out] patch     Patch version
 *
 * @warning Do not attempt to modify the returned string.
 *
 * @return  Version string. This value will contain git information for
 *          non-release builds (i.e., a short changeset and "dirty" status)
 */
const char * bladerf_version(unsigned int *major,
                             unsigned int *minor,
                             unsigned int *patch);

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
int bladerf_si5338_read(struct bladerf *dev, uint8_t address, uint8_t *val);

/**
 * Write a Si5338 register
 *
 * @param   dev         Device handle
 * @param   address     Si5338 register offset
 * @param   val         Data to write to register
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
int bladerf_si5338_write(struct bladerf *dev, uint8_t address, uint8_t val);

/**
 * Set frequency for TX clocks
 *
 * @param   dev         Device handle
 * @param   freq        Desired TX frequency in Hz
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
int bladerf_si5338_set_tx_freq(struct bladerf *dev, unsigned freq);

/**
 * Set frequency for RX clocks
 *
 * @param   dev         Device handle
 * @param   freq        Desired RX frequency in Hz
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
int bladerf_si5338_set_rx_freq(struct bladerf *dev, unsigned freq);


/**
 * Read a LMS register
 *
 * @param   dev         Device handle
 * @param   address     LMS register offset
 * @param   val         Pointer to variable the data should be read into
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
int bladerf_lms_read(struct bladerf *dev, uint8_t address, uint8_t *val);

/**
 * Write a LMS register
 *
 * @param   dev         Device handle
 * @param   address     LMS register offset
 * @param   val         Data to write to register
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
int bladerf_lms_write(struct bladerf *dev, uint8_t address, uint8_t val);

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
int bladerf_config_gpio_read(struct bladerf *dev, uint32_t *val);

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
int bladerf_config_gpio_write(struct bladerf *dev, uint32_t val);

/**
 * Write value to VCTCXO DAC
 *
 * @param   dev         Device handle
 * @param   val         Data to write to DAC register
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
int bladerf_dac_write(struct bladerf *dev, uint16_t val);

/* @} (End of LOW_LEVEL) */

#ifdef __cplusplus
}
#endif

#endif /* BLADERF_H_ */
