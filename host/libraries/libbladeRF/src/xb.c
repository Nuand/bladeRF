/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014 Nuand LLC
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
#include "log.h"

#include "libbladeRF.h"     /* Public API */
#include "bladerf_priv.h"   /* Implementation-specific items ("private") */

#define BLADERF_CONFIG_RX_SWAP_IQ 0x20000000
#define BLADERF_CONFIG_TX_SWAP_IQ 0x10000000

#define BLADERF_XB_CONFIG_TX_BYPASS       0x04
#define BLADERF_XB_CONFIG_TX_BYPASS_N     0x08
#define BLADERF_XB_CONFIG_TX_BYPASS_MASK  0x0C
#define BLADERF_XB_CONFIG_RX_BYPASS       0x10
#define BLADERF_XB_CONFIG_RX_BYPASS_N     0x20
#define BLADERF_XB_CONFIG_RX_BYPASS_MASK  0x30

#define BLADERF_XB_RF_ON     0x0800
#define BLADERF_XB_TX_ENABLE 0x1000
#define BLADERF_XB_RX_ENABLE 0x2000

#define BLADERF_XB_TX_RF_SW2 0x04000000
#define BLADERF_XB_TX_RF_SW1 0x08000000
#define BLADERF_XB_TX_MASK   0x0C000000
#define BLADERF_XB_TX_SHIFT  26

#define BLADERF_XB_RX_RF_SW2 0x10000000
#define BLADERF_XB_RX_RF_SW1 0x20000000
#define BLADERF_XB_RX_MASK   0x30000000
#define BLADERF_XB_RX_SHIFT  28

static int xb200_attach(struct bladerf *dev) {
    int status = 0;
    uint32_t val;
    unsigned int muxout = 6;
    const char *mux_lut[] = {
        "THREE-STATE OUTPUT",
        "DVdd",
        "DGND",
        "R COUNTER OUTPUT",
        "N DIVIDER OUTPUT",
        "ANALOG LOCK DETECT",
        "DIGITAL LOCK DETECT",
        "RESERVED"
    };


    log_debug("  Attaching transverter board\n");
    if ((status = bladerf_si5338_write(dev, 39, 2)))
        return status;
    if ((status = bladerf_si5338_write(dev, 34, 0x22)))
        return status;
    if ((status = bladerf_config_gpio_read(dev, &val)))
        return status;
    val |= 0x80000000;
    if ((status = bladerf_config_gpio_write(dev, val)))
        return status;
    if ((status = bladerf_expansion_gpio_read(dev, &val)))
        return status;

    if ((status = bladerf_expansion_gpio_dir_write(dev, 0x3C003836)))
        return status;

    if ((status = bladerf_expansion_gpio_write(dev, 0x800)))
        return status;

    // Load ADF4351 registers via SPI
    // Refer to ADF4351 reference manual for register set
    // The LO is set to a Int-N 1248MHz +3dBm tone
    // Registers are written in order from 5 downto 0
    if ((status = bladerf_xb_spi_write(dev, 0x580005)))
        return status;
    if ((status = bladerf_xb_spi_write(dev, 0x99A16C)))
        return status;
    if ((status = bladerf_xb_spi_write(dev, 0xC004B3)))
        return status;
    log_debug("  MUXOUT: %s\n", mux_lut[muxout]);

    if ((status = bladerf_xb_spi_write(dev, 0x60008E42 | (1<<8) | (muxout << 26))))
        return status;
    if ((status = bladerf_xb_spi_write(dev, 0x08008011)))
        return status;
    if ((status = bladerf_xb_spi_write(dev, 0x00410000)))
        return status;

    status = bladerf_expansion_gpio_read(dev, &val);
    if (!status && (val & 0x1))
        log_debug("  MUXOUT Bit set: OK\n");
    else {
        log_debug("  MUXOUT Bit not set: FAIL\n");
    }
    status = bladerf_expansion_gpio_write(dev, 0x3C000800);

    return status;
}


int bladerf_expansion_attach(struct bladerf *dev, bladerf_xb xb) {
    bladerf_xb attached;
    int status;
    status = bladerf_expansion_get_attached(dev, &attached);
    if (status)
        return status;

    if (xb == BLADERF_XB_200) {
        if (version_less_than(&dev->fpga_version, 0, 0, 5)) {
            log_warning("%s: XB200 support requires FPGA v0.0.5 or later\n", __FUNCTION__);
            return BLADERF_ERR_UNSUPPORTED;
        }

        if (attached != xb) {
            return xb200_attach(dev);
        }
    }
    return 0;
}

int bladerf_expansion_get_attached(struct bladerf *dev, bladerf_xb *xb) {
    int status;
    uint32_t val;

    status = bladerf_config_gpio_read(dev, &val);
    if (status)
        return status;

    *xb = (val >> 30) & 0x3;
    return 0;
}

int bladerf_xb200_enable(struct bladerf *dev, bool enable) {
    int status;
    uint32_t val, orig;

    status = bladerf_expansion_gpio_read(dev, &orig);
    if (status)
        return status;

    val = orig;
    if (enable)
        val |= BLADERF_XB_RF_ON;
    else
        val &= ~BLADERF_XB_RF_ON;

    if (status || (val == orig))
        return status;

    return bladerf_expansion_gpio_write(dev, val);
}

int bladerf_xb200_get_filterbank(struct bladerf *dev, bladerf_module module, bladerf_xb200_filter *filter) {
    int status;
    uint32_t val;

    status = bladerf_expansion_gpio_read(dev, &val);
    if (status)
        return status;

    *filter = (val >> ((module == BLADERF_MODULE_RX) ? BLADERF_XB_RX_SHIFT : BLADERF_XB_TX_SHIFT)) & 3;
    return 0;
}

int bladerf_xb200_set_filterbank(struct bladerf *dev, bladerf_module module, bladerf_xb200_filter filter) {
    int status;
    uint32_t orig, val, bits;

    bits = ((module == BLADERF_MODULE_RX) ? BLADERF_XB_RX_MASK : BLADERF_XB_TX_MASK);

    status = bladerf_expansion_gpio_read(dev, &orig);
    val = orig & ~bits;
    val |= filter << ((module == BLADERF_MODULE_RX) ? BLADERF_XB_RX_SHIFT : BLADERF_XB_TX_SHIFT);
    if (status || (orig == val))
        return status;

    return bladerf_expansion_gpio_write(dev, val);
}

int bladerf_xb200_set_path(struct bladerf *dev, bladerf_module module, bladerf_xb200_path path) {
    int status;
    uint32_t val;
    bool enable;

    uint8_t lval, lorig = 0;

    enable = (path == BLADERF_XB200_MIX);

    status = bladerf_lms_read( dev, 0x5A, &lorig );
    if (status)
        return status;
    lval = lorig;

#define LMS_RX_SWAP 0x40
#define LMS_TX_SWAP 0x08

    if (enable)
        lval |= (module == BLADERF_MODULE_RX) ? LMS_RX_SWAP : LMS_TX_SWAP;
    else
        lval &= ~((module == BLADERF_MODULE_RX) ? LMS_RX_SWAP : LMS_TX_SWAP);

    if (status || (lorig == lval))
        return status;

    status = bladerf_lms_write(dev, 0x5A, lval);
    if (status)
        return status;

    status = bladerf_expansion_gpio_read(dev, &val);
    if (status)
        return status;

    if (enable) {
        status = bladerf_expansion_gpio_read(dev, &val);
        if (status)
            return status;
        if (!(val & BLADERF_XB_RF_ON)) {
            status = xb200_attach(dev);
            if (status)
                return status;
        }
        val |= BLADERF_XB_RF_ON;
    }
    else
        val &= ~BLADERF_XB_RF_ON;

    val &= ~((module == BLADERF_MODULE_RX) ? (BLADERF_XB_CONFIG_RX_BYPASS_MASK | BLADERF_XB_RX_ENABLE) : (BLADERF_XB_CONFIG_TX_BYPASS_MASK | BLADERF_XB_TX_ENABLE));
    if (module == BLADERF_MODULE_RX)
        val |= enable ? (BLADERF_XB_RX_ENABLE | BLADERF_XB_CONFIG_RX_BYPASS) : BLADERF_XB_CONFIG_RX_BYPASS_N;
    else
        val |= enable ? (BLADERF_XB_TX_ENABLE | BLADERF_XB_CONFIG_TX_BYPASS) : BLADERF_XB_CONFIG_TX_BYPASS_N;

    return bladerf_expansion_gpio_write(dev, val);
}

int bladerf_xb200_get_path(struct bladerf *dev, bladerf_module module, bladerf_xb200_path *path) {
    int status;
    uint32_t val;

    status = bladerf_config_gpio_read(dev, &val);
    if (status)
        return status;
    if (module == BLADERF_MODULE_RX)
        *path = (val & BLADERF_XB_CONFIG_RX_BYPASS) ? BLADERF_XB200_BYPASS : BLADERF_XB200_MIX;
    else if (module == BLADERF_MODULE_TX)
        *path = (val & BLADERF_XB_CONFIG_TX_BYPASS) ? BLADERF_XB200_BYPASS : BLADERF_XB200_MIX;

    return 0;
}

