/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2013-2016 Nuand LLC
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#include <libbladeRF.h>

#include "rel_assert.h"
#include "log.h"
#define LOGGER_ID_STRING
#include "logger_id.h"
#include "logger_entry.h"

#include "backend/backend.h"
#include "backend/usb/usb.h"
#include "board/board.h"
#include "driver/fx3_fw.h"
#include "streaming/async.h"
#include "version.h"

#include "expansion/xb100.h"
#include "expansion/xb200.h"
#include "expansion/xb300.h"

#include "devinfo.h"
#include "helpers/file.h"
#include "helpers/interleave.h"

#include "conversions.h"
#include "parse.h"

/******************************************************************************/
/* Config file stuff */
/* TODO: move this somewhere more appropriate */
/******************************************************************************/

const struct numeric_suffix freq_suffixes[] = {
    { "G",      1000 * 1000 * 1000 },
    { "GHz",    1000 * 1000 * 1000 },
    { "M",      1000 * 1000 },
    { "MHz",    1000 * 1000 },
    { "k",      1000 } ,
    { "kHz",    1000 }
};
#define NUM_FREQ_SUFFIXES (sizeof(freq_suffixes) / sizeof(freq_suffixes[0]))

static int apply_config_options(struct bladerf *dev, struct config_options opt)
{
    int status;
    uint64_t freq;
    unsigned int bw;
    uint32_t val;
    bool ok;
    bladerf_gain_mode gain_mode;
    bladerf_sampling sampling_mode;
    bladerf_vctcxo_tamer_mode tamer_mode = BLADERF_VCTCXO_TAMER_INVALID;

    struct bladerf_rational_rate rate, actual;

    status = BLADERF_ERR_INVAL;

    if (!strcasecmp(opt.key, "fpga")) {
        status = bladerf_load_fpga(dev, opt.value);
        if (status < 0) {
            log_warning("Config line %d: could not load FPGA from `%s'\n",
                    opt.lineno, opt.value);
        }
        return status;
    } else if (!strcasecmp(opt.key, "frequency")) {
        freq = str2uint64_suffix( opt.value,
                0, BLADERF_FREQUENCY_MAX,
                freq_suffixes, NUM_FREQ_SUFFIXES, &ok);
        if (!ok)
            return BLADERF_ERR_INVAL;

        status = bladerf_set_frequency(dev, BLADERF_MODULE_RX, (unsigned int)freq);
        if (status < 0)
            return status;

        status = bladerf_set_frequency(dev, BLADERF_MODULE_TX, (unsigned int)freq);
    } else if (!strcasecmp(opt.key, "samplerate")) {
        freq = str2uint64_suffix( opt.value,
                0, UINT64_MAX,
                freq_suffixes, NUM_FREQ_SUFFIXES, &ok);
        if (!ok)
            return BLADERF_ERR_INVAL;

        rate.integer = freq;
        rate.num = 0;
        rate.den = 1;

        status = bladerf_set_rational_sample_rate(dev, BLADERF_MODULE_RX,
                                                    &rate, &actual);
        if (status < 0)
            return status;

        status = bladerf_set_rational_sample_rate(dev, BLADERF_MODULE_TX,
                                                    &rate, &actual);
    } else if (!strcasecmp(opt.key, "bandwdith")) {
        bw = str2uint_suffix( opt.value,
                BLADERF_BANDWIDTH_MIN, BLADERF_BANDWIDTH_MAX,
                freq_suffixes, NUM_FREQ_SUFFIXES, &ok);
        if (!ok)
            return BLADERF_ERR_INVAL;

        status = bladerf_set_bandwidth(dev, BLADERF_MODULE_RX, bw, NULL);
        if (status < 0)
            return status;

        status = bladerf_set_bandwidth(dev, BLADERF_MODULE_TX, bw, NULL);
    } else if (!strcasecmp(opt.key, "agc")) {
        gain_mode = str2bool(opt.value, &ok) ? BLADERF_GAIN_AUTOMATIC
                                            : BLADERF_GAIN_MANUAL;
        printf("%s %d \n", opt.value, ok);
        if (!ok)
            return BLADERF_ERR_INVAL;

        status = bladerf_set_gain_mode(dev, BLADERF_MODULE_RX, gain_mode);
    } else if (!strcasecmp(opt.key, "gpio")) {
        val = str2uint(opt.key, 0, -1, &ok);
        if (!ok)
            return BLADERF_ERR_INVAL;

        status = bladerf_config_gpio_write(dev, val);
    } else if (!strcasecmp(opt.key, "sampling")) {
        if (!strcasecmp(opt.value, "internal")) {
            sampling_mode = BLADERF_SAMPLING_INTERNAL;
        } else if (!strcasecmp(opt.value, "external")) {
            sampling_mode = BLADERF_SAMPLING_EXTERNAL;
        } else {
            return BLADERF_ERR_INVAL;
        }

        status = bladerf_set_sampling(dev, sampling_mode);
    } else if (!strcasecmp(opt.key, "trimdac")) {
        val = str2uint(opt.value, 0, -1, &ok);
        if (!ok)
            return BLADERF_ERR_INVAL;

        status = bladerf_dac_write(dev, val);
    } else if (!strcasecmp(opt.value, "vctcxo_tamer")) {
        if (!strcasecmp(opt.value, "disabled")     || !strcasecmp(opt.value, "off")) {
            tamer_mode = BLADERF_VCTCXO_TAMER_DISABLED;
        } else if (!strcasecmp(opt.value, "1PPS")  || !strcasecmp(opt.value, "1 PPS")) {
            tamer_mode = BLADERF_VCTCXO_TAMER_1_PPS;
        } else if (!strcasecmp(opt.value, "10MHZ") || !strcasecmp(opt.value, "10 MHZ")) {
            tamer_mode = BLADERF_VCTCXO_TAMER_10_MHZ;
        } else if (!strcasecmp(opt.value, "10M")) {
            tamer_mode = BLADERF_VCTCXO_TAMER_10_MHZ;
        } else {
            return BLADERF_ERR_INVAL;
        }

        status = bladerf_set_vctcxo_tamer_mode(dev, tamer_mode);
    } else {
        log_warning("Invalid key `%s' on line %d\n", opt.key, opt.lineno);
    }

    if (status < 0)
        log_warning("Error massage for option (%s) on line %d:\n%s\n",
                opt.key, opt.lineno, bladerf_strerror(status));

    return status;
}

int config_load_options_file(struct bladerf *dev)
{
    char *filename = NULL;
    int status = 0;

    uint8_t *buf = NULL;
    size_t  buf_size;

    int optc;
    int j;
    struct config_options *optv;

    filename = file_find("bladeRF.conf");
    if (!filename) {
        filename = file_find("bladerf.conf");

        /* A missing file that is optional is not an error */
        if (!filename) {
            return 0;
        }
    }

    status = file_read_buffer(filename, &buf, &buf_size);
    if (status < 0) {
        goto out;
    }

    optc = str2options(dev, (const char *)buf, buf_size, &optv);
    if (optc < 0) {
        status = BLADERF_ERR_INVAL;
        goto out_buf;
    }

    for (j = 0; j < optc; j++) {
        status = apply_config_options(dev, optv[j]);
        if (status < 0) {
            log_warning("Invalid config option `%s' on line %d\n",
                            optv[j].key, optv[j].lineno);
            break;
        }
    }

    free_opts(optv, optc);

out_buf:
    free(buf);
out:
    free(filename);
    return status;

}

/******************************************************************************/
/* Open / Close */
/******************************************************************************/

/* dev path becomes device specifier string (osmosdr-like) */
int bladerf_open(struct bladerf **dev, const char *dev_id)
{
    struct bladerf_devinfo devinfo;
    int status;

    *dev = NULL;

    /* Populate dev-info from string */
    status = str2devinfo(dev_id, &devinfo);
    if (!status) {
        status = bladerf_open_with_devinfo(dev, &devinfo);
    }

    return status;
}

int bladerf_open_with_devinfo(struct bladerf **opened_device,
                              struct bladerf_devinfo *devinfo)
{
    struct bladerf *dev;
    struct bladerf_devinfo any_device;
    unsigned int i;
    int status;

    if (devinfo == NULL) {
        bladerf_init_devinfo(&any_device);
        devinfo = &any_device;
    }

    *opened_device = NULL;

    dev = calloc(1, sizeof(struct bladerf));
    if (dev == NULL) {
        return BLADERF_ERR_MEM;
    }

    /* Open backend */
    status = backend_open(dev, devinfo);
    if (status != 0) {
        free(dev);
        return status;
    }

    /* Find matching board */
    for (i = 0; i < bladerf_boards_len; i++) {
        if (bladerf_boards[i]->matches(dev)) {
            dev->board = bladerf_boards[i];
            break;
        }
    }
    /* If no matching borad was found */
    if (i == bladerf_boards_len) {
        dev->backend->close(dev);
        free(dev);
        return BLADERF_ERR_NODEV;
    }

    MUTEX_INIT(&dev->lock);

    /* Open board */
    status = dev->board->open(dev, devinfo);

    if (status < 0) {
        bladerf_close(dev);
        return status;
    }

    /* Load configuration file */
    status = config_load_options_file(dev);

    if (status < 0) {
        bladerf_close(dev);
        return status;
    }

    *opened_device = dev;

    return 0;
}

int bladerf_get_devinfo(struct bladerf *dev, struct bladerf_devinfo *info)
{
    if (dev) {
        MUTEX_LOCK(&dev->lock);
        memcpy(info, &dev->ident, sizeof(struct bladerf_devinfo));
        MUTEX_UNLOCK(&dev->lock);
        return 0;
    } else {
        return BLADERF_ERR_INVAL;
    }
}

void bladerf_close(struct bladerf *dev)
{
    if (dev) {
        MUTEX_LOCK(&dev->lock);

        dev->board->close(dev);

        if (dev->backend) {
            dev->backend->close(dev);
        }

        MUTEX_UNLOCK(&dev->lock);

        free(dev);
    }
}

/******************************************************************************/
/* FX3 Firmware (common to bladerf1 and bladerf2) */
/******************************************************************************/

int bladerf_jump_to_bootloader(struct bladerf *dev)
{
    int status;

    if (!dev->backend->jump_to_bootloader) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    MUTEX_LOCK(&dev->lock);

    status = dev->backend->jump_to_bootloader(dev);

    MUTEX_UNLOCK(&dev->lock);

    return status;
}

int bladerf_get_bootloader_list(struct bladerf_devinfo **devices)
{
    return probe(BACKEND_PROBE_FX3_BOOTLOADER, devices);
}

int bladerf_load_fw_from_bootloader(const char *device_identifier,
                                    bladerf_backend backend,
                                    uint8_t bus, uint8_t addr,
                                    const char *file)
{
    int status;
    uint8_t *buf;
    size_t buf_len;
    struct fx3_firmware *fw = NULL;
    struct bladerf_devinfo devinfo;

    if (device_identifier == NULL) {
        bladerf_init_devinfo(&devinfo);
        devinfo.backend = backend;
        devinfo.usb_bus = bus;
        devinfo.usb_addr = addr;
    } else {
        status = str2devinfo(device_identifier, &devinfo);
        if (status != 0) {
            return status;
        }
    }

    status = file_read_buffer(file, &buf, &buf_len);
    if (status != 0) {
        return status;
    }

    status = fx3_fw_parse(&fw, buf, buf_len);
    free(buf);
    if (status != 0) {
        return status;
    }

    assert(fw != NULL);

    status = backend_load_fw_from_bootloader(devinfo.backend, devinfo.usb_bus,
                                             devinfo.usb_addr, fw);

    fx3_fw_free(fw);

    return status;
}

/* FIXME presumes bladerf1 capability bits */
#include "board/bladerf1/capabilities.h"

int bladerf_get_fw_log(struct bladerf *dev, const char *filename)
{
    int status;
    FILE *f = NULL;
    logger_entry e;

    MUTEX_LOCK(&dev->lock);

    if (!have_cap(dev->board->get_capabilities(dev), BLADERF_CAP_READ_FW_LOG_ENTRY)) {
        struct bladerf_version fw_version;

        if (dev->board->get_fw_version(dev, &fw_version) == 0) {
            log_debug("FX3 FW v%s does not support log retrieval.\n",
                      fw_version.describe);
        }

        status = BLADERF_ERR_UNSUPPORTED;
        goto error;
    }

    if (filename != NULL) {
        f = fopen(filename, "w");
        if (f == NULL) {
            switch (errno) {
                case ENOENT:
                    status = BLADERF_ERR_NO_FILE;
                    break;
                case EACCES:
                    status = BLADERF_ERR_PERMISSION;
                    break;
                default:
                    status = BLADERF_ERR_IO;
                    break;
            }
            goto error;
        }
    } else {
        f = stdout;
    }

    do {
        status = dev->backend->read_fw_log(dev, &e);
        if (status != 0) {
            log_debug("Failed to read FW log: %s\n", bladerf_strerror(status));
            goto error;
        }

        if (e == LOG_ERR) {
            fprintf(f, "<Unexpected error>,,\n");
        } else if (e != LOG_EOF) {
            uint8_t file_id;
            uint16_t line;
            uint16_t data;
            const char *src_file;

            logger_entry_unpack(e, &file_id, &line, &data);
            src_file = logger_id_string(file_id);

            fprintf(f, "%s, %u, 0x%04x\n", src_file, line, data);
        }
    } while (e != LOG_EOF && e != LOG_ERR);

error:
    MUTEX_UNLOCK(&dev->lock);

    if (f != NULL && f != stdout) {
        fclose(f);
    }

    return status;
}

/******************************************************************************/
/* Properties */
/******************************************************************************/

bladerf_dev_speed bladerf_device_speed(struct bladerf *dev)
{
    bladerf_dev_speed speed;
    MUTEX_LOCK(&dev->lock);

    speed = dev->board->device_speed(dev);

    MUTEX_UNLOCK(&dev->lock);
    return speed;
}

int bladerf_get_serial(struct bladerf *dev, char *serial)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->get_serial(dev, serial);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_get_fpga_size(struct bladerf *dev, bladerf_fpga_size *size)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->get_fpga_size(dev, size);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_is_fpga_configured(struct bladerf *dev)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->is_fpga_configured(dev);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

const char *bladerf_get_board_name(struct bladerf *dev)
{
    return dev->board->name;
}

/******************************************************************************/
/* Versions */
/******************************************************************************/

int bladerf_fpga_version(struct bladerf *dev, struct bladerf_version *version)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->get_fpga_version(dev, version);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_fw_version(struct bladerf *dev, struct bladerf_version *version)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->get_fw_version(dev, version);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

void bladerf_version(struct bladerf_version *version)
{
    /* Sanity checks for version reporting mismatches */
    #ifndef LIBBLADERF_API_VERSION
    #   error LIBBLADERF_API_VERSION is missing
    #endif
    #if LIBBLADERF_VERSION_MAJOR != ((LIBBLADERF_API_VERSION >> 24) & 0xff)
    #   error LIBBLADERF_API_VERSION: Major version mispatch
    #endif
    #if LIBBLADERF_VERSION_MINOR != ((LIBBLADERF_API_VERSION >> 16) & 0xff)
    #   error LIBBLADERF_API_VERSION: Minor version mispatch
    #endif
    #if LIBBLADERF_VERSION_PATCH != ((LIBBLADERF_API_VERSION >> 8) & 0xff)
    #   error LIBBLADERF_API_VERSION: Patch version mispatch
    #endif
    version->major = LIBBLADERF_VERSION_MAJOR;
    version->minor = LIBBLADERF_VERSION_MINOR;
    version->patch = LIBBLADERF_VERSION_PATCH;
    version->describe = LIBBLADERF_VERSION;
}

/******************************************************************************/
/* Enable/disable */
/******************************************************************************/

int bladerf_enable_module(struct bladerf *dev, bladerf_channel ch,
                          bool enable)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->enable_module(dev, ch, enable);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

/******************************************************************************/
/* Gain */
/******************************************************************************/

int bladerf_set_gain(struct bladerf *dev, bladerf_channel ch, int gain)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->set_gain(dev, ch, gain);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_get_gain(struct bladerf *dev, bladerf_channel ch, int *gain)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->get_gain(dev, ch, gain);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_set_gain_mode(struct bladerf *dev, bladerf_channel ch, bladerf_gain_mode mode)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->set_gain_mode(dev, ch, mode);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_get_gain_mode(struct bladerf *dev, bladerf_channel ch, bladerf_gain_mode *mode)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->get_gain_mode(dev, ch, mode);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_get_gain_range(struct bladerf *dev, bladerf_channel ch, struct bladerf_range *range)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->get_gain_range(dev, ch, range);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_set_gain_stage(struct bladerf *dev, bladerf_channel ch, const char *stage, int gain)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->set_gain_stage(dev, ch, stage, gain);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_get_gain_stage(struct bladerf *dev, bladerf_channel ch, const char *stage, int *gain)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->get_gain_stage(dev, ch, stage, gain);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_get_gain_stage_range(struct bladerf *dev, bladerf_channel ch, const char *stage, struct bladerf_range *range)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->get_gain_stage_range(dev, ch, stage, range);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_get_gain_stages(struct bladerf *dev, bladerf_channel ch, const char **stages, unsigned int count)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->get_gain_stages(dev, ch, stages, count);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

/******************************************************************************/
/* Sample Rate */
/******************************************************************************/

int bladerf_set_sample_rate(struct bladerf *dev, bladerf_channel ch,
                            uint32_t rate, uint32_t *actual)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->set_sample_rate(dev, ch, rate, actual);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_get_sample_rate(struct bladerf *dev, bladerf_channel ch,
                            unsigned int *rate)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->get_sample_rate(dev, ch, rate);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_get_sample_rate_range(struct bladerf *dev, bladerf_channel ch,
                                  struct bladerf_range *range)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->get_sample_rate_range(dev, ch, range);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_set_rational_sample_rate(struct bladerf *dev, bladerf_channel ch,
                                     struct bladerf_rational_rate *rate,
                                     struct bladerf_rational_rate *actual)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->set_rational_sample_rate(dev, ch, rate, actual);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_get_rational_sample_rate(struct bladerf *dev, bladerf_channel ch,
                                     struct bladerf_rational_rate *rate)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->get_rational_sample_rate(dev, ch, rate);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

/******************************************************************************/
/* Bandwidth */
/******************************************************************************/

int bladerf_set_bandwidth(struct bladerf *dev, bladerf_channel ch,
                          unsigned int bandwidth,
                          unsigned int *actual)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->set_bandwidth(dev, ch, bandwidth, actual);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_get_bandwidth(struct bladerf *dev, bladerf_channel ch,
                            unsigned int *bandwidth)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->get_bandwidth(dev, ch, bandwidth);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_get_bandwidth_range(struct bladerf *dev, bladerf_channel ch,
                                struct bladerf_range *range)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->get_bandwidth_range(dev, ch, range);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

/******************************************************************************/
/* Frequency */
/******************************************************************************/

int bladerf_set_frequency(struct bladerf *dev,
                          bladerf_channel ch, uint64_t frequency)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->set_frequency(dev, ch, frequency);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_get_frequency(struct bladerf *dev,
                          bladerf_channel ch, uint64_t *frequency)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->get_frequency(dev, ch, frequency);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_get_frequency_range(struct bladerf *dev, bladerf_channel ch,
                                struct bladerf_range *range)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->get_frequency_range(dev, ch, range);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_select_band(struct bladerf *dev, bladerf_channel ch,
                        uint64_t frequency)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->select_band(dev, ch, frequency);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

/******************************************************************************/
/* RF Ports*/
/******************************************************************************/

int bladerf_set_rf_port(struct bladerf *dev, bladerf_channel ch,
                        const char *port)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->set_rf_port(dev, ch, port);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_get_rf_port(struct bladerf *dev, bladerf_channel ch,
                        const char **port)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->get_rf_port(dev, ch, port);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_get_rf_ports(struct bladerf *dev, bladerf_channel ch,
                         const char **ports, unsigned int count)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->get_rf_ports(dev, ch, ports, count);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

/******************************************************************************/
/* Scheduled Tuning */
/******************************************************************************/

int bladerf_get_quick_tune(struct bladerf *dev,
                           bladerf_channel ch,
                           struct bladerf_quick_tune *quick_tune)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->get_quick_tune(dev, ch, quick_tune);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_schedule_retune(struct bladerf *dev,
                            bladerf_channel ch,
                            uint64_t timestamp,
                            uint64_t frequency,
                            struct bladerf_quick_tune *quick_tune)

{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->schedule_retune(dev, ch, timestamp, frequency, quick_tune);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_cancel_scheduled_retunes(struct bladerf *dev, bladerf_channel ch)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->cancel_scheduled_retunes(dev, ch);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

/******************************************************************************/
/* DC/Phase/Gain Correction */
/******************************************************************************/

int bladerf_get_correction(struct bladerf *dev, bladerf_channel ch,
                           bladerf_correction corr, int16_t *value)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->get_correction(dev, ch, corr, value);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_set_correction(struct bladerf *dev, bladerf_channel ch,
                           bladerf_correction corr, int16_t value)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->set_correction(dev, ch, corr, value);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

/******************************************************************************/
/* Trigger */
/******************************************************************************/

int bladerf_trigger_init(struct bladerf *dev,
                         bladerf_channel ch,
                         bladerf_trigger_signal signal,
                         struct bladerf_trigger *trigger)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->trigger_init(dev, ch, signal, trigger);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_trigger_arm(struct bladerf *dev,
                        const struct bladerf_trigger *trigger,
                        bool arm, uint64_t resv1, uint64_t resv2)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->trigger_arm(dev, trigger, arm, resv1, resv2);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_trigger_fire(struct bladerf *dev,
                         const struct bladerf_trigger *trigger)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->trigger_fire(dev, trigger);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_trigger_state(struct bladerf *dev,
                          const struct bladerf_trigger *trigger,
                          bool *is_armed,
                          bool *has_fired,
                          bool *fire_requested,
                          uint64_t *reserved1,
                          uint64_t *reserved2)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->trigger_state(dev, trigger, is_armed, has_fired, fire_requested, reserved1, reserved2);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_set_gain_mode(struct bladerf *dev, bladerf_module mod, bladerf_gain_mode mode)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->set_gain_mode(dev, mod, mode);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_get_gain_mode(struct bladerf *dev, bladerf_module mod, bladerf_gain_mode *mode)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->get_gain_mode(dev, mod, mode);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

/******************************************************************************/
/* Streaming */
/******************************************************************************/

int bladerf_init_stream(struct bladerf_stream **stream,
                        struct bladerf *dev,
                        bladerf_stream_cb callback,
                        void ***buffers,
                        size_t num_buffers,
                        bladerf_format format,
                        size_t samples_per_buffer,
                        size_t num_transfers,
                        void *data)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->init_stream(stream, dev, callback, buffers, num_buffers,
                                     format, samples_per_buffer, num_transfers,
                                     data);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_stream(struct bladerf_stream *stream, bladerf_channel_layout layout)
{
    return stream->dev->board->stream(stream, layout);
}

int bladerf_submit_stream_buffer(struct bladerf_stream *stream,
                                 void *buffer,
                                 unsigned int timeout_ms)
{
    return stream->dev->board->submit_stream_buffer(stream, buffer, timeout_ms, false);
}

int bladerf_submit_stream_buffer_nb(struct bladerf_stream *stream, void *buffer)
{
    return stream->dev->board->submit_stream_buffer(stream, buffer, 0, true);
}

void bladerf_deinit_stream(struct bladerf_stream *stream)
{
    if (stream) {
        stream->dev->board->deinit_stream(stream);
    }
}

int bladerf_set_stream_timeout(struct bladerf *dev, bladerf_direction dir,
                               unsigned int timeout) {
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->set_stream_timeout(dev, dir, timeout);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_get_stream_timeout(struct bladerf *dev, bladerf_direction dir,
                               unsigned int *timeout) {
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->get_stream_timeout(dev, dir, timeout);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_sync_config(struct bladerf *dev,
                        bladerf_channel_layout layout,
                        bladerf_format format,
                        unsigned int num_buffers,
                        unsigned int buffer_size,
                        unsigned int num_transfers,
                        unsigned int stream_timeout)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->sync_config(dev, layout, format, num_buffers, buffer_size, num_transfers, stream_timeout);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_sync_tx(struct bladerf *dev,
                    void const *samples,
                    unsigned int num_samples,
                    struct bladerf_metadata *metadata,
                    unsigned int timeout_ms)
{
    return dev->board->sync_tx(dev, samples, num_samples, metadata, timeout_ms);
}

int bladerf_sync_rx(struct bladerf *dev,
                    void *samples,
                    unsigned int num_samples,
                    struct bladerf_metadata *metadata,
                    unsigned int timeout_ms)
{
    return dev->board->sync_rx(dev, samples, num_samples, metadata, timeout_ms);
}

int bladerf_get_timestamp(struct bladerf *dev, bladerf_direction dir, uint64_t *value)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->get_timestamp(dev, dir, value);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_interleave_stream_buffer(bladerf_channel_layout layout,
                                     bladerf_format format,
                                     unsigned int buffer_size,
                                     void *samples)
{
    return _interleave_interleave_buf(layout, format, buffer_size, samples);
}

int bladerf_deinterleave_stream_buffer(bladerf_channel_layout layout,
                                       bladerf_format format,
                                       unsigned int buffer_size,
                                       void *samples)
{
    return _interleave_deinterleave_buf(layout, format, buffer_size, samples);
}

/******************************************************************************/
/* FPGA/Firmware Loading/Flashing */
/******************************************************************************/

int bladerf_load_fpga(struct bladerf *dev, const char *fpga_file)
{
    uint8_t *buf = NULL;
    size_t buf_size;
    int status;

    status = file_read_buffer(fpga_file, &buf, &buf_size);
    if (status != 0) {
        goto exit;
    }

    MUTEX_LOCK(&dev->lock);
    status = dev->board->load_fpga(dev, buf, buf_size);
    MUTEX_UNLOCK(&dev->lock);

exit:
    free(buf);
    return status;
}

int bladerf_flash_fpga(struct bladerf *dev, const char *fpga_file)
{
    uint8_t *buf = NULL;
    size_t buf_size;
    int status;

    status = file_read_buffer(fpga_file, &buf, &buf_size);
    if (status != 0) {
        goto exit;
    }

    MUTEX_LOCK(&dev->lock);
    status = dev->board->flash_fpga(dev, buf, buf_size);
    MUTEX_UNLOCK(&dev->lock);

exit:
    free(buf);
    return status;
}

int bladerf_erase_stored_fpga(struct bladerf *dev)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->erase_stored_fpga(dev);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_flash_firmware(struct bladerf *dev, const char *firmware_file)
{
    uint8_t *buf = NULL;
    size_t buf_size;
    int status;

    status = file_read_buffer(firmware_file, &buf, &buf_size);
    if (status != 0) {
        goto exit;
    }

    MUTEX_LOCK(&dev->lock);
    status = dev->board->flash_firmware(dev, buf, buf_size);
    MUTEX_UNLOCK(&dev->lock);

exit:
    free(buf);
    return status;
}

int bladerf_device_reset(struct bladerf *dev)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->device_reset(dev);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

/******************************************************************************/
/* Tuning mode */
/******************************************************************************/

int bladerf_set_tuning_mode(struct bladerf *dev, bladerf_tuning_mode mode)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->set_tuning_mode(dev, mode);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_get_tuning_mode(struct bladerf *dev, bladerf_tuning_mode *mode)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->get_tuning_mode(dev, mode);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

/******************************************************************************/
/* Loopback */
/******************************************************************************/

int bladerf_set_loopback(struct bladerf *dev, bladerf_loopback l)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->set_loopback(dev, l);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_get_loopback(struct bladerf *dev, bladerf_loopback *l)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->get_loopback(dev, l);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

/******************************************************************************/
/* Sample RX FPGA Mux */
/******************************************************************************/

int bladerf_set_rx_mux(struct bladerf *dev, bladerf_rx_mux mux)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->set_rx_mux(dev, mux);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_get_rx_mux(struct bladerf *dev, bladerf_rx_mux *mux)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->get_rx_mux(dev, mux);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

/******************************************************************************/
/* Low-level VCTCXO Tamer Mode */
/******************************************************************************/

int bladerf_set_vctcxo_tamer_mode(struct bladerf *dev,
                                  bladerf_vctcxo_tamer_mode mode)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->set_vctcxo_tamer_mode(dev, mode);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_get_vctcxo_tamer_mode(struct bladerf *dev,
                                  bladerf_vctcxo_tamer_mode *mode)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->get_vctcxo_tamer_mode(dev, mode);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

/******************************************************************************/
/* Low-level VCTCXO Trim DAC access */
/******************************************************************************/

int bladerf_get_vctcxo_trim(struct bladerf *dev, uint16_t *trim)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->get_vctcxo_trim(dev, trim);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_trim_dac_read(struct bladerf *dev, uint16_t *trim)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->trim_dac_read(dev, trim);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_trim_dac_write(struct bladerf *dev, uint16_t trim)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->trim_dac_write(dev, trim);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_dac_read(struct bladerf *dev, uint16_t *trim)
    __attribute__ ((alias ("bladerf_trim_dac_read")));

int bladerf_dac_write(struct bladerf *dev, uint16_t trim)
    __attribute__ ((alias ("bladerf_trim_dac_write")));

/******************************************************************************/
/* Low-level Trigger control access */
/******************************************************************************/

int bladerf_read_trigger(struct bladerf *dev, bladerf_channel ch, bladerf_trigger_signal trigger, uint8_t *val)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->read_trigger(dev, ch, trigger, val);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_write_trigger(struct bladerf *dev, bladerf_channel ch, bladerf_trigger_signal trigger, uint8_t val)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->write_trigger(dev, ch, trigger, val);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

/******************************************************************************/
/* Low-level Configuration GPIO access */
/******************************************************************************/

int bladerf_config_gpio_read(struct bladerf *dev, uint32_t *val)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->config_gpio_read(dev, val);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_config_gpio_write(struct bladerf *dev, uint32_t val)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->config_gpio_write(dev, val);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

/******************************************************************************/
/* Low-level SPI Flash access */
/******************************************************************************/

int bladerf_erase_flash(struct bladerf *dev, uint32_t erase_block, uint32_t count)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->erase_flash(dev, erase_block, count);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_read_flash(struct bladerf *dev, uint8_t *buf, uint32_t page, uint32_t count)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->read_flash(dev, buf, page, count);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_write_flash(struct bladerf *dev, const uint8_t *buf, uint32_t page, uint32_t count)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->write_flash(dev, buf, page, count);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

/******************************************************************************/
/* Helpers & Miscellaneous */
/******************************************************************************/

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
        case BLADERF_ERR_NODEV:
            return "No devices available";
        case BLADERF_ERR_UNSUPPORTED:
            return "Operation not supported";
        case BLADERF_ERR_MISALIGNED:
            return "Misaligned flash access";
        case BLADERF_ERR_CHECKSUM:
            return "Invalid checksum";
        case BLADERF_ERR_NO_FILE:
            return "File not found";
        case BLADERF_ERR_UPDATE_FPGA:
            return "An FPGA update is required";
        case BLADERF_ERR_UPDATE_FW:
            return "A firmware update is required";
        case BLADERF_ERR_TIME_PAST:
            return "Requested timestamp is in the past";
        case BLADERF_ERR_QUEUE_FULL:
            return "Could not enqueue data into full queue";
        case BLADERF_ERR_FPGA_OP:
            return "An FPGA operation reported a failure";
        case BLADERF_ERR_PERMISSION:
            return "Insufficient permissions for the requested operation";
        case BLADERF_ERR_WOULD_BLOCK:
            return "The operation would block, but has been requested to be non-blocking";
        case BLADERF_ERR_NOT_INIT:
            return "Insufficient initialization for the requested operation";
        case 0:
            return "Success";
        default:
            return "Unknown error code";
    }
}

const char * bladerf_backend_str(bladerf_backend backend)
{
    return backend2str(backend);
}

void bladerf_log_set_verbosity(bladerf_log_level level)
{
    log_set_verbosity(level);
#if defined(LOG_SYSLOG_ENABLED)
    log_debug("Log verbosity has been set to: %d", level);
#endif
}

void bladerf_set_usb_reset_on_open(bool enabled)
{
#if ENABLE_USB_DEV_RESET_ON_OPEN
    bladerf_usb_reset_device_on_open = enabled;

    log_verbose("USB reset on open %s\n", enabled ? "enabled" : "disabled");
#else
    log_verbose("%s has no effect. "
                "ENABLE_USB_DEV_RESET_ON_OPEN not set at compile-time.\n",
                __FUNCTION__);
#endif
}

/******************************************************************************/
/* Expansion board APIs */
/******************************************************************************/

int bladerf_expansion_attach(struct bladerf *dev, bladerf_xb xb)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->expansion_attach(dev, xb);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_expansion_get_attached(struct bladerf *dev, bladerf_xb *xb)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = dev->board->expansion_get_attached(dev, xb);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

/* XB100 */

int bladerf_expansion_gpio_read(struct bladerf *dev, uint32_t *val)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = xb100_gpio_read(dev, val);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_expansion_gpio_write(struct bladerf *dev, uint32_t val)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = xb100_gpio_write(dev, val);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_expansion_gpio_masked_write(struct bladerf *dev,
                                        uint32_t mask, uint32_t val)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = xb100_gpio_masked_write(dev, mask, val);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_expansion_gpio_dir_read(struct bladerf *dev, uint32_t *val)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = xb100_gpio_dir_read(dev, val);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_expansion_gpio_dir_write(struct bladerf *dev, uint32_t val)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = xb100_gpio_dir_write(dev, val);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_expansion_gpio_dir_masked_write(struct bladerf *dev,
                                            uint32_t mask, uint32_t val)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = xb100_gpio_dir_masked_write(dev, mask, val);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

/* XB200 */

int bladerf_xb200_set_filterbank(struct bladerf *dev,
                                 bladerf_channel ch,
                                 bladerf_xb200_filter filter)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = xb200_set_filterbank(dev, ch, filter);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_xb200_get_filterbank(struct bladerf *dev,
                                 bladerf_channel ch,
                                 bladerf_xb200_filter *filter)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = xb200_get_filterbank(dev, ch, filter);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_xb200_set_path(struct bladerf *dev,
                           bladerf_channel ch,
                           bladerf_xb200_path path)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = xb200_set_path(dev, ch, path);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_xb200_get_path(struct bladerf *dev,
                           bladerf_channel ch,
                           bladerf_xb200_path *path)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = xb200_get_path(dev, ch, path);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

/* XB300 */

int bladerf_xb300_set_trx(struct bladerf *dev, bladerf_xb300_trx trx)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = xb300_set_trx(dev, trx);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_xb300_get_trx(struct bladerf *dev, bladerf_xb300_trx *trx)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = xb300_get_trx(dev, trx);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_xb300_set_amplifier_enable(struct bladerf *dev,
                   bladerf_xb300_amplifier amp,
                   bool enable)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = xb300_set_amplifier_enable(dev, amp, enable);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_xb300_get_amplifier_enable(struct bladerf *dev,
                   bladerf_xb300_amplifier amp,
                   bool *enable)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = xb300_get_amplifier_enable(dev, amp, enable);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}

int bladerf_xb300_get_output_power(struct bladerf *dev,
                   float *val)
{
    int status;
    MUTEX_LOCK(&dev->lock);

    status = xb300_get_output_power(dev, val);

    MUTEX_UNLOCK(&dev->lock);
    return status;
}
