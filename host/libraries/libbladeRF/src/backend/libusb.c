#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <bladeRF.h>
#include <libusb.h>

#include "bladerf_priv.h"
#include "backend/libusb.h"
#include "minmax.h"
#include "debug.h"

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

    /* libusb recommends use of libusb_handle_events_timeout_completed() over
     * libusb_handle_events_timeout(). While nothing in the documentation [1]
     * jumped out at me after a quick read, I figured this should be used
     * for safe measures...or at least I revisit that documentation in more
     * detail. At this time, our "completed" indication is the stream
     * entering the DONE state.
     *
     * [1] http://libusb.sourceforge.net/api-1.0/mtasync.html
     */
    int  libusb_completed;
};

static int error_libusb2bladerf(int error)
{
    int ret;
    switch (error) {
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
                          uint16_t cmd, uint8_t ep_dir, int32_t *val)
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
        dbg_printf( "status < 0: %s\n", libusb_error_name(status) );
        status = error_libusb2bladerf(status);
    } else if (status != sizeof(buf)) {
        dbg_printf( "status != sizeof(buf): %s\n", libusb_error_name(status) );
        status = BLADERF_ERR_IO;
    } else {
        if( ep_dir == EP_DIR_IN ) {
            *val = buf;
        }
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
        dbg_printf("Received response of (%d): %s\n",
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
        dbg_printf( "Couldn't open libusb device - %s\n", libusb_error_name(err) );
    } else {
        if( desc.idVendor == USB_NUAND_VENDOR_ID && desc.idProduct == USB_NUAND_BLADERF_PRODUCT_ID ) {
            rv = 1;
        }
    }
    return rv;
}

static int lusb_get_devinfo(libusb_device *dev, struct bladerf_devinfo *info)
{
    int status = 0;
    libusb_device_handle *handle;

    status = libusb_open( dev, &handle );
    if( status ) {
        dbg_printf( "Couldn't populate devinfo - %s\n", libusb_error_name(status) );
        status = error_libusb2bladerf(status);
    } else {
        /* Populate device info */
        info->backend = BLADERF_BACKEND_LIBUSB;
        info->usb_bus = libusb_get_bus_number(dev);
        info->usb_addr = libusb_get_device_address(dev);

        /* FIXME We need to get the serial number here before we've opened
         *      the device. Clearing it out for now to avoid uninitialized
         *      memory issues. */
        memset(info->serial, 0, BLADERF_SERIAL_LENGTH);

        libusb_close( handle );
    }

    return status;
}

static int change_setting(struct bladerf *dev, uint8_t setting)
{
    struct bladerf_lusb *lusb = dev->backend ;
    if (dev->legacy  & LEGACY_ALT_SETTING) {
        dbg_printf("Legacy change to interface %d\n", setting);
        return libusb_set_interface_alt_setting(lusb->handle, setting, 0);
    } else {
        dbg_printf( "Change to alternate interface %d\n", setting);
        return libusb_set_interface_alt_setting(lusb->handle, 0, setting);
    }
}

static int lusb_open(struct bladerf **device, struct bladerf_devinfo *info)
{
    int status, i, n, inf, val;
    ssize_t count;
    struct bladerf *dev = NULL;
    struct bladerf_lusb *lusb = NULL;
    libusb_device **list;
    struct bladerf_devinfo thisinfo;

    libusb_context *context;

    *device = NULL;

    /* Initialize libusb for device tree walking */
    status = libusb_init(&context);
    if( status ) {
        dbg_printf( "Could not initialize libusb: %s\n", libusb_error_name(status) );
        status = error_libusb2bladerf(status);
        goto lusb_open_done;
    }

    count = libusb_get_device_list( context, &list );
    /* Iterate through all the USB devices */
    for( i = 0, n = 0; i < count; i++ ) {
        if( lusb_device_is_bladerf(list[i]) ) {
            dbg_printf( "Found a bladeRF\n" ) ;

            /* Open the USB device and get some information
             *
             * FIXME in order for the bladerf_devinfo_matches() to work, this
             *       routine has to be able to get the serial #.
             */
            status = lusb_get_devinfo( list[i], &thisinfo );
            if(status < 0) {
                dbg_printf( "Could not open bladeRF device: %s\n", libusb_error_name(status) );
                status = error_libusb2bladerf(status);
                goto lusb_open__err_context;
            }
            thisinfo.instance = n++;

            /* Check to see if this matches the info stuct */
            if( bladerf_devinfo_matches( &thisinfo, info ) ) {

                /* Allocate backend structure and populate*/
                dev = (struct bladerf *)malloc(sizeof(struct bladerf));
                lusb = (struct bladerf_lusb *)malloc(sizeof(struct bladerf_lusb));

                if (!dev || !lusb) {
                    free(dev);
                    free(lusb);
                    dbg_printf("Skipping instance %d due to memory allocation "
                               "error", thisinfo.instance);

                    continue;
                }

                /* Assign libusb function table, backend type and backend */
                dev->fn = &bladerf_lusb_fn;
                dev->backend = (void *)lusb;
                dev->backend_type = BLADERF_BACKEND_LIBUSB;
                dev->legacy = 0;

                /* Populate the backend information */
                lusb->context = context;
                lusb->dev = list[i];
                lusb->handle = NULL;

                status = libusb_open(list[i], &lusb->handle);
                if(status < 0) {
                    dbg_printf( "Could not open bladeRF device: %s\n", libusb_error_name(status) );
                    status = error_libusb2bladerf(status);
                    goto lusb_open__err_device_list;
                }

                bladerf_get_fw_version(dev, &dev->fw_major, &dev->fw_minor);
                if (dev->fw_major < FW_LEGACY_ALT_SETTING_MAJOR ||
                        (dev->fw_major == FW_LEGACY_ALT_SETTING_MAJOR && dev->fw_minor < FW_LEGACY_ALT_SETTING_MINOR)) {
                    dev->legacy = LEGACY_ALT_SETTING;
                }


                /* Claim interfaces */
                for( inf = 0; inf < ((dev->legacy & LEGACY_ALT_SETTING) ? 3 : 1) ; inf++ ) {
                    status = libusb_claim_interface(lusb->handle, inf);
                    if(status < 0) {
                        dbg_printf( "Could not claim interface %i - %s\n", inf, libusb_error_name(status) );
                        status = error_libusb2bladerf(status);
                        goto lusb_open__err_device_list;
                    }
                }

                dbg_printf( "Claimed all inferfaces successfully\n" );
                break;
            }
        }
    }

    if (dev) {
        if (lusb_is_fpga_configured(dev)) {
            status = change_setting(dev, USB_IF_RF_LINK);
            dbg_printf( "Changed into RF link mode: %s\n", libusb_error_name(status) ) ;
            val = 1;
            status = vendor_command_int(dev, BLADE_USB_CMD_RF_RX, EP_DIR_OUT, &val);
            if(status) {
                dbg_printf("Could not enable RF RX (%d): %s\n", status, libusb_error_name(status) );
            }

            status = vendor_command_int(dev, BLADE_USB_CMD_RF_TX, EP_DIR_OUT, &val);
            if(status) {
                dbg_printf("Could not enable RF TX (%d): %s\n", status, libusb_error_name(status) );
            }
        }
    }

/* XXX I'd prefer if we made a call here to lusb_close(), but that would result
 *     in an attempt to release interfaces we haven't claimed... thoughts? */
lusb_open__err_device_list:
    libusb_free_device_list( list, 1 );
    if (status != 0) {
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
            dbg_printf("No devices available on the libusb backend.\n");
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
            dbg_printf("error releasing interface %i\n", inf);
            status = error_libusb2bladerf(status);
        }
    }

    libusb_close(lusb->handle);
    libusb_exit(lusb->context);
    free(dev->backend);
    free(dev);

    return status;
}

static int lusb_load_fpga(struct bladerf *dev, uint8_t *image, size_t image_size)
{
    unsigned int wait_count;
    int status = 0, val;
    int transferred = 0;
    struct bladerf_lusb *lusb = dev->backend;

    /* Make sure we are using the configuration interface */
    status = change_setting(dev, USB_IF_CONFIG);
    if(status < 0) {
        status = error_libusb2bladerf(status);
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
        dbg_printf( "alt_setting issue: %s\n", libusb_error_name(status) );
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
    status = libusb_bulk_transfer(lusb->handle, 0x2, image, image_size,
                                  &transferred, 5 * BLADERF_LIBUSB_TIMEOUT_MS);
    if (status < 0) {
        status = error_libusb2bladerf(status);
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
        return status;
    }

    /*  End programming */
    status = end_fpga_programming(dev);
    if (status) {
        status = error_libusb2bladerf(status);
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
        return status;
    }

    /* Poll FPGA status to determine if programming was a success */
    wait_count = 1000;
    status = 0;

    while (wait_count > 0 && status == 0) {
        status = lusb_is_fpga_configured(dev);
        if (status == 1) {
            break;
        }

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

    /* Go into RF link mode by selecting interface 1 */
    status = change_setting(dev, USB_IF_RF_LINK);
    if(status) {
        dbg_printf("libusb_set_interface_alt_setting: %s\n", libusb_error_name(status));
    }

    val = 1;
    status = vendor_command_int(dev, BLADE_USB_CMD_RF_RX, EP_DIR_OUT, &val);
    if(status) {
        dbg_printf("Could not enable RF RX (%d): %s\n", status, libusb_error_name(status) );
    }

    status = vendor_command_int(dev, BLADE_USB_CMD_RF_TX, EP_DIR_OUT, &val);
    if(status) {
        dbg_printf("Could not enable RF TX (%d): %s\n", status, libusb_error_name(status) );
    }

    return status;
}

/* Note: n_bytes is rounded up to a multiple of the sector size here */
static int erase_flash(struct bladerf *dev, int sector_offset, int n_bytes)
{
    int status = 0;
    int sector_to_erase;
    int erase_ret;
    const int n_sectors = FLASH_BYTES_TO_SECTORS(n_bytes);
    struct bladerf_lusb *lusb = dev->backend;

    assert(sector_offset < FLASH_NUM_SECTORS);
    assert((sector_offset + n_sectors) < FLASH_NUM_SECTORS);

    dbg_printf("Erasing %d sectors starting @ sector %d\n",
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

        if (status != sizeof(erase_ret) || !erase_ret) {
            dbg_printf("Failed to erase sector %d\n", sector_to_erase);
            if (status < 0) {
                dbg_printf("libusb status: %s\n", libusb_error_name(status));
            } else if (!erase_ret) {
                dbg_printf("Received erase failure status from FX3.\n");
            } else {
                dbg_printf("Unexpected read size: %d\n", status);
            }
            status = BLADERF_ERR_IO;
        } else {
            dbg_printf("Erased sector %d...\n", sector_to_erase);
            status = 0;
        }
    }

    return status;
}

static int read_flash(struct bladerf *dev, int page_offset,
                        uint8_t *ptr, size_t n_bytes)
{
    int status = 0;
    int page_i, n_read;
    int read_size = dev->speed ? FLASH_PAGE_SIZE: 64;
    int pages_to_read = FLASH_BYTES_TO_PAGES(n_bytes);
    struct bladerf_lusb *lusb = dev->backend;

    assert(page_offset < FLASH_NUM_PAGES);
    assert((page_offset + n_bytes) < FLASH_NUM_PAGES);

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
                        page_i,
                        ptr,
                        read_size,
                        BLADERF_LIBUSB_TIMEOUT_MS);


            if (status != read_size) {
                if (status < 0) {
                    dbg_printf("Failed to read back page %d: %s\n", page_i,
                               libusb_error_name(status));
                } else {
                    dbg_printf("Unexpected read size: %d\n", status);
                }

                status = BLADERF_ERR_IO;
            } else {
                n_read += read_size;
                ptr += read_size;
                status = 0;
            }
        } while (n_read < FLASH_PAGE_SIZE && !status);
    }
    return status;
}

static int verify_flash(struct bladerf *dev, int page_offset,
                        uint8_t *image, size_t n_bytes)
{
    int status = 0;
    int page_i, check_i, n_read;
    int read_size = dev->speed ? FLASH_PAGE_SIZE: 64;
    int pages_to_read = FLASH_BYTES_TO_PAGES(n_bytes);
    struct bladerf_lusb *lusb = dev->backend;

    uint8_t page_buf[FLASH_PAGE_SIZE];
    uint8_t *image_page;

    dbg_printf("Verifying with read size = %d\n", read_size);

    assert(page_offset < FLASH_NUM_PAGES);
    assert((page_offset + n_bytes) < FLASH_NUM_PAGES);

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
                        page_i,
                        page_buf + n_read,
                        read_size,
                        BLADERF_LIBUSB_TIMEOUT_MS);


            if (status != read_size) {
                if (status < 0) {
                    dbg_printf("Failed to read back page %d: %s\n", page_i,
                               libusb_error_name(status));
                } else {
                    dbg_printf("Unexpected read size: %d\n", status);
                }

                status = BLADERF_ERR_IO;
            } else {
                n_read += read_size;
                status = 0;
            }
        } while (n_read < FLASH_PAGE_SIZE && !status);

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

static int write_flash(struct bladerf *dev, int page_offset,
                        uint8_t *data, size_t data_size)
{
    int status = 0;
    int i;
    int n_write;
    int write_size = dev->speed ? FLASH_PAGE_SIZE : 64;
    int pages_to_write = FLASH_BYTES_TO_PAGES(data_size);
    struct bladerf_lusb *lusb = dev->backend;
    uint8_t *data_page;

    dbg_printf("Flashing with write size = %d\n", write_size);

    assert(page_offset < FLASH_NUM_PAGES);
    assert((page_offset + pages_to_write) < FLASH_NUM_PAGES);

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
                        i,
                        (unsigned char *)data_page,
                        write_size,
                        BLADERF_LIBUSB_TIMEOUT_MS);

            if (status != write_size) {
                if (status < 0) {
                    dbg_printf("Failed to write page %d: %s\n", i,
                            libusb_error_name(status));
                } else {
                    dbg_printf("Got unexpected write size: %d\n", status);
                }
                status = BLADERF_ERR_IO;
            } else {
                n_write += write_size;
                status = 0;
            }
        } while (n_write < FLASH_PAGE_SIZE && !status);
    }

    return status;
}

static int lusb_flash_firmware(struct bladerf *dev,
                               uint8_t *image, size_t image_size)
{
    int status;

    status = change_setting(dev, USB_IF_SPI_FLASH);

    if (status) {
        dbg_printf("Failed to set interface: %s\n", libusb_error_name(status));
        status = BLADERF_ERR_IO;
    }

    if (status == 0) {
        status = erase_flash(dev, 0, image_size);
    }

    if (status == 0) {
        status = write_flash(dev, 0, image, image_size);
    }

    if (status == 0) {
        status = verify_flash(dev, 0, image, image_size);
    }

    /* A reset will be required at this point, so there's no sense in
     * bothering to set the interface back to USB_IF_RF_LINK */

    return status;
}

static int lusb_device_reset(struct bladerf *dev)
{
    struct bladerf_lusb *lusb = dev->backend;
    int status, ok;
    ok = 1;
    status = libusb_control_transfer(
                lusb->handle,
                LIBUSB_RECIPIENT_INTERFACE |
                    LIBUSB_REQUEST_TYPE_VENDOR |
                    EP_DIR_OUT,
                BLADE_USB_CMD_RESET,
                0,
                0,
                (unsigned char *)&ok,
                sizeof(ok),
                BLADERF_LIBUSB_TIMEOUT_MS
             );
    return status;
}

static int lusb_get_otp(struct bladerf *dev, char *otp)
{
    struct bladerf_lusb *lusb = dev->backend;
    int status;
    int read_size = dev->speed ? 256 : 64;
    int nbytes;

    for (nbytes = 0; nbytes < 256; nbytes += status) {
        status = libusb_control_transfer(
                lusb->handle,
                LIBUSB_RECIPIENT_INTERFACE |
                    LIBUSB_REQUEST_TYPE_VENDOR |
                    EP_DIR_IN,
                BLADE_USB_CMD_READ_OTP,
                0,
                0,
                (unsigned char *)&otp[nbytes],
                read_size,
                BLADERF_LIBUSB_TIMEOUT_MS);
        if (status < 0) {
            dbg_printf("Failed to read OTP (%d): %s\n", status, libusb_error_name(status));
            return error_libusb2bladerf(status);
        }
    }
    return 0;
}

static int lusb_get_cal(struct bladerf *dev, char *cal) {
    return read_flash(dev, 768, (uint8_t *)cal, 256);
}

static int lusb_get_fw_version(struct bladerf *dev,
                               unsigned int *maj, unsigned int *min)
{
    int status;

    /* FIXME  We're playing with fire here - these structures need to be
     *        serialized/deserialized when communicating them between the
     *        host and the FX3. If the contents are change to not
     *        conveniently land on word boundaries and the struct is
     *        padded, we'll run into trouble.
     */
    struct bladeRF_version fw_ver;
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
        *maj = *min = 0;
    } else {
        *maj = (unsigned int) LE16_TO_HOST(fw_ver.major);
        *min = (unsigned int) LE16_TO_HOST(fw_ver.minor);
    }

    return status;
}

static int lusb_get_fpga_version(struct bladerf *dev,
                                 unsigned int *maj, unsigned int *min)
{
    dbg_printf("FPGA currently does not have a version number.\n");
    *maj = 0;
    *min = 0;
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
        dbg_printf("Got unsupported or unknown device speed: %d\n", speed);
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
        dbg_printf("could not access peripheral\n");
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

    return status;
}

static int lusb_lms_write(struct bladerf *dev, uint8_t addr, uint8_t data)
{
    int status;
    struct uart_cmd cmd;
    struct bladerf_lusb *lusb = dev->backend;

    cmd.addr = addr;
    cmd.data = data;

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

    return status;
}

static int lusb_dac_write(struct bladerf *dev, uint16_t value)
{
    int status;
    struct uart_cmd cmd;
    struct bladerf_lusb *lusb = dev->backend;

    cmd.word = value;
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
    size_t bytes_total, bytes_remaining;
    struct bladerf_lusb *lusb = (struct bladerf_lusb *)dev->backend;
    uint8_t *samples8 = (uint8_t *)samples;
    int transferred, status;

    /* This is the only format we currently support */
    assert(format == BLADERF_FORMAT_SC16_Q12);

    bytes_total = bytes_remaining = c16_samples_to_bytes(n);

    while( bytes_remaining > 0 ) {
        transferred = 0;
        status = libusb_bulk_transfer(
                    lusb->handle,
                    EP_OUT(0x1),
                    samples8,
                    bytes_remaining,
                    &transferred,
                    BLADERF_LIBUSB_TIMEOUT_MS
                );
        if( status < 0 ) {
            dbg_printf( "Error transmitting samples (%d): %s\n", status, libusb_error_name(status) );
            return BLADERF_ERR_IO;
        } else {
            assert(transferred > 0);
            bytes_remaining -= transferred;
            samples8 += transferred;
        }
    }

    return bytes_to_c16_samples(bytes_total - bytes_remaining);
}

static int lusb_rx(struct bladerf *dev, bladerf_format format, void *samples,
                   int n, struct bladerf_metadata *metadata)
{
    int bytes_total, bytes_remaining = c16_samples_to_bytes(n);
    struct bladerf_lusb *lusb = (struct bladerf_lusb *)dev->backend;
    uint8_t *samples8 = (uint8_t *)samples;
    int transferred, status;

    /* The only format currently is assumed here */
    assert(format == BLADERF_FORMAT_SC16_Q12);
    bytes_total = bytes_remaining = c16_samples_to_bytes(n);

    while( bytes_remaining ) {
        transferred = 0;
        status = libusb_bulk_transfer(
                    lusb->handle,
                    EP_IN(0x1),
                    samples8,
                    bytes_remaining,
                    &transferred,
                    BLADERF_LIBUSB_TIMEOUT_MS
                );
        if( status < 0 ) {
            dbg_printf( "Error reading samples (%d): %s\n", status, libusb_error_name(status) );
            return BLADERF_ERR_IO;
        } else {
            assert(transferred > 0);
            bytes_remaining -= transferred;
            samples8 += transferred;
        }
    }

    return bytes_to_c16_samples(bytes_total - bytes_remaining);
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

    /* Check to see if the transfer has been cancelled or errored */
    if( transfer->status != LIBUSB_TRANSFER_COMPLETED ) {

        /* Errored out for some reason .. */
        stream->state = STREAM_SHUTTING_DOWN;

        switch(transfer->status) {
            case LIBUSB_TRANSFER_CANCELLED:
                /* We expect this case when we begin tearing down the stream */
                break;

            case LIBUSB_TRANSFER_STALL:
                dbg_printf("Hit stall for buffer %p\n", transfer->buffer);
                stream->error_code = BLADERF_ERR_IO;
                break;

            case LIBUSB_TRANSFER_ERROR:
                dbg_printf("Got transfer error for buffer %p\n",
                            transfer->buffer);
                stream->error_code = BLADERF_ERR_IO;
                break;

            case LIBUSB_TRANSFER_OVERFLOW :
                dbg_printf("Got transfer over for buffer %p, "
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
                dbg_printf( "Unexpected transfer status: %d\n", transfer->status );
                break;
        }

        /* This transfer is no longer active */
        stream_data->active_transfers--;

        for (i = 0; i < stream->num_transfers; i++ ) {
            status = libusb_cancel_transfer(stream_data->transfers[i]);
            if (status < 0 && status != LIBUSB_ERROR_NOT_FOUND) {
                dbg_printf("Error canceling transfer (%d): %s\n",
                            status, libusb_error_name(status));
            }
        }
    }

    /* Check to see if the stream is still valid */
    else if( stream->state == STREAM_RUNNING ) {

        /* Call user callback requesting more data to transmit */
        assert(transfer->length == transfer->actual_length);
        next_buffer = stream->cb(
                        stream->dev,
                        stream,
                        &metadata,
                        transfer->buffer,
                        bytes_to_c16_samples(transfer->length),
                        stream->user_data
                      );
        if( next_buffer == NULL ) {
            stream->state = STREAM_SHUTTING_DOWN;
            stream_data->active_transfers--;
        }
    } else {
        stream_data->active_transfers--;
    }

    /* Check the stream to make sure we're still valid and submit transfer,
     * as the user may want to stop the stream by returning NULL for the next buffer */
    if( next_buffer != NULL ) {
        /* Fill up the bulk transfer request */
        libusb_fill_bulk_transfer(
            transfer,
            lusb->handle,
            stream->module == BLADERF_MODULE_TX ? EP_OUT(0x01) : EP_IN(0x01),
            next_buffer,
            c16_samples_to_bytes(stream->samples_per_buffer),
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

static int lusb_stream(struct bladerf *dev, bladerf_module module,
                       bladerf_format format, struct bladerf_stream *stream)
{
    size_t i;
    void *buffer;
    struct bladerf_metadata metadata;
    int status;
    struct lusb_stream_data *stream_data;

    const size_t buffer_size = c16_samples_to_bytes(stream->samples_per_buffer);
    struct bladerf_lusb *lusb = dev->backend;
    struct timeval tv = { 5, 0 };

    /* Fill in backend stream information */
    stream_data = malloc( sizeof(struct lusb_stream_data) );
    if (!stream_data) {
        return BLADERF_ERR_MEM;
    }

    /* Backend stream information */
    stream->backend_data = stream_data;
    stream->module = module;

    /* Set our state to running */
    stream->state = STREAM_RUNNING;
    stream_data->libusb_completed = 0;

    /* Pointers for libusb transfers */
    stream_data->transfers = malloc(stream->num_transfers * sizeof(struct libusb_transfer *));
    if (!stream_data->transfers) {
        dbg_printf("Failed to allocate libusb tranfers\n");
        free(stream_data);
        stream->backend_data = NULL;
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

            /* Don't even start up */
            stream->state = STREAM_DONE;
            stream->error_code = BLADERF_ERR_MEM;
            break;
        }
    }

    /* Set up initial set of buffers */
    for( i = 0; i < stream->num_transfers; i++ ) {
        if( module == BLADERF_MODULE_TX ) {
            buffer = stream->cb(
                        dev,
                        stream,
                        &metadata,
                        NULL,
                        0,
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
            buffer_size,
            lusb_stream_cb,
            stream,
            BULK_TIMEOUT
        );

        dbg_printf("Initial transfer with buffer: %p (i=%zd)\n", buffer, i);

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
                        dbg_printf("Failed to cancel transfer %zd: %s\n",
                                   i, libusb_error_name(status));
                    }
                }
            }
        }
    }

    /* This loop is required so libusb can do callbacks and whatnot */
    while( stream->state != STREAM_DONE ) {
        status = libusb_handle_events_timeout(lusb->context,&tv);
        if (status < 0 && status != LIBUSB_ERROR_INTERRUPTED) {
            dbg_printf("Got unexpected return value from events processing: "
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
        dbg_printf( "Could not initialize libusb: %s\n", libusb_error_name(status) );
        goto lusb_probe_done;
    }

    count = libusb_get_device_list( context, &list );
    /* Iterate through all the USB devices */
    for( i = 0, n = 0; i < count && status == 0; i++ ) {
        if( lusb_device_is_bladerf(list[i]) ) {
            /* Open the USB device and get some information */
            status = lusb_get_devinfo( list[i], &info );
            if( status ) {
                dbg_printf( "Could not open bladeRF device: %s\n", libusb_error_name(status) );
            } else {
                info.instance = n++;
                status = bladerf_devinfo_list_add(info_list, &info);
                if( status ) {
                    dbg_printf( "Could not add device to list: %s\n", bladerf_strerror(status) );
                }
            }
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

    FIELD_INIT(.flash_firmware, lusb_flash_firmware),
    FIELD_INIT(.device_reset, lusb_device_reset),

    FIELD_INIT(.get_cal, lusb_get_cal),
    FIELD_INIT(.get_otp, lusb_get_otp),
    FIELD_INIT(.get_fw_version, lusb_get_fw_version),
    FIELD_INIT(.get_fpga_version, lusb_get_fpga_version),
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

    FIELD_INIT(.stream, lusb_stream),
    FIELD_INIT(.deinit_stream, lusb_deinit_stream),

    FIELD_INIT(.stats, lusb_get_stats)
};

