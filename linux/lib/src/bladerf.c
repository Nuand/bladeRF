#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#include "libbladeRF.h"     /* Public API */
#include "bladerf_priv.h"   /* Implementation-specific items ("private") */
#include "lms.h"
#include "si5338.h"
#include "file_ops.h"
#include "debug.h"
#include "backend.h"
#include "device_identifier.h"

/*------------------------------------------------------------------------------
 * Device discovery & initialization/deinitialization
 *----------------------------------------------------------------------------*/

ssize_t bladerf_get_device_list(struct bladerf_devinfo **devices)
{
    ssize_t ret;
    size_t num_devices;
    struct bladerf_devinfo *devices_local;
    int status;

    status = backend_probe(&devices_local, &num_devices);

    if (status < 0) {
        /* Note */
        ret = status;
    } else {
        assert(num_devices <= SSIZE_MAX);
        ret = (ssize_t)num_devices;
    }

    return ret;
}

void bladerf_free_device_list(struct bladerf_devinfo *devices)
{
}

struct bladerf * bladerf_open_with_devinfo(struct bladerf_devinfo *devinfo)
{
    return backend_open(devinfo);
}

/* dev path becomes device specifier string (osmosdr-like) */
struct bladerf * bladerf_open(const char *dev_id)
{
    struct bladerf_devinfo devinfo;
    struct bladerf *ret = NULL;
    int status;

    /* Populate dev-info from string */
    status = str2devinfo(dev_id, &devinfo);

    if (!status) {
        ret = bladerf_open_with_devinfo(&devinfo);
    }

    return ret;
}

void bladerf_close(struct bladerf *dev)
{
    dev->fn->close(dev);
}

int bladerf_enable_module(struct bladerf *dev,
                            bladerf_module_t m, bool enable)
{
    int status = BLADERF_ERR_UNEXPECTED;
    uint32_t gpio_reg;

    status = bladerf_gpio_read(dev, &gpio_reg);

    if (status == 0) {
        switch (m) {
            case TX:
                if (enable) {
                    gpio_reg |= BLADERF_GPIO_LMS_TX_ENABLE;
                } else {
                    gpio_reg &= ~BLADERF_GPIO_LMS_TX_ENABLE;
                }
                break;
            case RX:
                if (enable) {
                    gpio_reg |= BLADERF_GPIO_LMS_RX_ENABLE;
                } else {
                    gpio_reg &= ~BLADERF_GPIO_LMS_RX_ENABLE;
                }
                break;
        }

        status = bladerf_gpio_write(dev, gpio_reg);
    }

    return status;
}

int bladerf_set_loopback(struct bladerf *dev, bladerf_loopback_t l)
{
    lms_loopback_enable( dev, l );
    return 0;
}

int bladerf_set_sample_rate(struct bladerf *dev, bladerf_module_t module, unsigned int rate, unsigned int *actual)
{
    int ret = -1;
    /* TODO: Use module to pick the correct clock output to change */
    if( module == TX ) {
        ret = si5338_set_tx_freq(dev, rate<<1);
        dev->last_tx_sample_rate = rate;
    } else {
        ret = si5338_set_rx_freq(dev, rate<<1);
        dev->last_rx_sample_rate = rate;
    }
    *actual = rate;
    return ret;
}

int bladerf_set_rational_sample_rate(struct bladerf *dev, bladerf_module_t module, unsigned int integer, unsigned int num, unsigned int denom)
{
    /* TODO: Program the Si5338 to be 2x the desired sample rate */
    return 0;
}

int bladerf_get_sample_rate( struct bladerf *dev, bladerf_module_t module, unsigned int *rate)
{
    /* TODO Reconstruct samplerare from Si5338 readback */
    if (module == RX) {
        *rate = dev->last_rx_sample_rate;
    } else if (module == TX) {
        *rate = dev->last_tx_sample_rate;
    } else {
        return BLADERF_ERR_INVAL;
    }

    return 0;
}

int bladerf_get_rational_sample_rate(struct bladerf *dev, bladerf_module_t module, unsigned int integer, unsigned int num, unsigned int denom)
{
    /* TODO: Read the Si5338 and figure out the sample rate */
    return 0;
}

int bladerf_set_txvga2(struct bladerf *dev, int gain)
{
    if( gain > 25 ) {
        dbg_printf( "%s: %d being clamped to 25dB\n", __FUNCTION__, gain );
        gain = 25;
    }
    if( gain < 0 ) {
        dbg_printf( "%s: %d being clamped to 0dB\n", __FUNCTION__, gain );
        gain = 0;
    }
    /* TODO: Make return values for lms call and return it for failure */
    lms_txvga2_set_gain( dev, gain );
    return 0;
}

int bladerf_get_txvga2(struct bladerf *dev, int *gain)
{
    uint8_t gain_u8;
    /* TODO: Make return values for lms call and return it for failure */
    lms_txvga2_get_gain( dev, &gain_u8 );
    *gain = gain_u8;
    return 0;
}

int bladerf_set_txvga1(struct bladerf *dev, int gain)
{
    if( gain < -35 ) {
        dbg_printf( "%s: %d being clamped to -35dB\n", __FUNCTION__, gain );
        gain = -35;
    }
    if( gain > -4 ) {
        dbg_printf( "%s: %d being clamped to -4dB\n", __FUNCTION__, gain );
        gain = -4;
    }
    /* TODO: Make return values for lms call and return it for failure */
    lms_txvga1_set_gain( dev, gain );
    return 0;
}

int bladerf_get_txvga1(struct bladerf *dev, int *gain)
{
    *gain = 0;
    /* TODO: Make return values for lms call and return it for failure */
    lms_txvga1_get_gain( dev, (int8_t *)gain );
    *gain |= 0xffffff00 ;
    return 0;
}

int bladerf_set_lna_gain(struct bladerf *dev, bladerf_lna_gain_t gain)
{
    /* TODO: Make return values for lms call and return it for failure */
    lms_lna_set_gain( dev, gain );
    return 0;
}

int bladerf_get_lna_gain(struct bladerf *dev, bladerf_lna_gain_t *gain)
{
    /* TODO: Make return values for lms call and return it for failure */
    lms_lna_get_gain( dev, gain );
    return 0;
}

int bladerf_set_rxvga1(struct bladerf *dev, int gain)
{
    int r;
    /* TODO: Make return values for lms call and return it for failure */
    for (r=2; (r * r) < ((gain - 5) << 9); r++);
    lms_rxvga1_set_gain( dev, r );
    return 0;
}

int bladerf_get_rxvga1(struct bladerf *dev, int *gain)
{
    uint8_t gain_u8;
    /* TODO: Make return values for lms call and return it for failure */
    lms_rxvga1_get_gain( dev, &gain_u8 );
    *gain = 5 + (((int)gain_u8 * (int)gain_u8) >> 9);
    return 0;
}

int bladerf_set_rxvga2(struct bladerf *dev, int gain)
{
    /* TODO: Make return values for lms call and return it for failure */
    /* Raw value to write which is 3dB per unit */
    lms_rxvga2_set_gain( dev, (uint8_t)gain/3 );
    return 0;
}

int bladerf_get_rxvga2(struct bladerf *dev, int *gain)
{
    uint8_t gain_u8;
    /* TODO: Make return values for lms call and return it for failure */
    lms_rxvga2_get_gain( dev, &gain_u8 );
    /* Value returned from LMS lib is just the raw value, which is 3dB/unit */
    *gain = gain_u8 * 3;
    return 0;
}

int bladerf_set_bandwidth(struct bladerf *dev, bladerf_module_t module,
                            unsigned int bandwidth,
                            unsigned int *actual)
{
    /* TODO: Make return values for lms call and return it for failure */
    lms_bw_t bw = lms_uint2bw(bandwidth);
    lms_lpf_enable( dev, module, bw );
    *actual = lms_bw2uint(bw);
    return 0;
}

int bladerf_get_bandwidth(struct bladerf *dev, bladerf_module_t module,
                            unsigned int *bandwidth )
{
    /* TODO: Make return values for lms call and return it for failure */
    lms_bw_t bw = lms_get_bandwidth( dev, module );
    *bandwidth = lms_bw2uint(bw);
    return 0;
}

int bladerf_select_band(struct bladerf *dev, bladerf_module_t module,
                        unsigned int frequency)
{
    uint32_t gpio ;
    uint32_t band = 0;
    if( frequency >= 1500000000 ) {
        band = 1; // High Band selection
        lms_lna_select(dev, LNA_2);
    } else {
        band = 2; // Low Band selection
        lms_lna_select(dev, LNA_1);
    }
    bladerf_gpio_read(dev, &gpio);
    gpio &= ~(module == TX ? (3<<3) : (3<<5));
    gpio |= (module == TX ? (band<<3) : (band<<5));
    bladerf_gpio_write(dev, gpio);
    return 0;
}

int bladerf_set_frequency(struct bladerf *dev,
                            bladerf_module_t module, unsigned int frequency)
{
    /* TODO: Make return values for lms call and return it for failure */
    lms_set_frequency( dev, module, frequency );
    bladerf_select_band( dev, module, frequency );
    return 0;
}

int bladerf_get_frequency(struct bladerf *dev,
                            bladerf_module_t module, unsigned int *frequency)
{
    /* TODO: Make return values for lms call and return it for failure */
    struct lms_freq f;
    int rv = 0;
    lms_get_frequency( dev, module, &f );
    if( f.x == 0 ) {
        *frequency = 0 ;
        rv = BLADERF_ERR_INVAL;
    } else {
        *frequency = lms_frequency_to_hz(&f);
    }
    return rv;
}

ssize_t bladerf_tx(struct bladerf *dev, bladerf_format_t format, void *samples,
                   size_t num_samples, struct bladerf_metadata *metadata)
{
    return dev->fn->tx(dev, format, samples, num_samples, metadata);
}

ssize_t bladerf_rx(struct bladerf *dev, bladerf_format_t format, void *samples,
                   size_t num_samples, struct bladerf_metadata *metadata)
{
    return dev->fn->rx(dev, format, samples, num_samples, metadata);
}

/*------------------------------------------------------------------------------
 * Device Info
 *----------------------------------------------------------------------------*/

int bladerf_get_serial(struct bladerf *dev, uint64_t *serial)
{
    return dev->fn->get_serial(dev, serial);
}

int bladerf_get_fw_version(struct bladerf *dev,
                            unsigned int *major, unsigned int *minor)
{
    return dev->fn->get_fw_version(dev, major, minor);
}

int bladerf_is_fpga_configured(struct bladerf *dev)
{
    return dev->fn->is_fpga_configured(dev);
}

int bladerf_get_fpga_version(struct bladerf *dev,
                                unsigned int *major, unsigned int *minor)
{
    return dev->fn->get_fpga_version(dev, major, minor);
}

int bladerf_stats(struct bladerf *dev, struct bladerf_stats *stats)
{
    return dev->fn->stats(dev, stats);
}

/*------------------------------------------------------------------------------
 * Device Programming
 *----------------------------------------------------------------------------*/
int bladerf_flash_firmware(struct bladerf *dev, const char *firmware_file)
{
    int status;
    uint8_t *buf;
    size_t  buf_size;

    status = read_file(firmware_file, &buf, &buf_size);
    if (!status) {

        /* Sanity check firmware
         *
         * Quick and dirty check for any absurd sizes. This is arbitrarily
         * chosen based upon the current FX3 image size.
         *
         * TODO This should be replaced with something that also looks for:
         *  - Known header/footer on images?
         *  - Checksum/hash?
         */
        if (!getenv("BLADERF_SKIP_FW_SIZE_CHECK") &&
                (buf_size < (50 * 1024) || (buf_size > (1 * 1024 * 1024)))) {
            dbg_printf("Error: Detected potentially invalid firmware file.\n");
            dbg_printf("Define BLADERF_SKIP_FW_SIZE_CHECK in your evironment "
                       "to skip this check.\n");
            status = BLADERF_ERR_INVAL;
        } else {
            status = dev->fn->flash_firmware(dev, buf, buf_size);
        }

        free(buf);
    }

    return status;
}

int bladerf_load_fpga(struct bladerf *dev, const char *fpga_file)
{
    uint8_t *buf;
    size_t  buf_size;
    int status;
    int is_loaded;

    is_loaded = dev->fn->is_fpga_configured(dev);
    if (is_loaded > 0) {
        dbg_printf("FPGA is already loaded -- reloading.\n");
    } else if (is_loaded < 0) {
        dbg_printf("Failed to determine FPGA status. (%d) "
                "Attempting to load anyway...\n", is_loaded);
    }

    /* TODO sanity check FPGA:
     *  - Check for x40 vs x115 and verify FPGA image size
     *  - Known header/footer on images?
     *  - Checksum/hash?
     */

    status = read_file(fpga_file, &buf, &buf_size);
    if (!status) {
        status = dev->fn->load_fpga(dev, buf, buf_size);
        free(buf);
    }

    if (!status) {
        status = bladerf_init_device(dev);
    }

    return status;
}

/*------------------------------------------------------------------------------
 * Misc.
 *----------------------------------------------------------------------------*/

const char * bladerf_strerror(int error)
{
    switch (error) {
        case BLADERF_ERR_UNEXPECTED:
            return "An unexpected error occurred";
        case BLADERF_ERR_RANGE:
            return "Provided parameter was out of the allowable range";
        case BLADERF_ERR_INVAL:
            return "Invalid operation or parameter";
        case BLADERF_ERR_MEM:
            return "A memory allocation error occurred";
        case BLADERF_ERR_IO:
            return "File or device I/O failure";
        case BLADERF_ERR_TIMEOUT:
            return "Operation timed out";
        case 0:
            return "Success";
        default:
            return "Unknown error code";
    }
}

/*------------------------------------------------------------------------------
 * Si5338 register read / write functions
 *----------------------------------------------------------------------------*/

int bladerf_si5338_read(struct bladerf *dev, uint8_t address, uint8_t *val)
{
    return dev->fn->si5338_read(dev,address,val);
}

int bladerf_si5338_write(struct bladerf *dev, uint8_t address, uint8_t val)
{
    return dev->fn->si5338_write(dev,address,val);
}

/*------------------------------------------------------------------------------
 * LMS register read / write functions
 *----------------------------------------------------------------------------*/

int bladerf_lms_read(struct bladerf *dev, uint8_t address, uint8_t *val)
{
    return dev->fn->lms_read(dev,address,val);
}

int bladerf_lms_write(struct bladerf *dev, uint8_t address, uint8_t val)
{
    return dev->fn->lms_write(dev,address,val);
}

/*------------------------------------------------------------------------------
 * GPIO register read / write functions
 *----------------------------------------------------------------------------*/

int bladerf_gpio_read(struct bladerf *dev, uint32_t *val)
{
    return dev->fn->gpio_read(dev,val);
}

int bladerf_gpio_write(struct bladerf *dev, uint32_t val)
{
    return dev->fn->gpio_write(dev,val);

}

/*------------------------------------------------------------------------------
 * VCTCXO DAC register write
 *----------------------------------------------------------------------------*/

int bladerf_dac_write(struct bladerf *dev, uint16_t val)
{
    return dev->fn->dac_write(dev,val);
}
