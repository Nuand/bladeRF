#include <fcntl.h>
#include <unistd.h>
#include <ftw.h>
#include <sys/ioctl.h>

/* TODO    v--- Something here needs to be renamed! */
#include "bladerf.h"    /* C API */
#include "bladeRF.h"    /* Kernel module interface */
#include "debug.h"

#ifndef BLADERF_DEV_DIR
#   define BLADERF_DEV_DIR "/dev/"
#endif

#ifndef BLADERF_DEV_PFX
#   define BLADERF_PREFIX  "bladerf"
#endif

/* TODO Should there be a "big-lock" wrapped around accesses to a device */
struct bladerf {
    int fd;   /* File descriptor to associated driver device node */
    struct bladerf_stats stats;
};

static int check_entry(const char *fpath, const struct stat *sb,
                        int typeflag, struct FTW *ftwbuf)
{
    dbg_printf("Hit with fpath = %s\n", fpath);
    return 0;
}

ssize_t bladerf_get_device_list(struct bladerf_devinfo **devices)
{
    struct bladerf_devinfo *ret;
    size_t num_devices = 0;
    size_t devices_array_sz = 3; /* Initial guess. Realloc as neccessary */

    ret = calloc(devices_array_sz, sizeof(*ret));
    if (!ret) {
        *devices = NULL;
        return BLADERF_ERR_MEM;
    }

    if (nftw(BLADERF_DEV_DIR, check_entry, 256, FTW_F) < 0)
        goto bladerf_get_device_list_err;

    *devices = ret;
    return num_devices;

bladerf_get_device_list_err:
    free(ret);
    *devices = NULL;
    return 0;
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
    struct bladerf *ret = NULL;

    int fd = open(dev_path, O_RDWR);

    if (fd < 0)
        return ret;

    /* Test that this device is actually a bladeRF */
    return ret;
}

void bladerf_close(struct bladerf *dev)
{
    if (dev) {
        close(dev->fd);
        free(dev);
    }
}

int bladerf_set_loopback( struct bladerf *dev, enum bladerf_loopback l)
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

int bladerf_set_lna_gain(struct bladerf *dev, enum bladerf_lna_gain gain)
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
    return 0;
}

int bladerf_set_frequency(struct bladerf *dev,
                            enum bladerf_module module, unsigned int frequency)
{
    return 0;
}

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

int bladerf_get_serial(struct bladerf *dev, uint64_t *serial)
{
    return 0;
}

int bladerf_get_fw_version(struct bladerf *dev,
                            unsigned int *major, unsigned int minor)
{
    return 0;
}
