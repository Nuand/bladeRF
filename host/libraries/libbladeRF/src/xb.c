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

#include "libbladeRF.h"     /* Public API */
#include "bladerf_priv.h"   /* Implementation-specific items ("private") */
#include "capabilities.h"

#include "si5338.h"
#include "xb.h"
#include "lms.h"
#include "rel_assert.h"
#include "log.h"

#define BLADERF_CONFIG_RX_SWAP_IQ 0x20000000
#define BLADERF_CONFIG_TX_SWAP_IQ 0x10000000

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
    if ((status = SI5338_WRITE(dev, 39, 2)))
        return status;
    if ((status = SI5338_WRITE(dev, 34, 0x22)))
        return status;
    if ((status = CONFIG_GPIO_READ(dev, &val)))
        return status;
    val |= 0x80000000;
    if ((status = CONFIG_GPIO_WRITE(dev, val)))
        return status;
    if ((status = XB_GPIO_READ(dev, &val)))
        return status;

    if ((status = XB_GPIO_DIR_WRITE(dev, 0x3C00383E)))
        return status;

    if ((status = XB_GPIO_WRITE(dev, 0x800)))
        return status;

    // Load ADF4351 registers via SPI
    // Refer to ADF4351 reference manual for register set
    // The LO is set to a Int-N 1248MHz +3dBm tone
    // Registers are written in order from 5 downto 0
    if ((status = XB_SPI_WRITE(dev, 0x580005)))
        return status;
    if ((status = XB_SPI_WRITE(dev, 0x99A16C)))
        return status;
    if ((status = XB_SPI_WRITE(dev, 0xC004B3)))
        return status;
    log_debug("  MUXOUT: %s\n", mux_lut[muxout]);

    if ((status = XB_SPI_WRITE(dev, 0x60008E42 | (1<<8) | (muxout << 26))))
        return status;
    if ((status = XB_SPI_WRITE(dev, 0x08008011)))
        return status;
    if ((status = XB_SPI_WRITE(dev, 0x00410000)))
        return status;

    status = XB_GPIO_READ(dev, &val);
    if (!status && (val & 0x1))
        log_debug("  MUXOUT Bit set: OK\n");
    else {
        log_debug("  MUXOUT Bit not set: FAIL\n");
    }
    status = XB_GPIO_WRITE(dev, 0x3C000800);

    return status;
}

int xb200_enable(struct bladerf *dev, bool enable) {
    int status;
    uint32_t val, orig;

    status = XB_GPIO_READ(dev, &orig);
    if (status)
        return status;

    val = orig;
    if (enable)
        val |= BLADERF_XB_RF_ON;
    else
        val &= ~BLADERF_XB_RF_ON;

    if (status || (val == orig))
        return status;

    return XB_GPIO_WRITE(dev, val);
}

int xb_attach(struct bladerf *dev, bladerf_xb xb) {
    bladerf_xb attached;
    int status;

    status = xb_get_attached(dev, &attached);
    if (status != 0) {
        return status;
    }

    if (xb != attached && attached != BLADERF_XB_NONE) {
        log_debug("%s: Switching XB types is not supported.\n", __FUNCTION__);
        return BLADERF_ERR_UNSUPPORTED;
    }

    switch (xb) {
        /* This requires support from the FPGA and FX3 firmware to be added */
        case BLADERF_XB_100:
            log_debug("%s: The XB-100 is not currently supported by "
                      "libbladeRF\n", __FUNCTION__);
            status = BLADERF_ERR_UNSUPPORTED;
            break;

        case BLADERF_XB_200:
            if (!have_cap(dev, BLADERF_CAP_XB200)) {
                log_warning("%s: XB200 support requires FPGA v0.0.5 or later\n",
                            __FUNCTION__);
                status = BLADERF_ERR_UPDATE_FPGA;
            } else if (attached != xb) {
                log_verbose( "Attaching XB200\n" );
                status = xb200_attach(dev);
                if (status != 0) {
                    break;
                }
                log_verbose( "Enabling XB200\n" );
                status = xb200_enable(dev,true);
                if (status != 0) {
                    break;
                }
                log_verbose( "Setting RX path\n" );
                status = xb200_set_path(dev, BLADERF_MODULE_RX, BLADERF_XB200_BYPASS);
                if (status != 0) {
                    break;
                }
                log_verbose( "Setting TX path\n" );
                status = xb200_set_path(dev, BLADERF_MODULE_TX, BLADERF_XB200_BYPASS);
            }
            break;

        case BLADERF_XB_NONE:
            log_debug("%s: Disabling an attached XB is not supported.\n",
                      __FUNCTION__);
            status = BLADERF_ERR_UNSUPPORTED;
            break;

        default:
            log_debug("%s: Unknown xb type: %d\n", __FUNCTION__, xb);
            status = BLADERF_ERR_INVAL;
            break;
    }

    /* Cache what we have attached in our device handle to alleviate the
     * need to go read the device state */
    if (status == 0) {
        dev->xb = xb;
    }

    return status;
}

int xb_get_attached(struct bladerf *dev, bladerf_xb *xb) {
    int status;
    uint32_t val;

    status = CONFIG_GPIO_READ(dev, &val);
    if (status)
        return status;

    *xb = (val >> 30) & 0x3;
    return 0;
}

int xb200_get_filterbank(struct bladerf *dev, bladerf_module module,
                         bladerf_xb200_filter *filter) {
    int status;
    uint32_t val;
    unsigned int shift;

    status = check_module(module);
    if (status != 0) {
        return status;
    }

    status = XB_GPIO_READ(dev, &val);
    if (status != 0) {
        return status;
    }

    if (module == BLADERF_MODULE_RX) {
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

static int set_filterbank(struct bladerf *dev, bool update_auto_filt,
                          bladerf_module module, bladerf_xb200_filter filter)
{
    int status;
    uint32_t orig, val, mask;
    unsigned int shift;

    status = check_module(module);
    if (status != 0) {
        return status;
    }

    status = check_xb200_filter(filter);
    if (status != 0) {
        return status;
    }

    if (module == BLADERF_MODULE_RX) {
        mask = BLADERF_XB_RX_MASK;
        shift = BLADERF_XB_RX_SHIFT;
    } else {
        mask = BLADERF_XB_TX_MASK;
        shift = BLADERF_XB_TX_SHIFT;
    }

    status = XB_GPIO_READ(dev, &orig);
    if (status != 0) {
        return status;
    }

    val = orig & ~mask;
    val |= filter << shift;

    if (orig != val) {
        status = XB_GPIO_WRITE(dev, val);
        if (status != 0) {
            return status;
        }
    }


    /*
     * Update state information regarding automatically selected filter.
     * Invalidate the entry if we're not entering automatic selection mode.
     *
     * We'll suppress this update when we know we're calling this function
     * from xb200_auto_filter_selection for an automatic update so that we
     * don't clobber our auto_filter mode.
     */
    if (update_auto_filt) {
        if (filter == BLADERF_XB200_AUTO_1DB || filter == BLADERF_XB200_AUTO_3DB) {
            dev->auto_filter[module] = filter;
        } else {
            dev->auto_filter[module] = -1;
        }
    }

    return 0;
}

int xb200_set_filterbank(struct bladerf *dev,
                         bladerf_module module, bladerf_xb200_filter filter) {

    return set_filterbank(dev, true, module, filter);
}

int xb200_auto_filter_selection(struct bladerf *dev, bladerf_module mod,
                                unsigned int frequency) {
    int status;
    bladerf_xb200_filter filter;

    if (frequency >= 300000000u) {
        return 0;
    }

    status = check_module(mod);
    if (status != 0) {
        return status;
    }

    if (dev->auto_filter[mod] == BLADERF_XB200_AUTO_1DB) {
        if (37774405 <= frequency && frequency <= 59535436) {
            filter = BLADERF_XB200_50M;
        } else if (128326173 <= frequency && frequency <= 166711171) {
            filter = BLADERF_XB200_144M;
        } else if (187593160 <= frequency && frequency <= 245346403) {
            filter = BLADERF_XB200_222M;
        } else {
            filter = BLADERF_XB200_CUSTOM;
        }

        status = set_filterbank(dev, false, mod, filter);
    } else if (dev->auto_filter[mod] == BLADERF_XB200_AUTO_3DB) {
        if (34782924 <= frequency && frequency <= 61899260) {
            filter = BLADERF_XB200_50M;
        } else if (121956957 <= frequency && frequency <= 178444099) {
            filter = BLADERF_XB200_144M;
        } else if (177522675 <= frequency && frequency <= 260140935) {
            filter = BLADERF_XB200_222M;
        } else {
            filter = BLADERF_XB200_CUSTOM;
        }

        status = set_filterbank(dev, false, mod, filter);
    }

    return status;
}


#define LMS_RX_SWAP 0x40
#define LMS_TX_SWAP 0x08

int xb200_set_path(struct bladerf *dev,
                   bladerf_module module, bladerf_xb200_path path) {
    int status;
    uint32_t val;
    uint32_t mask;
    uint8_t lval, lorig = 0;

    status = check_module(module);
    if (status != 0) {
        return status;
    }

    status = check_xb200_path(path);
    if (status != 0) {
        return status;
    }

    status = LMS_READ( dev, 0x5A, &lorig );
    if (status != 0) {
        return status;
    }

    lval = lorig;

    if (path == BLADERF_XB200_MIX) {
        lval |= (module == BLADERF_MODULE_RX) ? LMS_RX_SWAP : LMS_TX_SWAP;
    } else {
        lval &= ~((module == BLADERF_MODULE_RX) ? LMS_RX_SWAP : LMS_TX_SWAP);
    }

    status = LMS_WRITE(dev, 0x5A, lval);
    if (status != 0) {
        return status;
    }

    status = XB_GPIO_READ(dev, &val);
    if (status != 0) {
        return status;
    }

    status = XB_GPIO_READ(dev, &val);
    if (status != 0) {
        return status;
    }

    if (!(val & BLADERF_XB_RF_ON)) {
        status = xb200_attach(dev);
        if (status != 0) {
            return status;
        }
    }

    if (module == BLADERF_MODULE_RX) {
        mask = (BLADERF_XB_CONFIG_RX_BYPASS_MASK | BLADERF_XB_RX_ENABLE);
    } else {
        mask = (BLADERF_XB_CONFIG_TX_BYPASS_MASK | BLADERF_XB_TX_ENABLE);
    }

    val |= BLADERF_XB_RF_ON;
    val &= ~mask;

    if (module == BLADERF_MODULE_RX) {
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

    return XB_GPIO_WRITE(dev, val);
}

int xb200_get_path(struct bladerf *dev,
                   bladerf_module module, bladerf_xb200_path *path) {
    int status;
    uint32_t val;

    status = check_module(module);
    if (status != 0) {
        return status;
    }

    status = XB_GPIO_READ(dev, &val);
    if (status != 0) {
        return status;
    }

    if (module == BLADERF_MODULE_RX) {
        *path = (val & BLADERF_XB_CONFIG_RX_BYPASS) ?
                    BLADERF_XB200_MIX : BLADERF_XB200_BYPASS;

    } else if (module == BLADERF_MODULE_TX) {
        *path = (val & BLADERF_XB_CONFIG_TX_BYPASS) ?
                    BLADERF_XB200_MIX : BLADERF_XB200_BYPASS;
    }

    return 0;
}

