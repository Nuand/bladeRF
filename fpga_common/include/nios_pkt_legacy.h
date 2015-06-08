/*
 * Copyright (c) 2013-2015 Nuand LLC
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

/* This file defines the legacy Host <-> FPGA (NIOS II) packet format */

#ifndef BLADERF_NIOS_PKT_LEGACY_H_
#define BLADERF_NIOS_PKT_LEGACY_H_

#define UART_PKT_DEV_GPIO_ADDR          0
#define UART_PKT_DEV_RX_GAIN_ADDR       4
#define UART_PKT_DEV_RX_PHASE_ADDR      6
#define UART_PKT_DEV_TX_GAIN_ADDR       8
#define UART_PKT_DEV_TX_PHASE_ADDR      10
#define UART_PKT_DEV_FGPA_VERSION_ID    12

struct uart_pkt {
    unsigned char magic;
#define UART_PKT_MAGIC          'N'

    //  | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
    //  |  dir  |  dev  |     cnt       |
    unsigned char mode;
#define UART_PKT_MODE_CNT_MASK   0x7
#define UART_PKT_MODE_CNT_SHIFT  0

#define UART_PKT_MODE_DEV_MASK   0x30
#define UART_PKT_MODE_DEV_SHIFT  4
#define UART_PKT_DEV_CONFIG      (0<<UART_PKT_MODE_DEV_SHIFT)
#define UART_PKT_DEV_LMS         (1<<UART_PKT_MODE_DEV_SHIFT)
#define UART_PKT_DEV_VCTCXO      (2<<UART_PKT_MODE_DEV_SHIFT)
#define UART_PKT_DEV_SI5338      (3<<UART_PKT_MODE_DEV_SHIFT)

#define UART_PKT_MODE_DIR_MASK   0xC0
#define UART_PKT_MODE_DIR_SHIFT  6
#define UART_PKT_MODE_DIR_READ   (2<<UART_PKT_MODE_DIR_SHIFT)
#define UART_PKT_MODE_DIR_WRITE  (1<<UART_PKT_MODE_DIR_SHIFT)
};

struct uart_cmd {
    union {
        struct {
            unsigned char addr;
            unsigned char data;
        };
        unsigned short word;
    };
};

#endif
