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

#include "xb200.h"

#include "driver/si5338.h"
#include "lms.h"
#include "rel_assert.h"
#include "log.h"

#define BLADERF_XB_CONFIG_TX_PATH_MIX     0x04
#define BLADERF_XB_CONFIG_TX_PATH_BYPASS  0x08
#define BLADERF_XB_CONFIG_TX_BYPASS       0x04
#define BLADERF_XB_CONFIG_TX_BYPASS_N     0x08
#define BLADERF_XB_CONFIG_TX_BYPASS_MASK  0x0C
#define BLADERF_XB_CONFIG_RX_PATH_MIX     0x10
#define BLADERF_XB_CONFIG_RX_PATH_BYPASS  0x20
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

struct xb200_xb_data {
    /* Track filterbank selection for RX and TX auto-selection */
    bladerf_xb200_filter auto_filter[2];
};

int xb200_attach(struct bladerf *dev)
{
    struct xb200_xb_data *xb_data;
    int status = 0;
    uint32_t val;
    uint8_t val8;
    unsigned int muxout   = 6;
    const char *mux_lut[] = { "THREE-STATE OUTPUT",
                              "DVdd",
                              "DGND",
                              "R COUNTER OUTPUT",
                              "N DIVIDER OUTPUT",
                              "ANALOG LOCK DETECT",
                              "DIGITAL LOCK DETECT",
                              "RESERVED" };

    xb_data = calloc(1, sizeof(struct xb200_xb_data));
    if (xb_data == NULL) {
        return BLADERF_ERR_MEM;
    }

    xb_data->auto_filter[BLADERF_CHANNEL_RX(0)] = -1;
    xb_data->auto_filter[BLADERF_CHANNEL_TX(0)] = -1;

    dev->xb_data = xb_data;

    log_debug("  Attaching transverter board\n");
    status = dev->backend->si5338_read(dev, 39, &val8);
    if (status < 0) {
        goto error;
    }
    val8 |= 2;
    if ((status = dev->backend->si5338_write(dev, 39, val8))) {
        goto error;
    }
    if ((status = dev->backend->si5338_write(dev, 34, 0x22))) {
        goto error;
    }
    if ((status = dev->backend->config_gpio_read(dev, &val))) {
        goto error;
    }
    val |= 0x80000000;
    if ((status = dev->backend->config_gpio_write(dev, val))) {
        goto error;
    }
    if ((status = dev->backend->expansion_gpio_read(dev, &val))) {
        goto error;
    }

    if ((status = dev->backend->expansion_gpio_dir_write(dev, 0xffffffff,
                                                         0x3C00383E))) {
        goto error;
    }

    if ((status = dev->backend->expansion_gpio_write(dev, 0xffffffff, 0x800))) {
        goto error;
    }

    // Load ADF4351 registers via SPI
    // Refer to ADF4351 reference manual for register set
    // The LO is set to a Int-N 1248MHz +3dBm tone
    // Registers are written in order from 5 downto 0
    if ((status = dev->backend->xb_spi(dev, 0x580005))) {
        goto error;
    }
    if ((status = dev->backend->xb_spi(dev, 0x99A16C))) {
        goto error;
    }
    if ((status = dev->backend->xb_spi(dev, 0xC004B3))) {
        goto error;
    }
    log_debug("  MUXOUT: %s\n", mux_lut[muxout]);

    if ((status = dev->backend->xb_spi(dev, 0x60008E42 | (1 << 8) |
                                                (muxout << 26)))) {
        goto error;
    }
    if ((status = dev->backend->xb_spi(dev, 0x08008011))) {
        goto error;
    }
    if ((status = dev->backend->xb_spi(dev, 0x00410000))) {
        goto error;
    }

    status = dev->backend->expansion_gpio_read(dev, &val);
    if (!status && (val & 0x1))
        log_debug("  MUXOUT Bit set: OK\n");
    else {
        log_debug("  MUXOUT Bit not set: FAIL\n");
    }
    if ((status =
             dev->backend->expansion_gpio_write(dev, 0xffffffff, 0x3C000800))) {
        goto error;
    }

    return 0;

error:
    free(dev->xb_data);
    dev->xb_data = NULL;
    return status;
}

void xb200_detach(struct bladerf *dev)
{
    if (dev->xb_data) {
        free(dev->xb_data);
        dev->xb_data = NULL;
    }
}

int xb200_enable(struct bladerf *dev, bool enable)
{
    int status;
    uint32_t val, orig;

    status = dev->backend->expansion_gpio_read(dev, &orig);
    if (status)
        return status;

    val = orig;
    if (enable)
        val |= BLADERF_XB_RF_ON;
    else
        val &= ~BLADERF_XB_RF_ON;

    if (status || (val == orig))
        return status;

    return dev->backend->expansion_gpio_write(dev, 0xffffffff, val);
}

int xb200_init(struct bladerf *dev)
{
    int status;

    log_verbose( "Setting RX path\n" );
    status = xb200_set_path(dev, BLADERF_CHANNEL_RX(0), BLADERF_XB200_BYPASS);
    if (status != 0) {
        return status;
    }

    log_verbose( "Setting TX path\n" );
    status = xb200_set_path(dev, BLADERF_CHANNEL_TX(0), BLADERF_XB200_BYPASS);
    if (status != 0) {
        return status;
    }

    log_verbose( "Setting RX filter\n" );
    status = xb200_set_filterbank(dev, BLADERF_CHANNEL_RX(0), BLADERF_XB200_AUTO_1DB);
    if (status != 0) {
        return status;
    }

    log_verbose( "Setting TX filter\n" );
    status = xb200_set_filterbank(dev, BLADERF_CHANNEL_TX(0), BLADERF_XB200_AUTO_1DB);
    if (status != 0) {
        return status;
    }

    return 0;
}

/**
 * Validate XB-200 filter selection
 *
 * @param[in]   f   Filter supplied by API user.
 *
 * @return 0 for a valid enumeration value, BLADERF_ERR_INVAL otherwise.
 */
static int check_xb200_filter(bladerf_xb200_filter f)
{
    int status;

    switch (f) {
        case BLADERF_XB200_50M:
        case BLADERF_XB200_144M:
        case BLADERF_XB200_222M:
        case BLADERF_XB200_CUSTOM:
        case BLADERF_XB200_AUTO_3DB:
        case BLADERF_XB200_AUTO_1DB:
            status = 0;
            break;

        default:
            log_debug("Invalid XB200 filter: %d\n", f);
            status = BLADERF_ERR_INVAL;
            break;
    }

    return status;
}

/**
 * Validate XB-200 path selection
 *
 * @param[in]   p   Path supplied by API user.
 *
 * @return 0 for a valid enumeration value, BLADERF_ERR_INVAL otherwise.
 */
static int check_xb200_path(bladerf_xb200_path p)
{
    int status;

    switch (p) {
        case BLADERF_XB200_BYPASS:
        case BLADERF_XB200_MIX:
            status = 0;
            break;

        default:
            status = BLADERF_ERR_INVAL;
            log_debug("Invalid XB200 path: %d\n", p);
            break;
    }

    return status;
}
int xb200_get_filterbank(struct bladerf *dev, bladerf_channel ch,
                         bladerf_xb200_filter *filter) {
    int status;
    uint32_t val;
    unsigned int shift;

    if (ch != BLADERF_CHANNEL_RX(0) && ch != BLADERF_CHANNEL_TX(0))
        return BLADERF_ERR_INVAL;

    status = dev->backend->expansion_gpio_read(dev, &val);
    if (status != 0) {
        return status;
    }

    if (ch == BLADERF_CHANNEL_RX(0)) {
        shift = BLADERF_XB_RX_SHIFT;
    } else {
        shift = BLADERF_XB_TX_SHIFT;
    }

    *filter = (val >> shift) & 3;

    status = check_xb200_filter(*filter);
    if (status != 0) {
        log_debug("Read back invalid GPIO state: 0x%08x\n", val);
        status = BLADERF_ERR_UNEXPECTED;
    }

    return status;
}

static int set_filterbank_mux(struct bladerf *dev, bladerf_channel ch, bladerf_xb200_filter filter)
{
    int status;
    uint32_t orig, val, mask;
    unsigned int shift;
    static const char *filters[] = { "50M", "144M", "222M", "custom" };

    assert(filter >= 0);
    assert(filter < ARRAY_SIZE(filters));

    if (ch == BLADERF_CHANNEL_RX(0)) {
        mask = BLADERF_XB_RX_MASK;
        shift = BLADERF_XB_RX_SHIFT;
    } else {
        mask = BLADERF_XB_TX_MASK;
        shift = BLADERF_XB_TX_SHIFT;
    }

    status = dev->backend->expansion_gpio_read(dev, &orig);
    if (status != 0) {
        return status;
    }

    val = orig & ~mask;
    val |= filter << shift;

    if (orig != val) {
        log_debug("Engaging %s band XB-200 %s filter\n", filters[filter],
            mask == BLADERF_XB_TX_MASK ? "TX" : "RX");

        status = dev->backend->expansion_gpio_write(dev, 0xffffffff, val);
        if (status != 0) {
            return status;
        }
    }


    return 0;
}

int xb200_set_filterbank(struct bladerf *dev,
                         bladerf_channel ch,
                         bladerf_xb200_filter filter)
{
    struct xb200_xb_data *xb_data = dev->xb_data;
    uint64_t frequency;

    int status = 0;

    if (ch != BLADERF_CHANNEL_RX(0) && ch != BLADERF_CHANNEL_TX(0)) {
        return BLADERF_ERR_INVAL;
    }

    if (NULL == xb_data) {
        log_error("xb_data is null (do you need to xb200_attach?)\n");
        return BLADERF_ERR_INVAL;
    }

    status = check_xb200_filter(filter);
    if (status != 0) {
        return status;
    }

    if (filter == BLADERF_XB200_AUTO_1DB || filter == BLADERF_XB200_AUTO_3DB) {
        /* Save which soft auto filter mode we're in */
        xb_data->auto_filter[ch] = filter;

        status = dev->board->get_frequency(dev, ch, &frequency);
        if (status == 0) {
            status = xb200_auto_filter_selection(dev, ch, frequency);
        }

    } else {
        /* Invalidate the soft auto filter mode entry */
        xb_data->auto_filter[ch] = -1;

        status = set_filterbank_mux(dev, ch, filter);
    }

    return status;
}

int xb200_auto_filter_selection(struct bladerf *dev,
                                bladerf_channel ch,
                                uint64_t frequency)
{
    struct xb200_xb_data *xb_data = dev->xb_data;
    bladerf_xb200_filter filter;

    int status = 0;

    if (frequency >= 300000000u) {
        return 0;
    }

    if (ch != BLADERF_CHANNEL_RX(0) && ch != BLADERF_CHANNEL_TX(0)) {
        return BLADERF_ERR_INVAL;
    }

    if (NULL == xb_data) {
        log_error("xb_data is null (do you need to xb200_attach?)\n");
        return BLADERF_ERR_INVAL;
    }

    if (xb_data->auto_filter[ch] == BLADERF_XB200_AUTO_1DB) {
        if (37774405 <= frequency && frequency <= 59535436) {
            filter = BLADERF_XB200_50M;
        } else if (128326173 <= frequency && frequency <= 166711171) {
            filter = BLADERF_XB200_144M;
        } else if (187593160 <= frequency && frequency <= 245346403) {
            filter = BLADERF_XB200_222M;
        } else {
            filter = BLADERF_XB200_CUSTOM;
        }

        status = set_filterbank_mux(dev, ch, filter);
    } else if (xb_data->auto_filter[ch] == BLADERF_XB200_AUTO_3DB) {
        if (34782924 <= frequency && frequency <= 61899260) {
            filter = BLADERF_XB200_50M;
        } else if (121956957 <= frequency && frequency <= 178444099) {
            filter = BLADERF_XB200_144M;
        } else if (177522675 <= frequency && frequency <= 260140935) {
            filter = BLADERF_XB200_222M;
        } else {
            filter = BLADERF_XB200_CUSTOM;
        }

        status = set_filterbank_mux(dev, ch, filter);
    }

    return status;
}

#define LMS_RX_SWAP 0x40
#define LMS_TX_SWAP 0x08

int xb200_set_path(struct bladerf *dev,
                   bladerf_channel ch, bladerf_xb200_path path) {
    int status;
    uint32_t val;
    uint32_t mask;
    uint8_t lval, lorig = 0;

    if (ch != BLADERF_CHANNEL_RX(0) && ch != BLADERF_CHANNEL_TX(0))
        return BLADERF_ERR_INVAL;

    status = check_xb200_path(path);
    if (status != 0) {
        return status;
    }

    status = LMS_READ(dev, 0x5A, &lorig);
    if (status != 0) {
        return status;
    }

    lval = lorig;

    if (path == BLADERF_XB200_MIX) {
        lval |= (ch == BLADERF_CHANNEL_RX(0)) ? LMS_RX_SWAP : LMS_TX_SWAP;
    } else {
        lval &= ~((ch == BLADERF_CHANNEL_RX(0)) ? LMS_RX_SWAP : LMS_TX_SWAP);
    }

    status = LMS_WRITE(dev, 0x5A, lval);
    if (status != 0) {
        return status;
    }

    status = dev->backend->expansion_gpio_read(dev, &val);
    if (status != 0) {
        return status;
    }

    status = dev->backend->expansion_gpio_read(dev, &val);
    if (status != 0) {
        return status;
    }

    if (!(val & BLADERF_XB_RF_ON)) {
        status = xb200_attach(dev);
        if (status != 0) {
            return status;
        }
    }

    if (ch == BLADERF_CHANNEL_RX(0)) {
        mask = (BLADERF_XB_CONFIG_RX_BYPASS_MASK | BLADERF_XB_RX_ENABLE);
    } else {
        mask = (BLADERF_XB_CONFIG_TX_BYPASS_MASK | BLADERF_XB_TX_ENABLE);
    }

    val |= BLADERF_XB_RF_ON;
    val &= ~mask;

    if (ch == BLADERF_CHANNEL_RX(0)) {
        if (path == BLADERF_XB200_MIX) {
            val |= (BLADERF_XB_RX_ENABLE | BLADERF_XB_CONFIG_RX_PATH_MIX);
        } else {
            val |= BLADERF_XB_CONFIG_RX_PATH_BYPASS;
        }
    } else {
        if (path == BLADERF_XB200_MIX) {
            val |= (BLADERF_XB_TX_ENABLE | BLADERF_XB_CONFIG_TX_PATH_MIX);
        } else {
            val |= BLADERF_XB_CONFIG_TX_PATH_BYPASS;
        }
    }

    return dev->backend->expansion_gpio_write(dev, 0xffffffff, val);
}

int xb200_get_path(struct bladerf *dev,
                   bladerf_channel ch, bladerf_xb200_path *path) {
    int status;
    uint32_t val;

    if (ch != BLADERF_CHANNEL_RX(0) && ch != BLADERF_CHANNEL_TX(0))
        return BLADERF_ERR_INVAL;

    status = dev->backend->expansion_gpio_read(dev, &val);
    if (status != 0) {
        return status;
    }

    if (ch == BLADERF_CHANNEL_RX(0)) {
        *path = (val & BLADERF_XB_CONFIG_RX_BYPASS) ?
                    BLADERF_XB200_MIX : BLADERF_XB200_BYPASS;

    } else if (ch == BLADERF_CHANNEL_TX(0)) {
        *path = (val & BLADERF_XB_CONFIG_TX_BYPASS) ?
                    BLADERF_XB200_MIX : BLADERF_XB200_BYPASS;
    }

    return 0;
}
