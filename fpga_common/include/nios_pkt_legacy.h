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

#ifndef BLADERF_NIOS_PKT_LEGACY_H_
#define BLADERF_NIOS_PKT_LEGACY_H_

/* This is the original packet format used to issue requests from the
 * host to the FPGA via the FX3 UART.
 *
 * This format remains supported for backwards compatibility, but should no
 * longer be added to.
 *
 * If you're looking to customize the FPGA, consider using
 * one of the "pkt_AxB" packet formats and handlers, or implementing a new
 * format and handler.
 *
 *                              Request
 *                      ----------------------
 *
 * +================+=========================================================+
 * |  Byte offset   |                       Description                       |
 * +================+=========================================================+
 * |        0       | Magic Value                                             |
 * +----------------+---------------------------------------------------------+
 * |        1       | Configuration byte (Note 1)                             |
 * +----------------+---------------------------------------------------------+
 * |      2 - 15    | Pairs of 8-bit addr, 8-bit data                         |
 * +----------------+---------------------------------------------------------+
 *
 *
 *
 * Note 1: Configuration byte:
 *
 * +================+========================+
 * |      Bit(s)    |         Value          |
 * +================+========================+
 * |        7       |   1 = Read operation   |
 * +----------------+------------------------+
 * |        6       |   1 = Write operation  |
 * +----------------+------------------------+
 * |       5:4      | Device:                |
 * |                |   00 - Config PIO      |
 * |                |   01 - LMS register    |
 * |                |   10 - VCTCXO Trim DAC |
 * |                |   11 - SI5338 register |
 * +----------------+------------------------+
 * |        3       | Unused                 |
 * +----------------+------------------------+
 * |       2:0      | Addr/Data pair count   |
 * |                | (Note 2)               |
 * +----------------+------------------------+
 *
 *
 * Note 2: "Count" field
 *
 * The original intent of this field was to allow multiple register
 * accesses to be requested at once.
 *
 * However, this feature was not leveraged by the host code for the LMS and
 * SI5338 accesses, so revised legacy packet handler only processes the
 * first addr/data pair.
 *
 * Readback of the time tamer values is the only case where this field
 * is set to a count greater than 1.
 *
 * Although config PIO values are larger than one byte, the host code
 * accessed these byte by byte through multiple requests.  For example,
 * 4 accesses would be required to fully read/write the configuration PIO.
 *
 * The above inefficiency is the motivation behind adding packet handlers
 * that can read/write 32 or 64 bits in a single request (e.g., pkt_8x32,
 * pkt_8x64).
 *
 *
 *
 *                              Response
 *                      ----------------------
 *
 * The response for the legacy packet is essentially just the device
 * echoing the request.
 *
 * On a read request, the number of requested items will be populated
 * in bytes 2:15.
 *
 * The remaining bytes, or all of bytes 2:15 on a write request, should
 * be regarded as "undefined" values and not used.
 *
 * +================+=========================================================+
 * |  Byte offset   |                       Description                       |
 * +================+=========================================================+
 * |        0       | Magic Value                                             |
 * +----------------+---------------------------------------------------------+
 * |        1       | Configuration byte                                      |
 * +----------------+---------------------------------------------------------+
 * |      2 - 15    | Pairs of 8-bit addr, 8-bit data                         |
 * +----------------+---------------------------------------------------------+
 *
 */

#define UART_PKT_MAGIC                  'N'

#define UART_PKT_DEV_GPIO_ADDR          0
#define UART_PKT_DEV_RX_GAIN_ADDR       4
#define UART_PKT_DEV_RX_PHASE_ADDR      6
#define UART_PKT_DEV_TX_GAIN_ADDR       8
#define UART_PKT_DEV_TX_PHASE_ADDR      10
#define UART_PKT_DEV_FGPA_VERSION_ID    12

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

struct uart_cmd {
    unsigned char addr;
    unsigned char data;
};

#endif
