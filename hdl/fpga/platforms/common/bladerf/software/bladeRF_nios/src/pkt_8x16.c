/* This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (c) 2015 Nuand LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <stdint.h>
#include <stdbool.h>
#include "pkt_handler.h"
#include "pkt_8x16.h"
#include "devices.h"
#include "debug.h"

static inline bool iq_corr_read(uint8_t addr, uint16_t *data)
{
    switch (addr) {
        case NIOS_PKT_8x16_ADDR_IQ_CORR_RX_GAIN:
            *data = iqbal_get_gain(BLADERF_MODULE_RX);
            break;

        case NIOS_PKT_8x16_ADDR_IQ_CORR_RX_PHASE:
            *data = iqbal_get_phase(BLADERF_MODULE_RX);
            break;

        case NIOS_PKT_8x16_ADDR_IQ_CORR_TX_GAIN:
            *data = iqbal_get_gain(BLADERF_MODULE_TX);
            break;

        case NIOS_PKT_8x16_ADDR_IQ_CORR_TX_PHASE:
            *data = iqbal_get_phase(BLADERF_MODULE_TX);
            break;

        default:
            DBG("%s: Invalid IQ corr addr: 0x%x\n", __FUNCTION__, addr);
            return false;
    }

    return true;
}

static inline bool iq_corr_write(uint8_t addr, uint16_t data)
{
    switch (addr) {
        case NIOS_PKT_8x16_ADDR_IQ_CORR_RX_GAIN:
            iqbal_set_gain(BLADERF_MODULE_RX, data);
            break;

        case NIOS_PKT_8x16_ADDR_IQ_CORR_RX_PHASE:
            iqbal_set_phase(BLADERF_MODULE_RX, data);
            break;

        case NIOS_PKT_8x16_ADDR_IQ_CORR_TX_GAIN:
            iqbal_set_gain(BLADERF_MODULE_TX, data);
            break;

        case NIOS_PKT_8x16_ADDR_IQ_CORR_TX_PHASE:
            iqbal_set_phase(BLADERF_MODULE_TX, data);
            break;

        default:
            DBG("%s: Invalid IQ corr addr: 0x%x\n", __FUNCTION__, addr);
            return false;
    }

    return true;
}

static inline bool perform_read(uint8_t id, uint8_t addr, uint16_t *data)
{
    bool success = true;

    switch (id) {
        case NIOS_PKT_8x16_TARGET_VCTCXO_DAC:
            vctcxo_trim_dac_read(addr, data);
            break;

        case NIOS_PKT_8x16_TARGET_IQ_CORR:
            iq_corr_read(addr, data);
            break;

#ifdef BOARD_BLADERF_MICRO
        case NIOS_PKT_8x16_TARGET_AD56X1_DAC:
            ad56x1_vctcxo_trim_dac_read(data);
            break;
#endif  // BOARD_BLADERF_MICRO

#ifdef BOARD_BLADERF_MICRO
        case NIOS_PKT_8x16_TARGET_INA219:
            *data = ina219_read(addr);
            break;
#endif  // BOARD_BLADERF_MICRO

        /* Add user customizations here

        case NIOS_PKT_8x8_TARGET_USR1:
        ...
        case NIOS_PKT_8x8_TARGET_USR128:

        */

        default:
            DBG("%s: Invalid ID: 0x%x\n", __FUNCTION__, id);
            return false;
    }

    return success;
}


static inline bool perform_write(uint8_t id, uint8_t addr, uint16_t data)
{
    switch (id) {
        case NIOS_PKT_8x16_TARGET_VCTCXO_DAC:
            vctcxo_trim_dac_write(addr, data);
            break;

        case NIOS_PKT_8x16_TARGET_IQ_CORR:
            iq_corr_write(addr, data);
            break;

        case NIOS_PKT_8x16_TARGET_AGC_CORR:
            agc_dc_corr_write(addr, data);
            break;

#ifdef BOARD_BLADERF_MICRO
        case NIOS_PKT_8x16_TARGET_AD56X1_DAC:
            ad56x1_vctcxo_trim_dac_write(data);
            break;
#endif  // BOARD_BLADERF_MICRO

#ifdef BOARD_BLADERF_MICRO
        case NIOS_PKT_8x16_TARGET_INA219:
            ina219_write(addr, data);
            break;
#endif  // BOARD_BLADERF_MICRO

        /* Add user customizations here

        case NIOS_PKT_8x8_TARGET_USR1:
        ...
        case NIOS_PKT_8x8_TARGET_USR128:

        */

        default:
            DBG("%s: Invalid ID: 0x%x\n", __FUNCTION__, id);
            return false;
    }

    return true;
}

void pkt_8x16(struct pkt_buf *b)
{
    uint8_t id;
    uint8_t addr;
    uint16_t data;
    bool is_write;
    bool success;

    nios_pkt_8x16_unpack(b->req, &id, &is_write, &addr, &data);

    if (is_write) {
        success = perform_write(id, addr, data);
    } else {
        success = perform_read(id, addr, &data);
    }

    nios_pkt_8x16_resp_pack(b->resp, id, is_write, addr, data, success);
}
