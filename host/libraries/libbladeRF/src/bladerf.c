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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "rel_assert.h"

#include "libbladeRF.h"     /* Public API */
#include "bladerf_priv.h"   /* Implementation-specific items ("private") */
#include "lms.h"
#include "si5338.h"
#include "file_ops.h"
#include "log.h"
#include "backend.h"
#include "device_identifier.h"
#include "version.h"       /* Generated at build time */
#include "conversions.h"

/*------------------------------------------------------------------------------
 * Device discovery & initialization/deinitialization
 *----------------------------------------------------------------------------*/

int bladerf_get_device_list(struct bladerf_devinfo **devices)
{
    int ret;
    size_t num_devices;
    struct bladerf_devinfo *devices_local;
    int status;

    status = backend_probe(&devices_local, &num_devices);

    if (status < 0) {
        ret = status;
    } else {
        assert(num_devices <= INT_MAX);
        ret = (int)num_devices;
        *devices = devices_local;
    }

    return ret;
}

void bladerf_free_device_list(struct bladerf_devinfo *devices)
{
    /* Admittedly, we could just have the user call free() directly,
     * but this creates a 1:1 pair of calls, and this gives us a spot
     * to do any additional cleanup here, if ever needed in the future */
    free(devices);
}

static void init_stats(struct bladerf_stats *stats)
{
    stats->rx_overruns = 0;
    stats->rx_throughput = 0;
    stats->tx_underruns = 0;
    stats->tx_throughput = 0;
}

int bladerf_open_with_devinfo(struct bladerf **device,
                                struct bladerf_devinfo *devinfo)
{
    struct bladerf *opened_device;
    int status;

    *device = NULL;
    status = backend_open(device, devinfo);

    if (!status) {

        /* Catch bugs from backends returning status = 0, but a NULL device */
        assert(*device);
        opened_device = *device;

        /* We got a device */
        bladerf_set_error(&opened_device->error, ETYPE_LIBBLADERF, 0);
        init_stats(&opened_device->stats);

        status = opened_device->fn->get_device_speed(opened_device,
                                                     &opened_device->usb_speed);

        if (status < 0 ||
                (opened_device->usb_speed != BLADERF_DEVICE_SPEED_HIGH &&
                 opened_device->usb_speed != BLADERF_DEVICE_SPEED_SUPER)) {
            opened_device->fn->close((*device));
            *device = NULL;
        } else {
            if (opened_device->legacy) {
                /* Currently two modes of legacy:
                 *  - ALT_SETTING
                 *  - CONFIG_IF
                 *
                 * If either of these are set, we should tell the user to update
                 */
                printf("********************************************************************************\n");
                printf("* ENTERING LEGACY MODE, PLEASE UPGRADE TO THE LATEST FIRMWARE BY RUNNING:\n");
                printf("* wget http://nuand.com/fx3/latest.img ; bladeRF-cli -f latest.img\n");
                printf("********************************************************************************\n");
            }

            if (!(opened_device->legacy & LEGACY_ALT_SETTING)) {

                status = bladerf_get_and_cache_vctcxo_trim(opened_device);
                if (status < 0) {
                    log_warning( "Could not extract VCTCXO trim value\n" ) ;
                }

                status = bladerf_get_and_cache_fpga_size(opened_device);
                if (status < 0) {
                    log_warning( "Could not extract FPGA size\n" ) ;
                }

                /* If any of these routines failed, the dev structure should
                 * still have had it's fields dummied, so they're safe to
                 * print here (i.e., not uninitialized) */
                log_debug("%s: fw=v%s serial=%s trim=0x%.4x fpga_size=%d\n",
                        __FUNCTION__, opened_device->fw_version.describe,
                        opened_device->ident.serial, opened_device->dac_trim,
                        opened_device->fpga_size);
            }

            /* All status in here is not fatal, so whatever */
            status = 0 ;
            if (bladerf_is_fpga_configured(opened_device)) {
                bladerf_init_device(opened_device);
            }
        }
    }

    return status;
}

/* dev path becomes device specifier string (osmosdr-like) */
int bladerf_open(struct bladerf **device, const char *dev_id)
{
    struct bladerf_devinfo devinfo;
    int status;

    *device = NULL;

    /* Populate dev-info from string */
    status = str2devinfo(dev_id, &devinfo);

    if (!status) {
        status = bladerf_open_with_devinfo(device, &devinfo);
    }

    return status;
}

void bladerf_close(struct bladerf *dev)
{
    if (dev) {
        dev->fn->close(dev);
    }
}

int bladerf_enable_module(struct bladerf *dev,
                            bladerf_module m, bool enable)
{
    int status;
    log_info("Enable Module: %s - %s\n", (m == BLADERF_MODULE_RX) ? "RX" : "TX", enable ? "True" : "False" ) ;
    status = dev->fn->enable_module(dev, m, enable);
    return status;
}

int bladerf_set_loopback(struct bladerf *dev, bladerf_loopback l)
{
    lms_loopback_enable( dev, l );
    return 0;
}


int bladerf_set_rational_sample_rate(struct bladerf *dev, bladerf_module module, struct bladerf_rational_rate *rate, struct bladerf_rational_rate *actual)
{
    return si5338_set_rational_sample_rate(dev, module, rate, actual);
}

int bladerf_set_sample_rate(struct bladerf *dev, bladerf_module module, uint32_t rate, uint32_t *actual)
{
    return si5338_set_sample_rate(dev, module, rate, actual);
}

int bladerf_get_rational_sample_rate(struct bladerf *dev, bladerf_module module, struct bladerf_rational_rate *rate)
{
    return si5338_get_rational_sample_rate(dev, module, rate);
}

int bladerf_get_sample_rate(struct bladerf *dev, bladerf_module module, unsigned int *rate)
{
    return si5338_get_sample_rate(dev, module, rate);
}

int bladerf_get_sampling(struct bladerf *dev, bladerf_sampling *sampling)
{
    int status = 0, external = 0;
    uint8_t val = 0;

    status = bladerf_lms_read( dev, 0x09, &val );
    if (status) {
        log_warning( "Could not read state of ADC pin connectivity\n" );
        goto bladerf_get_sampling__done;
    }
    external = val&(1<<7) ? 1 : 0;

    status = bladerf_lms_read( dev, 0x64, &val );
    if (status) {
        log_warning( "Could not read RXVGA2 state\n" );
        goto bladerf_get_sampling__done;
    }
    external |= val&(1<<1) ? 0 : 2;

    switch(external) {
        case 0  : *sampling = BLADERF_SAMPLING_INTERNAL; break;
        case 3  : *sampling = BLADERF_SAMPLING_EXTERNAL; break;
        default : *sampling = BLADERF_SAMPLING_UNKNOWN; break;
    }

bladerf_get_sampling__done:
    return status;
}

int bladerf_set_sampling(struct bladerf *dev, bladerf_sampling sampling)
{
    uint8_t val ;
    int status = 0 ;
    if (sampling == BLADERF_SAMPLING_INTERNAL) {
        /* Disconnect the ADC input from the outside world */
        status = bladerf_lms_read( dev, 0x09, &val );
        if (status) {
            log_warning( "Could not read LMS to connect ADC to external pins\n" );
            goto bladerf_set_sampling__done ;
        }

        val &= ~(1<<7) ;
        status = bladerf_lms_write( dev, 0x09, val );
        if (status) {
            log_warning( "Could not write LMS to connect ADC to external pins\n" );
            goto bladerf_set_sampling__done;
        }

        /* Turn on RXVGA2 */
        status = bladerf_lms_read( dev, 0x64, &val );
        if (status) {
            log_warning( "Could not read LMS to disable RXVGA2\n" );
            goto bladerf_set_sampling__done;
        }

        val |= (1<<1) ;
        status = bladerf_lms_write( dev, 0x64, val );
        if (status) {
            log_warning( "Could not write LMS to disable RXVGA2\n" );
            goto bladerf_set_sampling__done;
        }
    } else {
        /* Turn off RXVGA2 */
        status = bladerf_lms_read( dev, 0x64, &val );
        if (status) {
            log_warning( "Could not read the LMS to enable RXVGA2\n" );
            goto bladerf_set_sampling__done;
        }

        val &= ~(1<<1) ;
        status = bladerf_lms_write( dev, 0x64, val );
        if (status) {
            log_warning( "Could not write the LMS to enable RXVGA2\n" );
            goto bladerf_set_sampling__done;
        }

        /* Connect the external ADC pins to the internal ADC input */
        status = bladerf_lms_read( dev, 0x09, &val );
        if (status) {
            log_warning( "Could not read the LMS to connect ADC to internal pins\n" );
            goto bladerf_set_sampling__done;
        }

        val |= (1<<7) ;
        status = bladerf_lms_write( dev, 0x09, val );
        if (status) {
            log_warning( "Could not write the LMS to connect ADC to internal pins\n" );
        }
    }

bladerf_set_sampling__done:
    return status;
}

int bladerf_set_txvga2(struct bladerf *dev, int gain)
{
    if( gain > 25 ) {
        log_info( "%s: %d being clamped to 25dB\n", __FUNCTION__, gain );
        gain = 25;
    }
    if( gain < 0 ) {
        log_info( "%s: %d being clamped to 0dB\n", __FUNCTION__, gain );
        gain = 0;
    }
    /* TODO: Make return values for lms call and return it for failure */
    lms_txvga2_set_gain( dev, gain );
    return 0;
}

int bladerf_get_txvga2(struct bladerf *dev, int *gain)
{
    uint8_t gain_u8;
    /* TODO: Make return values for lms call and return it for failure */
    lms_txvga2_get_gain( dev, &gain_u8 );
    *gain = gain_u8;
    return 0;
}

int bladerf_set_txvga1(struct bladerf *dev, int gain)
{
    if( gain < -35 ) {
        log_info( "%s: %d being clamped to -35dB\n", __FUNCTION__, gain );
        gain = -35;
    }
    if( gain > -4 ) {
        log_info( "%s: %d being clamped to -4dB\n", __FUNCTION__, gain );
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
    int r;
    /* TODO: Make return values for lms call and return it for failure */
    for (r=2; (r * r) < ((gain - 5) << 9); r++);
    lms_rxvga1_set_gain( dev, r );
    return 0;
}

int bladerf_get_rxvga1(struct bladerf *dev, int *gain)
{
    uint8_t gain_u8;
    /* TODO: Make return values for lms call and return it for failure */
    lms_rxvga1_get_gain( dev, &gain_u8 );
    *gain = 5 + (((int)gain_u8 * (int)gain_u8) >> 9);
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
    uint8_t gain_u8;
    /* TODO: Make return values for lms call and return it for failure */
    lms_rxvga2_get_gain( dev, &gain_u8 );
    /* Value returned from LMS lib is just the raw value, which is 3dB/unit */
    *gain = gain_u8 * 3;
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

int bladerf_set_lpf_mode(struct bladerf *dev, bladerf_module module,
                         bladerf_lpf_mode mode)
{
    /* TODO: Make return values for lms call and return it for failure */
    lms_lpf_set_mode( dev, module, mode );
    return 0;
}

int bladerf_get_lpf_mode(struct bladerf *dev, bladerf_module module,
                         bladerf_lpf_mode *mode)
{
    /* TODO: Make return values for lms call and return it for failure */
    lms_lpf_get_mode( dev, module, mode );
    return 0;
}

int bladerf_select_band(struct bladerf *dev, bladerf_module module,
                        unsigned int frequency)
{
    uint32_t gpio ;
    uint32_t band = 0;
    lms_lna_t lna ;
    lms_pa_t pa ;
    if( frequency >= 1500000000 ) {
        band = 1; // High Band selection
        lna = LNA_2;
        pa = PA_2;
    } else {
        band = 2; // Low Band selection
        lna = LNA_1;
        pa = PA_1;
    }
    if (module == BLADERF_MODULE_TX) {
        lms_pa_enable(dev, pa);
    } else {
        lms_lna_select(dev, lna);
    }
    bladerf_config_gpio_read(dev, &gpio);
    gpio &= ~(module == BLADERF_MODULE_TX ? (3<<3) : (3<<5));
    gpio |= (module == BLADERF_MODULE_TX ? (band<<3) : (band<<5));
    bladerf_config_gpio_write(dev, gpio);
    return 0;
}

int bladerf_set_frequency(struct bladerf *dev,
                            bladerf_module module, unsigned int frequency)
{
    /* TODO: Make return values for lms call and return it for failure */
    lms_set_frequency( dev, module, frequency );
    bladerf_select_band( dev, module, frequency );
    return 0;
}

int bladerf_get_frequency(struct bladerf *dev,
                            bladerf_module module, unsigned int *frequency)
{
    /* TODO: Make return values for lms call and return it for failure */
    struct lms_freq f;
    int rv = 0;
    lms_get_frequency( dev, module, &f );
    if( f.x == 0 ) {
        *frequency = 0 ;
        rv = BLADERF_ERR_INVAL;
    } else {
        *frequency = lms_frequency_to_hz(&f);
    }
    return rv;
}

void bladerf_set_transfer_timeout(struct bladerf *dev, bladerf_module module, int timeout) {
    dev->fn->set_transfer_timeout(dev, module, timeout);
}

int get_transfer_timeout(struct bladerf *dev, bladerf_module module) {
    return dev->fn->get_transfer_timeout(dev, module);
}

int bladerf_tx(struct bladerf *dev, bladerf_format format, void *samples,
               int num_samples, struct bladerf_metadata *metadata)
{
    if (num_samples < 1024 || num_samples % 1024 != 0) {
        log_warning("num_samples must be multiples of 1024\n");
        return BLADERF_ERR_INVAL;
    }

    return dev->fn->tx(dev, format, samples, num_samples, metadata);
}

int bladerf_rx(struct bladerf *dev, bladerf_format format, void *samples,
                   int num_samples, struct bladerf_metadata *metadata)
{
    if (num_samples < 1024 || num_samples % 1024 != 0) {
        log_warning("num_samples must be multiples of 1024\n");
        return BLADERF_ERR_INVAL;
    }

    return dev->fn->rx(dev, format, samples, num_samples, metadata);
}

int bladerf_init_stream(struct bladerf_stream **stream,
                        struct bladerf *dev,
                        bladerf_stream_cb callback,
                        void ***buffers,
                        size_t num_buffers,
                        bladerf_format format,
                        size_t samples_per_buffer,
                        size_t num_transfers,
                        void *user_data)
{

    struct bladerf_stream *lstream;
    size_t buffer_size_bytes;
    size_t i;
    int status = 0;

    if (num_transfers > num_buffers) {
        log_warning("num_transfers must be <= num_buffers\n");
        return BLADERF_ERR_INVAL;
    }

    if (samples_per_buffer < 1024 || samples_per_buffer% 1024 != 0) {
        log_warning("samples_per_buffer must be multiples of 1024\n");
        return BLADERF_ERR_INVAL;
    }

    /* Create a stream and populate it with the appropriate information */
    lstream = malloc(sizeof(struct bladerf_stream));

    if (!lstream) {
        return BLADERF_ERR_MEM;
    }

    lstream->dev = dev;
    lstream->error_code = 0;
    lstream->state = STREAM_IDLE;
    lstream->samples_per_buffer = samples_per_buffer;
    lstream->num_buffers = num_buffers;
    lstream->num_transfers = num_transfers;
    lstream->format = format;
    lstream->cb = callback;
    lstream->user_data = user_data;
    lstream->buffers = NULL;

    switch(format) {
        case BLADERF_FORMAT_SC16_Q12:
            buffer_size_bytes = c16_samples_to_bytes(samples_per_buffer);
            break;

        default:
            status = BLADERF_ERR_INVAL;
            break;
    }

    if (!status) {
        lstream->buffers = calloc(num_buffers, sizeof(lstream->buffers[0]));
        if (lstream->buffers) {
            for (i = 0; i < num_buffers && !status; i++) {
                lstream->buffers[i] = calloc(1, buffer_size_bytes);
                if (!lstream->buffers[i]) {
                    status = BLADERF_ERR_MEM;
                }
            }
        } else {
            status = BLADERF_ERR_MEM;
        }
    }

    /* Clean up everything we've allocated if we hit any errors */
    if (status) {

        if (lstream->buffers) {
            for (i = 0; i < num_buffers; i++) {
                free(lstream->buffers[i]);
            }

            free(lstream->buffers);
        }

        free(lstream);
    } else {
        /* Perform any backend-specific stream initialization */
        status = dev->fn->init_stream(lstream);

        if (status < 0) {
            bladerf_deinit_stream(lstream);
            *stream = NULL;
        } else {
            /* Update the caller's pointers */
            *stream = lstream;
            *buffers = lstream->buffers;
        }
    }


    return status;
}

void bladerf_deinit_stream(struct bladerf_stream *stream)
{
    size_t i;

    if (!stream) {
        return;
    }

    while(stream->state != STREAM_DONE && stream->state != STREAM_IDLE) {
        log_info( "Stream not done...\n" );
        usleep(1000000);
    }

    /* Free up the backend data */
    stream->dev->fn->deinit_stream(stream);

    /* Free up the buffers */
    for (i = 0; i < stream->num_buffers; i++) {
        free(stream->buffers[i]);
    }

    /* Free up the pointer to the buffers */
    free(stream->buffers);

    /* Free up the stream itself */
    free(stream);

    /* Done */
    return ;
}

int bladerf_stream(struct bladerf_stream *stream, bladerf_module module)
{
    int status;
    struct bladerf *dev = stream->dev;

    stream->module = module;
    stream->state = STREAM_RUNNING;
    lms_enable_rffe(dev, module, true);
    status = dev->fn->stream(stream, module);
    lms_enable_rffe(dev, module, false);

    /* Backend return value takes precedence over stream error status */
    return status == 0 ? stream->error_code : status;
}

/*------------------------------------------------------------------------------
 * Device Info
 *----------------------------------------------------------------------------*/

int bladerf_get_serial(struct bladerf *dev, char *serial)
{
    strcpy(serial, dev->ident.serial);
    return 0;
}

int bladerf_get_vctcxo_trim(struct bladerf *dev, uint16_t *trim)
{
    *trim = dev->dac_trim;
    return 0;
}

int bladerf_get_fpga_size(struct bladerf *dev, bladerf_fpga_size *size)
{
    *size = dev->fpga_size;
    return 0;
}

int bladerf_fw_version(struct bladerf *dev, struct bladerf_version *version)
{
    memcpy(version, &dev->fw_version, sizeof(*version));
    return 0;
}

int bladerf_is_fpga_configured(struct bladerf *dev)
{
    return dev->fn->is_fpga_configured(dev);
}

int bladerf_fpga_version(struct bladerf *dev, struct bladerf_version *version)
{
    memcpy(version, &dev->fpga_version, sizeof(*version));
    return 0;
}

int bladerf_stats(struct bladerf *dev, struct bladerf_stats *stats)
{
    return dev->fn->stats(dev, stats);
}

bladerf_dev_speed bladerf_device_speed(struct bladerf *dev)
{
    return dev->usb_speed;
}

/*------------------------------------------------------------------------------
 * Device Programming
 *----------------------------------------------------------------------------*/
int bladerf_flash_firmware(struct bladerf *dev, const char *firmware_file)
{
    int status;
    uint8_t *buf, *buf_padded;
    size_t buf_size, buf_size_padded;

    status = read_file(firmware_file, &buf, &buf_size);
    if (!status) {
        /* Sanity check firmware
         *
         * Quick and dirty check for any absurd sizes. This is arbitrarily
         * chosen based upon the current FX3 image size.
         *
         * TODO This should be replaced with something that also looks for:
         *  - Known header/footer on images?
         *  - Checksum/hash?
         */
        if (!getenv("BLADERF_SKIP_FW_SIZE_CHECK") &&
                (buf_size < (50 * 1024) || (buf_size > (1 * 1024 * 1024)))) {
            log_error("Error: Detected potentially invalid firmware file.\n");
            log_error("Define BLADERF_SKIP_FW_SIZE_CHECK in your evironment "
                       "to skip this check.\n");
            status = BLADERF_ERR_INVAL;
        } else {
            /* Pad firmare data out to a flash sector size */
            const size_t sector_size = BLADERF_FLASH_SECTOR_SIZE;
            size_t buf_size_padding = sector_size - (buf_size % sector_size);

            buf_size_padded = buf_size + buf_size_padding;
            buf_padded = realloc(buf, buf_size_padded);
            if (!buf_padded) {
                status = BLADERF_ERR_MEM;
            } else {
                buf = buf_padded;
                memset(buf + buf_size, 0xFF, buf_size_padded - buf_size);
            }

            if (!status) {
                status = dev->fn->flash_firmware(dev, buf, buf_size_padded);
            }
            if (!status) {
                if (dev->legacy & LEGACY_ALT_SETTING) {
                    printf("DEVICE OPERATING IN LEGACY MODE, MANUAL RESET IS NECESSARY AFTER SUCCESSFUL UPGRADE\n");
                }
            }
        }

        free(buf);
    }

    return status;
}

int bladerf_erase_flash(struct bladerf *dev, uint32_t addr, uint32_t len)
{
    if (!dev->fn->erase_flash) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    return dev->fn->erase_flash(dev, addr, len);
}

int bladerf_read_flash(struct bladerf *dev, uint32_t addr,
                       uint8_t *buf, uint32_t len)
{
    if (!dev->fn->read_flash) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    return dev->fn->read_flash(dev, addr, buf, len);
}

int bladerf_write_flash(struct bladerf *dev, uint32_t addr,
                       uint8_t *buf, uint32_t len)
{
    if (!dev->fn->write_flash) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    return dev->fn->write_flash(dev, addr, buf, len);
}

int bladerf_device_reset(struct bladerf *dev)
{
    return dev->fn->device_reset(dev);
}

int bladerf_jump_to_bootloader(struct bladerf *dev)
{
    if (!dev->fn->jump_to_bootloader) {
        return BLADERF_ERR_UNSUPPORTED;
    }

    return dev->fn->jump_to_bootloader(dev);
}

int bladerf_flash_fpga(struct bladerf *dev, const char *fpga_file)
{
    int status;
    char fpga_len[10];
    uint8_t *buf, *buf_padded, *ver;
    size_t buf_size, buf_size_padded;
    int hp_idx = 0;

    if (strcmp("X", fpga_file) == 0) {
        printf("Disabling FPGA flash auto-load\n");
        return (dev->fn->erase_flash(dev, flash_from_sectors(4), BLADERF_FLASH_SECTOR_SIZE) != BLADERF_FLASH_SECTOR_SIZE);
    }

    status = read_file(fpga_file, &buf, &buf_size);
    if (status == 0) {
        if ((getenv("BLADERF_SKIP_FPGA_SIZE_CHECK") == 0)  &&
                (buf_size < (1 * 1024 * 1024) || (buf_size > (5 * 1024 * 1024)))) {
            log_error("Error: Detected potentially invalid firmware file.\n");
            log_error("Define BLADERF_SKIP_FPGA_SIZE_CHECK in your evironment "
                       "to skip this check.\n");
            status = BLADERF_ERR_INVAL;
        } else {
            const size_t page_size = BLADERF_FLASH_SECTOR_SIZE;
            size_t buf_size_padding = page_size - (buf_size % page_size);

            /* Pad firmare data out to a flash page size */
            buf_size_padded = buf_size + buf_size_padding + page_size;
            buf_padded = (uint8_t*)realloc(buf, buf_size_padded);
            if (buf_padded == NULL) {
                status = BLADERF_ERR_MEM;
            } else {
                buf = buf_padded;
                memset(buf + buf_size, 0xFF, buf_size_padded - buf_size - BLADERF_FLASH_SECTOR_SIZE);
                memmove(&buf[BLADERF_FLASH_PAGE_SIZE], buf, buf_size_padded - BLADERF_FLASH_SECTOR_SIZE);
                snprintf(fpga_len, 9, "%d", (int)buf_size);
                memset(buf, 0xff, BLADERF_FLASH_PAGE_SIZE);
                encode_field((char *)buf, BLADERF_FLASH_PAGE_SIZE, &hp_idx, "LEN", fpga_len);

                if (status == 0) {
                    status = dev->fn->erase_flash(dev, flash_from_sectors(4), (uint32_t)buf_size_padded);
                }

                if (status >= 0) {
                    status = dev->fn->write_flash(dev, flash_from_pages(1024), buf, (uint32_t)buf_size_padded);
                }

                ver = (uint8_t *)malloc(buf_size_padded);
                if (!ver)
                    status = BLADERF_ERR_MEM;

                if (status >= 0) {
                    status = dev->fn->read_flash(dev, flash_from_pages(1024), ver, (uint32_t)buf_size_padded);
                }

                if ((size_t)status == buf_size_padded) {
                    status = memcmp(buf, ver, buf_size_padded);
                }

                free(ver);
            }
        }
        free(buf);
    }

    return status;
}

int bladerf_load_fpga(struct bladerf *dev, const char *fpga_file)
{
    uint8_t *buf;
    size_t  buf_size;
    int status;

    /* TODO sanity check FPGA:
     *  - Check for x40 vs x115 and verify FPGA image size
     *  - Known header/footer on images?
     *  - Checksum/hash?
     */

    status = read_file(fpga_file, &buf, &buf_size);
    if (!status) {
        status = dev->fn->load_fpga(dev, buf, buf_size);
        free(buf);
    }

    if (!status) {
        status = bladerf_init_device(dev);
    }

    return status;
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
        case BLADERF_ERR_NODEV:
            return "No devices available";
        case BLADERF_ERR_UNSUPPORTED:
            return "Operation not supported";
        case BLADERF_ERR_MISALIGNED:
            return "Misaligned flash access";
        case 0:
            return "Success";
        default:
            return "Unknown error code";
    }
}

void bladerf_version(struct bladerf_version *version)
{
    version->major = LIBBLADERF_VERSION_MAJOR;
    version->minor = LIBBLADERF_VERSION_MINOR;
    version->patch = LIBBLADERF_VERSION_PATCH;
    version->describe = LIBBLADERF_VERSION;
}

void bladerf_log_set_verbosity(bladerf_log_level level)
{
    log_set_verbosity(level);
}

/*------------------------------------------------------------------------------
 * Device identifier information
 *----------------------------------------------------------------------------*/

void bladerf_init_devinfo(struct bladerf_devinfo *info)
{
    info->backend  = BLADERF_BACKEND_ANY;

    memset(info->serial, 0, BLADERF_SERIAL_LENGTH);
    strncpy(info->serial, DEVINFO_SERIAL_ANY, BLADERF_SERIAL_LENGTH - 1);

    info->usb_bus  = DEVINFO_BUS_ANY;
    info->usb_addr = DEVINFO_ADDR_ANY;
    info->instance = DEVINFO_INST_ANY;
}

int bladerf_get_devinfo(struct bladerf *dev, struct bladerf_devinfo *info)
{
    if (dev) {
        memcpy(info, &dev->ident, sizeof(struct bladerf_devinfo));
        return 0;
    } else {
        return BLADERF_ERR_INVAL;
    }
}

int bladerf_get_devinfo_from_str(const char *devstr,
                                 struct bladerf_devinfo *info)
{
    return str2devinfo(devstr, info);
}

bool bladerf_devinfo_matches(const struct bladerf_devinfo *a,
                             const struct bladerf_devinfo *b)
{
    return bladerf_instance_matches(a, b) &&
           bladerf_serial_matches(a, b)   &&
           bladerf_bus_addr_matches(a ,b);
}


bool bladerf_devstr_matches(const char *dev_str,
                            struct bladerf_devinfo *info)
{
    int status;
    bool ret;
    struct bladerf_devinfo from_str;

    status = str2devinfo(dev_str, &from_str);
    if (status < 0) {
        ret = false;
        log_error("Failed to parse device string: %s\n",
                  bladerf_strerror(status));
    } else {
        ret = bladerf_devinfo_matches(&from_str, info);
    }

    return ret;
}

/*------------------------------------------------------------------------------
 * Si5338 register read / write functions
 *----------------------------------------------------------------------------*/

int bladerf_si5338_read(struct bladerf *dev, uint8_t address, uint8_t *val)
{
    return dev->fn->si5338_read(dev,address,val);
}

int bladerf_si5338_write(struct bladerf *dev, uint8_t address, uint8_t val)
{
    return dev->fn->si5338_write(dev,address,val);
}

/*------------------------------------------------------------------------------
 * LMS register read / write functions
 *----------------------------------------------------------------------------*/

int bladerf_lms_read(struct bladerf *dev, uint8_t address, uint8_t *val)
{
    return dev->fn->lms_read(dev,address,val);
}

int bladerf_lms_write(struct bladerf *dev, uint8_t address, uint8_t val)
{
    return dev->fn->lms_write(dev,address,val);
}

/*------------------------------------------------------------------------------
 * GPIO register read / write functions
 *----------------------------------------------------------------------------*/

int bladerf_config_gpio_read(struct bladerf *dev, uint32_t *val)
{
    return dev->fn->config_gpio_read(dev,val);
}

int bladerf_config_gpio_write(struct bladerf *dev, uint32_t val)
{
    /* If we're connected at HS, we need to use smaller DMA transfers */
    if (dev->usb_speed == BLADERF_DEVICE_SPEED_HIGH   ) {
        val |= BLADERF_GPIO_FEATURE_SMALL_DMA_XFER;
    } else if (dev->usb_speed == BLADERF_DEVICE_SPEED_SUPER) {
        val &= ~BLADERF_GPIO_FEATURE_SMALL_DMA_XFER;
    } else {
        log_error("Encountered unknown USB speed in %s\n", __FUNCTION__);
        return BLADERF_ERR_UNEXPECTED;
    }

    return dev->fn->config_gpio_write(dev,val);

}

/*------------------------------------------------------------------------------
 * VCTCXO DAC register write
 *----------------------------------------------------------------------------*/

int bladerf_dac_write(struct bladerf *dev, uint16_t val)
{
    return dev->fn->dac_write(dev,val);
}

/*------------------------------------------------------------------------------
 * DC Calibration routines
 *----------------------------------------------------------------------------*/
 int bladerf_calibrate_dc(struct bladerf *dev, bladerf_cal_module module)
 {
    return lms_calibrate_dc(dev, module);
 }
