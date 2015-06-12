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
 * +================+============================+
 * |      Bit(s)    |         Value              |
 * +================+============================+
 * |        7       |   1 = Read operation       |
 * +----------------+----------------------------+
 * |        6       |   1 = Write operation      |
 * +----------------+----------------------------+
 * |       5:4      | Device:                    |
 * |                |   00 - Config PIO (Note 2) |
 * |                |   01 - LMS register        |
 * |                |   10 - VCTCXO Trim DAC     |
 * |                |   11 - SI5338 register     |
 * +----------------+----------------------------+
 * |        3       | Unused                     |
 * +----------------+----------------------------+
 * |       2:0      | Addr/Data pair count       |
 * |                | (Note 2)                   |
 * +----------------+----------------------------+
 *
 * Note 2: Config PIO addresses
 *
 * The NIOS II core and modules in the FPGA's programmable fabric are connected
 * via parallel IO (PIO). See the NIOS_PKT_LEGACY_PIO_ADDR_* definitions
 * in this file contain a virtual "register map" for these modules.
 *
 * Note 3: "Count" field
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

#define NIOS_PKT_LEGACY_MAGIC                  'N'

#define NIOS_PKT_LEGACY_DEV_GPIO_ADDR          0
#define NIOS_PKT_LEGACY_DEV_RX_GAIN_ADDR       4
#define NIOS_PKT_LEGACY_DEV_RX_PHASE_ADDR      6
#define NIOS_PKT_LEGACY_DEV_TX_GAIN_ADDR       8
#define NIOS_PKT_LEGACY_DEV_TX_PHASE_ADDR      10
#define NIOS_PKT_LEGACY_DEV_FPGA_VERSION_ID    12

#define NIOS_PKT_LEGACY_MODE_CNT_MASK   0x7
#define NIOS_PKT_LEGACY_MODE_CNT_SHIFT  0
#define NIOS_PKT_LEGACY_MODE_DEV_MASK   0x30
#define NIOS_PKT_LEGACY_MODE_DEV_SHIFT  4

#define NIOS_PKT_LEGACY_DEV_CONFIG      (0 << NIOS_PKT_LEGACY_MODE_DEV_SHIFT)
#define NIOS_PKT_LEGACY_DEV_LMS         (1 << NIOS_PKT_LEGACY_MODE_DEV_SHIFT)
#define NIOS_PKT_LEGACY_DEV_VCTCXO      (2 << NIOS_PKT_LEGACY_MODE_DEV_SHIFT)
#define NIOS_PKT_LEGACY_DEV_SI5338      (3 << NIOS_PKT_LEGACY_MODE_DEV_SHIFT)

#define NIOS_PKT_LEGACY_MODE_DIR_MASK   0xC0
#define NIOS_PKT_LEGACY_MODE_DIR_SHIFT  6
#define NIOS_PKT_LEGACY_MODE_DIR_READ   (2 << NIOS_PKT_LEGACY_MODE_DIR_SHIFT)
#define NIOS_PKT_LEGACY_MODE_DIR_WRITE  (1 << NIOS_PKT_LEGACY_MODE_DIR_SHIFT)


/* PIO address space */

/*
 * 32-bit Device control register.
 *
 * This is register accessed via the libbladeRF functions,
 * bladerf_config_gpio_write() and bladerf_config_gpio_read().
 */
#define NIOS_PKT_LEGACY_PIO_ADDR_CONTROL       0
#define NIOS_PKT_LEGACY_PIO_LEN_CONTROL        4

/*
 * IQ Correction: 16-bit RX Gain value
 */
#define NIOS_PKT_LEGACY_PIO_ADDR_IQ_RX_GAIN    4
#define NIOS_PKT_LEGACY_PIO_LEN_IQ_RX_GAIN     2

/*
 * IQ Correction: 16-bit RX Phase value
 */
#define NIOS_PKT_LEGACY_PIO_ADDR_IQ_RX_PHASE   6
#define NIOS_PKT_LEGACY_PIO_LEN_IQ_RX_PHASE    2

/*
 * IQ Correction: 16-bit TX Gain value
 */
#define NIOS_PKT_LEGACY_PIO_ADDR_IQ_TX_GAIN    8
#define NIOS_PKT_LEGACY_PIO_LEN_IQ_TX_GAIN     2

/*
 * IQ Correction: 16-bit TX Phase value
 */
#define NIOS_PKT_LEGACY_PIO_ADDR_IQ_TX_PHASE   10
#define NIOS_PKT_LEGACY_PIO_LEN_IQ_TX_PHASE    2

/*
 * 32-bit FPGA Version (read-only)
 */
#define NIOS_PKT_LEGACY_PIO_ADDR_FPGA_VERSION  12
#define NIOS_PKT_LEGACY_PIO_LEN_FPGA_VERSION   4

/*
 * 64-bit RX timestamp
 */
#define NIOS_PKT_LEGACY_PIO_ADDR_RX_TIMESTAMP  16
#define NIOS_PKT_LEGACY_PIO_LEN_RX_TIMESTAMP   8

/*
 * 64-bit TX timestamp
 */
#define NIOS_PKT_LEGACY_PIO_ADDR_TX_TIMESTAMP  24
#define NIOS_PKT_LEGACY_PIO_LEN_TX_TIMESTAMP   8

/*
 * VCTCXO Trim DAC value
 */
#define NIOS_PKT_LEGACY_PIO_ADDR_VCTCXO        34
#define NIOS_PKT_LEGACY_PIO_LEN_VCTCXO         2

/*
 * XB-200 ADF4351 Synthesizer
 */
#define NIOS_PKT_LEGACY_PIO_ADDR_XB200_SYNTH   36
#define NIOS_PKT_LEGACY_PIO_LEN_XB200_SYNTH    4

/*
 * Expansion IO
 */
#define NIOS_PKT_LEGACY_PIO_ADDR_EXP            40
#define NIOS_PKT_LEGACY_PIO_LEN_EXP             4

/*
 * Expansion IO Direction
 */
#define NIOS_PKT_LEGACY_PIO_ADDR_EXP_DIR       44
#define NIOS_PKT_LEGACY_PIO_LEN_EXP_DIR        4

struct uart_cmd {
    unsigned char addr;
    unsigned char data;
};

#endif
