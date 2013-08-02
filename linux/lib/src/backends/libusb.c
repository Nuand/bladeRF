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

#define EP_IN(x) (0x80 | x)
#define EP_OUT(x) (x)

struct lusb {
    libusb_device           *dev;
    libusb_device_handle    *handle;
    libusb_context          *context;
};

static int vendor_command(struct bladerf *dev, int cmd, int *result)
{
    *result = 0;
    int buf;
    int status;
    int transferred = 0;
    struct lusb *lusb = dev->backend;
    status = libusb_control_transfer(
                lusb->handle,
                LIBUSB_RECIPIENT_INTERFACE | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN,
                cmd,
                0,
                0,
                (unsigned char *)&buf,
                sizeof(buf),
                BLADERF_LIBUSB_TIMEOUT_MS
            );
    if (status < 0) {
        if (status == LIBUSB_ERROR_TIMEOUT) {
            status = BLADERF_ERR_TIMEOUT;
        } else {
            status = BLADERF_ERR_IO;
        }
    } else if (transferred != sizeof(buf)) {
        status = BLADERF_ERR_IO;
    } else {
        *result = buf;
        status = 0;
    }

    return status;
}

static int begin_fpga_programming(struct bladerf *dev)
{
    int result;
    int status = vendor_command(dev, BLADE_USB_CMD_BEGIN_PROG, &result);

    if (status < 0) {
        return status;
    } else {
        return result;
    }
}

static int end_fpga_programming(struct bladerf *dev)
{
    int result;
    int status = vendor_command(dev, BLADE_USB_CMD_QUERY_FPGA_STATUS, &result);
    if (status < 0) {
        fprintf(stderr, "%s: Received response of (%d): %s\n", __func__, status, libusb_error_name(status));
        return status;
    } else {
        return result;
    }
}

int is_fpga_configured(struct bladerf *dev)
{
    int result;
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
    struct lusb *lusb = dev->backend;

    /* Make sure we are using the configuration interface */
    /*
    status = libusb_set_interface_alt_setting(dev->usb_handle, 0, 0);
    if(status) {
        fprintf(stderr, "libusb_set_interface_alt_setting: ");
        print_libusb_error(status);
        goto deallocate;
    }*/

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
        status = is_fpga_configured(dev);
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
        fprintf(stderr, "libusb_set_interface_alt_setting: ");
    }

    return 0;
}

static int lusb_close(struct bladerf *dev)
{
    int status = 0;
    struct lusb *lusb = dev->backend;

    status = libusb_release_interface(lusb->handle, 0);

    if (status) {
        fprintf(stderr, "error releasing 0 usb_handle\n");
        status = BLADERF_ERR_IO;
    }

    status = libusb_release_interface(lusb->handle, 1);
    if (status) {
        fprintf(stderr, "error releasing 1 usb_handle\n");
        status = BLADERF_ERR_IO;
    }

    status = libusb_release_interface(lusb->handle, 2);
    if(status) {
        fprintf(stderr, "error releasing 2 usb_handle\n");
        status = BLADERF_ERR_IO;
    }

    libusb_close(lusb->handle);

    return status;
}

/* Returns BLADERF_ERR_* on failure */
static int access_peripheral(struct lusb *lusb, int per, int dir,
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
        fprintf(stderr, "could not access peripheral\n");
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
    struct lusb *lusb = dev->backend;

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
    struct lusb *lusb = dev->backend;

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
    struct lusb *lusb = dev->backend;

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
    struct lusb *lusb = dev->backend;

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
    struct lusb *lusb = dev->backend;

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
    struct lusb *lusb = dev->backend;

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
    struct lusb *lusb = dev->backend;

    cmd.word = value;
    status = access_peripheral(lusb, UART_PKT_DEV_VCTCXO,
                               UART_PKT_MODE_DIR_WRITE, &cmd);

    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
    }

    return status;
}

static ssize_t lusb_read_samples(struct bladerf *dev, int16_t *samples, size_t n)
{
    int status, transferred;
    size_t bytes_read;
    const size_t bytes_total = c16_samples_to_bytes(n);
    struct lusb *lusb = dev->backend;

    /* Unexpected overflow */
    assert(bytes_total <= (size_t)INT_MAX);

    status =  bytes_read = 0;
    while (bytes_read < bytes_total) {

        transferred = 0;
        status = libusb_bulk_transfer(lusb->handle, EP_IN(1),
                                      (unsigned char *)(samples + bytes_read),
                                      (int)(bytes_total - bytes_read),
                                      &transferred,
                                      BLADERF_LIBUSB_TIMEOUT_MS);

        if(status < 0) {
            fprintf(stderr, "error reading samples (%d): %s\n",
                        status, libusb_error_name(status));

            bladerf_set_error(&dev->error, ETYPE_BACKEND, status);
            return BLADERF_ERR_IO;
        } else {
            assert(transferred > 0);
            bytes_read += transferred;
        }
    }

    return bytes_to_c16_samples(bytes_read);
}

static ssize_t lusb_write_samples(struct bladerf *dev, int16_t *samples, size_t n)
{
    int status, transferred;
    size_t bytes_written;
    const size_t bytes_total = c16_samples_to_bytes(n);
    struct lusb *lusb = dev->backend;

    /* Unexpected overflow */
    assert(bytes_total <= (size_t)INT_MAX);

    status =  bytes_written = 0;
    while (bytes_written < bytes_total) {

        transferred = 0;
        status = libusb_bulk_transfer(lusb->handle, EP_OUT(1),
                                      (unsigned char *)(samples + bytes_written),
                                      (int)(bytes_total - bytes_written),
                                      &transferred,
                                      BLADERF_LIBUSB_TIMEOUT_MS);

        if(status < 0) {
            fprintf(stderr, "error writing samples (%d): %s\n",
                        status, libusb_error_name(status));

            bladerf_set_error(&dev->error, ETYPE_BACKEND, status);
            return BLADERF_ERR_IO;
        } else {
            assert(transferred > 0);
            bytes_written += transferred;
        }
    }

    return bytes_to_c16_samples(bytes_written);
}

const struct bladerf_fn lusb_fn = {
    .load_fpga = lusb_load_fpga,
    .close = lusb_close,
    .gpio_write = lusb_gpio_write,
    .gpio_read = lusb_gpio_read,
    .si5338_write = lusb_si5338_write,
    .si5338_read = lusb_si5338_read,
    .lms_write = lusb_lms_write,
    .lms_read = lusb_lms_read,
    .dac_write = lusb_dac_write,
    .write_samples = lusb_write_samples,
    .read_samples = lusb_read_samples,
};
