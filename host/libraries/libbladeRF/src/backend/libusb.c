#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include "rel_assert.h"
#include <stdio.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <bladeRF.h>
#include <libusb.h>

#include "ezusb.h"
#include "bladerf_priv.h"
#include "backend/libusb.h"
#include "conversions.h"
#include "minmax.h"
#include "log.h"

#define BLADERF_LIBUSB_TIMEOUT_MS 1000
#define BULK_TIMEOUT 1000

#define EP_DIR_IN   LIBUSB_ENDPOINT_IN
#define EP_DIR_OUT  LIBUSB_ENDPOINT_OUT

#define EP_IN(x) (LIBUSB_ENDPOINT_IN | x)
#define EP_OUT(x) (x)

struct bladerf_lusb {
    libusb_device           *dev;
    libusb_device_handle    *handle;
    libusb_context          *context;
};

const struct bladerf_fn bladerf_lusb_fn;

struct lusb_stream_data {
    struct libusb_transfer **transfers;
    size_t active_transfers;

    /* libusb recommends use of libusb_handle_events_timeout_completed() for
     * multithreaded applications.
     *    http://libusb.sourceforge.net/api-1.0/mtasync.html
     *
     * In our case, "completion" is associated with a transistion to the done
     * state.
     */
    int  libusb_completed;
};

static int error_libusb2bladerf(int error)
{
    int ret;
    switch (error) {
        case 0:
            ret = 0;
            break;
        case LIBUSB_ERROR_IO:
            ret = BLADERF_ERR_IO;
            break;

        case LIBUSB_ERROR_INVALID_PARAM:
            ret = BLADERF_ERR_INVAL;
            break;

        case LIBUSB_ERROR_BUSY:
        case LIBUSB_ERROR_NO_DEVICE:
            ret = BLADERF_ERR_NODEV;
            break;

        case LIBUSB_ERROR_TIMEOUT:
            ret = BLADERF_ERR_TIMEOUT;
            break;

        case LIBUSB_ERROR_NO_MEM:
            ret = BLADERF_ERR_MEM;
            break;

        case LIBUSB_ERROR_NOT_SUPPORTED:
            ret = BLADERF_ERR_UNSUPPORTED;
            break;

        case LIBUSB_ERROR_OVERFLOW:
        case LIBUSB_ERROR_PIPE:
        case LIBUSB_ERROR_INTERRUPTED:
        case LIBUSB_ERROR_ACCESS:
        case LIBUSB_ERROR_NOT_FOUND:
        default:
            ret = BLADERF_ERR_UNEXPECTED;
    }

    return ret;
}

/* Quick wrapper for vendor commands that get/send a 32-bit integer value */
static int vendor_command_int(struct bladerf *dev,
                          uint8_t cmd, uint8_t ep_dir, int32_t *val)
{
    int32_t buf;
    int status;
    struct bladerf_lusb *lusb = dev->backend;

    if( ep_dir == EP_DIR_IN ) {
        *val = 0;
    } else {
        buf = *val;
    }
    status = libusb_control_transfer(
                lusb->handle,
                LIBUSB_RECIPIENT_INTERFACE | LIBUSB_REQUEST_TYPE_VENDOR | ep_dir,
                cmd,
                0,
                0,
                (unsigned char *)&buf,
                sizeof(buf),
                BLADERF_LIBUSB_TIMEOUT_MS
             );

    if (status < 0) {
        log_error( "status < 0: %s\n", libusb_error_name(status) );
        status = error_libusb2bladerf(status);
    } else if (status != sizeof(buf)) {
        log_error( "status != sizeof(buf): %s\n", libusb_error_name(status) );
        status = BLADERF_ERR_IO;
    } else {
        if( ep_dir == EP_DIR_IN ) {
            *val = LE32_TO_HOST(buf);
        }
        status = 0;
    }

    return status;
}

/* Quick wrapper for vendor commands that get/send a 32-bit integer value + wValue */
static int vendor_command_int_value(struct bladerf *dev,
                          uint8_t cmd, uint16_t wValue, int32_t *val)
{
    int32_t buf;
    int status;
    struct bladerf_lusb *lusb = dev->backend;

    status = libusb_control_transfer(
                lusb->handle,
                LIBUSB_RECIPIENT_INTERFACE | LIBUSB_REQUEST_TYPE_VENDOR | EP_DIR_IN,
                cmd,
                wValue,
                0,
                (unsigned char *)&buf,
                sizeof(buf),
                BLADERF_LIBUSB_TIMEOUT_MS
             );

    if (status < 0) {
        log_error( "status < 0: %s\n", libusb_error_name(status) );
        status = error_libusb2bladerf(status);
    } else if (status != sizeof(buf)) {
        log_error( "status != sizeof(buf): %s\n", libusb_error_name(status) );
        status = BLADERF_ERR_IO;
    } else {
        *val = LE32_TO_HOST(buf);
        status = 0;
    }

    return status;
}

/* Quick wrapper for vendor commands that get/send a 32-bit integer value + wIndex */
static int vendor_command_int_index(struct bladerf *dev,
                          uint8_t cmd, uint16_t wIndex, int32_t *val)
{
    int32_t buf;
    int status;
    struct bladerf_lusb *lusb = dev->backend;

    status = libusb_control_transfer(
                lusb->handle,
                LIBUSB_RECIPIENT_INTERFACE | LIBUSB_REQUEST_TYPE_VENDOR | EP_DIR_IN,
                cmd,
                0,
                wIndex,
                (unsigned char *)&buf,
                sizeof(buf),
                BLADERF_LIBUSB_TIMEOUT_MS
             );

    if (status < 0) {
        log_error( "status < 0: %s\n", libusb_error_name(status) );
        status = error_libusb2bladerf(status);
    } else if (status != sizeof(buf)) {
        log_error( "status != sizeof(buf): %s\n", libusb_error_name(status) );
        status = BLADERF_ERR_IO;
    } else {
        *val = LE32_TO_HOST(buf);
        status = 0;
    }

    return status;
}

static int begin_fpga_programming(struct bladerf *dev)
{
    int result;
    int status = vendor_command_int(dev, BLADE_USB_CMD_BEGIN_PROG, EP_DIR_IN, &result);

    if (status < 0) {
        return status;
    } else {
        return 0;
    }
}

static int end_fpga_programming(struct bladerf *dev)
{
    int result;
    int status = vendor_command_int(dev, BLADE_USB_CMD_QUERY_FPGA_STATUS, EP_DIR_IN, &result);
    if (status < 0) {
        log_error("Received response of (%d): %s\n",
                    status, libusb_error_name(status));
        return status;
    } else {
        return 0;
    }
}

static int lusb_is_fpga_configured(struct bladerf *dev)
{
    int result;
    int status = vendor_command_int(dev, BLADE_USB_CMD_QUERY_FPGA_STATUS, EP_DIR_IN, &result);

    if (status < 0) {
        return status;
    } else {
        return result;
    }
}

static int lusb_device_is_bladerf(libusb_device *dev)
{
    int err;
    int rv = 0;
    struct libusb_device_descriptor desc;

    err = libusb_get_device_descriptor(dev, &desc);
    if( err ) {
        log_error( "Couldn't open libusb device - %s\n", libusb_error_name(err) );
    } else {
        if( desc.idVendor == USB_NUAND_VENDOR_ID && desc.idProduct == USB_NUAND_BLADERF_PRODUCT_ID ) {
            rv = 1;
        }
    }
    return rv;
}

static int lusb_device_is_fx3(libusb_device *dev)
{
    int err;
    int rv = 0;
    struct libusb_device_descriptor desc;

    err = libusb_get_device_descriptor(dev, &desc);
    if( err ) {
        log_error( "Couldn't open libusb device - %s\n", libusb_error_name(err) );
    } else {
        if(
            (desc.idVendor == USB_CYPRESS_VENDOR_ID && desc.idProduct == USB_FX3_PRODUCT_ID) ||
            (desc.idVendor == USB_NUAND_VENDOR_ID && desc.idProduct == USB_NUAND_BLADERF_BOOT_PRODUCT_ID)
            ) {
            rv = 1;
        }
    }
    return rv;
}


static int lusb_get_devinfo(libusb_device *dev, struct bladerf_devinfo *info)
{
    int status = 0;
    libusb_device_handle *handle;
    struct libusb_device_descriptor desc;

    status = libusb_open( dev, &handle );
    if( status ) {
        log_error( "Couldn't populate devinfo - %s\n", libusb_error_name(status) );
    } else {
        /* Populate device info */
        info->backend = BLADERF_BACKEND_LIBUSB;
        info->usb_bus = libusb_get_bus_number(dev);
        info->usb_addr = libusb_get_device_address(dev);

        status = libusb_get_device_descriptor(dev, &desc);
        if (status) {
            memset(info->serial, 0, BLADERF_SERIAL_LENGTH);
        } else {
            status = libusb_get_string_descriptor_ascii(handle,
                                                        desc.iSerialNumber,
                                                        (unsigned char *)&info->serial,
                                                        BLADERF_SERIAL_LENGTH);

            /* Consider this to be non-fatal, otherwise firmware <= 1.1
             * wouldn't be able to get far enough to upgrade */
            if (status < 0) {
                log_error("Failed to retrieve serial number\n");
                memset(info->serial, 0, BLADERF_SERIAL_LENGTH);
            }

            /* Additinally, adjust for > 0 return code */
            status = 0;
        }

        libusb_close( handle );
    }


    return status;
}

static int change_setting(struct bladerf *dev, uint8_t setting)
{
    int status;

    struct bladerf_lusb *lusb = dev->backend ;
    if (dev->legacy  & LEGACY_ALT_SETTING) {
        log_info("Legacy change to interface %d\n", setting);

        /*
         * Workaround: We're not seeing a transfer here in Windows, possibly because
         *             something in/under libusb_set_interface_alt_setting() assumes
         *             the interface is already set to the current value and
         *             suppresses this attempt. Hence, we force this transfer.
         */
#if WIN32
        status = libusb_control_transfer(lusb->handle,  LIBUSB_RECIPIENT_DEVICE | LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_ENDPOINT_OUT, 11, 0, setting, 0, 0, 0);
        if (status > 0) {
            status = 0;
        }
#else
        status = libusb_set_interface_alt_setting(lusb->handle, setting, 0);
#endif
        return status;
    } else {
        log_info( "Change to alternate interface %d\n", setting);
        return libusb_set_interface_alt_setting(lusb->handle, 0, setting);
    }
}

static int enable_rf(struct bladerf *dev) {
    int status;
    int32_t fx3_ret = -1;
    int ret_status = 0;
    uint16_t val;

    status = change_setting(dev, USB_IF_RF_LINK);
    if(status < 0) {
        status = error_libusb2bladerf(status);
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
        log_error( "alt_setting issue: %s\n", libusb_error_name(status) );
        return status;
    }
    log_info( "Changed into RF link mode: %s\n", libusb_error_name(status) ) ;
    val = 1;

    if (dev->legacy) {
        int32_t val32 = val;
        status = vendor_command_int(dev, BLADE_USB_CMD_RF_RX, EP_DIR_OUT, &val32);
        fx3_ret = 0;
    } else {
        status = vendor_command_int_value(
                dev, BLADE_USB_CMD_RF_RX,
                val, &fx3_ret);
    }
    if(status) {
        log_warning("Could not enable RF RX (%d): %s\n",
                status, libusb_error_name(status) );
        ret_status = BLADERF_ERR_UNEXPECTED;
    } else if(fx3_ret) {
        log_warning("Error enabling RF RX (%d)\n",
                fx3_ret );
        ret_status = BLADERF_ERR_UNEXPECTED;
    }

    if (dev->legacy) {
        int32_t val32 = val;
        status = vendor_command_int(dev, BLADE_USB_CMD_RF_TX, EP_DIR_OUT, &val32);
        fx3_ret = 0;
    } else {
        status = vendor_command_int_value(
                dev, BLADE_USB_CMD_RF_TX,
                val, &fx3_ret);
    }
    if(status) {
        log_warning("Could not enable RF TX (%d): %s\n",
                status, libusb_error_name(status) );
        ret_status = BLADERF_ERR_UNEXPECTED;
    } else if(fx3_ret) {
        log_warning("Error enabling RF TX (%d)\n",
                fx3_ret );
        ret_status = BLADERF_ERR_UNEXPECTED;
    }

    return ret_status;
}

static int lusb_open(struct bladerf **device, struct bladerf_devinfo *info)
{
    int status, i, n, inf;
    int fx3_status;
    ssize_t count;
    struct bladerf *dev = NULL;
    struct bladerf_lusb *lusb = NULL;
    libusb_device **list;
    struct bladerf_devinfo thisinfo;
    const struct libusb_version *version;

    libusb_context *context;

    *device = NULL;

    /* Initialize libusb for device tree walking */
    status = libusb_init(&context);
    if( status ) {
        log_error( "Could not initialize libusb: %s\n", libusb_error_name(status) );
        status = error_libusb2bladerf(status);
        goto lusb_open_done;
    }

    version = libusb_get_version();
    log_info( "Using libusb version %d.%d.%d.%d\n", version->major, version->minor, version->micro, version->nano );

    count = libusb_get_device_list( context, &list );
    /* Iterate through all the USB devices */
    for( i = 0, n = 0; i < count; i++ ) {
        if( lusb_device_is_bladerf(list[i]) ) {
            log_info( "Found a bladeRF\n" ) ;

            /* Open the USB device and get some information
             *
             * FIXME in order for the bladerf_devinfo_matches() to work, this
             *       routine has to be able to get the serial #.
             */
            status = lusb_get_devinfo( list[i], &thisinfo );
            if(status < 0) {
                log_error( "Could not open bladeRF device: %s\n", libusb_error_name(status) );
                status = error_libusb2bladerf(status);
                goto lusb_open__err_context;
            }
            thisinfo.instance = n++;

            /* Check to see if this matches the info stuct */
            if( bladerf_devinfo_matches( &thisinfo, info ) ) {

                /* Allocate backend structure and populate*/
                dev = (struct bladerf *)calloc(1, sizeof(struct bladerf));
                if (!dev) {
                    log_error("Skipping instance %d due to failed allocation: "
                              "device handle.\n", thisinfo.instance);
                    continue;
                }

                dev->fpga_version_str = calloc(1, BLADERF_VERSION_STR_MAX + 1);
                if (!dev->fpga_version_str) {
                    log_error("Skipping instance %d due to failed allocation: "
                              "FPGA version string.\n", thisinfo.instance);
                    free(dev);
                    dev = NULL;
                    continue;
                }

                dev->fw_version_str = calloc(1, BLADERF_VERSION_STR_MAX + 1);
                if (!dev->fw_version_str) {
                    log_error("Skipping instance %d due to failed allocation\n",
                              thisinfo.instance);
                    free(dev->fpga_version_str);
                    free(dev);
                    dev = NULL;
                    continue;
                }

                lusb = (struct bladerf_lusb *)malloc(sizeof(struct bladerf_lusb));
                if (!lusb) {
                    free(dev->fw_version_str);
                    free(dev->fpga_version_str);
                    free(dev);
                    lusb = NULL;
                    dev = NULL;
                    continue;
                }

                /* Assign libusb function table, backend type and backend */
                dev->fn = &bladerf_lusb_fn;
                dev->backend = (void *)lusb;
                dev->legacy = 0;

                memcpy(&dev->ident, &thisinfo, sizeof(struct bladerf_devinfo));

                /* Populate the backend information */
                lusb->context = context;
                lusb->dev = list[i];
                lusb->handle = NULL;

                status = libusb_open(list[i], &lusb->handle);
                if(status < 0) {
                    log_error( "Could not open bladeRF device: %s\n", libusb_error_name(status) );
                    status = error_libusb2bladerf(status);
                    goto lusb_open__err_device_list;
                }

                status = bladerf_fw_version(dev, &dev->fw_version);
                if (status < 0) {
                    goto lusb_open__err_device_list;
                }

                if (dev->fw_version.major < FW_LEGACY_ALT_SETTING_MAJOR ||
                        (dev->fw_version.major == FW_LEGACY_ALT_SETTING_MAJOR &&
                         dev->fw_version.minor < FW_LEGACY_ALT_SETTING_MINOR)) {
                    dev->legacy = LEGACY_ALT_SETTING;
                }

                if (dev->fw_version.major < FW_LEGACY_CONFIG_IF_MAJOR ||
                        (dev->fw_version.major == FW_LEGACY_CONFIG_IF_MAJOR && dev->fw_version.minor < FW_LEGACY_CONFIG_IF_MINOR)) {
                    dev->legacy |= LEGACY_CONFIG_IF;
                }

                /* Claim interfaces */
                for( inf = 0; inf < ((dev->legacy & LEGACY_ALT_SETTING) ? 3 : 1) ; inf++ ) {
                    status = libusb_claim_interface(lusb->handle, inf);
                    if(status < 0) {
                        log_error( "Could not claim interface %i - %s\n", inf, libusb_error_name(status) );
                        status = error_libusb2bladerf(status);
                        goto lusb_open__err_device_list;
                    }
                }

                log_info( "Claimed all inferfaces successfully\n" );
                break;
            }
        }

        if( lusb_device_is_fx3(list[i]) ) {
            fx3_status = lusb_get_devinfo( list[i], &thisinfo );
            if( fx3_status ) {
                log_error( "Could not open FX3 bootloader device: %s\n", libusb_error_name(fx3_status) );
                continue;
            }

            log_warning( "Found FX3 bootloader device libusb:device=%d:%d, could be bladeRF.\n",
                thisinfo.usb_bus, thisinfo.usb_addr);
            log_warning( "Use \"recover libusb:device=%d:%d <FX3 firmware>\" to boot bladeRF.\n",
                thisinfo.usb_bus, thisinfo.usb_addr);
        }
    }

    if (dev) {
        if (lusb_is_fpga_configured(dev)) {
            enable_rf(dev);
        }
    }

/* XXX I'd prefer if we made a call here to lusb_close(), but that would result
 *     in an attempt to release interfaces we haven't claimed... thoughts? */
lusb_open__err_device_list:
    libusb_free_device_list( list, 1 );
    if (status != 0 && lusb != NULL) {
        if (lusb->handle) {
            libusb_close(lusb->handle);
        }

        free(lusb);
        free(dev);
        lusb = NULL;
        dev = NULL;
    }

lusb_open__err_context:
    if( dev == NULL ) {
        libusb_exit(context);
    }

lusb_open_done:
    if (!status) {

        if (dev) {
            *device = dev;
        } else {
            log_error("No devices available on the libusb backend.\n");
            status = BLADERF_ERR_NODEV;
        }
    }

    return status;
}

static int lusb_close(struct bladerf *dev)
{
    int status = 0;
    int inf = 0;
    struct bladerf_lusb *lusb = dev->backend;

    for( inf = 0; inf < ((dev->legacy & LEGACY_ALT_SETTING) ? 3 : 1) ; inf++ ) {
        status = libusb_release_interface(lusb->handle, inf);
        if (status < 0) {
            log_error("error releasing interface %i\n", inf);
            status = error_libusb2bladerf(status);
        }
    }

    libusb_close(lusb->handle);
    libusb_exit(lusb->context);
    free(dev->backend);
    free(dev->fpga_version_str);
    free(dev->fw_version_str);
    free(dev);

    return status;
}

static int lusb_load_fpga(struct bladerf *dev, uint8_t *image, size_t image_size)
{
    unsigned int wait_count;
    int status = 0;
    int transferred = 0;
    struct bladerf_lusb *lusb = dev->backend;

    /* Make sure we are using the configuration interface */
    if (dev->legacy & LEGACY_CONFIG_IF) {
        status = change_setting(dev, USB_IF_LEGACY_CONFIG);
    } else {
        status = change_setting(dev, USB_IF_CONFIG);
    }

    if(status < 0) {
        status = error_libusb2bladerf(status);
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
        log_error( "alt_setting issue: %s\n", libusb_error_name(status) );
        return status;;
    }

    /* Begin programming */
    status = begin_fpga_programming(dev);
    if (status < 0) {
        status = error_libusb2bladerf(status);
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
        return status;
    }

    /* Send the file down */
    assert(image_size <= INT_MAX);
    status = libusb_bulk_transfer(lusb->handle, 0x2, image, (int)image_size,
                                  &transferred, 5 * BLADERF_LIBUSB_TIMEOUT_MS);
    if (status < 0) {
        status = error_libusb2bladerf(status);
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
        return status;
    }

    /* End programming */
    status = end_fpga_programming(dev);
    if (status) {
        status = error_libusb2bladerf(status);
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
        return status;
    }

    /* Poll FPGA status to determine if programming was a success */
    wait_count = 10;
    status = 0;

    while (wait_count > 0 && status == 0) {
        status = lusb_is_fpga_configured(dev);
        if (status == 1) {
            break;
        }

        usleep(200000);
        wait_count--;
    }

    /* Failed to determine if FPGA is loaded */
    if (status < 0) {
        status = error_libusb2bladerf(status);
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
        return status;
    } else if (wait_count == 0 && !status) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, BLADERF_ERR_TIMEOUT);
        return BLADERF_ERR_TIMEOUT;
    }

    status = enable_rf(dev);

    return status;
}

/* Note: n_bytes is rounded up to a multiple of the sector size here */
static int lusb_erase_flash(struct bladerf *dev, int sector_offset, int n_bytes)
{
    int status = 0;
    int sector_to_erase;
    int erase_ret;
    const int n_sectors = FLASH_BYTES_TO_SECTORS(n_bytes);
    struct bladerf_lusb *lusb = dev->backend;

    assert(sector_offset < FLASH_NUM_SECTORS);
    assert((sector_offset + n_sectors) < FLASH_NUM_SECTORS);

    status = change_setting(dev, USB_IF_SPI_FLASH);

    if (status) {
        log_error("Failed to set interface: %s\n", libusb_error_name(status));
        status = BLADERF_ERR_IO;
    }

    log_info("Erasing %d sectors starting @ sector %d\n",
                n_sectors, sector_offset);

    for (sector_to_erase = sector_offset; sector_to_erase < (sector_offset + n_sectors) && !status; sector_to_erase++) {
        status = libusb_control_transfer(
                                lusb->handle,
                                LIBUSB_RECIPIENT_INTERFACE |
                                    LIBUSB_REQUEST_TYPE_VENDOR |
                                    EP_DIR_IN,
                                BLADE_USB_CMD_FLASH_ERASE,
                                0,
                                sector_to_erase,
                                (unsigned char *)&erase_ret,
                                sizeof(erase_ret),
                                BLADERF_LIBUSB_TIMEOUT_MS);

        if (status != sizeof(erase_ret) || ((!(dev->legacy & LEGACY_CONFIG_IF)) &&  erase_ret != 0)) {
            log_error("Failed to erase sector %d\n", sector_to_erase);
            if (status < 0) {
                log_error("libusb status: %s\n", libusb_error_name(status));
            } else if (erase_ret != 0) {
                log_error("Received erase failure status from FX3. error = %d\n",
                        erase_ret);
            } else {
                log_error("Unexpected read size: %d\n", status);
            }
            status = BLADERF_ERR_IO;
        } else {
            log_info("Erased sector %d...\n", sector_to_erase);
            status = 0;
        }
    }

    if (status == 0) {
        return n_sectors;
    } else {
        return status;
    }
}

static int read_buffer(struct bladerf *dev, int request,
        int read_size, size_t total_size, uint8_t *ptr)
{
    struct bladerf_lusb *lusb = dev->backend;

    size_t buf_off;
    int status;

    assert(total_size <= UINT16_MAX);

    for(buf_off = 0; buf_off < total_size; buf_off += read_size)
    {
        status = libusb_control_transfer(
                    lusb->handle,
                    LIBUSB_RECIPIENT_INTERFACE |
                        LIBUSB_REQUEST_TYPE_VENDOR |
                        EP_DIR_IN,
                    request,
                    0,
                    (uint16_t)buf_off,
                    &ptr[buf_off],
                    read_size,
                    BLADERF_LIBUSB_TIMEOUT_MS);

        if(status < 0) {
            log_error("Failed to read page buffer at offset 0x%02x: %s\n",
                    buf_off, libusb_error_name(status));
            return error_libusb2bladerf(status);
        } else if(status != read_size) {
            log_error("Got unexpected read size when writing page buffer at offset 0x%02x: "
                    "expected %d, got %d\n",
                    read_size, status);
            return BLADERF_ERR_IO;
        }
    }

    return 0;
}

static int read_page_buffer(struct bladerf *dev, int read_size, uint8_t *ptr)
{
    return read_buffer(dev, BLADE_USB_CMD_READ_PAGE_BUFFER,
            read_size, FLASH_PAGE_SIZE, ptr);
}

static int legacy_lusb_read_flash(struct bladerf *dev, int page_offset,
                        uint8_t *ptr, size_t n_bytes)
{
    int status = 0;
    size_t page_i;
    int n_read, total_read;
    int read_size = dev->speed ? FLASH_PAGE_SIZE: 64;
    size_t pages_to_read = FLASH_BYTES_TO_PAGES(n_bytes);
    struct bladerf_lusb *lusb = dev->backend;

    assert(page_offset < FLASH_NUM_PAGES);
    assert((page_offset + n_bytes) < FLASH_NUM_PAGES);
    /* FIXME: support data_size that are not multiple of pages */
    assert(n_bytes % FLASH_PAGE_SIZE == 0);

    total_read = 0;

    for (page_i = page_offset;
         page_i < (page_offset + pages_to_read) && !status;
         page_i++) {

        /* Read back a page */
        n_read = 0;
        do {
            status = libusb_control_transfer(
                        lusb->handle,
                        LIBUSB_RECIPIENT_INTERFACE |
                            LIBUSB_REQUEST_TYPE_VENDOR |
                            EP_DIR_IN,
                        BLADE_USB_CMD_FLASH_READ,
                        0,
                        (uint16_t)page_i,
                        ptr,
                        read_size,
                        BLADERF_LIBUSB_TIMEOUT_MS);


            if (status != read_size) {
                if (status < 0) {
                    log_error("Failed to read back page %d: %s\n", page_i,
                               libusb_error_name(status));
                } else {
                    log_error("Unexpected read size: %d\n", status);
                }

                status = BLADERF_ERR_IO;
            } else {
                n_read += read_size;
                total_read += read_size;
                ptr += read_size;
                status = 0;
            }
        } while (n_read < FLASH_PAGE_SIZE && !status);
    }

    if (status == 0) {
        return total_read;
    } else {
        return status;
    }
}

static int read_one_page(struct bladerf *dev, int read_size, uint16_t page_offset, uint8_t *ptr)
{
    int32_t read_status = -1;
    int status;

    if (dev->legacy & LEGACY_CONFIG_IF)
        return (legacy_lusb_read_flash(dev, page_offset, ptr, FLASH_PAGE_SIZE) != FLASH_PAGE_SIZE);

    status = vendor_command_int_index(
            dev, BLADE_USB_CMD_FLASH_READ,
            page_offset, &read_status);
    if(status != LIBUSB_SUCCESS) {
        return status;
    }

    if(read_status != 0) {
        log_error("Failed to read page %d: %d\n",
                page_offset, read_status);
        status = BLADERF_ERR_UNEXPECTED;
    }

    return read_page_buffer(dev, read_size, ptr);
}

static int lusb_read_flash(struct bladerf *dev, int page_offset,
                        uint8_t *ptr, size_t n_bytes)
{
    int status = 0;
    uint16_t page_i;
    int total_read;
    int read_size = dev->speed ? FLASH_PAGE_SIZE : 64;
    size_t pages_to_read = FLASH_BYTES_TO_PAGES(n_bytes);

    if (dev->legacy & LEGACY_CONFIG_IF)
        return legacy_lusb_read_flash(dev, page_offset, ptr, n_bytes);

    status = change_setting(dev, USB_IF_SPI_FLASH);

    if (status) {
        log_error("Failed to set interface: %s\n", libusb_error_name(status));
        status = BLADERF_ERR_IO;
    }

    assert(page_offset < FLASH_NUM_PAGES);
    assert((page_offset + n_bytes) < FLASH_NUM_PAGES);
    assert(n_bytes % FLASH_PAGE_SIZE == 0);

    assert(page_offset + pages_to_read <= UINT16_MAX);

    total_read = 0;

    for (page_i = page_offset;
         page_i < (page_offset + pages_to_read) && !status;
         page_i++) {

        /* Read back a page */
        status = read_one_page(dev, read_size, page_i, ptr);
        ptr += FLASH_PAGE_SIZE;
        total_read += FLASH_PAGE_SIZE;
    }

    if (status == 0) {
        return total_read;
    } else {
        return status;
    }
}

static int verify_flash(struct bladerf *dev, int page_offset,
                        uint8_t *image, size_t n_bytes)
{
    int status = 0;
    uint16_t page_i;
    int check_i;
    int read_size = dev->speed ? FLASH_PAGE_SIZE: 64;
    size_t pages_to_read = FLASH_BYTES_TO_PAGES(n_bytes);

    uint8_t page_buf[FLASH_PAGE_SIZE];
    uint8_t *image_page;

    log_info("Verifying with read size = %d\n", read_size);

    assert(page_offset < FLASH_NUM_PAGES);
    assert((page_offset + pages_to_read) < FLASH_NUM_PAGES);

    assert(page_offset + pages_to_read <= UINT16_MAX);

    for (page_i = page_offset;
         page_i < (page_offset + pages_to_read) && !status;
         page_i++) {

        /* Read back a page */
        status = read_one_page(dev, read_size, page_i, page_buf);

        /* Verify the page */
        for (check_i = 0; check_i < FLASH_PAGE_SIZE && !status; check_i++) {
            image_page = image + (page_i - page_offset) * FLASH_PAGE_SIZE + check_i;
            if (page_buf[check_i] != *image_page) {
                fprintf(stderr,
                        "Error: bladeRF firmware verification failed at byte %d"
                        " Read 0x%02X, expected 0x%02X\n",
                        page_i * FLASH_PAGE_SIZE + check_i,
                        page_buf[check_i],
                        *image_page);

                status = BLADERF_ERR_IO;
            }
        }
    }

    return status;
}

static int write_one_page(struct bladerf *dev, int write_size, uint16_t page_offset, uint8_t *data)
{
    struct bladerf_lusb *lusb = dev->backend;
    uint16_t buf_off;
    int32_t write_status = -1;
    int status;

    assert(write_size <= UINT16_MAX);

    for(buf_off = 0; buf_off < FLASH_PAGE_SIZE; buf_off += (uint16_t)write_size)
    {
        status = libusb_control_transfer(
                    lusb->handle,
                    LIBUSB_RECIPIENT_INTERFACE |
                        LIBUSB_REQUEST_TYPE_VENDOR |
                        EP_DIR_OUT,
                    BLADE_USB_CMD_WRITE_PAGE_BUFFER,
                    0,
                    buf_off,
                    &data[buf_off],
                    write_size,
                    BLADERF_LIBUSB_TIMEOUT_MS);

        if(status < 0) {
            log_error("Failed to write page buffer at offset 0x%02x for page %d: %s\n",
                    buf_off, page_offset, libusb_error_name(status));
            return error_libusb2bladerf(status);
        } else if(status != write_size) {
            log_error("Got unexpected write size when writing page buffer at offset 0x%02x: "
                    "expected %d, got %d\n",
                    write_size, status);
            return BLADERF_ERR_IO;
        }
    }

    status = vendor_command_int_index(
            dev, BLADE_USB_CMD_FLASH_WRITE,
            page_offset, &write_status);
    if(status != LIBUSB_SUCCESS) {
        return status;
    }

    if(write_status != 0) {
        log_error("Failed to write page %d: %d\n",
                page_offset, write_status);
        status = BLADERF_ERR_UNEXPECTED;
    }

    return 0;
}

static int legacy_lusb_write_flash(struct bladerf *dev, int page_offset,
                        uint8_t *data, size_t data_size)
{
    int status = 0;
    size_t i;
    int n_write, total_written;
    int write_size = dev->speed ? FLASH_PAGE_SIZE : 64;
    size_t pages_to_write = FLASH_BYTES_TO_PAGES(data_size);
    struct bladerf_lusb *lusb = dev->backend;
    uint8_t *data_page;

    log_info("Flashing with write size = %d\n", write_size);
    status = change_setting(dev, USB_IF_SPI_FLASH);

    if (status) {
        log_error("Failed to set interface: %s\n", libusb_error_name(status));
        status = BLADERF_ERR_IO;
    }

    log_info("Flashing with write size = %d\n", write_size);

    assert(page_offset < FLASH_NUM_PAGES);
    assert((page_offset + pages_to_write) < FLASH_NUM_PAGES);
    /* FIXME: support data_size that are not multiple of pages */
    assert(data_size % FLASH_PAGE_SIZE == 0);

    total_written = 0;

    for (i = page_offset; i < (page_offset + pages_to_write) && !status; i++) {
        n_write = 0;
        do {
            data_page = data + (i - page_offset) * FLASH_PAGE_SIZE + n_write;
            status = libusb_control_transfer(
                        lusb->handle,
                        LIBUSB_RECIPIENT_INTERFACE |
                            LIBUSB_REQUEST_TYPE_VENDOR |
                            EP_DIR_OUT,
                        BLADE_USB_CMD_FLASH_WRITE,
                        0,
                        (uint16_t)i,
                        (unsigned char *)data_page,
                        write_size,
                        BLADERF_LIBUSB_TIMEOUT_MS);

            if (status != write_size) {
                if (status < 0) {
                    log_error("Failed to write page %d: %s\n", i,
                            libusb_error_name(status));
                } else {
                    log_error("Got unexpected write size: %d\n", status);
                }
                status = BLADERF_ERR_IO;
            } else {
                n_write += write_size;
                total_written += write_size;
                status = 0;
            }
        } while (n_write < FLASH_PAGE_SIZE && !status);
    }

    if (status == 0) {
        return total_written;
    } else {
        return status;
    }
}

static int lusb_write_flash(struct bladerf *dev, int page_offset,
                        uint8_t *data, size_t data_size)
{
    int status = 0;
    uint16_t i;
    int total_written;
    size_t pages_to_write = FLASH_BYTES_TO_PAGES(data_size);
    int write_size = dev->speed ? FLASH_PAGE_SIZE : 64;

    if (dev->legacy & LEGACY_CONFIG_IF)
        return legacy_lusb_write_flash(dev, page_offset, data, data_size);

    status = change_setting(dev, USB_IF_SPI_FLASH);

    if (status) {
        log_error("Failed to set interface: %s\n", libusb_error_name(status));
        status = BLADERF_ERR_IO;
    }

    log_info("Flashing with write size = %d\n", write_size);

    assert(page_offset < FLASH_NUM_PAGES);
    assert((page_offset + pages_to_write) < FLASH_NUM_PAGES);
    assert(data_size % FLASH_PAGE_SIZE == 0);
    assert(page_offset + pages_to_write <= UINT16_MAX);

    total_written = 0;

    for (i = page_offset; i < (page_offset + pages_to_write) && !status; i++) {
        status = write_one_page(dev, write_size, i, data);
        total_written += FLASH_PAGE_SIZE;
        data += FLASH_PAGE_SIZE;
    }

    if (status == 0) {
        return total_written;
    } else {
        return status;
    }
}

static int find_fx3_via_info(
        libusb_context * context,
        struct bladerf_devinfo *info,
        libusb_device_handle **handle) {
    int status, i;
    struct bladerf_devinfo thisinfo;
    libusb_device *dev, **devs;
    libusb_device *found_dev = NULL;
    ssize_t status_sz;

    status_sz = libusb_get_device_list(context, &devs);
    if (status_sz < 0) {
        log_error("libusb_get_device_list() failed: %d %s\n", status_sz, libusb_error_name((int)status_sz));
        return error_libusb2bladerf((int)status_sz);
    }

    for (i=0; (dev=devs[i]) != NULL; i++) {
        if (!lusb_device_is_fx3(dev)) {
            continue;
        }

        status = lusb_get_devinfo(dev, &thisinfo);
        if (status < 0) {
            log_error( "Could not open bladeRF device: %s\n", libusb_error_name(status) );
            status = error_libusb2bladerf(status);
            break;
        }

        if (bladerf_devinfo_matches(&thisinfo, info)) {
            found_dev = dev;
            break;
        }
    }

    if (found_dev == NULL) {
        libusb_free_device_list(devs, 1);
        log_error("could not find a known device - try specifing bus, dev\n");
        return BLADERF_ERR_NODEV;
    }

    status = libusb_open(found_dev, handle);
    libusb_free_device_list(devs, 1);
    if (status != 0) {
        log_error("Error opening device: %s\n", libusb_error_name(status));
        return error_libusb2bladerf(status);
    }

    return 0;
}

static int lusb_recover(
        struct bladerf_devinfo *devinfo,
        const char *fname
        )
{
    int status;
    libusb_device_handle *device = NULL;

    libusb_context *context;

    status = libusb_init(&context);
    if (status != 0) {
        log_error( "Could not initialize libusb: %s\n", libusb_error_name(status) );
        return error_libusb2bladerf(status);
    }

    status = find_fx3_via_info(context, devinfo, &device);
    if (status == 0) {
        log_info("Attempting load with file %s\n", fname);
        status = ezusb_load_ram(device, fname, FX_TYPE_FX3, IMG_TYPE_IMG, 0);

        libusb_close(device);
    } else {
        log_error("Failed to locate FX3 bootloader: %s\n", bladerf_strerror(status) );
    }

    libusb_exit(context);

    return status;
}

static int lusb_flash_firmware(struct bladerf *dev,
                               uint8_t *image, size_t image_size)
{
    int status;

    assert(image_size <= INT_MAX);

    status = lusb_erase_flash(dev, 0, (int)image_size);

    if (status >= 0) {
        status = lusb_write_flash(dev, 0, image, (int)image_size);
    }

    if (status >= 0) {
        status = verify_flash(dev, 0, image, (int)image_size);
    }

    /* A reset will be required at this point, so there's no sense in
     * bothering to set the interface back to USB_IF_RF_LINK */

    return status;
}

static int lusb_device_reset(struct bladerf *dev)
{
    struct bladerf_lusb *lusb = dev->backend;
    int status;
    status = libusb_control_transfer(
                lusb->handle,
                LIBUSB_RECIPIENT_INTERFACE |
                    LIBUSB_REQUEST_TYPE_VENDOR |
                    EP_DIR_OUT,
                BLADE_USB_CMD_RESET,
                0,
                0,
                0,
                0,
                BLADERF_LIBUSB_TIMEOUT_MS
             );

    if(status != LIBUSB_SUCCESS) {
        log_error("Error issuing reset: %s\n", libusb_error_name(status));
        return error_libusb2bladerf(status);
    } else {
        return status;
    }
}

static int lusb_jump_to_bootloader(struct bladerf *dev)
{
    struct bladerf_lusb *lusb = dev->backend;
    int status;
    status = libusb_control_transfer(
                lusb->handle,
                LIBUSB_RECIPIENT_INTERFACE |
                    LIBUSB_REQUEST_TYPE_VENDOR |
                    EP_DIR_OUT,
                BLADE_USB_CMD_JUMP_TO_BOOTLOADER,
                0,
                0,
                0,
                0,
                BLADERF_LIBUSB_TIMEOUT_MS
             );

    if (status < 0) {
        log_error( "Error jumping to bootloader: %s\n", libusb_error_name(status) );
        status = error_libusb2bladerf(status);
    }

    return status;
}

static int lusb_get_otp(struct bladerf *dev, char *otp)
{
    int status;
    int read_size = dev->speed ? 256 : 64;
    int otp_page = 0;
    int32_t read_status = -1;

    status = change_setting(dev, USB_IF_SPI_FLASH);
    if (status) {
        log_error("Failed to set interface: %s\n", libusb_error_name(status));
        return BLADERF_ERR_IO;
    }

    status = vendor_command_int_index(
            dev, BLADE_USB_CMD_READ_OTP,
            otp_page, &read_status);
    if(status != LIBUSB_SUCCESS) {
        return status;
    }

    if(read_status != 0) {
        log_error("Failed to read OTP page %d: %d\n",
                otp_page, read_status);
        return BLADERF_ERR_UNEXPECTED;
    }

    return read_page_buffer(dev, read_size, (uint8_t*)otp);
}

static int lusb_get_cal(struct bladerf *dev, char *cal) {
    int read_size = dev->speed ? 256 : 64;
    return read_buffer(dev, BLADE_USB_CMD_READ_CAL_CACHE,
            read_size, CAL_BUFFER_SIZE, (uint8_t*)cal);
}

/* FW < 1.5.0  does not have version strings */
static int lusb_fw_version_legacy(struct bladerf *dev,
                                    struct bladerf_version *version)
{
    int status;

    struct bladerf_fx3_version fw_ver;
    struct bladerf_lusb *lusb = dev->backend;

    status = libusb_control_transfer(
                lusb->handle,
                LIBUSB_RECIPIENT_INTERFACE |
                    LIBUSB_REQUEST_TYPE_VENDOR |
                    EP_DIR_IN,
                BLADE_USB_CMD_QUERY_VERSION,
                0,
                0,
                (unsigned char *)&fw_ver,
                sizeof(fw_ver),
                BLADERF_LIBUSB_TIMEOUT_MS
             );

    if (status < 0) {
        status = BLADERF_ERR_IO;
    } else {
        version->major = LE16_TO_HOST(fw_ver.major);
        version->minor = LE16_TO_HOST(fw_ver.minor);
        version->patch = 0;
        version->describe = dev->fw_version_str;
        snprintf(dev->fw_version_str, BLADERF_VERSION_STR_MAX, "%d.%d.%d",
                 version->major, version->minor, version->patch);
        status = 0;
    }

    return status;
}

static int lusb_fw_version(struct bladerf *dev,
                           struct bladerf_version *version)
{
    int status;
    struct bladerf_lusb *lusb = dev->backend;

    status = libusb_get_string_descriptor_ascii(lusb->handle,
                                                BLADE_USB_STR_INDEX_FW_VER,
                                                (unsigned char *)dev->fw_version_str,
                                                BLADERF_VERSION_STR_MAX);

    /* If we ran into an issue, we're likely dealing with an older firmware.
     * Fall back to the legacy version*/
    if (status < 0) {
        status = lusb_fw_version_legacy(dev, version);
    } else {
        status = str2version(dev->fw_version_str, version);
        if (status != 0) {
            status = BLADERF_ERR_UNEXPECTED;
        }
    }

    return status;
}

static int lusb_fpga_version(struct bladerf *dev,
                             struct bladerf_version *version)
{
    log_debug("FPGA currently does not have a version number.\n");
    version->major = 0;
    version->minor = 0;
    version->patch = 0;
    version->describe = dev->fpga_version_str;
    return 0;
}

static int lusb_get_device_speed(struct bladerf *dev, int *device_speed)
{
    int speed;
    int status = 0;
    struct bladerf_lusb *lusb = dev->backend;

    speed = libusb_get_device_speed(lusb->dev);
    if (speed == LIBUSB_SPEED_SUPER) {
        *device_speed = 1;
    } else if (speed == LIBUSB_SPEED_HIGH) {
        *device_speed = 0;
    } else {
        /* FIXME - We should have a better error code...
         * BLADERF_ERR_UNSUPPORTED? */
        log_error("Got unsupported or unknown device speed: %d\n", speed);
        status = BLADERF_ERR_INVAL;
    }

    return status;
}

/* Returns BLADERF_ERR_* on failure */
static int access_peripheral(struct bladerf_lusb *lusb, int per, int dir,
                                struct uart_cmd *cmd)
{
    uint8_t buf[16] = { 0 };    /* Zeroing out to avoid some valgrind noise
                                 * on the reserved items that aren't currently
                                 * used (i.e., bytes 4-15 */

    int status, libusb_status, transferred;

    /* Populate the buffer for transfer */
    buf[0] = UART_PKT_MAGIC;
    buf[1] = dir | per | 0x01;
    buf[2] = cmd->addr;
    buf[3] = cmd->data;

    /* Write down the command */
    libusb_status = libusb_bulk_transfer(lusb->handle, 0x02, buf, 16,
                                           &transferred,
                                           BLADERF_LIBUSB_TIMEOUT_MS);

    if (libusb_status < 0) {
        log_error("could not access peripheral\n");
        return BLADERF_ERR_IO;
    }

    /* If it's a read, we'll want to read back the result */
    transferred = 0;
    libusb_status = status =  0;
    while (libusb_status == 0 && transferred != 16) {
        libusb_status = libusb_bulk_transfer(lusb->handle, 0x82, buf, 16,
                                             &transferred,
                                             BLADERF_LIBUSB_TIMEOUT_MS);
    }

    if (libusb_status < 0) {
        return BLADERF_ERR_IO;
    }

    /* Save off the result if it was a read */
    if (dir == UART_PKT_MODE_DIR_READ) {
        cmd->data = buf[3];
    }

    return status;
}

static int lusb_config_gpio_write(struct bladerf *dev, uint32_t val)
{
    int i = 0;
    int status = 0;
    struct uart_cmd cmd;
    struct bladerf_lusb *lusb = dev->backend;

    for (i = 0; status == 0 && i < 4; i++) {
        cmd.addr = i;
        cmd.data = (val>>(8*i))&0xff;
        status = access_peripheral(
                                    lusb,
                                    UART_PKT_DEV_GPIO,
                                    UART_PKT_MODE_DIR_WRITE,
                                    &cmd
                                  );

        if (status < 0) {
            break;
        }
    }

    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
    }

    return status;
}

static int lusb_config_gpio_read(struct bladerf *dev, uint32_t *val)
{
    int i = 0;
    int status = 0;
    struct uart_cmd cmd;
    struct bladerf_lusb *lusb = dev->backend;

    *val = 0;
    for(i = 0; status == 0 && i < 4; i++) {
        cmd.addr = i;
        cmd.data = 0xff;
        status = access_peripheral(
                                    lusb,
                                    UART_PKT_DEV_GPIO, UART_PKT_MODE_DIR_READ,
                                    &cmd
                                  );

        if (status < 0) {
            break;
        }

        *val |= (cmd.data << (8*i));
    }

    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
    }

    return status;
}

static int lusb_si5338_write(struct bladerf *dev, uint8_t addr, uint8_t data)
{
    int status;
    struct uart_cmd cmd;
    struct bladerf_lusb *lusb = dev->backend;

    cmd.addr = addr;
    cmd.data = data;

    log_verbose( "%s: 0x%2.2x 0x%2.2x\n", __FUNCTION__, addr, data );

    status = access_peripheral(lusb, UART_PKT_DEV_SI5338,
                               UART_PKT_MODE_DIR_WRITE, &cmd);

    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
    }

    return status;
}

static int lusb_si5338_read(struct bladerf *dev, uint8_t addr, uint8_t *data)
{
    int status = 0;
    struct uart_cmd cmd;
    struct bladerf_lusb *lusb = dev->backend;

    cmd.addr = addr;
    cmd.data = 0xff;

    status = access_peripheral(lusb, UART_PKT_DEV_SI5338,
                               UART_PKT_MODE_DIR_READ, &cmd);

    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
    } else {
        *data = cmd.data;
    }

    log_verbose( "%s: 0x%2.2x 0x%2.2x\n", __FUNCTION__, addr, *data );

    return status;
}

static int lusb_lms_write(struct bladerf *dev, uint8_t addr, uint8_t data)
{
    int status;
    struct uart_cmd cmd;
    struct bladerf_lusb *lusb = dev->backend;

    cmd.addr = addr;
    cmd.data = data;

    log_verbose( "%s: 0x%2.2x 0x%2.2x\n", __FUNCTION__, addr, data );

    status = access_peripheral(lusb, UART_PKT_DEV_LMS,
                                UART_PKT_MODE_DIR_WRITE, &cmd);

    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
    }

    return status;
}

static int lusb_lms_read(struct bladerf *dev, uint8_t addr, uint8_t *data)
{
    int status;
    struct uart_cmd cmd;
    struct bladerf_lusb *lusb = dev->backend;

    cmd.addr = addr;
    cmd.data = 0xff;

    status = access_peripheral(lusb, UART_PKT_DEV_LMS,
                               UART_PKT_MODE_DIR_READ, &cmd);

    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
    } else {
        *data = cmd.data;
    }

    log_verbose( "%s: 0x%2.2x 0x%2.2x\n", __FUNCTION__, addr, *data );

    return status;
}

static int lusb_dac_write(struct bladerf *dev, uint16_t value)
{
    int status;
    struct uart_cmd cmd;
    struct bladerf_lusb *lusb = dev->backend;

    cmd.addr = 0 ;
    cmd.data = value & 0xff ;
    status = access_peripheral(lusb, UART_PKT_DEV_VCTCXO,
                               UART_PKT_MODE_DIR_WRITE, &cmd);

    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
        return status;
    }

    cmd.addr = 1 ;
    cmd.data = (value>>8)&0xff ;
    status = access_peripheral(lusb, UART_PKT_DEV_VCTCXO,
                               UART_PKT_MODE_DIR_WRITE, &cmd);

    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
    }

    return status;
}

static int lusb_tx(struct bladerf *dev, bladerf_format format, void *samples,
                   int n, struct bladerf_metadata *metadata)
{
    size_t bytes_total, bytes_remaining, ret;
    struct bladerf_lusb *lusb = (struct bladerf_lusb *)dev->backend;
    uint8_t *samples8 = (uint8_t *)samples;
    int transferred, status;

    /* This is the only format we currently support */
    assert(format == BLADERF_FORMAT_SC16_Q12);

    bytes_total = bytes_remaining = c16_samples_to_bytes(n);

    assert(bytes_remaining <= INT_MAX);

    while( bytes_remaining > 0 ) {
        transferred = 0;
        status = libusb_bulk_transfer(
                    lusb->handle,
                    EP_OUT(0x1),
                    samples8,
                    (int)bytes_remaining,
                    &transferred,
                    BLADERF_LIBUSB_TIMEOUT_MS
                );
        if( status < 0 ) {
            log_error( "Error transmitting samples (%d): %s\n", status, libusb_error_name(status) );
            return BLADERF_ERR_IO;
        } else {
            assert(transferred > 0);
            assert(bytes_remaining >= (size_t)transferred);
            bytes_remaining -= transferred;
            samples8 += transferred;
        }
    }

    ret = bytes_to_c16_samples(bytes_total - bytes_remaining);
    assert(ret <= INT_MAX);
    return (int)ret;
}

static int lusb_rx(struct bladerf *dev, bladerf_format format, void *samples,
                   int n, struct bladerf_metadata *metadata)
{
    size_t bytes_total, bytes_remaining, ret;
    struct bladerf_lusb *lusb = (struct bladerf_lusb *)dev->backend;
    uint8_t *samples8 = (uint8_t *)samples;
    int transferred, status;

    /* The only format currently is assumed here */
    assert(format == BLADERF_FORMAT_SC16_Q12);
    bytes_total = bytes_remaining = c16_samples_to_bytes(n);

    assert(bytes_remaining <= INT_MAX);

    while( bytes_remaining ) {
        transferred = 0;
        status = libusb_bulk_transfer(
                    lusb->handle,
                    EP_IN(0x1),
                    samples8,
                    (int)bytes_remaining,
                    &transferred,
                    BLADERF_LIBUSB_TIMEOUT_MS
                );
        if( status < 0 ) {
            log_error( "Error reading samples (%d): %s\n", status, libusb_error_name(status) );
            return BLADERF_ERR_IO;
        } else {
            assert(transferred > 0);
            assert(bytes_remaining >= (size_t)transferred);
            bytes_remaining -= transferred;
            samples8 += transferred;
        }
    }

    assert(bytes_total >= bytes_remaining);
    ret = bytes_to_c16_samples(bytes_total - bytes_remaining);
    assert(ret <= INT_MAX);
    return (int)ret;
}


static void LIBUSB_CALL lusb_stream_cb(struct libusb_transfer *transfer)
{
    struct bladerf_stream *stream  = transfer->user_data;
    struct bladerf_lusb *lusb = stream->dev->backend;
    void *next_buffer = NULL;
    struct bladerf_metadata metadata;
    struct lusb_stream_data *stream_data = stream->backend_data;
    size_t i;
    int status;
    size_t bytes_per_buffer;

    /* Check to see if the transfer has been cancelled or errored */
    if( transfer->status != LIBUSB_TRANSFER_COMPLETED ) {

        /* Errored out for some reason .. */
        stream->state = STREAM_SHUTTING_DOWN;

        switch(transfer->status) {
            case LIBUSB_TRANSFER_CANCELLED:
                /* We expect this case when we begin tearing down the stream */
                break;

            case LIBUSB_TRANSFER_STALL:
                log_error("Hit stall for buffer %p\n", transfer->buffer);
                stream->error_code = BLADERF_ERR_IO;
                break;

            case LIBUSB_TRANSFER_ERROR:
                log_error("Got transfer error for buffer %p\n",
                            transfer->buffer);
                stream->error_code = BLADERF_ERR_IO;
                break;

            case LIBUSB_TRANSFER_OVERFLOW :
                log_error("Got transfer over for buffer %p, "
                            "transfer \"actual_length\" = %d\n",
                            transfer->buffer, transfer->actual_length);
                stream->error_code = BLADERF_ERR_IO;
                break;

            case LIBUSB_TRANSFER_TIMED_OUT:
                stream->error_code = BLADERF_ERR_TIMEOUT;
                break;

            case LIBUSB_TRANSFER_NO_DEVICE:
                stream->error_code = BLADERF_ERR_NODEV;
                break;

            default:
                log_error( "Unexpected transfer status: %d\n", transfer->status );
                break;
        }

        /* This transfer is no longer active */
        stream_data->active_transfers--;

        for (i = 0; i < stream->num_transfers; i++ ) {
            status = libusb_cancel_transfer(stream_data->transfers[i]);
            if (status < 0 && status != LIBUSB_ERROR_NOT_FOUND) {
                log_error("Error canceling transfer (%d): %s\n",
                            status, libusb_error_name(status));
            }
        }
    }

    /* Check to see if the stream is still valid */
    else if( stream->state == STREAM_RUNNING ) {

        /* Call user callback requesting more data to transmit */
        if (transfer->length != transfer->actual_length) {
            log_warning( "Received short transfer\n" );
            if ((transfer->actual_length & 3) != 0) {
                log_warning( "Fractional samples received - stream likely corrupt\n" );
            }
        }
        next_buffer = stream->cb(
                        stream->dev,
                        stream,
                        &metadata,
                        transfer->buffer,
                        bytes_to_c16_samples(transfer->actual_length),
                        stream->user_data
                      );
        if( next_buffer == NULL ) {
            stream->state = STREAM_SHUTTING_DOWN;
            stream_data->active_transfers--;
        }
    } else {
        stream_data->active_transfers--;
    }

    bytes_per_buffer = c16_samples_to_bytes(stream->samples_per_buffer);
    assert(bytes_per_buffer <= INT_MAX);

    /* Check the stream to make sure we're still valid and submit transfer,
     * as the user may want to stop the stream by returning NULL for the next buffer */
    if( next_buffer != NULL ) {
        /* Fill up the bulk transfer request */
        libusb_fill_bulk_transfer(
            transfer,
            lusb->handle,
            stream->module == BLADERF_MODULE_TX ? EP_OUT(0x01) : EP_IN(0x01),
            next_buffer,
            (int)bytes_per_buffer,
            lusb_stream_cb,
            stream,
            BULK_TIMEOUT
        );
        /* Submit the tranfer */
        libusb_submit_transfer(transfer);
    }

    /* Check to see if all the transfers have been cancelled,
     * and if so, clean up the stream */
    if( stream->state == STREAM_SHUTTING_DOWN ) {
        if( stream_data->active_transfers == 0 ) {
            stream->state = STREAM_DONE;
            stream_data->libusb_completed = 1;
        }
    }
}

static int lusb_stream_init(struct bladerf_stream *stream)
{
    size_t i;
    struct lusb_stream_data *stream_data;
    int status = 0;

    /* Fill in backend stream information */
    stream_data = malloc(sizeof(struct lusb_stream_data));
    if (!stream_data) {
        return BLADERF_ERR_MEM;
    }

    /* Backend stream information */
    stream->backend_data = stream_data;

    stream_data->libusb_completed = 0;

    /* Pointers for libusb transfers */
    stream_data->transfers =
        calloc(stream->num_transfers, sizeof(struct libusb_transfer *));

    if (!stream_data->transfers) {
        log_error("Failed to allocate libusb tranfers\n");
        free(stream_data);
        stream->backend_data = NULL;
        return BLADERF_ERR_MEM;
    }

    stream_data->active_transfers = 0;

    /* Create the libusb transfers */
    for( i = 0; i < stream->num_transfers; i++ ) {
        stream_data->transfers[i] = libusb_alloc_transfer(0);

        /* Upon error, start tearing down anything we've started allocating
         * and report that the stream is in a bad state */
        if (!stream_data->transfers[i]) {

            /* Note: <= 0 not appropriate as we're dealing
             *       with an unsigned index */
            while (i > 0) {
                if (--i) {
                    libusb_free_transfer(stream_data->transfers[i]);
                    stream_data->transfers[i] = NULL;
                }
            }

            status = BLADERF_ERR_MEM;
            break;
        }
    }

    return status;
}

static int lusb_stream(struct bladerf_stream *stream, bladerf_module module)
{
    size_t i;
    int status;
    void *buffer;
    struct bladerf_metadata metadata;
    struct bladerf *dev = stream->dev;
    struct bladerf_lusb *lusb = dev->backend;
    struct lusb_stream_data *stream_data = stream->backend_data;
    struct timeval tv = { 5, 0 };
    const size_t buffer_size = c16_samples_to_bytes(stream->samples_per_buffer);

    assert(buffer_size <= INT_MAX);

    /* Currently unused, so zero it out for a sanity check when debugging */
    memset(&metadata, 0, sizeof(metadata));

    /* Set up initial set of buffers */
    for( i = 0; i < stream->num_transfers; i++ ) {
        if( module == BLADERF_MODULE_TX ) {
            buffer = stream->cb(
                        dev,
                        stream,
                        &metadata,
                        NULL,
                        stream->samples_per_buffer,
                        stream->user_data
                     );

            if (!buffer) {
                /* If we have transfers in flight and the user prematurely
                 * cancels the stream, we'll start shutting down */
                if (stream_data->active_transfers > 0) {
                    stream->state = STREAM_SHUTTING_DOWN;
                } else {
                    /* No transfers have been shipped out yet so we can
                     * simply enter our "done" state */
                    stream->state = STREAM_DONE;
                }

                /* In either of the above we don't want to attempt to
                 * get any more buffers from the user */
                break;
            }
        } else {
            buffer = stream->buffers[i];
        }

        /* Fill and submit the bulk transfer request */
        libusb_fill_bulk_transfer(
                stream_data->transfers[i],
                lusb->handle,
                module == BLADERF_MODULE_TX ? EP_OUT(0x01) : EP_IN(0x01),
                buffer,
                (int)buffer_size,
                lusb_stream_cb,
                stream,
                BULK_TIMEOUT
                );

        log_debug("Initial transfer with buffer: %p (i=%zd)\n", buffer, i);

        stream_data->active_transfers++;
        status = libusb_submit_transfer(stream_data->transfers[i]);

        /* If we failed to submit any transfers, cancel everything in flight.
         * We'll leave the stream in the running state so we can have
         * libusb fire off callbacks with the cancelled status*/
        if (status < 0) {
            stream->error_code = error_libusb2bladerf(status);

            /* Note: <= 0 not appropriate as we're dealing
             *       with an unsigned index */
            while (i > 0) {
                if (--i) {
                    status = libusb_cancel_transfer(stream_data->transfers[i]);
                    if (status < 0) {
                        log_error("Failed to cancel transfer %zd: %s\n",
                                i, libusb_error_name(status));
                    }
                }
            }
        }
    }

    /* This loop is required so libusb can do callbacks and whatnot */
    while( stream->state != STREAM_DONE ) {
        status = libusb_handle_events_timeout_completed(lusb->context, &tv,
                                                &stream_data->libusb_completed);

        if (status < 0 && status != LIBUSB_ERROR_INTERRUPTED) {
            log_warning("unexpected value from events processing: "
                                "%d: %s\n", status, libusb_error_name(status));
        }
    }

    return 0;
}

void lusb_deinit_stream(struct bladerf_stream *stream)
{
    size_t i;
    struct lusb_stream_data *stream_data = stream->backend_data;

    for (i = 0; i < stream->num_transfers; i++ ) {
        libusb_free_transfer(stream_data->transfers[i]);
        stream_data->transfers[i] = NULL;
    }

    free(stream_data->transfers);
    free(stream->backend_data);

    stream->backend_data = NULL;

    return;
}

int lusb_probe(struct bladerf_devinfo_list *info_list)
{
    int status, i, n;
    ssize_t count;
    libusb_device **list;
    struct bladerf_devinfo info;

    libusb_context *context;

    /* Initialize libusb for device tree walking */
    status = libusb_init(&context);
    if( status ) {
        log_error( "Could not initialize libusb: %s\n", libusb_error_name(status) );
        goto lusb_probe_done;
    }

    count = libusb_get_device_list( context, &list );
    /* Iterate through all the USB devices */
    for( i = 0, n = 0; i < count && status == 0; i++ ) {
        if( lusb_device_is_bladerf(list[i]) ) {
            /* Open the USB device and get some information */
            status = lusb_get_devinfo( list[i], &info );
            if( status ) {
                log_error( "Could not open bladeRF device: %s\n", libusb_error_name(status) );
            } else {
                info.instance = n++;
                status = bladerf_devinfo_list_add(info_list, &info);
                if( status ) {
                    log_error( "Could not add device to list: %s\n", bladerf_strerror(status) );
                }
            }
        }

        if( lusb_device_is_fx3(list[i]) ) {
            status = lusb_get_devinfo( list[i], &info );
            if( status ) {
                log_error( "Could not open bladeRF device: %s\n", libusb_error_name(status) );
                continue;
            }

            log_warning( "Found FX3 bootloader device libusb:device=%d:%d, could be bladeRF.\n",
                info.usb_bus, info.usb_addr);
            log_warning( "Use \"recover libusb:device=%d:%d <FX3 firmware>\" to boot bladeRF.\n",
                info.usb_bus, info.usb_addr);
        }
    }
    libusb_free_device_list(list,1);
    libusb_exit(context);

lusb_probe_done:
    return status;
}

static int lusb_get_stats(struct bladerf *dev, struct bladerf_stats *stats)
{
    /* TODO Implementation requires FPGA support */
    return BLADERF_ERR_UNSUPPORTED;
}

const struct bladerf_fn bladerf_lusb_fn = {
    FIELD_INIT(.probe, lusb_probe),

    FIELD_INIT(.open, lusb_open),
    FIELD_INIT(.close, lusb_close),

    FIELD_INIT(.load_fpga, lusb_load_fpga),
    FIELD_INIT(.is_fpga_configured, lusb_is_fpga_configured),

    FIELD_INIT(.recover, lusb_recover),
    FIELD_INIT(.flash_firmware, lusb_flash_firmware),
    FIELD_INIT(.erase_flash, lusb_erase_flash),
    FIELD_INIT(.read_flash, lusb_read_flash),
    FIELD_INIT(.write_flash, lusb_write_flash),
    FIELD_INIT(.device_reset, lusb_device_reset),
    FIELD_INIT(.jump_to_bootloader, lusb_jump_to_bootloader),

    FIELD_INIT(.get_cal, lusb_get_cal),
    FIELD_INIT(.get_otp, lusb_get_otp),
    FIELD_INIT(.fw_version, lusb_fw_version),
    FIELD_INIT(.fpga_version, lusb_fpga_version),
    FIELD_INIT(.get_device_speed, lusb_get_device_speed),

    FIELD_INIT(.config_gpio_write, lusb_config_gpio_write),
    FIELD_INIT(.config_gpio_read, lusb_config_gpio_read),

    FIELD_INIT(.si5338_write, lusb_si5338_write),
    FIELD_INIT(.si5338_read, lusb_si5338_read),

    FIELD_INIT(.lms_write, lusb_lms_write),
    FIELD_INIT(.lms_read, lusb_lms_read),

    FIELD_INIT(.dac_write, lusb_dac_write),

    FIELD_INIT(.rx, lusb_rx),
    FIELD_INIT(.tx, lusb_tx),

    FIELD_INIT(.init_stream, lusb_stream_init),
    FIELD_INIT(.stream, lusb_stream),
    FIELD_INIT(.deinit_stream, lusb_deinit_stream),

    FIELD_INIT(.stats, lusb_get_stats)
};

