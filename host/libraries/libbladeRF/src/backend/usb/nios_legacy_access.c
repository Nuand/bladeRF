/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014-2015 Nuand LLC
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

/* This file implements functionality used to communicate with the NIOS II
 * soft-core processor running on the FPGA versions prior to v0.2.x.
 * (Although post v0.2.x FPGAs support this for reverse compatibility).
 *
 * The host communicates with the NIOS II via USB transfers to the Cypress FX3,
 * which then communicates with the FPGA via a UART.
 *
 * See the files in the top-level fpga_common/ directory for description
 * and macros pertaining to the legacy packet format.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "bladerf_priv.h"
#include "capabilities.h"
#include "usb.h"
#include "nios_legacy_access.h"
#include "nios_pkt_formats.h"
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

/* Access device/module via the legacy NIOS II packet format. */
static int nios_access(struct bladerf *dev, uint8_t peripheral,
                       usb_direction dir, struct uart_cmd *cmd,
                       size_t len)
{
    void *driver;
    struct bladerf_usb *usb = usb_backend(dev, &driver);

    int status;
    size_t i;
    uint8_t buf[16] = { 0 };
    const uint8_t pkt_mode_dir = (dir == USB_DIR_HOST_TO_DEVICE) ?
                                 NIOS_PKT_LEGACY_MODE_DIR_WRITE :
                                 NIOS_PKT_LEGACY_MODE_DIR_READ;

    assert(len <= ((sizeof(buf) - 2) / 2));

    /* Populate the buffer for transfer, given address data pairs */
    buf[0] = NIOS_PKT_LEGACY_MAGIC;
    buf[1] = pkt_mode_dir | peripheral | (uint8_t)len;

    for (i = 0; i < len; i++) {
        buf[i * 2 + 2] = cmd[i].addr;
        buf[i * 2 + 3] = cmd[i].data;
    }

    print_buf("NIOS II access request:\n", buf, 16);

    /* Send the command */
    status = usb->fn->bulk_transfer(driver, PERIPHERAL_EP_OUT,
                                     buf, sizeof(buf),
                                     PERIPHERAL_TIMEOUT_MS);
    if (status != 0) {
        log_debug("Failed to submit NIOS II request: %s\n",
                  bladerf_strerror(status));
        return status;
    }

    /* Read back the ACK. The command data is only used for a read operation,
     * and is thrown away otherwise */
    status = usb->fn->bulk_transfer(driver, PERIPHERAL_EP_IN,
                                    buf, sizeof(buf),
                                    PERIPHERAL_TIMEOUT_MS);

    if (dir == NIOS_PKT_LEGACY_MODE_DIR_READ && status == 0) {
        for (i = 0; i < len; i++) {
            cmd[i].data = buf[i * 2 + 3];
        }
    }

    if (status == 0) {
        print_buf("NIOS II access response:\n", buf, 16);
    } else {
        log_debug("Failed to receive NIOS II response: %s\n",
                  bladerf_strerror(status));
    }


    return status;
}

int nios_legacy_pio_read(struct bladerf *dev, uint8_t addr, uint32_t *data)
{
    int status;
    size_t i;
    struct uart_cmd cmd;

    *data = 0;

    for (i = 0; i < sizeof(*data); i++) {
        assert((addr + i) <= UINT8_MAX);
        cmd.addr = (uint8_t)(addr + i);
        cmd.data = 0xff;

        status = nios_access(dev, NIOS_PKT_LEGACY_DEV_CONFIG,
                             USB_DIR_DEVICE_TO_HOST, &cmd, 1);

        if (status < 0) {
            *data = 0;
            return status;
        }

        *data |= (cmd.data << (i * 8));
    }

    return 0;
}

int nios_legacy_pio_write(struct bladerf *dev, uint8_t addr, uint32_t data)
{
    int status;
    size_t i;
    struct uart_cmd cmd;

    for (i = 0; i < sizeof(data); i++) {
        assert((addr + i) <= UINT8_MAX);
        cmd.addr = (uint8_t)(addr + i);
        cmd.data = (data >> (i * 8)) & 0xff;

        status = nios_access(dev, NIOS_PKT_LEGACY_DEV_CONFIG,
                             USB_DIR_HOST_TO_DEVICE, &cmd, 1);

        if (status < 0) {
            return status;
        }
    }

    return 0;
}

int nios_legacy_config_read(struct bladerf *dev, uint32_t *val)
{
    int status;

    status = nios_legacy_pio_read(dev, NIOS_PKT_LEGACY_PIO_ADDR_CONTROL, val);
    if (status == 0) {
        log_verbose("%s: 0x%08x\n", __FUNCTION__, val);
    }

    return status;
}

int nios_legacy_config_write(struct bladerf *dev, uint32_t val)
{
    log_verbose("%s: Writing 0x%08x\n", __FUNCTION__, val);
    return nios_legacy_pio_write(dev, NIOS_PKT_LEGACY_PIO_ADDR_CONTROL, val);
}

int nios_legacy_get_fpga_version(struct bladerf *dev,
                                 struct bladerf_version *ver)
{
    int i, status;
    struct uart_cmd cmd;

    for (i = 0; i < 4; i++) {
        cmd.addr = NIOS_PKT_LEGACY_DEV_FPGA_VERSION_ID + i;
        cmd.data = 0xff;

        status = nios_access(dev, NIOS_PKT_LEGACY_DEV_CONFIG,
                             USB_DIR_DEVICE_TO_HOST, &cmd, 1);

        if (status != 0) {
            memset(&dev->fpga_version, 0, sizeof(dev->fpga_version));
            log_debug("Failed to read FPGA version[%d]: %s\n", i,
                      bladerf_strerror(status));

            return status;
        }

        switch (i) {
            case 0:
                ver->major = cmd.data;
                break;
            case 1:
                ver->minor = cmd.data;
                break;
            case 2:
                ver->patch = cmd.data;
                break;
            case 3:
                ver->patch |= (cmd.data << 8);
                break;
            default:
                assert(!"Bug");
        }
    }

    snprintf((char*)ver->describe, BLADERF_VERSION_STR_MAX,
             "%d.%d.%d", ver->major, ver->minor, ver->patch);

    return status;
}

int nios_legacy_get_timestamp(struct bladerf *dev, bladerf_module mod,
                              uint64_t *value)
{
    int status = 0;
    struct uart_cmd cmds[4];
    uint8_t timestamp_bytes[8];
    size_t i;

    /* Offset 16 is the time tamer according to the Nios firmware */
    cmds[0].addr = (mod == BLADERF_MODULE_RX ? 16 : 24);
    cmds[1].addr = (mod == BLADERF_MODULE_RX ? 17 : 25);
    cmds[2].addr = (mod == BLADERF_MODULE_RX ? 18 : 26);
    cmds[3].addr = (mod == BLADERF_MODULE_RX ? 19 : 27);
    cmds[0].data = cmds[1].data = cmds[2].data = cmds[3].data = 0xff;

    status = nios_access(dev,
                         NIOS_PKT_LEGACY_DEV_CONFIG,
                         USB_DIR_DEVICE_TO_HOST,
                         cmds, ARRAY_SIZE(cmds));
    if (status != 0) {
        return status;
    } else {
        for (i = 0; i < 4; i++) {
            timestamp_bytes[i] = cmds[i].data;
        }
    }

    cmds[0].addr = (mod == BLADERF_MODULE_RX ? 20 : 28);
    cmds[1].addr = (mod == BLADERF_MODULE_RX ? 21 : 29);
    cmds[2].addr = (mod == BLADERF_MODULE_RX ? 22 : 30);
    cmds[3].addr = (mod == BLADERF_MODULE_RX ? 23 : 31);
    cmds[0].data = cmds[1].data = cmds[2].data = cmds[3].data = 0xff;

    status = nios_access(dev,
                         NIOS_PKT_LEGACY_DEV_CONFIG,
                         USB_DIR_DEVICE_TO_HOST,
                         cmds, ARRAY_SIZE(cmds));

    if (status) {
        return status;
    } else {
        for (i = 0; i < 4; i++) {
            timestamp_bytes[i + 4] = cmds[i].data;
        }
    }

    memcpy(value, timestamp_bytes, sizeof(*value));
    *value = LE64_TO_HOST(*value);

    return 0;
}

int nios_legacy_si5338_read(struct bladerf *dev, uint8_t addr, uint8_t *data)
{
    int status;
    struct uart_cmd cmd;

    cmd.addr = addr;
    cmd.data = 0xff;

    status = nios_access(dev, NIOS_PKT_LEGACY_DEV_SI5338,
                         USB_DIR_DEVICE_TO_HOST, &cmd, 1);

    if (status == 0) {
        *data = cmd.data;
        log_verbose("%s: 0x%2.2x 0x%2.2x\n", __FUNCTION__, addr, *data);
    }

    return status;
}

int nios_legacy_si5338_write(struct bladerf *dev, uint8_t addr, uint8_t data)
{
    struct uart_cmd cmd;

    cmd.addr = addr;
    cmd.data = data;

    log_verbose( "%s: 0x%2.2x 0x%2.2x\n", __FUNCTION__, addr, data );

    return nios_access(dev, NIOS_PKT_LEGACY_DEV_SI5338,
                       USB_DIR_HOST_TO_DEVICE, &cmd, 1);
}

int nios_legacy_lms6_read(struct bladerf *dev, uint8_t addr, uint8_t *data)
{
    int status;
    struct uart_cmd cmd;

    cmd.addr = addr;
    cmd.data = 0xff;

    status = nios_access(dev, NIOS_PKT_LEGACY_DEV_LMS,
                         USB_DIR_DEVICE_TO_HOST, &cmd, 1);

    if (status == 0) {
        *data = cmd.data;
        log_verbose( "%s: 0x%2.2x 0x%2.2x\n", __FUNCTION__, addr, *data );
    }

    return status;
}

int nios_legacy_lms6_write(struct bladerf *dev, uint8_t addr, uint8_t data)
{
    int status;
    struct uart_cmd cmd;

    cmd.addr = addr;
    cmd.data = data;

    log_verbose( "%s: 0x%2.2x 0x%2.2x\n", __FUNCTION__, addr, data );

    status = nios_access(dev, NIOS_PKT_LEGACY_DEV_LMS,
                         USB_DIR_HOST_TO_DEVICE, &cmd, 1);

    return status;
}

int nios_legacy_vctcxo_trim_dac_write(struct bladerf *dev, uint16_t value)
{
    int status;
    struct uart_cmd cmd;
    int base;

    /* FPGA v0.0.4 introduced a change to the location of the DAC registers */
    const bool legacy_location = !have_cap(dev, BLADERF_CAP_UPDATED_DAC_ADDR);

    base = legacy_location ? 0 : 34;

    cmd.addr = base;
    cmd.data = value & 0xff;
    status = nios_access(dev,
                         legacy_location ?
                            NIOS_PKT_LEGACY_DEV_VCTCXO :
                            NIOS_PKT_LEGACY_DEV_CONFIG,
                         USB_DIR_HOST_TO_DEVICE, &cmd, 1);

    if (status < 0) {
        return status;
    }

    cmd.addr = base + 1;
    cmd.data = (value >> 8) & 0xff;
    status = nios_access(dev,
                         legacy_location ?
                            NIOS_PKT_LEGACY_DEV_VCTCXO :
                            NIOS_PKT_LEGACY_DEV_CONFIG,
                         USB_DIR_HOST_TO_DEVICE, &cmd, 1);

    return status;
}

static int get_iq_correction(struct bladerf *dev, uint8_t addr, int16_t *value)
{
    int i;
    int status;
    struct uart_cmd cmd;

    *value = 0;
    for (i = status = 0; status == 0 && i < 2; i++) {
        cmd.addr = i + addr;
        cmd.data = 0xff;
        status = nios_access(dev, NIOS_PKT_LEGACY_DEV_CONFIG,
                             USB_DIR_DEVICE_TO_HOST, &cmd, 1);

        *value |= (cmd.data << (i * 8));
    }

    return status;
}

static int set_iq_correction(struct bladerf *dev, uint8_t addr, int16_t value)
{
    int i;
    int status;
    struct uart_cmd cmd;

    for (i = status = 0; status == 0 && i < 2; i++) {
        cmd.addr = i + addr;

        cmd.data = (value >> (i * 8)) & 0xff;
        status = nios_access(dev, NIOS_PKT_LEGACY_DEV_CONFIG,
                             USB_DIR_HOST_TO_DEVICE, &cmd, 1);
    }

    return status;
}


int nios_legacy_get_iq_gain_correction(struct bladerf *dev,
                                       bladerf_module module, int16_t *value)
{
    int status;
    uint8_t addr;

    switch (module) {
        case BLADERF_MODULE_RX:
            addr = NIOS_PKT_LEGACY_DEV_RX_GAIN_ADDR;
            break;

        case BLADERF_MODULE_TX:
            addr = NIOS_PKT_LEGACY_DEV_TX_GAIN_ADDR;
            break;

        default:
            log_debug("%s: invalid module provided (%d)\n",
                      __FUNCTION__, module);

            return BLADERF_ERR_INVAL;
    }

    status = get_iq_correction(dev, addr, value);

    return status;
}

int nios_legacy_get_iq_phase_correction(struct bladerf *dev,
                                        bladerf_module module, int16_t *value)
{
    uint8_t addr;

    switch (module) {
        case BLADERF_MODULE_RX:
            addr = NIOS_PKT_LEGACY_DEV_RX_PHASE_ADDR;
            break;

        case BLADERF_MODULE_TX:
            addr = NIOS_PKT_LEGACY_DEV_TX_PHASE_ADDR;
            break;

        default:
            log_debug("%s: invalid module provided (%d)\n",
                      __FUNCTION__, module);

            return BLADERF_ERR_INVAL;
    }

    return get_iq_correction(dev, addr, value);
}

int nios_legacy_set_iq_gain_correction(struct bladerf *dev,
                                       bladerf_module module, int16_t value)
{
    uint8_t addr;

    switch (module) {
        case BLADERF_MODULE_RX:
            addr = NIOS_PKT_LEGACY_DEV_RX_GAIN_ADDR;
            log_verbose("Setting RX IQ Correction phase: %d\n", value);
            break;

        case BLADERF_MODULE_TX:
            addr = NIOS_PKT_LEGACY_DEV_TX_GAIN_ADDR;
            log_verbose("Setting TX IQ Correction phase: %d\n", value);
            break;

        default:
            log_debug("%s: invalid module provided (%d)\n",
                      __FUNCTION__, module);

            return BLADERF_ERR_INVAL;
    }

    return set_iq_correction(dev, addr, value);
}

int nios_legacy_set_iq_phase_correction(struct bladerf *dev,
                                        bladerf_module module, int16_t value)
{
    uint8_t addr;

    switch (module) {
        case BLADERF_MODULE_RX:
            log_verbose("Setting RX IQ Correction phase: %d\n", value);
            addr = NIOS_PKT_LEGACY_DEV_RX_PHASE_ADDR;
            break;

        case BLADERF_MODULE_TX:
            log_verbose("Setting TX IQ Correction phase: %d\n", value);
            addr = NIOS_PKT_LEGACY_DEV_TX_PHASE_ADDR;
            break;

        default:
            log_debug("%s: invalid module provided (%d)\n",
                      __FUNCTION__, module);

            return BLADERF_ERR_INVAL;
    }

    return set_iq_correction(dev, addr, value);
}

int nios_legacy_xb200_synth_write(struct bladerf *dev, uint32_t value)
{
    log_verbose("%s: 0x%08x\n", __FUNCTION__, value);
    return nios_legacy_pio_write(dev,
                                 NIOS_PKT_LEGACY_PIO_ADDR_XB200_SYNTH, value);
}

int nios_legacy_expansion_gpio_read(struct bladerf *dev, uint32_t *val)
{
    int status = nios_legacy_pio_read(dev, NIOS_PKT_LEGACY_PIO_ADDR_EXP, val);

    if (status == 0) {
        log_verbose("%s: 0x%08x\n", __FUNCTION__, val);
    }

    return status;
}

int nios_legacy_expansion_gpio_write(struct bladerf *dev, uint32_t val)
{
    log_verbose("%s: 0x%08x\n", val);
    return nios_legacy_pio_write(dev, NIOS_PKT_LEGACY_PIO_ADDR_EXP, val);
}

int nios_legacy_expansion_gpio_dir_read(struct bladerf *dev, uint32_t *val)
{
    int status = nios_legacy_pio_read(dev,
                                      NIOS_PKT_LEGACY_PIO_ADDR_EXP_DIR, val);

    if (status == 0) {
        log_verbose("%s: 0x%08x\n", __FUNCTION__, val);
    }

    return status;
}

int nios_legacy_expansion_gpio_dir_write(struct bladerf *dev, uint32_t val)
{
    log_verbose("%s: 0x%08\n", __FUNCTION__, val);
    return nios_legacy_pio_write(dev, NIOS_PKT_LEGACY_PIO_ADDR_EXP_DIR, val);
}
