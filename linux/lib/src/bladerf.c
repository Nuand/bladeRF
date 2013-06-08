#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <assert.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "bladeRF.h"        /* Driver interface */
#include "libbladeRF.h"     /* API */
#include "bladerf_priv.h"   /* Implementation-specific items ("private") */
#include "debug.h"

#ifndef BLADERF_DEV_DIR
#   define BLADERF_DEV_DIR "/dev/"
#endif

#ifndef BLADERF_DEV_PFX
#   define BLADERF_DEV_PFX  "bladerf"
#endif

/*******************************************************************************
 * Device discovery & initialization/deinitialization
 ******************************************************************************/

/* Return 0 if dirent name matches that of what we expect for a bladerf dev */
static int bladerf_filter(const struct dirent *d)
{
    const size_t pfx_len = strlen(BLADERF_DEV_PFX);
    long int tmp;
    char *endptr;

    if (strlen(d->d_name) > pfx_len &&
        !strncmp(d->d_name, BLADERF_DEV_PFX, pfx_len)) {

        /* Is the remainder of the entry a valid (positive) integer? */
        tmp = strtol(&d->d_name[pfx_len], &endptr, 10);

        /* Nope */
        if (*endptr != '\0' || tmp < 0 ||
            (errno == ERANGE && (tmp == LONG_MAX || tmp == LONG_MIN)))
            return 0;

        /* Looks like a bladeRF by name... we'll check more later. */
        return 1;
    }

    return 0;
}

/* Helper routine for freeing dirent list from scandir() */
static inline void free_dirents(struct dirent **d, int n)
{
    if (d && n > 0 ) {
        while (n--)
            free(d[n]);
        free(d);
    }
}

/* Open and if a non-NULL bladerf_devinfo ptr is provided, attempt to verify
 * that the device we opened is a bladeRF via a info calls.
 * (Does not fill out devinfo's path) */
struct bladerf * _bladerf_open_info(const char *dev_path,
                                    struct bladerf_devinfo *i)
{
    struct bladerf *ret;

    ret = malloc(sizeof(*ret));
    if (!ret)
        return NULL;

    /* TODO -- spit out error/warning message to assist in debugging
     * device node permissions issues?
     */
    if ((ret->fd = open(dev_path, O_RDWR)) < 0)
        goto bladerf_open__err;

    /* TODO -- spit our errors/warning here depending on library verbosity? */
    if (i) {
        if (bladerf_get_serial(ret, &i->serial) < 0)
            goto bladerf_open__err;

        i->fpga_configured = bladerf_is_fpga_configured(ret);
        if (i->fpga_configured < 0)
            goto bladerf_open__err;

        if (bladerf_get_fw_version(ret, &i->fw_ver_maj, &i->fw_ver_min) < 0)
            goto bladerf_open__err;

        if (bladerf_get_fpga_version(ret,
                                     &i->fpga_ver_maj, &i->fpga_ver_min) < 0)
            goto bladerf_open__err;
    }

    return ret;

bladerf_open__err:
    free(ret);
    return NULL;
}

ssize_t bladerf_get_device_list(struct bladerf_devinfo **devices)
{
    struct bladerf_devinfo *ret;
    ssize_t num_devices;
    struct dirent **matches;
    int num_matches, i;
    struct bladerf *dev;
    char *dev_path;

    ret = NULL;
    num_devices = 0;

    num_matches = scandir(BLADERF_DEV_DIR, &matches, bladerf_filter, alphasort);
    if (num_matches > 0) {

        ret = malloc(num_matches * sizeof(*ret));
        if (!ret) {
            num_devices = BLADERF_ERR_MEM;
            goto bladerf_get_device_list_out;
        }

        for (i = 0; i < num_matches; i++) {
            dev_path = malloc(strlen(BLADERF_DEV_DIR) +
                                strlen(matches[i]->d_name) + 1);

            if (dev_path) {
                strcpy(dev_path, BLADERF_DEV_DIR);
                strcat(dev_path, matches[i]->d_name);

                dev = _bladerf_open_info(dev_path, &ret[num_devices]);

                if (dev) {
                    ret[num_devices++].path = dev_path;
                    bladerf_close(dev);
                } else
                    free(dev_path);
            } else {
                num_devices = BLADERF_ERR_MEM;
                goto bladerf_get_device_list_out;
            }
        }
    }


bladerf_get_device_list_out:
    *devices = ret;
    free_dirents(matches, num_matches);
    return num_devices;
}

void bladerf_free_device_list(struct bladerf_devinfo *devices, size_t n)
{
    size_t i;

    if (devices) {
        for (i = 0; i < n; i++)
            free(devices[i].path);
        free(devices);
    }
}

struct bladerf * bladerf_open(const char *dev_path)
{
    struct bladerf_devinfo i;

    /* Use open with devinfo check to verify that this is actually a bladeRF */
    return _bladerf_open_info(dev_path, &i);
}

void bladerf_close(struct bladerf *dev)
{
    if (dev) {
        close(dev->fd);
        free(dev);
    }
}

int bladerf_set_loopback(struct bladerf *dev, bladerf_loopback l)
{
    return 0;
}

int bladerf_set_sample_rate(struct bladerf *dev, unsigned int rate)
{
    return 0;
}


int bladerf_set_txvga2(struct bladerf *dev, int gain)
{
    return 0;
}

int bladerf_set_txvga1(struct bladerf *dev, int gain)
{
    return 0;
}

int bladerf_set_lna_gain(struct bladerf *dev, bladerf_lna_gain gain)
{
    return 0;
}

int bladerf_set_rxvga1(struct bladerf *dev, int gain)
{
    return 0;
}

int bladerf_set_rxvga2(struct bladerf *dev, int gain)
{
    return 0;
}

int bladerf_set_bandwidth(struct bladerf *dev, unsigned int bandwidth,
                            unsigned int *bandwidth_actual)
{
    int ret = -1;
    ret = si5338_set_tx_freq(dev, bandwidth);
    if (!ret)
        ret = si5338_set_rx_freq(dev, bandwidth);
    *bandwidth_actual = bandwidth;
    return ret;
}

int bladerf_set_frequency(struct bladerf *dev,
                            bladerf_module module, unsigned int frequency)
{
    return 0;
}

/*******************************************************************************
 * Data transmission and reception
 ******************************************************************************/

ssize_t bladerf_send_c12(struct bladerf *dev, int16_t *samples, size_t n)
{
    return 0;
}

ssize_t bladerf_send_c16(struct bladerf *dev, int16_t *samples, size_t n)
{
    return 0;
}

ssize_t bladerf_read_c16(struct bladerf *dev,
                            int16_t *samples, size_t max_samples)
{
    return 0;
}

/*******************************************************************************
 * Device info
 ******************************************************************************/

/* TODO - Devices do not currently support serials */
int bladerf_get_serial(struct bladerf *dev, uint64_t *serial)
{
    assert(dev && serial);

    *serial = 0;
    return 0;
}

int bladerf_is_fpga_configured(struct bladerf *dev)
{
    int status;
    int configured;

    assert(dev);

    status = ioctl(dev->fd, BLADE_QUERY_FPGA_STATUS, &configured);

    if (status || configured < 0 || configured > 1)
        configured = BLADERF_ERR_IO;

    return configured;
}

/* TODO Not yet supported */
int bladerf_get_fpga_version(struct bladerf *dev,
                                unsigned int *major, unsigned int *minor)
{
    assert(dev && major && minor);

    *major = 0;
    *minor = 0;

    return 0;
}

int bladerf_get_fw_version(struct bladerf *dev,
                            unsigned int *major, unsigned int *minor)
{
    int status;
    struct bladeRF_version ver;

    assert(dev && major && minor);

    status = ioctl(dev->fd, BLADE_QUERY_VERSION, &ver);
    if (!status) {
        *major = ver.major;
        *minor = ver.minor;
        return 0;
    }

    /* TODO return more appropriate error code based upon errno */
    return BLADERF_ERR_IO;
}

/*------------------------------------------------------------------------------
 * Device programming
 *----------------------------------------------------------------------------*/

/* TODO Unimplemented: flashing FX3 firmware */
int bladerf_flash_firmware(struct bladerf *dev, const char *firmware)
{
    assert(dev && firmware);
    return 0;
}

static int time_past(struct timeval ref, struct timeval now) {
    if (now.tv_sec > ref.tv_sec)
        return 1;

    if (now.tv_sec == ref.tv_sec && now.tv_usec > ref.tv_usec)
        return 1;

    return 0;
}

#define STACK_BUFFER_SZ 1024

/* TODO Unimplemented: loading FPGA */
int bladerf_load_fpga(struct bladerf *dev, const char *fpga)
{
    int ret;
    int fpga_fd, nread;
    int programmed;
    size_t bytes;
    struct stat stat;
    char buf[STACK_BUFFER_SZ];
    struct timeval end_time, curr_time;
    struct timespec ts;

    assert(dev && fpga);

    fpga_fd = open(fpga, 0);
    if (fpga_fd == -1)
        return 1;
    ioctl(dev->fd, BLADE_BEGIN_PROG, &ret);
    fstat(fpga_fd, &stat);

#define min(x,y) ((x<y)?x:y)
    for (bytes = stat.st_size; bytes;) {
        nread = read(fpga_fd, buf, min(STACK_BUFFER_SZ, bytes));
        write(dev->fd, buf, nread);
        bytes -= nread;
        ts.tv_sec = 0;
        ts.tv_nsec = 4000000;
        nanosleep(&ts, NULL);
    }
    close(fpga_fd);

    gettimeofday(&end_time, NULL);
    end_time.tv_sec++;

    programmed = 0;
    do {
        ioctl(dev->fd, BLADE_QUERY_FPGA_STATUS, &ret);
        if (ret) {
            programmed = 1;
            break;
        }
        gettimeofday(&curr_time, NULL);
    } while(!time_past(end_time, curr_time));

    ioctl(dev->fd, BLADE_END_PROG, &ret);

    return !programmed;
}

/*------------------------------------------------------------------------------
 * Si5338 register read / write functions
 */

int si5338_i2c_write(struct bladerf *dev, uint8_t address, uint8_t val)
{
    struct uart_cmd uc;
    uc.addr = address;
    uc.data = val;
    return ioctl(dev->fd, BLADE_SI5338_WRITE, &uc);
}

/*------------------------------------------------------------------------------
 * LMS register read / write functions
 */

int lms_spi_read(struct bladerf *dev, uint8_t address, uint8_t *val)
{
    int ret;
    struct uart_cmd uc;
    address &= 0x7f;
    uc.addr = address;
    uc.data = 0xff;
    ret = ioctl(dev->fd, BLADE_LMS_READ, &uc);
    *val = uc.data;
    return ret;
}

int lms_spi_write(struct bladerf *dev, uint8_t address, uint8_t val)
{
    struct uart_cmd uc;
    uc.addr = address;
    uc.data = val;
    return ioctl(dev->fd, BLADE_LMS_WRITE, &uc);
}
