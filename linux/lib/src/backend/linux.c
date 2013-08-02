#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>

#include "bladeRF.h"
#include "libbladeRF.h"
#include "bladerf_priv.h"
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

/* TODO clean this up with some labels for erroring out */
/* XXX: This function definition changed so it needs to be revisited */
int linux_load_fpga(struct bladerf *dev, uint8_t *image, size_t image_size)
{
    int ret, fpga_status;
    size_t written;         /* Total # of bytes written */
    ssize_t write_tmp;      /* # bytes written in a single write() call */
    struct timeval end_time, curr_time;
    bool timed_out;
    struct bladerf_linux *backend = (struct bladerf_linux *)dev->backend;
    assert(dev && image);

    /* TODO Check FPGA on the board versus size of image */

    if (ioctl(backend->fd, BLADE_QUERY_FPGA_STATUS, &fpga_status) < 0) {
        dbg_printf("ioctl(BLADE_QUERY_FPGA_STATUS) failed: %s\n",
                    strerror(errno));
        return BLADERF_ERR_UNEXPECTED;
    }

    /* FPGA is already programmed */
    if (fpga_status) {
        fprintf( stderr, "FPGA aleady loaded - reloading!\n" );
    }

    if (ioctl(backend->fd, BLADE_BEGIN_PROG, &fpga_status)) {
        dbg_printf("ioctl(BLADE_BEGIN_PROG) failed: %s\n", strerror(errno));
        return BLADERF_ERR_UNEXPECTED;
    }

    written = 0;
    do {
        write_tmp = write(backend->fd, image + written, image_size - written);
        if (write_tmp < 0) {
            /* Failing out...at least attempt to "finish" programming */
            ioctl(backend->fd, BLADE_END_PROG, &ret);
            dbg_printf("Write failure: %s\n", strerror(errno));
            return BLADERF_ERR_IO;
        } else {
            written += write_tmp;
        }
    } while(written < image_size);


    /* FIXME? Perhaps it would be better if the driver blocked on the
     * write call, rather than sleeping in userspace? */
    usleep(4000);

    /* Time out within 1 second */
    gettimeofday(&end_time, NULL);
    end_time.tv_sec++;

    ret = 0;
    do {
        if (ioctl(backend->fd, BLADE_QUERY_FPGA_STATUS, &fpga_status) < 0) {
            dbg_printf("Failed to query FPGA status: %s\n", strerror(errno));
            ret = BLADERF_ERR_UNEXPECTED;
        }
        gettimeofday(&curr_time, NULL);
        timed_out = time_past(end_time, curr_time);
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


int linux_flash_firmware(struct bladerf *dev, const char *firmware)
{
    int fw_fd, upgrade_status, ret;
    struct stat fw_stat;
    struct bladeRF_firmware fw_param;
    ssize_t n_read, read_status;
    struct bladerf_linux *backend = (struct bladerf_linux*)backend ;

    assert(dev && firmware);

    fw_fd = open(firmware, O_RDONLY);
    if (fw_fd < 0) {
        dbg_printf("Failed to open firmware file: %s\n", strerror(errno));
        return BLADERF_ERR_IO;
    }

    if (fstat(fw_fd, &fw_stat) < 0) {
        dbg_printf("Failed to stat firmware file: %s\n", strerror(errno));
        close(fw_fd);
        return BLADERF_ERR_IO;
    }

    fw_param.len = fw_stat.st_size;

    /* Quick sanity check: We know the firmware file is roughly 100K
     * Env var is a quick opt-out of this check - can it ever get this large?
     *
     * TODO: Query max flash size for upper bound?
     */
    if (!getenv("BLADERF_SKIP_FW_SIZE_CHECK") &&
            (fw_param.len < (50 * 1024) || fw_param.len > (1 * 1024 * 1024))) {
        dbg_printf("Detected potentially invalid firmware file. Aborting!\n");
        close(fw_fd);
        return BLADERF_ERR_INVAL;
    }

    fw_param.ptr = malloc(fw_param.len);
    if (!fw_param.ptr) {
        dbg_printf("Failed to allocate firmware buffer: %s\n", strerror(errno));
        close(fw_fd);
        return BLADERF_ERR_MEM;
    }

    n_read = 0;
    do {
        read_status = read(fw_fd, fw_param.ptr + n_read, fw_param.len - n_read);
        if (read_status < 0) {
            dbg_printf("Failed to read firmware file: %s\n", strerror(errno));
            free(fw_param.ptr);
            close(fw_fd);
            return BLADERF_ERR_IO;
        } else {
            n_read += read_status;
        }
    } while(n_read < fw_param.len);
    close(fw_fd);

    /* Bug catcher */
    assert(n_read == fw_param.len);

    ret = 0;
    upgrade_status = ioctl(backend->fd, BLADE_UPGRADE_FW, &fw_param);
    if (upgrade_status < 0) {
        dbg_printf("Firmware upgrade failed: %s\n", strerror(errno));
        ret = BLADERF_ERR_UNEXPECTED;
    }

    free(fw_param.ptr);
    return ret;
}

int linux_get_fw_version(struct bladerf *dev,
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

/*------------------------------------------------------------------------------
 * Si5338 register read / write functions
 */

int linux_si5338_read(struct bladerf *dev, uint8_t address, uint8_t *val)
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

int linux_si5338_write(struct bladerf *dev, uint8_t address, uint8_t val)
{
    struct uart_cmd uc;
    struct bladerf_linux *backend = (struct bladerf_linux *)dev->backend;
    uc.addr = address;
    uc.data = val;
    return ioctl(backend->fd, BLADE_SI5338_WRITE, &uc);
}

/*------------------------------------------------------------------------------
 * LMS register read / write functions
 */

int linux_lms_read(struct bladerf *dev, uint8_t address, uint8_t *val)
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

int linux_lms_write(struct bladerf *dev, uint8_t address, uint8_t val)
{
    struct uart_cmd uc;
    struct bladerf_linux *backend = (struct bladerf_linux *)dev->backend;
    uc.addr = address;
    uc.data = val;
    return ioctl(backend->fd, BLADE_LMS_WRITE, &uc);
}

/*------------------------------------------------------------------------------
 * GPIO register read / write functions
 */
int linux_gpio_read(struct bladerf *dev, uint32_t *val)
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

int linux_gpio_write(struct bladerf *dev, uint32_t val)
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
 */
int linux_dac_write(struct bladerf *dev, uint16_t val)
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

/* TODO Fail out if n > ssize_t max, as we can't return that. */
ssize_t linux_write_samples(struct bladerf *dev, int16_t *samples, size_t n)
{
    ssize_t i, ret = 0;
    size_t bytes_written = 0;
    const size_t bytes_total = c16_samples_to_bytes(n);
    struct bladerf_linux *backend = (struct bladerf_linux *)dev->backend;

    while (bytes_written < bytes_total) {

        i = write(backend->fd, samples + bytes_written, bytes_total - bytes_written);

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
ssize_t linux_read_samples(struct bladerf *dev,
                            int16_t *samples, size_t n)
{
    ssize_t i, ret = 0;
    size_t bytes_read = 0;
    const size_t bytes_total = c16_samples_to_bytes(n);
    struct bladerf_linux *backend = (struct bladerf_linux *)dev->backend;

    while (bytes_read < bytes_total) {

        i = read(backend->fd, samples + bytes_read, bytes_total - bytes_read);

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

/* XXX: For realsies */
int linux_get_fpga_version(struct bladerf *dev, unsigned int *maj, unsigned int *min)
{
    *min = *maj = 0;
    return 0;
}

/* XXX: For realsies */
int linux_get_serial(struct bladerf *dev, uint64_t *serial)
{
    *serial = 0xdeadbeef;
    return 0;
}

int linux_close(struct bladerf *dev)
{
    return 0;
}

const struct bladerf_fn bladerf_linux_fn = {
    .load_fpga              =   linux_load_fpga,
    .is_fpga_configured     =   linux_is_fpga_configured,

    .get_serial             =   linux_get_serial,
    .get_fw_version         =   linux_get_fw_version,
    .get_fpga_version       =   linux_get_fpga_version,

    .close                  =   linux_close,

    .gpio_write             =   linux_gpio_write,
    .gpio_read              =   linux_gpio_read,

    .si5338_write           =   linux_si5338_write,
    .si5338_read            =   linux_si5338_read,

    .lms_write              =   linux_lms_write,
    .lms_read               =   linux_lms_read,

    .dac_write              =   linux_dac_write,

    .write_samples          =   linux_write_samples,
    .read_samples           =   linux_read_samples
};

