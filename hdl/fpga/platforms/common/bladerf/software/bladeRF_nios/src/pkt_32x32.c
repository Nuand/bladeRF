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
#include "pkt_32x32.h"
#include "devices.h"
#include "debug.h"

static inline bool perform_write(uint8_t id, uint32_t addr, uint32_t data)
{
    switch (id) {
        /* The address is used as a bitmask for the expansion IO registers.
         * We'll skip RMWs if all bits are being written */
        case NIOS_PKT_32x32_TARGET_EXP:
            if (addr != 0xffffffff) {
                uint32_t tmp = expansion_port_read();
                tmp &= ~(addr);
                tmp |= (data & addr);
                expansion_port_write(tmp);
            } else {
                expansion_port_write(data);
            }
            break;

        case NIOS_PKT_32x32_TARGET_EXP_DIR:
            if (addr != 0xffffffff) {
                uint32_t tmp = expansion_port_get_direction();
                tmp &= ~(addr);
                tmp |= (data & addr);
                expansion_port_set_direction(tmp);
            } else {
                expansion_port_set_direction(data);
            }
            break;

#ifdef BOARD_BLADERF_MICRO
        case NIOS_PKT_32x32_TARGET_ADI_AXI:
            adi_axi_write(addr, data);
            break;
#endif  // BOARD_BLADERF_MICRO

        /* Add user customizations here

        case NIOS_PKT_8x8_TARGET_USR1:
        ...
        case NIOS_PKT_8x8_TARGET_USR128:

        */

        default:
            DBG("Invalid ID: 0x%x\n", id);
            return false;
    }

    return true;
}

static inline bool perform_read(uint8_t id, uint32_t addr, uint32_t *data)
{
    switch (id) {
        /* The address is used as a bitmask for the expansion IO registers */
        case NIOS_PKT_32x32_TARGET_EXP:
            *data = expansion_port_read() & addr;
            break;

        case NIOS_PKT_32x32_TARGET_EXP_DIR:
            *data = expansion_port_get_direction() & addr;
            break;

#ifdef BOARD_BLADERF_MICRO
        case NIOS_PKT_32x32_TARGET_ADI_AXI:
            *data = adi_axi_read(addr);
            break;
#endif  // BOARD_BLADERF_MICRO

        /* Add user customizations here

        case NIOS_PKT_8x8_TARGET_USR1:
        ...
        case NIOS_PKT_8x8_TARGET_USR128:

        */

        default:
            DBG("Invalid ID: 0x%x\n", id);
            return false;
    }

    return true;
}

void pkt_32x32(struct pkt_buf *b)
{
    uint8_t id;
    uint32_t addr;
    uint32_t data;
    bool is_write;
    bool success;

    nios_pkt_32x32_unpack(b->req, &id, &is_write, &addr, &data);

    if (is_write) {
        success = perform_write(id, addr, data);
    } else {
        success = perform_read(id, addr, &data);
    }

    nios_pkt_32x32_resp_pack(b->resp, id, is_write, addr, data, success);
}
