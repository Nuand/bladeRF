/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2013 Nuand LLC
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/time.h>
#include <dirent.h>

#include "bladeRF.h"
#include "libbladeRF.h"
#include "bladerf_priv.h"
#include "backend/linux.h"
#include "conversions.h"
#include "minmax.h"
#include "log.h"

#ifndef BLADERF_DEV_DIR
#   define BLADERF_DEV_DIR "/dev/"
#endif

#ifndef BLADERF_DEV_PFX
#   define BLADERF_DEV_PFX  "bladerf"
#endif

/* Linux device driver information */
struct bladerf_linux {
    int fd;         /* File descriptor to associated driver device node */
};

const struct bladerf_fn bladerf_linux_fn;

static bool time_past(struct timeval ref, struct timeval now) {
    if (now.tv_sec > ref.tv_sec)
        return true;

    if (now.tv_sec == ref.tv_sec && now.tv_usec > ref.tv_usec)
        return true;

    return false;
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

static inline int linux_begin_fpga_programming(int fd)
{
    int status = 0;
    int fpga_status;
    if (ioctl(fd, BLADE_BEGIN_PROG, &fpga_status)) {
        log_error("ioctl(BLADE_BEGIN_PROG) failed: %s\n", strerror(errno));
        status = BLADERF_ERR_UNEXPECTED;
    }

    return status;
}

static int linux_end_fpga_programming(int fd)
{
    int status = 0;
    int fpga_status;
    if (ioctl(fd, BLADE_END_PROG, &fpga_status)) {
        log_error("Failed to end programming procedure: %s\n",
                  strerror(errno));
        status = BLADERF_ERR_UNEXPECTED;
    }

    return status;
}

static int linux_load_fpga(struct bladerf *dev,
                           uint8_t *image, size_t image_size)
{
    int ret, fpga_status, end_prog_status;
    size_t written = 0;     /* Total # of bytes written */
    size_t to_write;
    ssize_t write_tmp;      /* # bytes written in a single write() call */
    struct timeval end_time, curr_time;
    bool timed_out;
    struct bladerf_linux *backend = (struct bladerf_linux *)dev->backend;
    assert(dev && image);

    ret = linux_begin_fpga_programming(backend->fd);
    if (ret < 0) {
        return ret;
    }

    /* FIXME This loops is just here to work around the fact that the
     *       driver currently can't handle large writes... */
    while (written < image_size) {
        do {
            to_write = min_sz(1024, image_size - written);
            write_tmp = write(backend->fd, image + written, to_write);
            if (write_tmp < 0) {
                /* Failing out...at least attempt to "finish" programming */
                ioctl(backend->fd, BLADE_END_PROG, &ret);
                log_error("Write failure: %s\n", strerror(errno));
                return BLADERF_ERR_IO;
            } else {
                assert(write_tmp == (ssize_t)to_write);
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


    end_prog_status = linux_end_fpga_programming(backend->fd);

    /* Return the first error encountered */
    if (end_prog_status < 0 && ret == 0) {
        ret = end_prog_status;
    }

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
        log_error("Firmware upgrade failed: %s\n", strerror(errno));
        ret = BLADERF_ERR_UNEXPECTED;
    }

    return ret;
}
static int linux_device_reset(struct bladerf *dev)
{
    int ret = 0;
    assert(dev);
    struct bladerf_linux *backend = (struct bladerf_linux*)dev->backend;

    ret = ioctl(backend->fd, BLADE_DEVICE_RESET, NULL);
    return ret;
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
static int linux_config_gpio_read(struct bladerf *dev, uint32_t *val)
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

static int linux_config_gpio_write(struct bladerf *dev, uint32_t val)
{
    struct uart_cmd uc;
    int ret;
    int i;
    struct bladerf_linux *backend = (struct bladerf_linux *)dev->backend;

    i = 0;

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
 * Set IQ Correction parameters
 *----------------------------------------------------------------------------*/
static int linux_config_dc_correction_write(struct bladerf *dev, int16_t dc_i, int16_t dc_q)
{
    int i = 0;
    int status = 0;
    uint32_t tmp_data;
    struct uart_cmd cmd;
    struct bladerf_lusb *lusb = dev->backend;

    tmp_data = (dc_i << 16 )| (dc_q);

    for (i = 0; status == 0 && i < 4; i++) {
        cmd.addr = i + UART_PKT_DEV_DC_CORR_ADDR;

        cmd.data = (tmp_data>>(8*i))&0xff;
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

static int linux_config_set_gain_phase_correction(struct bladerf *dev, int16_t gain, int16_t phase)
{
    int i = 0;
    int status = 0;
    uint32_t tmp_data;
    struct uart_cmd cmd;
    struct bladerf_lusb *lusb = dev->backend;

    tmp_data = ((uint32_t) phase << 16 )| (gain);

    for (i = 0; status == 0 && i < 4; i++) {
        cmd.addr = i + UART_PKT_DEV_GAIN_PHASE_CORR_ADDR;

        cmd.data = (tmp_data>>(8*i))&0xff;
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


static int linux_get_dc_correction(struct bladerf *dev, int16_t *dc_real, int16_t *dc_imag)
{
    int i = 0;
    int status = 0;
    struct uart_cmd cmd;
    struct bladerf_lusb *lusb = dev->backend;
    uint32_t tmp_data = 0;

    for (i = 0; status == 0 && i < 4; i++){
        cmd.addr = i + UART_PKT_DEV_DC_CORR_ADDR;
        cmd.data = 0xff;

        status = access_peripheral(
                                    lusb,
                                    UART_PKT_DEV_GPIO,
                                    UART_PKT_MODE_DIR_READ,
                                    &cmd
                                    );

        if (status < 0) {
            break;
        }
        tmp_data |= (cmd.data << (i * 8));
    }
    if (status < 0){
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF,status);
    }

    return status;
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
 * Platform information
 *----------------------------------------------------------------------------*/

static int linux_get_otp(struct bladerf *dev, char *otp)
{
    int status;
    struct bladeRF_sector bs;
    struct bladerf_linux *backend = dev->backend;
    bs.idx = 0;
    bs.ptr = (unsigned char *)otp;
    bs.len = 256;

    status = ioctl(backend->fd, BLADE_OTP_READ, &bs);
    if (status < 0) {
        log_error("Failed to read OTP with errno=%d: %s\n", errno, strerror(errno));
        status = BLADERF_ERR_IO;
    }
    return status;
}

static int linux_get_cal(struct bladerf *dev, char *cal)
{
    int status;
    struct bladeRF_sector bs;
    struct bladerf_linux *backend = dev->backend;
    bs.idx = 768;
    bs.ptr = (unsigned char *)cal;
    bs.len = 256;

    status = ioctl(backend->fd, BLADE_FLASH_READ, &bs);
    if (status < 0) {
        log_error("Failed to read calibration sector with errno=%d: %s\n", errno, strerror(errno));
        status = BLADERF_ERR_IO;
    }
    return status;
}

static int linux_populate_fpga_version(struct bladerf *dev)
{
    log_debug("FPGA currently does not have a version number.\n");
    dev->fpga_version.major = 0;
    dev->fpga_version.minor = 0;
    dev->fpga_version.patch = 0;
    snprintf((char*)dev->fpga_version.describe, BLADERF_VERSION_STR_MAX, "0.0.0");
    return 0;
}

/* TODO: Add support for patch version and string */
static int linux_populate_fw_version(struct bladerf *dev)
{
    int status;
    struct bladerf_fx3_version ver;
    struct bladerf_linux *backend = (struct bladerf_linux *)dev->backend;

    status = ioctl(backend->fd, BLADE_QUERY_VERSION, &ver);
    if (!status) {
        dev->fw_version.major = ver.major;
        dev->fw_version.minor = ver.minor;
        dev->fw_version.patch = 0;
        snprintf((char *)dev->fw_version.describe,
                 BLADERF_VERSION_STR_MAX, "%d.%d.%d",
                 dev->fw_version.major, dev->fw_version.minor,
                 dev->fw_version.patch);
        return 0;
    }

    /* TODO return more appropriate error code based upon errno */
    return BLADERF_ERR_IO;
}


static int linux_get_device_speed(struct bladerf *dev,
                                  bladerf_dev_speed  *device_speed)
{
    int status = 0;
    struct bladerf_linux *backend = dev->backend;
    int speed_from_driver;

    status = ioctl(backend->fd, BLADE_GET_SPEED, &speed_from_driver);
    if (status < 0) {
        log_error("Failed to get device speed: %s\n", strerror(errno));
        status = BLADERF_ERR_IO;
    } else if (speed_from_driver == 0) {
        *device_speed = BLADERF_DEVICE_SPEED_HIGH;
    } else if (speed_from_driver == 1) {
        *device_speed = BLADERF_DEVICE_SPEED_SUPER;
    } else {
        log_error("Got invalid device speed from driver: %d\n",
                  speed_from_driver);
        status = BLADERF_ERR_UNEXPECTED;
    }

    if (status < 0) {
        *device_speed = BLADERF_DEVICE_SPEED_UNKNOWN;
    }

    return status;
}

/*------------------------------------------------------------------------------
 * Init/deinit
 *----------------------------------------------------------------------------*/
int linux_close(struct bladerf *dev)
{
    int status;
    struct bladerf_linux *backend = dev->backend;
    assert(backend);

    status = close(backend->fd);
    free((void*)dev->fpga_version.describe);
    free((void*)dev->fw_version.describe);
    free(backend);
    free(dev);

    if (status < 0) {
        return BLADERF_ERR_IO;
    } else {
        return 0;
    }
}

static int linux_populate_devinfo(struct bladerf *dev,
                                    struct bladerf_devinfo *devinfo,
                                    unsigned int instance)
{
    int status, tmp;
    struct bladerf_linux *backend = dev->backend;

    devinfo->backend = BLADERF_BACKEND_LINUX;
    devinfo->instance = instance;

    /* Fetch USB bus and address */
    status = ioctl(backend->fd, BLADE_GET_BUS, &tmp);
    if (status < 0) {
        status = BLADERF_ERR_IO;
    } else {
        assert(tmp >= 0 || tmp < (int)DEVINFO_INST_ANY);
        devinfo->usb_bus = tmp;
    }

    status = ioctl(backend->fd, BLADE_GET_ADDR, &tmp);
    if (status < 0) {
        status = BLADERF_ERR_IO;
    } else {
        assert(tmp >= 0 && tmp < DEVINFO_ADDR_ANY);
        devinfo->usb_addr = tmp;
    }

    /* Fetch device's serial # */
    status = bladerf_read_serial(dev, (char *)&devinfo->serial);
    if (status < 0) {
        status = BLADERF_ERR_IO;
    }

    return status;
}

/* Forward declared so linux_open can use it for the wildcard case */
static int linux_probe(struct bladerf_devinfo_list *info_list);

static int linux_open( struct bladerf **device, struct bladerf_devinfo *info)
{
    char dev_name[32];
    struct bladerf_linux *backend;
    struct bladerf *ret = NULL;
    int status = BLADERF_ERR_IO;

    int fd; /* Everything here starts with a driver file descriptor,
             * so no need to allocate backend and ret until we know we
             * have said fd */

    assert(info->backend == BLADERF_BACKEND_LINUX ||
           info->backend == BLADERF_BACKEND_ANY);

    /* If an instance is specified, we start with that */
    if (info->instance != DEVINFO_INST_ANY) {
        snprintf(dev_name, sizeof(dev_name), "/dev/bladerf%d", info->instance);
        fd = open(dev_name, O_RDWR);

        if (fd >= 0) {
            backend = calloc(1, sizeof(*backend));
            if (backend == NULL) {
                return BLADERF_ERR_MEM;
            }

            ret = calloc(1, sizeof(*ret));
            if (ret == NULL) {
                free(backend);
                return BLADERF_ERR_MEM;
            }

            ret->fpga_version.describe = calloc(1, BLADERF_VERSION_STR_MAX);
            if (ret->fpga_version.describe == NULL) {
                free(ret);
                free(backend);
                return BLADERF_ERR_MEM;
            }

            ret->fw_version.describe = calloc(1, BLADERF_VERSION_STR_MAX);
            if (ret->fw_version.describe == NULL) {
                free((void*)ret->fpga_version.describe);
                free(ret);
                free(backend);
                return BLADERF_ERR_MEM;
            }

            backend->fd = fd;
            ret->fn = &bladerf_linux_fn;
            ret->backend = backend;
            *device = ret;

            /* Ensure all dev-info fields are written */
            status = linux_populate_devinfo(ret, &ret->ident, info->instance);
            if (status < 0) {
                goto linux_open_err;
            }

            /* Populate version fields */
            status = linux_populate_fpga_version(ret);
            if (status < 0) {
                goto linux_open_err;
            }

            status = linux_populate_fw_version(ret);

linux_open_err:
            if (status < 0) {
                free((void*)ret->fw_version.describe);
                free((void*)ret->fpga_version.describe);
                free(ret);
                free(backend);
            }

        } else {
            log_error("Failed to open %s: %s\n", dev_name, strerror(errno));
        }
    } else {
        /* Otherwise, we user our probe routine to get a device info list,
         * and then search it */
        struct bladerf_devinfo_list list;
        size_t i;

        status = bladerf_devinfo_list_init(&list);

        if (status < 0) {
            log_error("Failed to initialized devinfo list!\n");
        } else {
            status = linux_probe(&list);
            if (status < 0) {
                if (status == BLADERF_ERR_NODEV) {
                    log_debug("No devices available on the Linux driver backend.\n");
                } else {
                    log_error("Probe failed: %s\n", bladerf_strerror(status));
                }
            } else {
                for (i = 0; i < list.num_elt && !ret; i++) {
                    if (bladerf_devinfo_matches(&list.elt[i], info)) {
                        status = linux_open(device, &list.elt[i]);

                        if (status) {
                            log_error("Failed to open instance %d - "
                                      "trying next\n", list.elt[i].instance);

                        } else {
                            backend =(*device)->backend;
                        }
                    }
                }

                free(list.elt);
            }
        }
    }

    return status;
}


/*------------------------------------------------------------------------------
 * Device probing
 *----------------------------------------------------------------------------*/
static int device_filter(const struct dirent *d)
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

/* Assumes bladerF_dev is BLADERF_DEV_PFX followed by a number */
static int str2instance(const char *bladerf_dev)
{
    const size_t pfx_len = strlen(BLADERF_DEV_PFX);
    int instance;
    bool ok;

    /* Validate assumption - bug catcher */
    assert(strlen(bladerf_dev) > pfx_len);

    instance = str2uint(&bladerf_dev[pfx_len], 0, UINT_MAX, &ok);

    if (!ok) {
        instance = DEVINFO_INST_ANY - 1;
        log_warning("Failed to convert to instance: %s\n", bladerf_dev);
        log_warning("Returning a value likely to fail: %d\n", instance);
    }

    return instance;
}

static int linux_probe(struct bladerf_devinfo_list *info_list)
{
    int status = 0;
    struct dirent **matches;
    int num_matches, i;
    struct bladerf *dev;
    struct bladerf_devinfo devinfo;

    num_matches = scandir(BLADERF_DEV_DIR, &matches, device_filter, alphasort);
    if (num_matches > 0) {

        for (i = 0; i < num_matches; i++) {
            status = 0;

            /* Open this specific instance. */
            bladerf_init_devinfo(&devinfo);
            devinfo.instance = str2instance(matches[i]->d_name);

            status = linux_open(&dev, &devinfo);

            if (status < 0) {
                log_error("Failed to open instance=%d\n", devinfo.instance);
            } else {
                /* Since this device was opened by instance, it will have
                 * had it's device info (dev->ident) filled out already */
                bladerf_devinfo_list_add(info_list, &dev->ident);
                linux_close(dev);
            }
        }
    }

    free_dirents(matches, num_matches);

    return (!status &&  num_matches > 0) ? status : BLADERF_ERR_NODEV;
}

static int linux_stream_init(struct bladerf_stream *stream)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static int linux_stream(struct bladerf_stream *stream, bladerf_module module)
{
    return BLADERF_ERR_UNSUPPORTED;
}

static void linux_deinit_stream(struct bladerf_stream *stream)
{
}

/*------------------------------------------------------------------------------
 * Function table
 *----------------------------------------------------------------------------*/

const struct bladerf_fn bladerf_linux_fn = {
    FIELD_INIT(.probe, linux_probe),

    FIELD_INIT(.open, linux_open),
    FIELD_INIT(.close, linux_close),

    FIELD_INIT(.load_fpga, linux_load_fpga),
    FIELD_INIT(.is_fpga_configured, linux_is_fpga_configured),

    FIELD_INIT(.flash_firmware, linux_flash_firmware),
    FIELD_INIT(.erase_flash, NULL),
    FIELD_INIT(.read_flash, NULL),
    FIELD_INIT(.write_flash, NULL),
    FIELD_INIT(.device_reset, linux_device_reset),
    FIELD_INIT(.jump_to_bootloader, NULL),

    FIELD_INIT(.get_cal, linux_get_cal),
    FIELD_INIT(.get_otp, linux_get_otp),
    FIELD_INIT(.get_device_speed, linux_get_device_speed),

    FIELD_INIT(.config_gpio_write, linux_config_gpio_write),
    FIELD_INIT(.config_gpio_read, linux_config_gpio_read),

    FIELD_INIT(.config_dc_gain_write, linux_config_dc_gain_write),

    FIELD_INIT(.si5338_write, linux_si5338_write),
    FIELD_INIT(.si5338_read, linux_si5338_read),

    FIELD_INIT(.lms_write, linux_lms_write),
    FIELD_INIT(.lms_read, linux_lms_read),

    FIELD_INIT(.dac_write, linux_dac_write),

    FIELD_INIT(.enable_module, NULL),

    FIELD_INIT(.init_stream, linux_stream_init),
    FIELD_INIT(.stream, linux_stream),
    FIELD_INIT(.deinit_stream, linux_deinit_stream),
};

