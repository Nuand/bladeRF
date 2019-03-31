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

#include "log.h"
#include "conversions.h"

#include "usb.h"
#include "nios_access.h"
#include "nios_pkt_formats.h"

#include "board/board.h"
#include "helpers/version.h"

#if 0
static void print_buf(const char *msg, const uint8_t *buf, size_t len)
{
    size_t i;
    if (msg != NULL) {
        fputs(msg, stderr);
    }

    for (i = 0; i < len; i++) {
        fprintf(stderr, " %02x", buf[i]);
    }

    fputc('\n', stderr);
}
#else
#define print_buf(msg, data, len) do {} while(0)
#endif

/* Buf is assumed to be NIOS_PKT_LEN bytes */
static int nios_access(struct bladerf *dev, uint8_t *buf)
{
    struct bladerf_usb *usb = dev->backend_data;
    int status;

    print_buf("NIOS II REQ:", buf, NIOS_PKT_LEN);

    /* Send the command */
    status = usb->fn->bulk_transfer(usb->driver, PERIPHERAL_EP_OUT, buf,
                                    NIOS_PKT_LEN, PERIPHERAL_TIMEOUT_MS);
    if (status != 0) {
        log_error("Failed to send NIOS II request: %s\n",
                  bladerf_strerror(status));
        return status;
    }

    /* Retrieve the request */
    status = usb->fn->bulk_transfer(usb->driver, PERIPHERAL_EP_IN, buf,
                                    NIOS_PKT_LEN, PERIPHERAL_TIMEOUT_MS);
    if (status != 0) {
        log_error("Failed to receive NIOS II response: %s\n",
                  bladerf_strerror(status));
    }

    print_buf("NIOS II res:", buf, NIOS_PKT_LEN);

    return status;
}

/* Variant that doesn't output to log_error on error. */
static int nios_access_quiet(struct bladerf *dev, uint8_t *buf)
{
    struct bladerf_usb *usb = dev->backend_data;
    int status;

    print_buf("NIOS II REQ:", buf, NIOS_PKT_LEN);

    /* Send the command */
    status = usb->fn->bulk_transfer(usb->driver, PERIPHERAL_EP_OUT, buf,
                                    NIOS_PKT_LEN, PERIPHERAL_TIMEOUT_MS);
    if (status != 0) {
        return status;
    }

    /* Retrieve the request */
    status = usb->fn->bulk_transfer(usb->driver, PERIPHERAL_EP_IN, buf,
                                    NIOS_PKT_LEN, PERIPHERAL_TIMEOUT_MS);

    print_buf("NIOS II res:", buf, NIOS_PKT_LEN);

    return status;
}

static int nios_8x8_read(struct bladerf *dev, uint8_t id,
                         uint8_t addr, uint8_t *data)
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

static int nios_8x8_write(struct bladerf *dev, uint8_t id,
                          uint8_t addr, uint8_t data)
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

static int nios_8x16_read(struct bladerf *dev, uint8_t id,
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

static int nios_8x16_write(struct bladerf *dev, uint8_t id,
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

static int nios_8x32_read(struct bladerf *dev, uint8_t id,
                          uint8_t addr, uint32_t *data)
{
    int status;
    uint8_t buf[NIOS_PKT_LEN];
    bool success;

    nios_pkt_8x32_pack(buf, id, false, addr, 0);

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

static int nios_8x32_write(struct bladerf *dev, uint8_t id,
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

static int nios_16x64_read(struct bladerf *dev,
                           uint8_t id,
                           uint16_t addr,
                           uint64_t *data)
{
    int status;
    uint8_t buf[NIOS_PKT_LEN];
    bool success;

    nios_pkt_16x64_pack(buf, id, false, addr, 0);

    /* RFIC access times out occasionally, and this is fine. */
    if (NIOS_PKT_16x64_TARGET_RFIC == id) {
        status = nios_access_quiet(dev, buf);
    } else {
        status = nios_access(dev, buf);
    }

    if (status != 0) {
        return status;
    }

    nios_pkt_16x64_resp_unpack(buf, NULL, NULL, NULL, data, &success);

    if (success) {
        return 0;
    } else {
        *data = 0;
        log_debug("%s: response packet reported failure.\n", __FUNCTION__);
        return BLADERF_ERR_FPGA_OP;
    }
}

static int nios_16x64_write(struct bladerf *dev,
                            uint8_t id,
                            uint16_t addr,
                            uint64_t data)
{
    int status;
    uint8_t buf[NIOS_PKT_LEN];
    bool success;

    nios_pkt_16x64_pack(buf, id, true, addr, data);

    /* RFIC access times out occasionally, and this is fine. */
    if (NIOS_PKT_16x64_TARGET_RFIC == id) {
        status = nios_access_quiet(dev, buf);
    } else {
        status = nios_access(dev, buf);
    }

    if (status != 0) {
        return status;
    }

    nios_pkt_16x64_resp_unpack(buf, NULL, NULL, NULL, NULL, &success);

    if (success) {
        return 0;
    } else {
        log_debug("%s: response packet reported failure.\n", __FUNCTION__);
        return BLADERF_ERR_FPGA_OP;
    }
}

static int nios_32x32_read(struct bladerf *dev, uint32_t id,
                          uint32_t addr, uint32_t *data)
{
    int status;
    uint8_t buf[NIOS_PKT_LEN];
    bool success;

    nios_pkt_32x32_pack(buf, id, false, addr, 0);

    status = nios_access(dev, buf);
    if (status != 0) {
        return status;
    }

    nios_pkt_32x32_resp_unpack(buf, NULL, NULL, NULL, data, &success);

    if (success) {
        return 0;
    } else {
        *data = 0;
        log_debug("%s: response packet reported failure.\n", __FUNCTION__);
        return BLADERF_ERR_FPGA_OP;
    }
}

static int nios_32x32_write(struct bladerf *dev, uint32_t id,
                           uint32_t addr, uint32_t data)
{
    int status;
    uint8_t buf[NIOS_PKT_LEN];
    bool success;

    nios_pkt_32x32_pack(buf, id, true, addr, data);

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

int nios_get_timestamp(struct bladerf *dev,
                       bladerf_direction dir,
                       uint64_t *timestamp)
{
    int status;
    uint8_t buf[NIOS_PKT_LEN];
    uint8_t addr;
    bool success;

    switch (dir) {
        case BLADERF_RX:
            addr = NIOS_PKT_8x64_TIMESTAMP_RX;
            break;

        case BLADERF_TX:
            addr = NIOS_PKT_8x64_TIMESTAMP_TX;
            break;

        default:
            log_debug("Invalid direction: %d\n", dir);
            return BLADERF_ERR_INVAL;
    }

    nios_pkt_8x64_pack(buf, NIOS_PKT_8x64_TARGET_TIMESTAMP, false, addr, 0);

    status = nios_access(dev, buf);
    if (status != 0) {
        return status;
    }

    nios_pkt_8x64_resp_unpack(buf, NULL, NULL, NULL, timestamp, &success);

    if (success) {
        log_verbose("%s: Read %s timestamp: %" PRIu64 "\n", __FUNCTION__,
                    direction2str(dir), *timestamp);
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

#ifdef ENABLE_LIBBLADERF_NIOS_ACCESS_LOG_VERBOSE
    if (status == 0) {
        log_verbose("%s: Read 0x%02x from addr 0x%02x\n",
                    __FUNCTION__, *data, addr);
    }
#endif

    return status;
}

int nios_si5338_write(struct bladerf *dev, uint8_t addr, uint8_t data)
{
    int status = nios_8x8_write(dev, NIOS_PKT_8x8_TARGET_SI5338, addr, data);

#ifdef ENABLE_LIBBLADERF_NIOS_ACCESS_LOG_VERBOSE
    if (status == 0) {
        log_verbose("%s: Wrote 0x%02x to addr 0x%02x\n",
                    __FUNCTION__, data, addr);
    }
#endif

    return status;
}

int nios_lms6_read(struct bladerf *dev, uint8_t addr, uint8_t *data)
{
    int status = nios_8x8_read(dev, NIOS_PKT_8x8_TARGET_LMS6, addr, data);

#ifdef ENABLE_LIBBLADERF_NIOS_ACCESS_LOG_VERBOSE
    if (status == 0) {
        log_verbose("%s: Read 0x%02x from addr 0x%02x\n",
                    __FUNCTION__, *data, addr);
    }
#endif

    return status;
}

int nios_lms6_write(struct bladerf *dev, uint8_t addr, uint8_t data)
{
    int status = nios_8x8_write(dev, NIOS_PKT_8x8_TARGET_LMS6, addr, data);

#ifdef ENABLE_LIBBLADERF_NIOS_ACCESS_LOG_VERBOSE
    if (status == 0) {
        log_verbose("%s: Wrote 0x%02x to addr 0x%02x\n",
                    __FUNCTION__, data, addr);
    }
#endif

    return status;
}

int nios_ina219_read(struct bladerf *dev, uint8_t addr, uint16_t *data)
{
    int status;

    status = nios_8x16_read(dev, NIOS_PKT_8x16_TARGET_INA219, addr, data);
    if (status == 0) {
        log_verbose("%s: Read 0x%04x from addr 0x%02x\n",
                    __FUNCTION__, *data, addr);
    }

    return status;
}

int nios_ina219_write(struct bladerf *dev, uint8_t addr, uint16_t data)
{
    int status;

    status = nios_8x16_write(dev, NIOS_PKT_8x16_TARGET_INA219, addr, data);
    if (status == 0) {
        log_verbose("%s: Wrote 0x%04x to addr 0x%02x\n",
                    __FUNCTION__, data, addr);
    }

    return status;
}

#define VERBOSE_OUT_SINGLEBYTE "%s: %s 0x%02x @ addr 0x%04x\n"
#define VERBOSE_OUT_MULTIBYTE "%s: %s 0x%02x @ addr 0x%04x (%d/%d)\n"

int nios_ad9361_spi_read(struct bladerf *dev, uint16_t cmd, uint64_t *data)
{
    int status;

    status = nios_16x64_read(dev, NIOS_PKT_16x64_TARGET_AD9361, cmd, data);

#ifdef ENABLE_LIBBLADERF_NIOS_ACCESS_LOG_VERBOSE
    if (log_get_verbosity() == BLADERF_LOG_LEVEL_VERBOSE && status == 0) {
        size_t bytes = (((cmd >> 12) & 0x7) + 1);
        size_t addr  = cmd & 0xFFF;

        if (bytes > 1) {
            size_t i;
            for (i = 0; i < bytes; ++i) {
                uint8_t byte = (*data >> (56 - 8 * i)) & 0xFF;
                log_verbose(VERBOSE_OUT_MULTIBYTE, "ad9361_spi", " MRead", byte,
                            addr - i, i + 1, bytes);
            }
        } else {
            uint8_t byte = (*data >> 56) & 0xFF;
            log_verbose(VERBOSE_OUT_SINGLEBYTE, "ad9361_spi", "  Read", byte,
                        addr);
        }
    }
#endif

    return status;
}

int nios_ad9361_spi_write(struct bladerf *dev, uint16_t cmd, uint64_t data)
{
    int status;

    status = nios_16x64_write(dev, NIOS_PKT_16x64_TARGET_AD9361, cmd, data);

#ifdef ENABLE_LIBBLADERF_NIOS_ACCESS_LOG_VERBOSE
    if (log_get_verbosity() == BLADERF_LOG_LEVEL_VERBOSE && status == 0) {
        size_t bytes = (((cmd >> 12) & 0x7) + 1) & 0xFF;
        size_t addr  = cmd & 0xFFF;

        if (bytes > 1) {
            size_t i;
            for (i = 0; i < bytes; ++i) {
                uint8_t byte = (data >> (56 - 8 * i)) & 0xFF;
                log_verbose(VERBOSE_OUT_MULTIBYTE, "ad9361_spi", "MWrite", byte,
                            addr - i, i + 1, bytes);
            }
        } else {
            uint8_t byte = (data >> 56) & 0xFF;
            log_verbose(VERBOSE_OUT_SINGLEBYTE, "ad9361_spi", " Write", byte,
                        addr);
        }
    }
#endif

    return status;
}

int nios_adi_axi_read(struct bladerf *dev, uint32_t addr, uint32_t *data)
{
    int status;

    status = nios_32x32_read(dev, NIOS_PKT_32x32_TARGET_ADI_AXI, addr, data);

#ifdef ENABLE_LIBBLADERF_NIOS_ACCESS_LOG_VERBOSE
    if (status == 0) {
        log_verbose("%s:  Read  0x%08" PRIx32 " from addr 0x%04" PRIx32 "\n",
                    __FUNCTION__, *data, addr);
    }
#endif

    return status;
}

int nios_adi_axi_write(struct bladerf *dev, uint32_t addr, uint32_t data)
{
    int status;

    status = nios_32x32_write(dev, NIOS_PKT_32x32_TARGET_ADI_AXI, addr, data);

#ifdef ENABLE_LIBBLADERF_NIOS_ACCESS_LOG_VERBOSE
    if (status == 0) {
        log_verbose("%s: Wrote 0x%08" PRIx32 " to   addr 0x%04" PRIx32 "\n",
                    __FUNCTION__, data, addr);
    }
#endif

    return status;
}

int nios_rfic_command_read(struct bladerf *dev, uint16_t cmd, uint64_t *data)
{
    int status;

    status = nios_16x64_read(dev, NIOS_PKT_16x64_TARGET_RFIC, cmd, data);

#ifdef ENABLE_LIBBLADERF_NIOS_ACCESS_LOG_VERBOSE
    if (status == 0) {
        log_verbose("%s: Read 0x%04x 0x%08x\n", __FUNCTION__, cmd, *data);
    }
#endif

    return status;
}

int nios_rfic_command_write(struct bladerf *dev, uint16_t cmd, uint64_t data)
{
    int status;

    status = nios_16x64_write(dev, NIOS_PKT_16x64_TARGET_RFIC, cmd, data);

#ifdef ENABLE_LIBBLADERF_NIOS_ACCESS_LOG_VERBOSE
    if (status == 0) {
        log_verbose("%s: Write 0x%04x 0x%08x\n", __FUNCTION__, cmd, data);
    }
#endif

    return status;
}

int nios_rffe_control_read(struct bladerf *dev, uint32_t *value)
{
    int status;

    status = nios_8x32_read(dev, NIOS_PKT_8x32_TARGET_RFFE_CSR, 0, value);

#ifdef ENABLE_LIBBLADERF_NIOS_ACCESS_LOG_VERBOSE
    if (status == 0) {
        log_verbose("%s: Read 0x%08x\n", __FUNCTION__, *value);
    }
#endif

    return status;
}

int nios_rffe_control_write(struct bladerf *dev, uint32_t value)
{
    int status;

    status = nios_8x32_write(dev, NIOS_PKT_8x32_TARGET_RFFE_CSR, 0, value);

#ifdef ENABLE_LIBBLADERF_NIOS_ACCESS_LOG_VERBOSE
    if (status == 0) {
        log_verbose("%s: Wrote 0x%08x\n", __FUNCTION__, value);
    }
#endif

    return status;
}

int nios_rffe_fastlock_save(struct bladerf *dev, bool is_tx,
                            uint8_t rffe_profile, uint16_t nios_profile)
{
    int status;
    uint8_t  addr;
    uint32_t data = 0;

    addr = is_tx ? 1 : 0;
    data = (rffe_profile << 16) | nios_profile;

    status = nios_8x32_write(dev, NIOS_PKT_8x32_TARGET_FASTLOCK, addr, data);

#ifdef ENABLE_LIBBLADERF_NIOS_ACCESS_LOG_VERBOSE
    if (status == 0) {
        log_verbose("%s: Wrote 0x%08x\n", __FUNCTION__, data);
    }
#endif

    return status;
}

int nios_ad56x1_vctcxo_trim_dac_read(struct bladerf *dev, uint16_t *value)
{
    int status;

    status = nios_8x16_read(dev, NIOS_PKT_8x16_TARGET_AD56X1_DAC, 0, value);
    if (status == 0) {
        log_verbose("%s: Read 0x%04x\n", __FUNCTION__, *value);
    }

    return status;
}

int nios_ad56x1_vctcxo_trim_dac_write(struct bladerf *dev, uint16_t value)
{
    int status;

    status = nios_8x16_write(dev, NIOS_PKT_8x16_TARGET_AD56X1_DAC, 0, value);
    if (status == 0) {
        log_verbose("%s: Wrote 0x%04x\n", __FUNCTION__, value);
    }

    return status;
}

int nios_adf400x_read(struct bladerf *dev, uint8_t addr, uint32_t *data)
{
    int status;

    status = nios_8x32_read(dev, NIOS_PKT_8x32_TARGET_ADF400X, addr, data);
    if (status == 0) {
        log_verbose("%s: Read 0x%08x from addr 0x%02x\n", __FUNCTION__, *data, addr);
    }

    return status;
}

int nios_adf400x_write(struct bladerf *dev, uint8_t addr, uint32_t data)
{
    int status;

    data &= ~(0x3);

    status = nios_8x32_write(dev, NIOS_PKT_8x32_TARGET_ADF400X, 0, data | (addr & 0x3));
    if (status == 0) {
        log_verbose("%s: Wrote 0x%08x to addr 0x%02x\n", __FUNCTION__, data, addr);
    }

    return status;
}

int nios_vctcxo_trim_dac_write(struct bladerf *dev, uint8_t addr, uint16_t value)
{
    return nios_8x16_write(dev, NIOS_PKT_8x16_TARGET_VCTCXO_DAC, addr, value);
}

int nios_vctcxo_trim_dac_read(struct bladerf *dev, uint8_t addr, uint16_t *value)
{
    return nios_8x16_read(dev, NIOS_PKT_8x16_TARGET_VCTCXO_DAC, addr, value);
}

int nios_set_vctcxo_tamer_mode(struct bladerf *dev,
                               bladerf_vctcxo_tamer_mode mode)
{
    int status;

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


int nios_get_iq_gain_correction(struct bladerf *dev, bladerf_channel ch,
                                int16_t *value)
{
    int status = BLADERF_ERR_INVAL;
    uint16_t tmp = 0;

    switch (ch) {
        case BLADERF_CHANNEL_RX(0):
            status = nios_8x16_read(dev, NIOS_PKT_8x16_TARGET_IQ_CORR,
                                    NIOS_PKT_8x16_ADDR_IQ_CORR_RX_GAIN, &tmp);
            break;

        case BLADERF_CHANNEL_TX(0):
            status = nios_8x16_read(dev, NIOS_PKT_8x16_TARGET_IQ_CORR,
                                    NIOS_PKT_8x16_ADDR_IQ_CORR_TX_GAIN, &tmp);
            break;

        default:
            log_debug("Invalid channel: 0x%x\n", ch);
    }

    *value = (int16_t) tmp;

    if (status == 0) {
        log_verbose("%s: Read %s %d\n",
                    __FUNCTION__, channel2str(ch), *value);
    }

    return status;
}

int nios_get_iq_phase_correction(struct bladerf *dev, bladerf_channel ch,
                                 int16_t *value)
{
    int status = BLADERF_ERR_INVAL;
    uint16_t tmp = 0;

    switch (ch) {
        case BLADERF_CHANNEL_RX(0):
            status = nios_8x16_read(dev, NIOS_PKT_8x16_TARGET_IQ_CORR,
                                    NIOS_PKT_8x16_ADDR_IQ_CORR_RX_PHASE, &tmp);
            break;

        case BLADERF_CHANNEL_TX(0):
            status = nios_8x16_read(dev, NIOS_PKT_8x16_TARGET_IQ_CORR,
                                    NIOS_PKT_8x16_ADDR_IQ_CORR_TX_PHASE, &tmp);
            break;

        default:
            log_debug("Invalid channel: 0x%x\n", ch);
    }

    *value = (int16_t) tmp;

    if (status == 0) {
        log_verbose("%s: Read %s %d\n",
                    __FUNCTION__, channel2str(ch), *value);
    }

    return status;
}

int nios_set_iq_gain_correction(struct bladerf *dev, bladerf_channel ch,
                                int16_t value)
{
    int status = BLADERF_ERR_INVAL;

    switch (ch) {
        case BLADERF_CHANNEL_RX(0):
            log_verbose("Setting RX IQ Correction gain: %d\n", value);
            status = nios_8x16_write(dev, NIOS_PKT_8x16_TARGET_IQ_CORR,
                                     NIOS_PKT_8x16_ADDR_IQ_CORR_RX_GAIN, value);
            break;

        case BLADERF_CHANNEL_TX(0):
            log_verbose("Setting TX IQ Correction gain: %d\n", value);
            status = nios_8x16_write(dev, NIOS_PKT_8x16_TARGET_IQ_CORR,
                                     NIOS_PKT_8x16_ADDR_IQ_CORR_TX_GAIN, value);
            break;

        default:
            log_debug("Invalid channel: 0x%x\n", ch);
    }

    if (status == 0) {
        log_verbose("%s: Wrote %s %d\n",
                    __FUNCTION__, channel2str(ch), value);
    }

    return status;
}

int nios_set_iq_phase_correction(struct bladerf *dev, bladerf_channel ch,
                                 int16_t value)
{
    int status = BLADERF_ERR_INVAL;

    switch (ch) {
        case BLADERF_CHANNEL_RX(0):
            log_verbose("Setting RX IQ Correction phase: %d\n", value);
            status = nios_8x16_write(dev, NIOS_PKT_8x16_TARGET_IQ_CORR,
                                     NIOS_PKT_8x16_ADDR_IQ_CORR_RX_PHASE, value);
            break;

        case BLADERF_CHANNEL_TX(0):
            log_verbose("Setting TX IQ Correction phase: %d\n", value);
            status = nios_8x16_write(dev, NIOS_PKT_8x16_TARGET_IQ_CORR,
                                     NIOS_PKT_8x16_ADDR_IQ_CORR_TX_PHASE, value);
            break;

        default:
            log_debug("Invalid channel: 0x%x\n", ch);
    }

    if (status == 0) {
        log_verbose("%s: Wrote %s %d\n",
                    __FUNCTION__, channel2str(ch), value);
    }


    return status;
}

int nios_set_agc_dc_correction(struct bladerf *dev, int16_t q_max, int16_t i_max,
                               int16_t q_mid, int16_t i_mid,
                               int16_t q_low, int16_t i_low)
{
    int status;

    status = nios_8x16_write(dev, NIOS_PKT_8x16_TARGET_AGC_CORR, NIOS_PKT_8x16_ADDR_AGC_DC_Q_MAX, q_max);

    if (!status)
        status = nios_8x16_write(dev, NIOS_PKT_8x16_TARGET_AGC_CORR, NIOS_PKT_8x16_ADDR_AGC_DC_I_MAX, i_max);
    if (!status)
        status = nios_8x16_write(dev, NIOS_PKT_8x16_TARGET_AGC_CORR, NIOS_PKT_8x16_ADDR_AGC_DC_Q_MID, q_mid);
    if (!status)
        status = nios_8x16_write(dev, NIOS_PKT_8x16_TARGET_AGC_CORR, NIOS_PKT_8x16_ADDR_AGC_DC_I_MID, i_mid);
    if (!status)
        status = nios_8x16_write(dev, NIOS_PKT_8x16_TARGET_AGC_CORR, NIOS_PKT_8x16_ADDR_AGC_DC_Q_MIN, q_low);
    if (!status)
        status = nios_8x16_write(dev, NIOS_PKT_8x16_TARGET_AGC_CORR, NIOS_PKT_8x16_ADDR_AGC_DC_I_MIN, i_low);

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

int nios_retune(struct bladerf *dev, bladerf_channel ch,
                uint64_t timestamp, uint16_t nint, uint32_t nfrac,
                uint8_t freqsel, uint8_t vcocap, bool low_band,
                uint8_t xb_gpio, bool quick_tune)
{
    int status;
    uint8_t buf[NIOS_PKT_LEN];

    uint8_t resp_flags;
    uint64_t duration;

    if (timestamp == NIOS_PKT_RETUNE_CLEAR_QUEUE) {
        log_verbose("Clearing %s retune queue.\n", channel2str(ch));
    } else {
        log_verbose("%s: channel=%s timestamp=%"PRIu64" nint=%u nfrac=%u\n\t\t\t\t"
                    "freqsel=0x%02x vcocap=0x%02x low_band=%d quick_tune=%d\n",
                    __FUNCTION__, channel2str(ch), timestamp, nint, nfrac,
                    freqsel, vcocap, low_band, quick_tune);
    }

    nios_pkt_retune_pack(buf, ch, timestamp,
                         nint, nfrac, freqsel, vcocap, low_band,
                         xb_gpio, quick_tune);

    status = nios_access(dev, buf);
    if (status != 0) {
        return status;
    }

    nios_pkt_retune_resp_unpack(buf, &duration, &vcocap, &resp_flags);

    if (resp_flags & NIOS_PKT_RETUNERESP_FLAG_TSVTUNE_VALID) {
        log_verbose("%s retune operation: vcocap=%u, duration=%"PRIu64"\n",
                    channel2str(ch), vcocap, duration);
    } else {
        log_verbose("%s operation duration: %"PRIu64"\n",
                    channel2str(ch), duration);
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

int nios_retune2(struct bladerf *dev, bladerf_channel ch,
                 uint64_t timestamp, uint16_t nios_profile,
                 uint8_t rffe_profile, uint8_t port,
                 uint8_t spdt)
{
    int status;
    uint8_t buf[NIOS_PKT_LEN];

    uint8_t resp_flags;
    uint64_t duration;

    if (timestamp == NIOS_PKT_RETUNE2_CLEAR_QUEUE) {
        log_verbose("Clearing %s retune queue.\n", channel2str(ch));
    } else {
        log_verbose("%s: channel=%s timestamp=%"PRIu64" nios_profile=%u "
                    "rffe_profile=%u\n\t\t\t\tport=0x%02x spdt=0x%02x\n",
                    __FUNCTION__, channel2str(ch), timestamp, nios_profile,
                    rffe_profile, port, spdt);
    }

    nios_pkt_retune2_pack(buf, ch, timestamp, nios_profile, rffe_profile,
                          port, spdt);

    status = nios_access(dev, buf);
    if (status != 0) {
        return status;
    }

    nios_pkt_retune2_resp_unpack(buf, &duration, &resp_flags);

    if (resp_flags & NIOS_PKT_RETUNE2_RESP_FLAG_TSVTUNE_VALID) {
        log_verbose("%s retune operation: duration=%"PRIu64"\n",
                    channel2str(ch), duration);
    } else {
        log_verbose("%s operation duration: %"PRIu64"\n",
                    channel2str(ch), duration);
    }

    if ((resp_flags & NIOS_PKT_RETUNE2_RESP_FLAG_SUCCESS) == 0) {
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

int nios_read_trigger(struct bladerf *dev, bladerf_channel ch,
                      bladerf_trigger_signal trigger, uint8_t *value)
{
    int status;
    uint8_t nios_id;

    switch (ch) {
        case BLADERF_CHANNEL_TX(0):
            nios_id = NIOS_PKT_8x8_TX_TRIGGER_CTL;
            break;

        case BLADERF_CHANNEL_RX(0):
            nios_id = NIOS_PKT_8x8_RX_TRIGGER_CTL;
            break;

        default:
            log_debug("Invalid channel: 0x%x\n", ch);
            return BLADERF_ERR_INVAL;
    }

    /* Only 1 external trigger is currently supported */
    switch (trigger) {
        case BLADERF_TRIGGER_J71_4:
        case BLADERF_TRIGGER_J51_1:
        case BLADERF_TRIGGER_MINI_EXP_1:
            break;

        default:
            log_debug("Invalid trigger: %d\n", trigger);
            return BLADERF_ERR_INVAL;
    }

    status = nios_8x8_read(dev, nios_id, 0, value);
    if (status == 0) {
        log_verbose("%s trigger read value 0x%02x\n", channel2str(ch), *value);
    }

    return status;
}

int nios_write_trigger(struct bladerf *dev, bladerf_channel ch,
                       bladerf_trigger_signal trigger, uint8_t value)
{
    int status;
    uint8_t nios_id;

    switch (ch) {
        case BLADERF_CHANNEL_TX(0):
            nios_id = NIOS_PKT_8x8_TX_TRIGGER_CTL;
            break;

        case BLADERF_CHANNEL_RX(0):
            nios_id = NIOS_PKT_8x8_RX_TRIGGER_CTL;
            break;

        default:
            log_debug("Invalid channel: 0x%x\n", ch);
            return BLADERF_ERR_INVAL;
    }

    /* Only 1 external trigger is currently supported */
    switch (trigger) {
        case BLADERF_TRIGGER_J71_4:
        case BLADERF_TRIGGER_J51_1:
        case BLADERF_TRIGGER_MINI_EXP_1:
            break;

        default:
            log_debug("Invalid trigger: %d\n", trigger);
            return BLADERF_ERR_INVAL;
    }

    status = nios_8x8_write(dev, nios_id, 0, value);
    if (status == 0) {
        log_verbose("%s trigger write value 0x%02x\n", channel2str(ch), value);
    }

    return status;
}
