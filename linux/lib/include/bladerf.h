/**
 * bladeRF library
 */
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

/**
 * @defgroup    RETCODES    Error return codes
 *
 * bladerf library routines return negative values to indicate erros. Values
 * >= 0 are reserved for returning other meaningful values upon success.
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
#define BLADERF_ERR_UNEXPECTED (-1)  /**< An unexpected failure occurred */
#define BLADERF_ERR_RANGE      (-2)  /**< Provided parameter is out of range */
#define BLADERF_ERR_INVAL      (-3)  /**< Invalid paramter provided */
#define BLADERF_ERR_MEM        (-4)  /**< Memory allocation error */
#define BLADERF_ERR_IO         (-5)  /**< File/Device I/O error */

/** @} (End RETCODES) */

/**
 * bladerRF device handle
 */
struct bladerf;

/**
 * Information about a bladeRF attached to the system
 */
struct bladerf_devinfo {
    char *path;         /**< Path to devices */
    uint64_t serial;    /**< Device's serial number */
};

/**
 * TODO describe "module" in more detail
 */
enum bladerf_module { TX, RX } ;

/**
 * TODO describe LNA gain in more detail
 */
enum bladerf_lna_gain { LOW, MED, HIGH } ;

/**
 * TODO describe various loopback modes
 */
enum bladerf_loopback {
    LB_BB_LPF = 0,
    LB_BB_VGA2,
    LB_BB_OP,
    LB_RF_LNA1,
    LB_RF_LNA2,
    LB_RF_LNA3,
    LB_NONE
};


/**
 * @defgroup FN_INIT    Initialization/deinitialization
 *
 * The below snippet shows the general process of finding and acquiring a handle
 * to a bladeRF device.
 *
 * @code
 *  struct bladerf_devinfo *available_devs = NULL;
 *  ssize_t num_available_devs;
 *  struct bladerf *dev = NULL;
 *  char *desired_dev_path;
 *
 *  num_availble_devs = bladerf_get_device_list(&available_devs);
 *  if (num_availble_devs >= 0) {
 *      desired_dev_path = desired_device(available_devs, num_available_devs);
 *      dev = bladerf_open(desired_dev_path);
 *      bladerf_free_device_list(available_devs, num_available_devs);
 *  }
 *
 * @endcode
 *
 * @{
 */

/**
 * Obtain a list of bladeRF devices attached to the system
 *
 * @param[out]  devices
 *
 * @return number of items in returned device list, or BLADERF_ERR_* failure
 */
ssize_t bladerf_get_device_list(struct bladerf_devinfo **devices);

/**
 * Free device list returned by bladerf_get_device_list()
 *
 * @param   devices     List of available devices
 * @param   n           Number of devices in the list
 */
void bladerf_free_device_list(struct bladerf_devinfo *devices, size_t n);

/**
 * Open specified device
 *
 * @param   dev_path  Path to bladeRF device
 *
 * @return device handle or NULL on failure
 */
struct bladerf * bladerf_open(const char *dev_path);

/**
 * Close device
 *
 * @note    Failing to close a device may result in memory leaks.
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
 * @return 0 on success, BLADERF_ERR_* failure
 */
int bladerf_enable_module(struct bladerf *dev,
                            enum bladerf_module m, bool enable) ;

/**
 * Apply specified loopback mode
 *
 * @param       dev     Device handle
 * @param       l       Loopback mode. Note that LB_NONE disables the use
 *                      of loopback functionality.
 *
 * @return 0 on success, BLADERF_ERR_* failure
 */
int bladerf_set_loopback( struct bladerf *dev, enum bladerf_loopback l);


/**
 * Configure the device's sample rate, in (TODO Units?)
 *
 * TODO what's the unit here? Hz?
 *
 * @param[in]    dev        Device handle
 * @param[out]   rate       Sample rate
 *
 * @return 0 on success, BLADERF_ERR_* failure
 */
int bladerf_set_sample_rate(struct bladerf *dev, unsigned int rate);

/**
 * Set the PA gain in dB
 *
 * @param       dev         Device handle
 * @param       gain        Desired gain
 *
 * @return 0 on success, BLADERF_ERR_* failure
 */
int bladerf_set_txvga2(struct bladerf *dev, int gain);

/**
 * Set the post-LPF gain in dB
 *
 * @param       dev         Device handle
 * @param       gain        Desired gain
 *
 * @return 0 on success, BLADERF_ERR_* failure
 */
int bladerf_set_txvga1(struct bladerf *dev, int gain);

/**
 * Set the LNA gain
 *
 * @param       dev         Device handle
 * @param       gain        Desired gain level
 *
 * @return 0 on success, BLADERF_ERR_* failure
 */
int bladerf_set_lna_gain(struct bladerf *dev, enum bladerf_lna_gain gain);

/**
 * Set the pre-LPF VGA gain
 *
 * @param       dev         Device handle
 * @param       gain        Desired gain
 *
 * @return 0 on success, BLADERF_ERR_* failure
 */
int bladerf_set_rxvga1(struct bladerf *dev, int gain);

/**
 * Set the post-LPF VGA gain
 *
 * @param       dev         Device handle
 * @param       gain        Desired gain
 *
 * @return 0 on success, BLADERF_ERR_* failure
 */
int bladerf_set_rxvga2(struct bladerf *dev, int gain);

/**
 * Set the bandwidth to specified value (TODO units?)
 *
 * @param       dev                 Device handle
 * @param       bandwidth           Desired bandwidth
 * @param       bandwidth_actual    If non-NULL, written with the actual
 *                                  bandwidth that the device was able to
 *                                  achieve
 *
 * @return 0 on success, BLADERF_ERR_* failure
 */
int bladerf_set_bandwidth(struct bladerf *dev, unsigned int bandwidth,
                            unsigned int *bandwidth_actual);

/**
 * Set module's frequency (TODO units?)
 *
 * @param       dev         Device handle
 * @param       module      Module to configure
 * @param       frequency   Desired frequency
 *
 * @return 0 on success, BLADERF_ERR_* failure
 */
int bladerf_set_frequency(struct bladerf *dev,
                            enum bladerf_module module, unsigned int frequency);

/** @} (End of FN_CTRL) */


/**
 * @defgroup FN_DATA    Data transmission and reception
 *
 * @{
 */

/**
 * Send complex, packed 12-bit signed samples
 *
 * @param       dev         Device handle
 * @param       samples     Array of samples
 * @param       n           Size of samples array, in elements
 *
 * @return number of samples sent or BLADERF_ERR_* upon failure
 */
ssize_t bladerf_send_c12(struct bladerf *dev, int16_t *samples, size_t n);

/**
 * Send complex, 16-bit signed samples
 *
 * @param       dev         Device handle
 * @param       samples     Array of samples
 * @param       n           Size of samples array, in elements
 *
 * @return number of sampels sent on success, BLADERF_ERR_* failure
 */
ssize_t bladerf_send_c16(struct bladerf *dev, int16_t *samples, size_t n);

/**
 * Read 16-bit signed samples
 *
 * @param       dev         Device handle
 * @param       samples     Buffer to store samples in
 * @param       max_samples Max number of sample to read
 *
 * @return number of samples read or BLADERF_ERR_* failure
 */
ssize_t bladerf_read_c16(struct bladerf *dev,
                            int16_t *samples, size_t max_samples);

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
 * @return 0 on success, BLADERF_ERR_* failure
 */
int bladerf_get_serial(struct bladerf *dev, uint64_t *serial);

/**
 * Query firmware version
 *
 * @param[in]   dev     Device handle
 * @param[out]  major   Firmware major revision
 * @param[out]  minor   Firmware minor revision
 *
 * @return 0 on success, BLADERF_ERR_* failure
 */
int bladerf_get_fw_version(struct bladerf *dev,
                            unsigned int *major, unsigned int minor);

/**
 * Check FPGA status
 *
 * @param   dev     Device handle
 *
 * @return  1 if FPGA is programmed,
 *          0 if it is not,
 *          and BLADERF_ERR_* on failure
 */
int bladerf_is_fpga_programmed(struct bladerf *dev);


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
 *
 */
int bladerf_flash_firmware(struct bladerf *dev, const char *firmware);

/**
 * Flash firmware onto device
 *
 * @note This will require a power cycle to take effect
 *
 * @param   dev         Device handle
 * @param   firmware    Full path to firmware file
 *
 * @return 0 on success, BLADERF_ERR_* failure
 */
int bladerf_flash_firmware(struct bladerf *dev, const char *firmware);

/**
 * Load device's FPGA
 *
 * @param   dev         Device handle
 * @param   fpga        Full path to FPGA bitstream
 *
 * @return 0 on success, BLADERF_ERR_* failure
 */
int bladerf_load_fpga(struct bladerf *dev, const char *fpga);


/* @} (End of FN_PROG) */
