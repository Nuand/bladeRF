#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <assert.h>
#include <time.h>
#include <stdbool.h>
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


static inline size_t min_sz(size_t x, size_t y)
{
    return x < y ? x : y;
}

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

    /* Successfully opened, so save off the path */
    if( (ret->path = strdup(dev_path)) == NULL) {
        /* Not so successful string duplication */
        goto bladerf_open__err;
    }

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

char * bladerf_dev_path(struct bladerf *dev)
{
    if( dev ) {
        return dev->path;
    } else {
        return NULL;
    }
}

struct bladerf * bladerf_open(const char *dev_path)
{
    struct bladerf_devinfo i;

    /* Use open with devinfo check to verify that this is actually a bladeRF */
    return _bladerf_open_info(dev_path, &i);
}

struct bladerf * bladerf_open_any()
{
    struct bladerf *ret = NULL;
    struct bladerf_devinfo *devices;
    ssize_t n_devices;

    n_devices = bladerf_get_device_list(&devices);
    if (n_devices > 0) {
        ret = bladerf_open(devices[0].path);
        bladerf_free_device_list(devices, n_devices);
    }

    return ret;
}


void bladerf_close(struct bladerf *dev)
{
    if (dev) {
        close(dev->fd);
        free(dev->path);
        free(dev);
    }
}

int bladerf_set_loopback(struct bladerf *dev, bladerf_loopback l)
{
    lms_loopback_enable( dev, l );
    return 0;
}

int bladerf_set_sample_rate(struct bladerf *dev, bladerf_module module, unsigned int rate, unsigned int *actual)
{
    /* TODO: Program the Si5338 to be 2x the desired sample rate */
    int ret = -1;
    /* TODO: Use module to pick the correct clock output to change */
    if( module == TX ) {
        ret = si5338_set_tx_freq(dev, rate);
    } else {
        ret = si5338_set_rx_freq(dev, rate);
    }
    *actual = rate;
    return ret;
}

int bladerf_set_rational_sample_rate(struct bladerf *dev, bladerf_module module, unsigned int integer, unsigned int num, unsigned int denom)
{
    /* TODO: Program the Si5338 to be 2x the desired sample rate */
    return 0;
}

int bladerf_get_sample_rate( struct bladerf *dev, bladerf_module module, unsigned int *rate)
{
    /* TODO: Read the Si5338 and figure out the sample rate */
    return 0;
}

int bladerf_get_rational_sample_rate(struct bladerf *dev, bladerf_module module, unsigned int integer, unsigned int num, unsigned int denom)
{
    /* TODO: Read the Si5338 and figure out the sample rate */
    return 0;
}

int bladerf_set_txvga2(struct bladerf *dev, int gain)
{
    if( gain > 25 ) {
        fprintf( stderr, "%s: %d being clamped to 25dB\n", __FUNCTION__, gain );
        gain = 25;
    }
    if( gain < 0 ) {
        fprintf( stderr, "%s: %d being clamped to 0dB\n", __FUNCTION__, gain );
        gain = 0;
    }
    /* TODO: Make return values for lms call and return it for failure */
    lms_txvga2_set_gain( dev, gain );
    return 0;
}

int bladerf_get_txvga2(struct bladerf *dev, int *gain)
{
    *gain = 0;
    /* TODO: Make return values for lms call and return it for failure */
    lms_txvga2_get_gain( dev, (uint8_t *)gain );
    return 0;
}

int bladerf_set_txvga1(struct bladerf *dev, int gain)
{
    if( gain < -35 ) {
        fprintf( stderr, "%s: %d being clamped to -35dB\n", __FUNCTION__, gain );
        gain = -35;
    }
    if( gain > -4 ) {
        fprintf( stderr, "%s: %d being clamped to -4dB\n", __FUNCTION__, gain );
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

int bladerf_set_lna_gain(struct bladerf *dev, bladerf_lna_gain gain)
{
    /* TODO: Make return values for lms call and return it for failure */
    lms_lna_set_gain( dev, gain );
    return 0;
}

int bladerf_get_lna_gain(struct bladerf *dev, bladerf_lna_gain *gain)
{
    /* TODO: Make return values for lms call and return it for failure */
    lms_lna_get_gain( dev, gain );
    return 0;
}

int bladerf_set_rxvga1(struct bladerf *dev, int gain)
{
    /* TODO: Make return values for lms call and return it for failure */
    lms_rxvga1_set_gain( dev, (uint8_t)gain );
    return 0;
}

int bladerf_get_rxvga1(struct bladerf *dev, int *gain)
{
    *gain = 0;
    /* TODO: Make return values for lms call and return it for failure */
    lms_rxvga1_get_gain( dev, (uint8_t *)gain );
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
    *gain = 0;
    /* TODO: Make return values for lms call and return it for failure */
    lms_rxvga2_get_gain( dev, (uint8_t *)gain );
    /* Value returned from LMS lib is just the raw value, which is 3dB/unit */
    *gain = *gain*3 ;
    return 0;
}

int bladerf_set_bandwidth(struct bladerf *dev, bladerf_module module,
                            unsigned int bandwidth,
                            unsigned int *actual)
{
    /* TODO: Make return values for lms call and return it for failure */
    lms_bw_t bw = lms_uint2bw(bandwidth);
    lms_lpf_enable( dev, module, bw );
    *actual = lms_bw2uint(bw);
    return 0;
}

int bladerf_get_bandwidth(struct bladerf *dev, bladerf_module module,
                            unsigned int *bandwidth )
{
    /* TODO: Make return values for lms call and return it for failure */
    lms_bw_t bw = lms_get_bandwidth( dev, module );
    *bandwidth = lms_bw2uint(bw);
    return 0;
}

int bladerf_set_frequency(struct bladerf *dev,
                            bladerf_module module, unsigned int frequency)
{
    /* TODO: Make return values for lms call and return it for failure */
    lms_set_frequency( dev, module, frequency );
    return 0;
}

int bladerf_get_frequency(struct bladerf *dev,
                            bladerf_module module, unsigned int *frequency)
{
    /* TODO: Make return values for lms call and return it for failure */
    struct lms_freq f;
    lms_get_frequency( dev, module, &f );
    *frequency = (unsigned int)(((uint64_t)((f.nint<<23) + f.nfrac)) * (f.reference/f.x) >>23);
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
    return read( dev->fd, samples, max_samples );
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
 * Device programming
 *----------------------------------------------------------------------------*/

int bladerf_flash_firmware(struct bladerf *dev, const char *firmware)
{
    int fw_fd, upgrade_status, ret;
    struct stat fw_stat;
    struct bladeRF_firmware fw_param;
    ssize_t n_read, read_status;

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
    upgrade_status = ioctl(dev->fd, BLADE_UPGRADE_FW, &fw_param);
    if (upgrade_status < 0) {
        dbg_printf("Firmware upgrade failed: %s\n", strerror(errno));
        ret = BLADERF_ERR_UNEXPECTED;
    }

    free(fw_param.ptr);
    return ret;
}

static bool time_past(struct timeval ref, struct timeval now) {
    if (now.tv_sec > ref.tv_sec)
        return true;

    if (now.tv_sec == ref.tv_sec && now.tv_usec > ref.tv_usec)
        return true;

    return false;
}

#define STACK_BUFFER_SZ 1024
int bladerf_load_fpga(struct bladerf *dev, const char *fpga)
{
    int ret, fpga_status, fpga_fd;
    ssize_t nread, written, write_tmp;
    size_t bytes;
    struct stat stat;
    char buf[STACK_BUFFER_SZ];
    struct timeval end_time, curr_time;
    bool timed_out;

    assert(dev && fpga);

    /* TODO Check FPGA on the board versus size of image */

    if (ioctl(dev->fd, BLADE_QUERY_FPGA_STATUS, &fpga_status) < 0) {
        dbg_printf("ioctl(BLADE_QUERY_FPGA_STATUS) failed: %s\n",
                    strerror(errno));
        return BLADERF_ERR_UNEXPECTED;
    }

    /* FPGA is already programmed */
    if (fpga_status) {
        printf( "FPGA aleady loaded - reloading!\n" );
    }

    fpga_fd = open(fpga, 0);
    if (fpga_fd < 0) {
        dbg_printf("Failed to open device (%s): %s\n", fpga, strerror(errno));
        return BLADERF_ERR_IO;
    }

    if (fstat(fpga_fd, &stat) < 0) {
        dbg_printf("Failed to stat fpga file (%s): %s\n", fpga, strerror(errno));
        close(fpga_fd);
        return BLADERF_ERR_IO;
    }

    if (ioctl(dev->fd, BLADE_BEGIN_PROG, &fpga_status)) {
        dbg_printf("ioctl(BLADE_BEGIN_PROG) failed: %s\n", strerror(errno));
        close(fpga_fd);
        return BLADERF_ERR_UNEXPECTED;
    }

    for (bytes = stat.st_size; bytes;) {
        nread = read(fpga_fd, buf, min_sz(STACK_BUFFER_SZ, bytes));

        written = 0;
        do {
            write_tmp = write(dev->fd, buf + written, nread - written);
            if (write_tmp < 0) {
                /* Failing out...at least attempt to "finish" programming */
                ioctl(dev->fd, BLADE_END_PROG, &ret);
                dbg_printf("Write failure: %s\n", strerror(errno));
                close(fpga_fd);
                return BLADERF_ERR_IO;
            } else {
                written += write_tmp;
            }
        } while(written < nread);

        bytes -= nread;

        /* FIXME? Perhaps it would be better if the driver blocked on the
         * write call, rather than sleeping in userspace? */
        usleep(4000);
    }
    close(fpga_fd);

    /* Debug mode bug catcher */
    assert(bytes == 0);

    /* Time out within 1 second */
    gettimeofday(&end_time, NULL);
    end_time.tv_sec++;

    ret = 0;
    do {
        if (ioctl(dev->fd, BLADE_QUERY_FPGA_STATUS, &fpga_status) < 0) {
            dbg_printf("Failed to query FPGA status: %s\n", strerror(errno));
            ret = BLADERF_ERR_UNEXPECTED;
        }
        gettimeofday(&curr_time, NULL);
        timed_out = time_past(end_time, curr_time);
    } while(!fpga_status && !timed_out && !ret);

    if (ioctl(dev->fd, BLADE_END_PROG, &fpga_status)) {
        dbg_printf("Failed to end programming procedure: %s\n",
                strerror(errno));

        /* Don't clobber a previous error */
        if (!ret)
            ret = BLADERF_ERR_UNEXPECTED;
    }

    return ret;
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

/*------------------------------------------------------------------------------
 * GPIO register read / write functions
 */
int gpio_read(struct bladerf *dev, uint32_t *val)
{
    int ret;
    struct uart_cmd uc;
    uc.addr = 0;
    uc.data = 0xff;
    ret = ioctl(dev->fd, BLADE_GPIO_READ, &uc);
    *val = uc.data;
    return ret;
}

int gpio_write(struct bladerf *dev, uint32_t val)
{
    struct uart_cmd uc;
    uc.addr = 0;
    uc.data = val;
    return ioctl(dev->fd, BLADE_GPIO_WRITE, &uc);
}
