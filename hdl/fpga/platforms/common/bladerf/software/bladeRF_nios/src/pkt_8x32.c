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
#include "nios_pkt_8x32.h"
#include "pkt_8x32.h"
#include "devices.h"
#include "debug.h"

static inline bool perform_read(uint8_t id, uint8_t addr, uint32_t *data)
{
    switch (id) {
        case NIOS_PKT_8x32_TARGET_VERSION:
            *data = fpga_version();
            break;

        case NIOS_PKT_8x32_TARGET_CONTROL:
            *data = control_reg_read();
            break;

        case NIOS_PKT_8x32_TARGET_ADF4351:
            DBG("Illegal read from ADF4351.\n");
            *data = 0x00;
            return false;

        case NIOS_PKT_8x32_TARGET_RFFE_CSR:
            *data = rffe_csr_read();
            break;

#ifdef BOARD_BLADERF_MICRO
        case NIOS_PKT_8x32_TARGET_ADF400X:
            *data = adf400x_spi_read(addr);
            break;
#endif  // BOARD_BLADERF_MICRO

#ifdef BOARD_BLADERF_MICRO
        case NIOS_PKT_8x32_TARGET_FASTLOCK:
            DBG("Read from AD9361 fast lock not supported.\n");
            *data = 0x00;
            return false;
#endif  // BOARD_BLADERF_MICRO

        default:
            DBG("Invalid id: 0x%x\n", id);
            *data = 0x00;
            return false;
    }

    return true;
}

static inline bool perform_write(uint8_t id, uint8_t addr, uint32_t data)
{
    switch (id) {
        case NIOS_PKT_8x32_TARGET_VERSION:
            DBG("Invalid write to version register.\n");
            return false;

        case NIOS_PKT_8x32_TARGET_CONTROL:
            control_reg_write(data);
            break;

        case NIOS_PKT_8x32_TARGET_ADF4351:
            adf4351_write(data);
            break;

        case NIOS_PKT_8x32_TARGET_RFFE_CSR:
            rffe_csr_write(data);
            break;

#ifdef BOARD_BLADERF_MICRO
        case NIOS_PKT_8x32_TARGET_ADF400X:
            adf400x_spi_write(data);
            break;
#endif  // BOARD_BLADERF_MICRO

#ifdef BOARD_BLADERF_MICRO
        case NIOS_PKT_8x32_TARGET_FASTLOCK:
            adi_fastlock_save( (addr == 1), (data >> 16), (data & 0xff));
            break;
#endif  // BOARD_BLADERF_MICRO

        default:
            DBG("Invalid id: 0x%x\n", id);
            return false;
    }

    return true;
}

void pkt_8x32(struct pkt_buf *b)
{
    uint8_t id;
    uint8_t addr;
    uint32_t data;
    bool is_write;
    bool success;

    nios_pkt_8x32_unpack(b->req, &id, &is_write, &addr, &data);

    if (is_write) {
        success = perform_write(id, addr, data);
    } else {
        success = perform_read(id, addr, &data);
    }

    nios_pkt_8x32_resp_pack(b->resp, id, is_write, addr, data, success);
}
