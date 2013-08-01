#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <bladeRF.h>
#include <libusb-1.0/libusb.h>

#include "bladerf_priv.h"

#define BLADERF_LIBUSB_TIMEOUT_MS 1000

struct lusb {
    libusb_device           *dev;
    libusb_device_handle    *handle;
    libusb_context          *context;
};

static int vendor_command(struct bladerf *dev, int cmd, int *result)
{
    *result = 0 ;
    int buf ;
    int status ;
    int transferred = 0 ;
    struct lusb *lusb = dev->driver;
    status = libusb_control_transfer(
                lusb->handle,
                LIBUSB_RECIPIENT_INTERFACE | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN,
                cmd,
                0,
                0,
                (unsigned char *)&buf,
                sizeof(buf),
                BLADERF_LIBUSB_TIMEOUT_MS
             ) ;
    if (status < 0) {
        if (status == LIBUSB_ERROR_TIMEOUT) {
            status = BLADERF_ERR_TIMEOUT;
        } else {
            status = BLADERF_ERR_IO;
        }
    } else if (transferred != sizeof(buf)) {
        status = BLADERF_ERR_IO;
    } else {
        *result = buf ;
        status = 0;
    }

    return status ;
}

static int begin_fpga_programming(struct bladerf *dev)
{
    int result ;
    int status = vendor_command(dev, BLADE_USB_CMD_BEGIN_PROG, &result);

    if (status < 0) {
        return status;
    } else {
        return result;
    }
}

static int end_fpga_programming( struct bladerf *dev )
{
    int result ;
    int status = vendor_command(dev, BLADE_USB_CMD_QUERY_FPGA_STATUS, &result);
    if (status < 0) {
        fprintf( stderr, "%s: Received response of (%d): %s\n", __func__, status, libusb_error_name(status) );
        return status;
    } else {
        return result;
    }
}

int is_fpga_configured(struct bladerf *dev)
{
    int result ;
    int status = vendor_command(dev, BLADE_USB_CMD_QUERY_FPGA_STATUS, &result);

    if (status < 0) {
        return status;
    } else {
        return result;
    }
}


static int lusb_load_fpga(struct bladerf *dev, uint8_t *image, size_t image_size)
{
    unsigned int wait_count;
    int status = 0;
    int transferred = 0;
    struct lusb *lusb = dev->driver;

    /* Make sure we are using the configuration interface */
    /*
    status = libusb_set_interface_alt_setting(dev->usb_handle, 0, 0);
    if( status ) {
        fprintf( stderr, "libusb_set_interface_alt_setting: " );
        print_libusb_error( status );
        goto deallocate;
    }*/

    /* Begin programming */
    status = begin_fpga_programming(dev);
    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_DRIVER, status);
        return BLADERF_ERR_IO;
    }

    /* Send the file down */
    status = libusb_bulk_transfer(lusb->handle, 0x2, image, image_size,
                                  &transferred, 5 * BLADERF_LIBUSB_TIMEOUT_MS);
    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_DRIVER, status);
        return BLADERF_ERR_IO;
    }

    /*  End programming */
    status = end_fpga_programming(dev);
    if (status) {
        bladerf_set_error(&dev->error, ETYPE_DRIVER, status);
        return BLADERF_ERR_IO;
    }

    /* Poll FPGA status to determine if programming was a success */
    wait_count = 10;
    status = 0;

    while (wait_count > 0 && status == 0) {
        status = is_fpga_configured(dev);
        if (status == 1) {
            break;
        }

        wait_count--;
        usleep(10000);
    }

    /* Failed to determine if FPGA is loaded */
    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_DRIVER, status);
        return  BLADERF_ERR_IO;
    } else if (wait_count == 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, BLADERF_ERR_TIMEOUT);
        return BLADERF_ERR_TIMEOUT;
    }

    /* Go into RF link mode by selecting interface 1 */
    status = libusb_set_interface_alt_setting(lusb->handle, 1, 0);
    if( status ) {
        fprintf( stderr, "libusb_set_interface_alt_setting: " );
    }

    return 0;
}

const struct bladerf_fn lusb_fn = {
    .load_fpga = lusb_load_fpga,
};

