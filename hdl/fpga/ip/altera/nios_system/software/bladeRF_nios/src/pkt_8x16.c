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

#define CFG_DIR_READ    (1<<7)
#define CFG_DEV_MASK    (0x3f)

/*
 * Devices which use 8x16 addressing:
 *  ID          Device
 *  0           VCTCXO Trim Dac
 *                  DACREG for the DAC161S055 device
 *
 *  1           FPGA Config Registers
 *
 *                  Address     Description
 *                  0           RX Gain Adjustment
 *                  1           RX Phase Adjustment
 *                  2           TX Gain Adjustment
 *                  3           TX Phase Adjustment
 */

static inline void pkt_read(uint8_t dev, struct pkt_buf *b) {
    /* TODO: Fill this in */
    return;
}

static inline void pkt_write(uint8_t dev, struct pkt_buf *b) {
    /* TODO: Fill this in */
    return;
}

void pkt_8x16(struct pkt_buf *b)
{
    /* Parse configuration word */
    const uint8_t cfg = b->req[PKT_CFG_IDX];
    const bool is_read = (cfg & (CFG_DIR_READ)) != 0;
    const uint8_t dev_id = (cfg & CFG_DEV_MASK);

    if (is_read) {
        pkt_read(dev_id, b);
    } else /* is_write */ {
        pkt_write(dev_id, b);
    }

    return;
}
