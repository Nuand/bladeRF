#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>
#include <bladeRF.h>
#include <libusb-1.0/libusb.h>

#include "bladerf_priv.h"
#include "debug.h"

#define BLADERF_LIBUSB_TIMEOUT_MS 10000

#define EP_IN(x) (0x80 | x)
#define EP_OUT(x) (x)

struct bladerf_lusb {
    libusb_device           *dev;
    libusb_device_handle    *handle;
    libusb_context          *context;
};

const struct bladerf_fn bladerf_lusb_fn;

static int vendor_command(struct bladerf *dev, int cmd, int ep_dir, int *val)
{
    int buf;
    int status;
    struct bladerf_lusb *lusb = dev->backend;

    if( ep_dir == LIBUSB_ENDPOINT_IN ) {
        *val = 0 ;
    } else {
        buf = *val ;
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
        if (status == LIBUSB_ERROR_TIMEOUT) {
            status = BLADERF_ERR_TIMEOUT;
        } else {
            status = BLADERF_ERR_IO;
        }
    } else if (status != sizeof(buf)) {
        dbg_printf( "status != sizeof(buf): %s\n", libusb_error_name(status) );
        status = BLADERF_ERR_IO;
    } else {
        if( ep_dir == LIBUSB_ENDPOINT_IN ) {
            *val = buf;
        }
        status = 0;
    }

    return status;
}

static int begin_fpga_programming(struct bladerf *dev)
{
    int result;
    int status = vendor_command(dev, BLADE_USB_CMD_BEGIN_PROG, LIBUSB_ENDPOINT_IN, &result);

    if (status < 0) {
        return status;
    } else {
        return 0;
    }
}

static int end_fpga_programming(struct bladerf *dev)
{
    int result;
    int status = vendor_command(dev, BLADE_USB_CMD_QUERY_FPGA_STATUS, LIBUSB_ENDPOINT_IN, &result);
    if (status < 0) {
        dbg_printf("Received response of (%d): %s\n",
                    status, libusb_error_name(status));
        return status;
    } else {
        return 0;
    }
}

int lusb_is_fpga_configured(struct bladerf *dev)
{
    int result;
    int status = vendor_command(dev, BLADE_USB_CMD_QUERY_FPGA_STATUS, LIBUSB_ENDPOINT_IN, &result);

    if (status < 0) {
        return status;
    } else {
        return result;
    }
}

int lusb_device_is_bladerf(libusb_device *dev)
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

int lusb_get_devinfo(libusb_device *dev, struct bladerf_devinfo *info)
{
    int status = 0;
    libusb_device_handle *handle;

    status = libusb_open( dev, &handle );
    if( status ) {
        dbg_printf( "Couldn't populate devinfo - %s\n", libusb_error_name(status) );
        status = BLADERF_ERR_IO;
    } else {
        /* Populate */
        info->backend = BACKEND_LIBUSB;
        info->serial = 0;
        info->usb_bus = libusb_get_bus_number(dev);
        info->usb_addr = libusb_get_device_address(dev);

        libusb_close( handle );
    }

    return status ;
}

static struct bladerf * lusb_open(struct bladerf_devinfo *info)
{
    int status, i, n, inf;
    ssize_t count;
    struct bladerf *dev = NULL;
    struct bladerf_lusb *lusb = NULL;
    libusb_device **list;
    struct bladerf_devinfo thisinfo;

    libusb_context *context ;

    /* Initialize libusb for device tree walking */
    status = libusb_init(&context);
    if( status ) {
        dbg_printf( "Could not initialize libusb: %s\n", libusb_error_name(status) );
        goto done;
    }

    count = libusb_get_device_list( NULL, &list );
    /* Iterate through all the USB devices */
    for( i = 0, n = 0 ; i < count ; i++ ) {
        if( lusb_device_is_bladerf(list[i]) ) {
            /* Open the USB device and get some information */
            status = lusb_get_devinfo( list[i], &thisinfo );
            if( status ) {
                dbg_printf( "Could not open bladeRF device: %s\n", libusb_error_name(status) );
                goto error_context;
            }
            thisinfo.instance = n++;

            /* Check to see if this matches the info stuct */
            if( bladerf_devinfo_matches( &thisinfo, info ) ) {
                /* Allocate backend structure and populate*/
                dev = (struct bladerf *)malloc(sizeof(struct bladerf));
                lusb = (struct bladerf_lusb *)malloc(sizeof(struct bladerf_lusb));

                /* Assign libusb function table, backend type and backend */
                dev->fn = &bladerf_lusb_fn;
                dev->backend = (void *)lusb;
                dev->backend_type = BACKEND_LIBUSB;

                /* Populate the backend information */
                lusb->context = context;
                lusb->dev = list[i];
                status = libusb_open(list[i], &lusb->handle);
                if( status ) {
                    dbg_printf( "Could not open bladeRF device: %s\n", libusb_error_name(status) );
                    free(lusb);
                    lusb = NULL;
                    free(dev);
                    dev = NULL;
                    goto error_device_list;
                }

                /* Claim interfaces */
                for( inf = 0 ; inf < 3 ; inf++ ) {
                    status = libusb_claim_interface(lusb->handle, inf);
                    if( status ) {
                        dbg_printf( "Could not claim interface %i - %s\n", inf, libusb_error_name(status) );
                        free(lusb);
                        lusb = NULL;
                        free(dev);
                        dev = NULL;
                        goto error_device_list;
                    }
                }

                /* Assign top level stuff */
                dev->speed = libusb_get_device_speed(lusb->dev) - 3;
                dev->last_tx_sample_rate = 0;
                dev->last_rx_sample_rate = 0;

               dbg_printf( "Claimed all inferfaces successfully\n" );
                break ;
            }
        }
    }

error_device_list:
    libusb_free_device_list( list, 1 );

error_context:
    if( dev == NULL ) {
        libusb_exit(context);
    }

done:
    return dev;
}

static int lusb_close(struct bladerf *dev)
{
    int status = 0;
    int inf = 0;
    struct bladerf_lusb *lusb = dev->backend;


    for( inf = 0 ; inf < 2 ; inf++ ) {
        status = libusb_release_interface(lusb->handle, inf);
        if (status) {
            dbg_printf("error releasing interface %i\n", inf);
            status = BLADERF_ERR_IO;
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
    status = libusb_set_interface_alt_setting(lusb->handle, 0, 0);
    if(status) {
        bladerf_set_error(&dev->error, ETYPE_BACKEND, status);
        dbg_printf( "alt_setting issue: %s\n", libusb_error_name(status) );
        return BLADERF_ERR_IO;;
    }

    /* Begin programming */
    status = begin_fpga_programming(dev);
    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_BACKEND, status);
        return BLADERF_ERR_IO;
    }

    /* Send the file down */
    status = libusb_bulk_transfer(lusb->handle, 0x2, image, image_size,
                                  &transferred, 5 * BLADERF_LIBUSB_TIMEOUT_MS);
    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_BACKEND, status);
        return BLADERF_ERR_IO;
    }

    /*  End programming */
    status = end_fpga_programming(dev);
    if (status) {
        bladerf_set_error(&dev->error, ETYPE_BACKEND, status);
        return BLADERF_ERR_IO;
    }

    /* Poll FPGA status to determine if programming was a success */
    wait_count = 10;
    status = 0;

    while (wait_count > 0 && status == 0) {
        status = lusb_is_fpga_configured(dev);
        if (status == 1) {
            break;
        }

        wait_count--;
        usleep(10000);
    }

    /* Failed to determine if FPGA is loaded */
    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_BACKEND, status);
        return  BLADERF_ERR_IO;
    } else if (wait_count == 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, BLADERF_ERR_TIMEOUT);
        return BLADERF_ERR_TIMEOUT;
    }

    /* Go into RF link mode by selecting interface 1 */
    status = libusb_set_interface_alt_setting(lusb->handle, 1, 0);
    if(status) {
        dbg_printf("libusb_set_interface_alt_setting: %s", libusb_error_name(status));
    }

    val = 1;
    status = vendor_command(dev, BLADE_USB_CMD_RF_RX, LIBUSB_ENDPOINT_OUT, &val);
    if(status) {
        dbg_printf("Could not enable RF RX (%d): %s\n", status, libusb_error_name(status) );
    }

    status = vendor_command(dev, BLADE_USB_CMD_RF_TX, LIBUSB_ENDPOINT_OUT, &val);
    if(status) {
        dbg_printf("Could not enable RF TX (%d): %s\n", status, libusb_error_name(status) );
    }

done:
    return status;
}

static int lusb_flash_firmware(struct bladerf *dev,
                               uint8_t *image, size_t image_size)
{
    dbg_printf("Firmware flashing not suport in libusb yet");
    return BLADERF_ERR_IO;
}

static int lusb_get_serial(struct bladerf *dev, uint64_t *serial)
{
    return 0;
}

static int lusb_get_fw_version(struct bladerf *dev,
                               unsigned int *maj, unsigned int *min)
{
    return 0;
}

static int lusb_get_fpga_version(struct bladerf *dev,
                                 unsigned int *maj, unsigned int *min)
{
    return 0;
}


/* Returns BLADERF_ERR_* on failure */
static int access_peripheral(struct bladerf_lusb *lusb, int per, int dir,
                                struct uart_cmd *cmd)
{
    uint8_t buf[16];
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

static int lusb_gpio_write(struct bladerf *dev, uint32_t val)
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

static int lusb_gpio_read(struct bladerf *dev, uint32_t *val)
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

//static ssize_t lusb_read_samples(struct bladerf *dev, int16_t *samples, size_t n)
//{
//    int status, transferred;
//    size_t bytes_read;
//    const size_t bytes_total = c16_samples_to_bytes(n);
//    struct bladerf_lusb *lusb = dev->backend;
//
//    /* Unexpected overflow */
//    assert(bytes_total <= (size_t)INT_MAX);
//
//    status =  bytes_read = 0;
//    while (bytes_read < bytes_total) {
//
//        transferred = 0;
//        status = libusb_bulk_transfer(lusb->handle, EP_IN(1),
//                                      (unsigned char *)(samples + bytes_read),
//                                      (int)(bytes_total - bytes_read),
//                                      &transferred,
//                                      BLADERF_LIBUSB_TIMEOUT_MS);
//
//        if(status < 0) {
//            dbg_printf("error reading samples (%d): %s\n",
//                        status, libusb_error_name(status));
//
//            bladerf_set_error(&dev->error, ETYPE_BACKEND, status);
//            return BLADERF_ERR_IO;
//        } else {
//            assert(transferred > 0);
//            bytes_read += transferred;
//        }
//    }
//
//    return bytes_to_c16_samples(bytes_read);
//}

static ssize_t lusb_tx(struct bladerf *dev, bladerf_format_t format, void *samples,
                       size_t n, struct bladerf_metadata *metadata)
{
    size_t bytes_total, bytes_remaining;
    struct bladerf_lusb *lusb = (struct bladerf_lusb *)dev->backend;
    uint8_t *samples8 = (uint8_t *)samples;
    int transferred, status;

    assert(format==FORMAT_SC16);

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

static ssize_t lusb_rx(struct bladerf *dev, bladerf_format_t format, void *samples,
                       size_t n, struct bladerf_metadata *metadata)
{
    ssize_t bytes_total, bytes_remaining = c16_samples_to_bytes(n) ;
    struct bladerf_lusb *lusb = (struct bladerf_lusb *)dev->backend;
    uint8_t *samples8 = (uint8_t *)samples;
    int transferred, status;

    assert(format==FORMAT_SC16);

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

//static ssize_t lusb_write_samples(struct bladerf *dev,
//                                  int16_t *samples, size_t n)
//{
//    int status, transferred;
//    size_t bytes_written;
//    const size_t bytes_total = c16_samples_to_bytes(n);
//    struct bladerf_lusb *lusb = dev->backend;
//
//    /* Unexpected overflow */
//    assert(bytes_total <= (size_t)INT_MAX);
//
//    status =  bytes_written = 0;
//    while (bytes_written < bytes_total) {
//
//        transferred = 0;
//        status = libusb_bulk_transfer(lusb->handle, EP_OUT(1),
//                                     (unsigned char *)(samples + bytes_written),
//                                     (int)(bytes_total - bytes_written),
//                                     &transferred,
//                                     BLADERF_LIBUSB_TIMEOUT_MS);
//
//        if(status < 0) {
//            dbg_printf("error writing samples (%d): %s\n",
//                        status, libusb_error_name(status));
//
//            bladerf_set_error(&dev->error, ETYPE_BACKEND, status);
//            return BLADERF_ERR_IO;
//        } else {
//            assert(transferred > 0);
//            bytes_written += transferred;
//        }
//    }
//
//    return bytes_to_c16_samples(bytes_written);
//}

const struct bladerf_fn bladerf_lusb_fn = {
    .probe              = NULL,

    .open               = lusb_open,
    .close              = lusb_close,

    .load_fpga          = lusb_load_fpga,
    .is_fpga_configured = lusb_is_fpga_configured,

    .flash_firmware     = lusb_flash_firmware,

    .get_serial         = lusb_get_serial,
    .get_fw_version     = lusb_get_fw_version,
    .get_fpga_version   = lusb_get_fpga_version,

    .gpio_write         = lusb_gpio_write,
    .gpio_read          = lusb_gpio_read,

    .si5338_write       = lusb_si5338_write,
    .si5338_read        = lusb_si5338_read,

    .lms_write          = lusb_lms_write,
    .lms_read           = lusb_lms_read,

    .dac_write          = lusb_dac_write,

    .rx                 = lusb_rx,
    .tx                 = lusb_tx,
};

