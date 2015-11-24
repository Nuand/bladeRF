/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2015 Nuand LLC
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
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

#include "bladerf_priv.h"
#include "usb.h"
#include "nios_access.h"
#include "nios_pkt_formats.h"
#include "capabilities.h"
#include "log.h"

//#define PRINT_BUFFERS
#ifdef PRINT_BUFFERS
static void print_buf(const char *msg, const uint8_t *buf, size_t len)
{
    size_t i;
    if (msg != NULL) {
        fputs(msg, stderr);
    }

    for (i = 0; i < len; i++) {
        if (i == 0) {
            fprintf(stderr, "  0x%02x,", buf[i]);
        } else if ((i + 1) % 8 == 0) {
            fprintf(stderr, " 0x%02x,\n ", buf[i]);
        } else {
            fprintf(stderr, " 0x%02x,", buf[i]);
        }
    }

    fputc('\n', stderr);
}
#else
#define print_buf(msg, data, len) do {} while(0)
#endif

/* Buf is assumed to be NIOS_PKT_LEN bytes */
static int nios_access(struct bladerf *dev, uint8_t *buf)
{
    int status;
    void *driver;
    struct bladerf_usb *usb = usb_backend(dev, &driver);

    print_buf("NIOS II request:\n", buf, NIOS_PKT_LEN);

    /* Send the command */
    status = usb->fn->bulk_transfer(driver, PERIPHERAL_EP_OUT,
                                     buf, NIOS_PKT_LEN,
                                     PERIPHERAL_TIMEOUT_MS);
    if (status != 0) {
        log_debug("Failed to send NIOS II request: %s\n",
                  bladerf_strerror(status));
        return status;
    }

    /* Retrieve the request */
    status = usb->fn->bulk_transfer(driver, PERIPHERAL_EP_IN,
                                    buf, NIOS_PKT_LEN,
                                    PERIPHERAL_TIMEOUT_MS);

    if (status != 0) {
        log_debug("Failed to receive NIOS II response: %s\n",
                  bladerf_strerror(status));
    }

    print_buf("NIOS II response:\n", buf, NIOS_PKT_LEN);

    return status;
}

int nios_8x8_read(struct bladerf *dev, uint8_t id, uint8_t addr, uint8_t *data)
{
    int status;
    uint8_t buf[NIOS_PKT_LEN];
    bool success;

    nios_pkt_8x8_pack(buf, id, false, addr, 0);

    status = nios_access(dev, buf);
    if (status != 0) {
        return status;
    }

    nios_pkt_8x8_resp_unpack(buf, NULL, NULL, NULL, data, &success);

    if (success) {
        return 0;
    } else {
        log_debug("%s: response packet reported failure.\n", __FUNCTION__);
        *data = 0;
        return BLADERF_ERR_FPGA_OP;
    }
}

int nios_8x8_write(struct bladerf *dev, uint8_t id, uint8_t addr, uint8_t data)
{
    int status;
    uint8_t buf[NIOS_PKT_LEN];
    bool success;

    nios_pkt_8x8_pack(buf, id, true, addr, data);

    status = nios_access(dev, buf);
    if (status != 0) {
        return status;
    }

    nios_pkt_8x8_resp_unpack(buf, NULL, NULL, NULL, NULL, &success);

    if (success) {
        return 0;
    } else {
        log_debug("%s: response packet reported failure.\n", __FUNCTION__);
        return BLADERF_ERR_FPGA_OP;
    }
}

int nios_8x16_read(struct bladerf *dev, uint8_t id,
                   uint8_t addr, uint16_t *data)
{
    int status;
    uint8_t buf[NIOS_PKT_LEN];
    bool success;
    uint16_t tmp;

    nios_pkt_8x16_pack(buf, id, false, addr, 0);

    status = nios_access(dev, buf);
    if (status != 0) {
        return status;
    }

    nios_pkt_8x16_resp_unpack(buf, NULL, NULL, NULL, &tmp, &success);

    if (success) {
        *data = tmp;
        return 0;
    } else {
        *data = 0;
        log_debug("%s: response packet reported failure.\n", __FUNCTION__);
        return BLADERF_ERR_FPGA_OP;
    }
}

int nios_8x16_write(struct bladerf *dev, uint8_t id,
                    uint8_t addr, uint16_t data)
{
    int status;
    uint8_t buf[NIOS_PKT_LEN];
    bool success;

    nios_pkt_8x16_pack(buf, id, true, addr, data);

    status = nios_access(dev, buf);
    if (status != 0) {
        return status;
    }

    nios_pkt_8x16_resp_unpack(buf, NULL, NULL, NULL, NULL, &success);

    if (success) {
        return 0;
    } else {
        log_debug("%s: response packet reported failure.\n", __FUNCTION__);
        return BLADERF_ERR_FPGA_OP;
    }
}

int nios_8x32_read(struct bladerf *dev, uint8_t id,
                   uint8_t addr, uint32_t *data)
{
    int status;
    uint8_t buf[NIOS_PKT_LEN];
    bool success;

    nios_pkt_8x32_pack(buf, id, false, 0, 0);

    status = nios_access(dev, buf);
    if (status != 0) {
        return status;
    }

    nios_pkt_8x32_resp_unpack(buf, NULL, NULL, NULL, data, &success);

    if (success) {
        return 0;
    } else {
        *data = 0;
        log_debug("%s: response packet reported failure.\n", __FUNCTION__);
        return BLADERF_ERR_FPGA_OP;
    }
}

int nios_8x32_write(struct bladerf *dev, uint8_t id,
                    uint8_t addr, uint32_t data)
{
    int status;
    uint8_t buf[NIOS_PKT_LEN];
    bool success;

    nios_pkt_8x32_pack(buf, id, true, addr, data);

    status = nios_access(dev, buf);
    if (status != 0) {
        return status;
    }

    nios_pkt_8x32_resp_unpack(buf, NULL, NULL, NULL, NULL, &success);

    if (success) {
        return 0;
    } else {
        log_debug("%s: response packet reported failure.\n", __FUNCTION__);
        return BLADERF_ERR_FPGA_OP;
    }
}

static int nios_32x32_masked_read(struct bladerf *dev, uint8_t id,
                                  uint32_t mask, uint32_t *val)
{
    int status;
    bool success;
    uint8_t buf[NIOS_PKT_LEN];

    /* The address is used as a mask of bits to read and return */
    nios_pkt_32x32_pack(buf, id, false, mask, 0);

    status = nios_access(dev, buf);
    if (status != 0) {
        return status;
    }

    nios_pkt_32x32_resp_unpack(buf, NULL, NULL, NULL, val, &success);

    if (success) {
        return 0;
    } else {
        *val = 0;
        log_debug("%s: response packet reported failure.\n", __FUNCTION__);
        return BLADERF_ERR_FPGA_OP;
    }
}

static int nios_32x32_masked_write(struct bladerf *dev, uint8_t id,
                                   uint32_t mask, uint32_t val)
{
    int status;
    bool success;
    uint8_t buf[NIOS_PKT_LEN];

    /* The address is used as a mask of bits to read and return */
    nios_pkt_32x32_pack(buf, id, true, mask, val);

    status = nios_access(dev, buf);
    if (status != 0) {
        return status;
    }

    nios_pkt_32x32_resp_unpack(buf, NULL, NULL, NULL, NULL, &success);

    if (success) {
        return 0;
    } else {
        log_debug("%s: response packet reported failure.\n", __FUNCTION__);
        return BLADERF_ERR_FPGA_OP;
    }
}


int nios_config_read(struct bladerf *dev, uint32_t *val)
{
    int status = nios_8x32_read(dev, NIOS_PKT_8x32_TARGET_CONTROL, 0, val);

    if (status == 0) {
        log_verbose("%s: Read 0x%08x\n", __FUNCTION__, *val);
    }

    return status;
}

int nios_config_write(struct bladerf *dev, uint32_t val)
{
    int status = nios_8x32_write(dev, NIOS_PKT_8x32_TARGET_CONTROL, 0, val);

    if (status == 0) {
        log_verbose("%s: Wrote 0x%08x\n", __FUNCTION__, val);
    }

    return status;
}

int nios_get_fpga_version(struct bladerf *dev, struct bladerf_version *ver)
{
    uint32_t regval;
    int status = nios_8x32_read(dev, NIOS_PKT_8x32_TARGET_VERSION, 0, &regval);

    if (status == 0) {
        log_verbose("%s: Read FPGA version word: 0x%08x\n",
                    __FUNCTION__, regval);

        ver->major = (regval >> 24) & 0xff;
        ver->minor = (regval >> 16) & 0xff;
        ver->patch = LE16_TO_HOST(regval & 0xffff);

        snprintf((char*)ver->describe, BLADERF_VERSION_STR_MAX,
                 "%d.%d.%d", ver->major, ver->minor, ver->patch);

        return 0;
    }

    return status;
}

int nios_get_timestamp(struct bladerf *dev, bladerf_module module,
                       uint64_t *timestamp)
{
    int status;
    uint8_t buf[NIOS_PKT_LEN];
    uint8_t addr;
    bool success;

    switch (module) {
        case BLADERF_MODULE_RX:
            addr = NIOS_PKT_8x64_TIMESTAMP_RX;
            break;

        case BLADERF_MODULE_TX:
            addr = NIOS_PKT_8x64_TIMESTAMP_TX;
            break;

        default:
            log_debug("Invalid module: %d\n", module);
            return BLADERF_ERR_INVAL;
    }

    nios_pkt_8x64_pack(buf, NIOS_PKT_8x64_TARGET_TIMESTAMP, false, addr, 0);

    status = nios_access(dev, buf);
    if (status != 0) {
        return status;
    }

    nios_pkt_8x64_resp_unpack(buf, NULL, NULL, NULL, timestamp, &success);

    if (success) {
        log_verbose("%s: Read %s timstamp: 0x%"PRIu64"\n",
                    __FUNCTION__, module2str(module), timestamp);
        return 0;
    } else {
        log_debug("%s: response packet reported failure.\n", __FUNCTION__);
        *timestamp = 0;
        return BLADERF_ERR_FPGA_OP;
    }
}

int nios_si5338_read(struct bladerf *dev, uint8_t addr, uint8_t *data)
{
    int status = nios_8x8_read(dev, NIOS_PKT_8x8_TARGET_SI5338, addr, data);

    if (status == 0) {
        log_verbose("%s: Read 0x%02x from addr 0x%02x\n",
                    __FUNCTION__, *data, addr);
    }

    return status;
}

int nios_si5338_write(struct bladerf *dev, uint8_t addr, uint8_t data)
{
    int status = nios_8x8_write(dev, NIOS_PKT_8x8_TARGET_SI5338, addr, data);

    if (status == 0) {
        log_verbose("%s: Wrote 0x%02x to addr 0x%02x\n",
                    __FUNCTION__, data, addr);
    }

    return status;
}

int nios_lms6_read(struct bladerf *dev, uint8_t addr, uint8_t *data)
{
    int status = nios_8x8_read(dev, NIOS_PKT_8x8_TARGET_LMS6, addr, data);

    if (status == 0) {
        log_verbose("%s: Read 0x%02x from addr 0x%02x\n",
                    __FUNCTION__, *data, addr);
    }

    return status;
}

int nios_lms6_write(struct bladerf *dev, uint8_t addr, uint8_t data)
{
    int status = nios_8x8_write(dev, NIOS_PKT_8x8_TARGET_LMS6, addr, data);

    if (status == 0) {
        log_verbose("%s: Wrote 0x%02x to addr 0x%02x\n",
                    __FUNCTION__, data, addr);
    }

    return status;
}

int nios_vctcxo_trim_dac_write(struct bladerf *dev, uint16_t value)
{
    int status;

    /* Ensure device is in write-through mode */
    status = nios_8x16_write(dev, NIOS_PKT_8x16_TARGET_VCTCXO_DAC, 0x28, 0);
    if (status == 0) {
        /* Write DAC value to channel 0 */
        status = nios_8x16_write(dev, NIOS_PKT_8x16_TARGET_VCTCXO_DAC,
                                 0x08, value);

        log_verbose("%s: Wrote 0x%04x\n", __FUNCTION__, value);
    }

    return status;
}

int nios_vctcxo_trim_dac_read(struct bladerf *dev, uint16_t *value)
{
    int status;

    if (!have_cap(dev, BLADERF_CAP_VCTCXO_TRIMDAC_READ)) {
        *value = 0x0000;

        log_debug("FPGA %s does not support VCTCXO trimdac readback.\n",
                  dev->fpga_version.describe);

        return BLADERF_ERR_UNSUPPORTED;
    }

    status = nios_8x16_read(dev, NIOS_PKT_8x16_TARGET_VCTCXO_DAC, 0x98, value);
    if (status != 0) {
        *value = 0;
    } else {
        log_verbose("%s: Read 0x%04x\n", __FUNCTION__, *value);
    }

    return status;
}

int nios_set_vctcxo_tamer_mode(struct bladerf *dev,
                               bladerf_vctcxo_tamer_mode mode)
{
    int status;

    if (!have_cap(dev, BLADERF_CAP_VCTCXO_TAMING_MODE)) {
        log_debug("FPGA %s does not support VCTCXO taming via an input source\n",
                  dev->fpga_version.describe);

        return BLADERF_ERR_UNSUPPORTED;
    }


    status = nios_8x8_write(dev,
                            NIOS_PKT_8x8_TARGET_VCTCXO_TAMER, 0xff,
                            (uint8_t) mode);

    if (status == 0) {
        log_verbose("%s: Wrote mode=0x%02x\n", __FUNCTION__, mode);
    }

    return status;
}

int nios_get_vctcxo_tamer_mode(struct bladerf *dev,
                               bladerf_vctcxo_tamer_mode *mode)
{
    int status;
    uint8_t tmp;

    *mode = BLADERF_VCTCXO_TAMER_INVALID;

    if (!have_cap(dev, BLADERF_CAP_VCTCXO_TAMING_MODE)) {
        log_debug("FPGA %s does not support VCTCXO taming via an input source\n",
                  dev->fpga_version.describe);

        return BLADERF_ERR_UNSUPPORTED;
    }

    status = nios_8x8_read(dev, NIOS_PKT_8x8_TARGET_VCTCXO_TAMER, 0xff, &tmp);
    if (status == 0) {
        log_verbose("%s: Read mode=0x%02x\n", __FUNCTION__, tmp);

        switch ((bladerf_vctcxo_tamer_mode) tmp) {
            case BLADERF_VCTCXO_TAMER_DISABLED:
            case BLADERF_VCTCXO_TAMER_1_PPS:
            case BLADERF_VCTCXO_TAMER_10_MHZ:
                *mode = (bladerf_vctcxo_tamer_mode) tmp;
                break;

            default:
                status = BLADERF_ERR_UNEXPECTED;
        }
    }

    return status;
}


int nios_get_iq_gain_correction(struct bladerf *dev, bladerf_module module,
                                int16_t *value)
{
    int status = BLADERF_ERR_INVAL;
    uint16_t tmp = 0;

    switch (module) {
        case BLADERF_MODULE_RX:
            status = nios_8x16_read(dev, NIOS_PKT_8x16_TARGET_IQ_CORR,
                                    NIOS_PKT_8x16_ADDR_IQ_CORR_RX_GAIN, &tmp);
            break;

        case BLADERF_MODULE_TX:
            status = nios_8x16_read(dev, NIOS_PKT_8x16_TARGET_IQ_CORR,
                                    NIOS_PKT_8x16_ADDR_IQ_CORR_TX_GAIN, &tmp);
            break;

        default:
            log_debug("Invalid module: %d\n", module);
    }

    *value = (int16_t) tmp;

    if (status == 0) {
        log_verbose("%s: Read %s %d\n",
                    __FUNCTION__, module2str(module), *value);
    }

    return status;
}

int nios_get_iq_phase_correction(struct bladerf *dev, bladerf_module module,
                                 int16_t *value)
{
    int status = BLADERF_ERR_INVAL;
    uint16_t tmp = 0;

    switch (module) {
        case BLADERF_MODULE_RX:
            status = nios_8x16_read(dev, NIOS_PKT_8x16_TARGET_IQ_CORR,
                                    NIOS_PKT_8x16_ADDR_IQ_CORR_RX_PHASE, &tmp);
            break;

        case BLADERF_MODULE_TX:
            status = nios_8x16_read(dev, NIOS_PKT_8x16_TARGET_IQ_CORR,
                                    NIOS_PKT_8x16_ADDR_IQ_CORR_TX_PHASE, &tmp);
            break;

        default:
            log_debug("Invalid module: %d\n", module);
    }

    *value = (int16_t) tmp;

    if (status == 0) {
        log_verbose("%s: Read %s %d\n",
                    __FUNCTION__, module2str(module), *value);
    }

    return status;
}

int nios_set_iq_gain_correction(struct bladerf *dev, bladerf_module module,
                                int16_t value)
{
    int status = BLADERF_ERR_INVAL;

    switch (module) {
        case BLADERF_MODULE_RX:
            log_verbose("Setting RX IQ Correction gain: %d\n", value);
            status = nios_8x16_write(dev, NIOS_PKT_8x16_TARGET_IQ_CORR,
                                     NIOS_PKT_8x16_ADDR_IQ_CORR_RX_GAIN, value);
            break;

        case BLADERF_MODULE_TX:
            log_verbose("Setting TX IQ Correction gain: %d\n", value);
            status = nios_8x16_write(dev, NIOS_PKT_8x16_TARGET_IQ_CORR,
                                     NIOS_PKT_8x16_ADDR_IQ_CORR_TX_GAIN, value);
            break;

        default:
            log_debug("Invalid module: %d\n", module);
    }

    if (status == 0) {
        log_verbose("%s: Wrote %s %d\n",
                    __FUNCTION__, module2str(module), value);
    }

    return status;
}

int nios_set_iq_phase_correction(struct bladerf *dev, bladerf_module module,
                                 int16_t value)
{
    int status = BLADERF_ERR_INVAL;

    switch (module) {
        case BLADERF_MODULE_RX:
            log_verbose("Setting RX IQ Correction phase: %d\n", value);
            status = nios_8x16_write(dev, NIOS_PKT_8x16_TARGET_IQ_CORR,
                                     NIOS_PKT_8x16_ADDR_IQ_CORR_RX_PHASE, value);
            break;

        case BLADERF_MODULE_TX:
            log_verbose("Setting TX IQ Correction phase: %d\n", value);
            status = nios_8x16_write(dev, NIOS_PKT_8x16_TARGET_IQ_CORR,
                                     NIOS_PKT_8x16_ADDR_IQ_CORR_TX_PHASE, value);
            break;

        default:
            log_debug("Invalid module: %d\n", module);
    }

    if (status == 0) {
        log_verbose("%s: Wrote %s %d\n",
                    __FUNCTION__, module2str(module), value);
    }


    return status;
}

int nios_xb200_synth_write(struct bladerf *dev, uint32_t value)
{
    int status = nios_8x32_write(dev, NIOS_PKT_8x32_TARGET_ADF4351, 0, value);

    if (status == 0) {
        log_verbose("%s: Wrote 0x%08x\n", __FUNCTION__, value);
    }

    return status;
}

int nios_expansion_gpio_read(struct bladerf *dev, uint32_t *val)
{
    int status = nios_32x32_masked_read(dev, NIOS_PKT_32x32_TARGET_EXP,
                                        0xffffffff, val);

    if (status == 0) {
        log_verbose("%s: Read 0x%08x\n", __FUNCTION__, *val);
    }

    return status;
}

int nios_expansion_gpio_write(struct bladerf *dev, uint32_t mask, uint32_t val)
{
    int status = nios_32x32_masked_write(dev, NIOS_PKT_32x32_TARGET_EXP,
                                         mask, val);

    if (status == 0) {
        log_verbose("%s: Wrote 0x%08x (with mask 0x%08x)\n",
                    __FUNCTION__, val, mask);
    }

    return status;
}

int nios_expansion_gpio_dir_read(struct bladerf *dev, uint32_t *val)
{
    int status = nios_32x32_masked_read(dev, NIOS_PKT_32x32_TARGET_EXP_DIR,
                                        0xffffffff, val);

    if (status == 0) {
        log_verbose("%s: Read 0x%08x\n", __FUNCTION__, *val);
    }

    return status;
}

int nios_expansion_gpio_dir_write(struct bladerf *dev,
                                  uint32_t mask, uint32_t val)
{
    int status = nios_32x32_masked_write(dev, NIOS_PKT_32x32_TARGET_EXP_DIR,
                                         mask, val);

    if (status == 0) {
        log_verbose("%s: Wrote 0x%08x (with mask 0x%08x)\n",
                    __FUNCTION__, val, mask);
    }

    return status;
}

int nios_retune(struct bladerf *dev, bladerf_module module,
                uint64_t timestamp, uint16_t nint, uint32_t nfrac,
                uint8_t freqsel, uint8_t vcocap, bool low_band,
                bool quick_tune)
{
    int status;
    uint8_t buf[NIOS_PKT_LEN];

    uint8_t resp_flags;
    uint64_t duration;

    if (timestamp == NIOS_PKT_RETUNE_CLEAR_QUEUE) {
        log_verbose("Clearing %s retune queue.\n", module2str(module));
    } else {
        log_verbose("%s: module=%s timestamp=%"PRIu64" nint=%u nfrac=%u\n\t\t\t\t"
                    "freqsel=0x%02x vcocap=0x%02x low_band=%d quick_tune=%d\n",
                    __FUNCTION__, module2str(module), timestamp, nint, nfrac,
                    freqsel, vcocap, low_band, quick_tune);
    }

    nios_pkt_retune_pack(buf, module, timestamp,
                         nint, nfrac, freqsel, vcocap, low_band, quick_tune);

    status = nios_access(dev, buf);
    if (status != 0) {
        return status;
    }

    nios_pkt_retune_resp_unpack(buf, &duration, &vcocap, &resp_flags);

    if (resp_flags & NIOS_PKT_RETUNERESP_FLAG_TSVTUNE_VALID) {
        log_verbose("%s retune operation: vcocap=%u, duration=%"PRIu64"\n",
                    module2str(module), vcocap, duration);
    } else {
        log_verbose("%s operation duration: %"PRIu64"\n",
                    module2str(module), duration);
    }

    if ((resp_flags & NIOS_PKT_RETUNERESP_FLAG_SUCCESS) == 0) {
        if (timestamp == BLADERF_RETUNE_NOW) {
            log_debug("FPGA tuning reported failure.\n");
            status = BLADERF_ERR_UNEXPECTED;
        } else {
            log_debug("The FPGA's retune queue is full. Try again after "
                      "a previous request has completed.\n");
            status = BLADERF_ERR_QUEUE_FULL;
        }
    }

    return status;
}
