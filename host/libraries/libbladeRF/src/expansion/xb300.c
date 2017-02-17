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

#include "xb300.h"

#include "log.h"

#define BLADERF_XB_AUX_EN   0x000002
#define BLADERF_XB_TX_LED   0x000010
#define BLADERF_XB_RX_LED   0x000020
#define BLADERF_XB_TRX_TXn  0x000040
#define BLADERF_XB_TRX_RXn  0x000080
#define BLADERF_XB_TRX_MASK 0x0000c0
#define BLADERF_XB_PA_EN    0x000200
#define BLADERF_XB_LNA_ENn  0x000400
#define BLADERF_XB_CS       0x010000
#define BLADERF_XB_CSEL     0x040000
#define BLADERF_XB_DOUT     0x100000
#define BLADERF_XB_SCLK     0x400000

int xb300_attach(struct bladerf *dev) {
    int status;
    uint32_t val;

    val = BLADERF_XB_TX_LED | BLADERF_XB_RX_LED | BLADERF_XB_TRX_MASK;
    val |= BLADERF_XB_PA_EN | BLADERF_XB_LNA_ENn;
    val |= BLADERF_XB_CSEL  | BLADERF_XB_SCLK | BLADERF_XB_CS;

    if ((status = dev->backend->expansion_gpio_dir_write(dev, 0xffffffff, val)))
        return status;

    val = BLADERF_XB_CS | BLADERF_XB_LNA_ENn;
    if ((status = dev->backend->expansion_gpio_write(dev, 0xffffffff, val)))
        return status;

    return status;
}

void xb300_detach(struct bladerf *dev)
{
}

int xb300_enable(struct bladerf *dev, bool enable)
{
    int status;
    uint32_t val;
    float pwr;

    val = BLADERF_XB_CS | BLADERF_XB_CSEL | BLADERF_XB_LNA_ENn;
    if ((status = dev->backend->expansion_gpio_write(dev, 0xffffffff, val)))
        return status;

    status = xb300_get_output_power(dev, &pwr);

    return status;
}

int xb300_init(struct bladerf *dev)
{
    int status;

    log_verbose( "Setting TRX path to TX\n" );
    status = xb300_set_trx(dev, BLADERF_XB300_TRX_TX);
    if (status != 0) {
        return status;
    }

    return 0;
}

int xb300_set_trx(struct bladerf *dev, bladerf_xb300_trx trx)
{
    int status;
    uint32_t val;

    status = dev->backend->expansion_gpio_read(dev, &val);
    if (status != 0) {
        return status;
    }

    val &= ~(BLADERF_XB_TRX_MASK);

    switch (trx) {
        case BLADERF_XB300_TRX_RX:
            val |= BLADERF_XB_TRX_RXn;
            break;

        case BLADERF_XB300_TRX_TX:
            val |= BLADERF_XB_TRX_TXn;
            break;

        case BLADERF_XB300_TRX_UNSET:
            break;

        default:
            log_debug("Invalid TRX option: %d\n", trx);
            return BLADERF_ERR_INVAL;
    }

    status = dev->backend->expansion_gpio_write(dev, 0xffffffff, val);
    return status;
}

int xb300_get_trx(struct bladerf *dev, bladerf_xb300_trx *trx)
{
    int status;
    uint32_t val;

    *trx = BLADERF_XB300_TRX_INVAL;

    status = dev->backend->expansion_gpio_read(dev, &val);
    if (status != 0) {
        return status;
    }

    val &= BLADERF_XB_TRX_MASK;

    if (!val) {
        *trx = BLADERF_XB300_TRX_UNSET;
    } else {
        *trx = (val & BLADERF_XB_TRX_RXn) ? BLADERF_XB300_TRX_RX : BLADERF_XB300_TRX_TX;
    }

    /* Sanity check */
    switch (*trx) {
        case BLADERF_XB300_TRX_TX:
        case BLADERF_XB300_TRX_RX:
        case BLADERF_XB300_TRX_UNSET:
            break;

        default:
            log_debug("Read back invalid TRX setting value: %d\n", *trx);
            status = BLADERF_ERR_INVAL;
    }

    return status;
}

int xb300_set_amplifier_enable(struct bladerf *dev,
                               bladerf_xb300_amplifier amp, bool enable) {
    int status;
    uint32_t val;

    status = dev->backend->expansion_gpio_read(dev, &val);
    if (status != 0) {
        return status;
    }

    switch (amp) {
        case BLADERF_XB300_AMP_PA:
            if (enable) {
                val |= BLADERF_XB_TX_LED;
                val |= BLADERF_XB_PA_EN;
            } else {
                val &= ~BLADERF_XB_TX_LED;
                val &= ~BLADERF_XB_PA_EN;
            }
            break;

        case BLADERF_XB300_AMP_LNA:
            if (enable) {
                val |= BLADERF_XB_RX_LED;
                val &= ~BLADERF_XB_LNA_ENn;
            } else {
                val &= ~BLADERF_XB_RX_LED;
                val |= BLADERF_XB_LNA_ENn;
            }
            break;

        case BLADERF_XB300_AMP_PA_AUX:
            if (enable) {
                val |= BLADERF_XB_AUX_EN;
            } else {
                val &= ~BLADERF_XB_AUX_EN;
            }
            break;

        default:
            log_debug("Invalid amplifier selection: %d\n", amp);
            return BLADERF_ERR_INVAL;
    }

    status = dev->backend->expansion_gpio_write(dev, 0xffffffff, val);

    return status;
}

int xb300_get_amplifier_enable(struct bladerf *dev,
                               bladerf_xb300_amplifier amp, bool *enable)
{
    int status;
    uint32_t val;

    *enable = false;

    status = dev->backend->expansion_gpio_read(dev, &val);
    if (status != 0) {
        return status;
    }

    switch (amp) {
        case BLADERF_XB300_AMP_PA:
            *enable = (val & BLADERF_XB_PA_EN);
            break;

        case BLADERF_XB300_AMP_LNA:
            *enable = !(val & BLADERF_XB_LNA_ENn);
            break;

        case BLADERF_XB300_AMP_PA_AUX:
            *enable = (val & BLADERF_XB_AUX_EN);
            break;

        default:
            log_debug("Read back invalid amplifier setting: %d\n", amp);
            status = BLADERF_ERR_INVAL;
    }

    return status;
}

int xb300_get_output_power(struct bladerf *dev, float *pwr)
{
    uint32_t val, rval;
    int i;
    int ret = 0;
    int status;
    float volt, volt2, volt3, volt4;

    status = dev->backend->expansion_gpio_read(dev, &val);
    if (status) {
        return status;
    }

    val &= ~(BLADERF_XB_CS | BLADERF_XB_SCLK | BLADERF_XB_CSEL);

    status = dev->backend->expansion_gpio_write(dev, 0xffffffff, BLADERF_XB_SCLK | val);
    if (status) {
        return status;
    }

    status = dev->backend->expansion_gpio_write(dev, 0xffffffff, BLADERF_XB_CS | BLADERF_XB_SCLK | val);
    if (status) {
        return status;
    }

    for (i = 1; i <= 14; i++) {
        status = dev->backend->expansion_gpio_write(dev, 0xffffffff, val);
        if (status) {
            return status;
        }

        status = dev->backend->expansion_gpio_write(dev, 0xffffffff, BLADERF_XB_SCLK | val);
        if (status) {
            return status;
        }

        status = dev->backend->expansion_gpio_read(dev, &rval);
        if (status) {
            return status;
        }

        if (i >= 2 && i <= 11) {
            ret |= (!!(rval & BLADERF_XB_DOUT)) << (11 - i);
        }
    }

    volt = (1.8f/1024.0f) * ret;
    volt2 = volt  * volt;
    volt3 = volt2 * volt;
    volt4 = volt3 * volt;

    *pwr =  -503.933f  * volt4 +
            1409.489f  * volt3 -
            1487.84f   * volt2 +
             722.9793f * volt  -
             114.7529f;

    return 0;

}
