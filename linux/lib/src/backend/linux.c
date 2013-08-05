#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>

#include "bladeRF.h"
#include "libbladeRF.h"
#include "bladerf_priv.h"
#include "backend/linux.h"
#include "debug.h"

/* Linux device driver information */
struct bladerf_linux {
    int fd;         /* File descriptor to associated driver device node */
};

static bool time_past(struct timeval ref, struct timeval now) {
    if (now.tv_sec > ref.tv_sec)
        return true;

    if (now.tv_sec == ref.tv_sec && now.tv_usec > ref.tv_usec)
        return true;

    return false;
}

static inline size_t min_sz(size_t x, size_t y)
{
    return x < y ? x : y;
}

/*------------------------------------------------------------------------------
 * FPGA & Firmware loading
 *----------------------------------------------------------------------------*/

static int linux_is_fpga_configured(struct bladerf *dev)
{
    int status;
    int configured;
    struct bladerf_linux *backend = (struct bladerf_linux *)dev->backend;

    assert(dev);

    status = ioctl(backend->fd, BLADE_QUERY_FPGA_STATUS, &configured);

    if (status || configured < 0 || configured > 1)
        configured = BLADERF_ERR_IO;

    return configured;
}


static int linux_load_fpga(struct bladerf *dev,
                           uint8_t *image, size_t image_size)
{
    int ret, fpga_status;
    size_t written = 0;     /* Total # of bytes written */
    size_t to_write;
    ssize_t write_tmp;      /* # bytes written in a single write() call */
    struct timeval end_time, curr_time;
    bool timed_out;
    struct bladerf_linux *backend = (struct bladerf_linux *)dev->backend;
    assert(dev && image);

    if (ioctl(backend->fd, BLADE_BEGIN_PROG, &fpga_status)) {
        dbg_printf("ioctl(BLADE_BEGIN_PROG) failed: %s\n", strerror(errno));
        return BLADERF_ERR_UNEXPECTED;
    }

    /* FIXME This loops is just here to work around the fact that the
     *       driver currently can't handle large writes... */
    while (written < image_size) {
        to_write = min_sz(1024, image_size - written);
        do {
            write_tmp = write(backend->fd, image + written, to_write);
            if (write_tmp < 0) {
                /* Failing out...at least attempt to "finish" programming */
                ioctl(backend->fd, BLADE_END_PROG, &ret);
                dbg_printf("Write failure: %s\n", strerror(errno));
                return BLADERF_ERR_IO;
            } else {
                written += write_tmp;
            }
        } while(written < image_size);
    }


    /* FIXME? Perhaps it would be better if the driver blocked on the
     * write call, rather than sleeping in userspace? */
    usleep(4000);

    /* Time out within 1 second */
    gettimeofday(&end_time, NULL);
    end_time.tv_sec++;

    ret = 0;
    do {
        fpga_status = linux_is_fpga_configured(dev);
        if (fpga_status < 0) {
            ret = fpga_status;
        } else {
            gettimeofday(&curr_time, NULL);
            timed_out = time_past(end_time, curr_time);
        }
    } while(!fpga_status && !timed_out && !ret);

    if (ioctl(backend->fd, BLADE_END_PROG, &fpga_status)) {
        dbg_printf("Failed to end programming procedure: %s\n",
                strerror(errno));

        /* Don't clobber a previous error */
        if (!ret)
            ret = BLADERF_ERR_UNEXPECTED;
    }

    /* Now that the FPGA is loaded, initialize the device */
    bladerf_init_device(dev);

    return ret;
}


static int linux_flash_firmware(struct bladerf *dev,
                                uint8_t *image, size_t image_size)
{
    int upgrade_status;
    int ret = 0;
    struct bladeRF_firmware fw_param;
    struct bladerf_linux *backend = (struct bladerf_linux*)dev->backend;

    assert(dev && image);

    fw_param.ptr = image;
    fw_param.len = image_size;

    upgrade_status = ioctl(backend->fd, BLADE_UPGRADE_FW, &fw_param);
    if (upgrade_status < 0) {
        dbg_printf("Firmware upgrade failed: %s\n", strerror(errno));
        ret = BLADERF_ERR_UNEXPECTED;
    }

    return ret;
}

static int linux_get_fw_version(struct bladerf *dev,
                                unsigned int *major, unsigned int *minor)
{
    int status;
    struct bladeRF_version ver;
    struct bladerf_linux *backend = (struct bladerf_linux *)dev->backend;

    assert(dev && major && minor);

    status = ioctl(backend->fd, BLADE_QUERY_VERSION, &ver);
    if (!status) {
        *major = ver.major;
        *minor = ver.minor;
        return 0;
    }

    /* TODO return more appropriate error code based upon errno */
    return BLADERF_ERR_IO;
}

/*------------------------------------------------------------------------------
 * Si5338 register access
 *----------------------------------------------------------------------------*/


static int linux_si5338_read(struct bladerf *dev, uint8_t address, uint8_t *val)
{
    int ret;
    struct uart_cmd uc;
    struct bladerf_linux *backend = (struct bladerf_linux *)dev->backend;

    address &= 0x7f;
    uc.addr = address;
    uc.data = 0xff;
    ret = ioctl(backend->fd, BLADE_SI5338_READ, &uc);
    *val = uc.data;
    return ret;
}

static int linux_si5338_write(struct bladerf *dev, uint8_t address, uint8_t val)
{
    struct uart_cmd uc;
    struct bladerf_linux *backend = (struct bladerf_linux *)dev->backend;
    uc.addr = address;
    uc.data = val;
    return ioctl(backend->fd, BLADE_SI5338_WRITE, &uc);
}

/*------------------------------------------------------------------------------
 * LMS register access
 *----------------------------------------------------------------------------*/

static int linux_lms_read(struct bladerf *dev, uint8_t address, uint8_t *val)
{
    int ret;
    struct uart_cmd uc;
    struct bladerf_linux *backend = (struct bladerf_linux *)dev->backend;
    address &= 0x7f;
    uc.addr = address;
    uc.data = 0xff;
    ret = ioctl(backend->fd, BLADE_LMS_READ, &uc);
    *val = uc.data;
    return ret;
}

static int linux_lms_write(struct bladerf *dev, uint8_t address, uint8_t val)
{
    struct uart_cmd uc;
    struct bladerf_linux *backend = (struct bladerf_linux *)dev->backend;
    uc.addr = address;
    uc.data = val;
    return ioctl(backend->fd, BLADE_LMS_WRITE, &uc);
}

/*------------------------------------------------------------------------------
 * GPIO register access
 *----------------------------------------------------------------------------*/
static int linux_gpio_read(struct bladerf *dev, uint32_t *val)
{
    int ret;
    int i;
    uint32_t rval;
    struct uart_cmd uc;
    struct bladerf_linux *backend = (struct bladerf_linux *)dev->backend;
    rval = 0;
    for (i = 0; i < 4; i++) {
        uc.addr = i;
        uc.data = 0xff;
        ret = ioctl(backend->fd, BLADE_GPIO_READ, &uc);
        if (ret) {
            if (errno == ETIMEDOUT) {
                ret = BLADERF_ERR_TIMEOUT;
            } else {
                ret = BLADERF_ERR_UNEXPECTED;
            }
            break;
        } else {
            rval |= uc.data << (i * 8);
        }
    }
    *val = rval;
    return ret;
}

static int linux_gpio_write(struct bladerf *dev, uint32_t val)
{
    struct uart_cmd uc;
    int ret;
    int i;
    struct bladerf_linux *backend = (struct bladerf_linux *)dev->backend;

    i = 0;

    /* If we're connected at HS, we need to use smaller DMA transfers */
    if (dev->speed == 0)
        val |= BLADERF_GPIO_FEATURE_SMALL_DMA_XFER;

    for (i = 0; i < 4; i++) {
        uc.addr = i;
        uc.data = val >> (i * 8);
        ret = ioctl(backend->fd, BLADE_GPIO_WRITE, &uc);
        if (ret) {
            if (errno == ETIMEDOUT) {
                ret = BLADERF_ERR_TIMEOUT;
            } else {
                ret = BLADERF_ERR_UNEXPECTED;
            }
            break;
        }
    }

    return ret;
}

/*------------------------------------------------------------------------------
 * VCTCXO DAC register write
 *----------------------------------------------------------------------------*/
static int linux_dac_write(struct bladerf *dev, uint16_t val)
{
    struct uart_cmd uc;
    uc.word = val;
    int i;
    int ret;
    struct bladerf_linux *backend = (struct bladerf_linux *)dev->backend;
    for (i = 0; i < 4; i++) {
        uc.addr = i;
        uc.data = val >> (i * 8);
        ret = ioctl(backend->fd, BLADE_VCTCXO_WRITE, &uc);
        if (ret)
            break;
    }
    return ret;
}

/*------------------------------------------------------------------------------
 * Data transfer
 *----------------------------------------------------------------------------*/
/* TODO Fail out if n > ssize_t max, as we can't return that. */
static ssize_t linux_tx(struct bladerf *dev, bladerf_format_t format,
                        void *samples, size_t n, struct bladerf_metadata *metadata)
//static ssize_t linux_write_samples(struct bladerf *dev, int16_t *samples, size_t n)
{
    ssize_t i, ret = 0;
    size_t bytes_written = 0;
    const size_t bytes_total = c16_samples_to_bytes(n);
    struct bladerf_linux *backend = (struct bladerf_linux *)dev->backend;
    int8_t *samples8 = (int8_t *)samples;

    while (bytes_written < bytes_total) {

        i = write(backend->fd, samples8 + bytes_written, bytes_total - bytes_written);

        if (i < 0 && errno != EINTR) {
            int errno_val = errno;
            bladerf_set_error(&dev->error, ETYPE_ERRNO, errno_val);
            bytes_written = BLADERF_ERR_IO;
            dbg_printf("Failed to write with errno=%d: %s\n",
                        errno_val, strerror(errno_val));
            break;
        } else if (i > 0) {
            bytes_written += i;
        } else {
            dbg_printf("\nInterrupted in bladerf_send_c16 (%zd/%zd\n",
                       bytes_written, bytes_total);
        }
    }

    if (ret < 0) {
        return ret;
    } else {
        return bytes_to_c16_samples(bytes_written);
    }
}

/* TODO Fail out if n > ssize_t max, as we can't return that */
static ssize_t linux_rx(struct bladerf *dev, bladerf_format_t format,
                        void *samples, size_t n, struct bladerf_metadata *metadata)
//static ssize_t linux_read_samples(struct bladerf *dev, int16_t *samples, size_t n)
{
    ssize_t i, ret = 0;
    size_t bytes_read = 0;
    const size_t bytes_total = c16_samples_to_bytes(n);
    struct bladerf_linux *backend = (struct bladerf_linux *)dev->backend;
    int8_t *samples8 = (int8_t *)samples;

    while (bytes_read < bytes_total) {

        i = read(backend->fd, samples8 + bytes_read, bytes_total - bytes_read);

        if (i < 0 && errno != EINTR) {
            int errno_val = errno;
            bladerf_set_error(&dev->error, ETYPE_ERRNO, errno_val);
            ret = BLADERF_ERR_IO;
            dbg_printf("Read failed with errno=%d: %s\n",
                        errno_val, strerror(errno_val));
            break;
        } else if (i > 0) {
            bytes_read += i;
        } else {
            dbg_printf("\nInterrupted in bladerf_read_c16 (%zd/%zd\n",
                       bytes_read, bytes_total);
        }
    }

    if (ret < 0) {
        return ret;
    } else {
        return bytes_to_c16_samples(bytes_read);
    }
}

/*------------------------------------------------------------------------------
 * Platform information
 *----------------------------------------------------------------------------*/

/* XXX: For realsies */
static int linux_get_fpga_version(struct bladerf *dev, unsigned int *maj, unsigned int *min)
{
    *min = *maj = 0;
    return 0;
}

/* XXX: For realsies */
static int linux_get_serial(struct bladerf *dev, uint64_t *serial)
{
    *serial = 0xdeadbeef;
    return 0;
}

/*------------------------------------------------------------------------------
 * Init/deinit
 *----------------------------------------------------------------------------*/


static struct bladerf * linux_open(struct bladerf_devinfo *info)
{
    char dev_name[32];
    struct bladerf_linux *backend;
    struct bladerf *ret = NULL;

    int fd; /* Everything here starts with a driver file descriptor,
             * so no need to allocate backend and ret until we know we
             * have said fd */

    assert(info->backend == BACKEND_LINUX);

    /* If an instance is specified, we start with that */
    if (info->instance != DEVINFO_INST_ANY) {
        snprintf(dev_name, sizeof(dev_name), "/dev/bladerf%d", info->instance);
        fd = open(dev_name, O_RDWR);

        if (fd >= 0) {
            backend = malloc(sizeof(*backend));
            ret = malloc(sizeof(*ret));

            if (backend && ret) {
                backend->fd = fd;
                ret->fn = &bladerf_linux_fn;
                ret->backend = backend;
            } else {
                free(backend);
                free(ret);
            }
        } else {
            dbg_printf("Failed to open %s: %s\n", dev_name, strerror(errno));
        }
    } else {
        /* Otherwise, we user our probe routine to get a device info list,
         * and then search it */
        dbg_printf("No instance specified- call to linux_probe() goes here\n");

        /* Probe */

        /* Iterate over list ... */
#if 0
        for each in list {

            /* Note that these indicate a match for any wildcards */
            if (instance_matches(a, b) &&
                usb_bus_addr_match(a, b) &&
                serial_matches(a, b)) {

                ret = linux_open(a)
                break;
            }


        }
#endif
    }

    return ret;
}

int linux_close(struct bladerf *dev)
{
    int status;
    struct bladerf_linux *backend = dev->backend;
    assert(backend);

    status = close(backend->fd);
    free(backend);
    free(dev);

    if (status < 0) {
        return BLADERF_ERR_IO;
    } else {
        return 0;
    }
}

/*------------------------------------------------------------------------------
 * Init/deinit
 *----------------------------------------------------------------------------*/

const struct bladerf_fn bladerf_linux_fn = {
    .open                   =   linux_open,
    .close                  =   linux_close,

    .load_fpga              =   linux_load_fpga,
    .is_fpga_configured     =   linux_is_fpga_configured,

    .flash_firmware         =   linux_flash_firmware,

    .get_serial             =   linux_get_serial,
    .get_fw_version         =   linux_get_fw_version,
    .get_fpga_version       =   linux_get_fpga_version,

    .gpio_write             =   linux_gpio_write,
    .gpio_read              =   linux_gpio_read,

    .si5338_write           =   linux_si5338_write,
    .si5338_read            =   linux_si5338_read,

    .lms_write              =   linux_lms_write,
    .lms_read               =   linux_lms_read,

    .dac_write              =   linux_dac_write,

    .rx                     =   linux_rx,
    .tx                     =   linux_tx
};

