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
#include "pkt_8x8.h"
#include "devices.h"
#include "debug.h"

static inline bool perform_read(uint8_t id, uint8_t addr, uint8_t *data)
{
    switch (id) {
        case NIOS_PKT_8x8_TARGET_LMS6:
            *data = lms6_read(addr);
            break;

        case NIOS_PKT_8x8_TARGET_SI5338:
            *data = si5338_read(addr);
            break;

        case NIOS_PKT_8x8_TARGET_VCTCXO_TAMER:
            switch (addr) {
                /* Using 0xff for mode selection operation so that we can
                 * reserve lower values for register-level access */
                case 0xff:
                    *data = (uint8_t) vctcxo_tamer_get_tune_mode();
                    break;

                default:
                    *data = 0x00;
            }
            break;

        case NIOS_PKT_8x8_TX_TRIGGER_CTL:
            *data = tx_trigger_ctl_read();
            break;

        case NIOS_PKT_8x8_RX_TRIGGER_CTL:
            *data = rx_trigger_ctl_read();
            break;

        /* Add user customizations here

        case NIOS_PKT_8x8_TARGET_USR1:
        ...
        case NIOS_PKT_8x8_TARGET_USR128:

        */

        default:
            DBG("%s: Invalid ID: 0x%x\n", __FUNCTION__, id);
            *data = 0xff;
            return false;
    }

    return true;
}

static inline bool perform_write(uint8_t id, uint8_t addr, uint8_t data)
{
    switch (id) {
        case NIOS_PKT_8x8_TARGET_LMS6:
            lms6_write(addr, data);
            break;

        case NIOS_PKT_8x8_TARGET_SI5338:
            si5338_write(addr, data);
            break;

        case NIOS_PKT_8x8_TARGET_VCTCXO_TAMER:
            switch (addr) {
                /* Using 0xff for mode selection operation so that we can
                 * reserve lower values for register-level access */
                case 0xff:
                    vctcxo_tamer_set_tune_mode((bladerf_vctcxo_tamer_mode) data);
                    break;

                default:
                    /* Throw away data for unused subaddress values */
                    break;
            }
            break;

        case NIOS_PKT_8x8_TX_TRIGGER_CTL:
            tx_trigger_ctl_write(data);
            break;

        case NIOS_PKT_8x8_RX_TRIGGER_CTL:
            rx_trigger_ctl_write(data);
            break;

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

void pkt_8x8(struct pkt_buf *b)
{
    uint8_t id;
    uint8_t addr;
    uint8_t data;
    bool    is_write;
    bool    success;

    nios_pkt_8x8_unpack(b->req, &id, &is_write, &addr, &data);

    if (is_write) {
        success = perform_write(id, addr, data);
    } else {
        success = perform_read(id, addr, &data);
    }

    nios_pkt_8x8_resp_pack(b->resp, id, is_write, addr, data, success);
}
