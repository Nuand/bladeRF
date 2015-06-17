/*
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

#ifndef BLADERF_NIOS_PKT_8x32_H_
#define BLADERF_NIOS_PKT_8x32_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/*
 * This file defines the Host <-> FPGA (NIOS II) packet formats for accesses
 * to devices/blocks with 8-bit addresses and 32-bit data
 *
 *
 *                              Request
 *                      ----------------------
 *
 * +================+=========================================================+
 * |  Byte offset   |                       Description                       |
 * +================+=========================================================+
 * |        0       | Magic Value                                             |
 * +----------------+---------------------------------------------------------+
 * |        1       | Target ID (Note 1)                                      |
 * +----------------+---------------------------------------------------------+
 * |        2       | Flags (Note 2)                                          |
 * +----------------+---------------------------------------------------------+
 * |        3       | Reserved. Set to 0x00.                                  |
 * +----------------+---------------------------------------------------------+
 * |        4       | 8-bit address                                           |
 * +----------------+---------------------------------------------------------+
 * |       8:5      | 32-bit data, little-endian                              |
 * +----------------+---------------------------------------------------------+
 * |      15:9      | Reserved. Set to 0.                                     |
 * +----------------+---------------------------------------------------------+
 *
 *
 *                              Response
 *                      ----------------------
 *
 * The response packet contains the same information as the request.
 * A status flag will be set if the operation completed successfully.
 *
 * In the case of a read request, the data field will contain the read data, if
 * the read succeeded.
 *
 * (Note 1)
 *  The "Target ID" refers to the peripheral, device, or block to access.
 *  See the NIOS_PKT_8x32_TARGET_* values.
 *
 * (Note 2)
 *  The flags are defined as follows:
 *
 *    +================+========================+
 *    |      Bit(s)    |         Value          |
 *    +================+========================+
 *    |       7:2      | Reserved. Set to 0.    |
 *    +----------------+------------------------+
 *    |                | Status. Only used in   |
 *    |                | response packet.       |
 *    |                | Ignored in request.    |
 *    |        1       |                        |
 *    |                |   1 = Success          |
 *    |                |   0 = Failure          |
 *    +----------------+------------------------+
 *    |        0       |   0 = Read operation   |
 *    |                |   1 = Write operation  |
 *    +----------------+------------------------+
 *
 */

#define NIOS_PKT_8x32_MAGIC         ((uint8_t) 'C')

/* Request packet indices */
#define NIOS_PKT_8x32_IDX_MAGIC      0
#define NIOS_PKT_8x32_IDX_TARGET_ID  1
#define NIOS_PKT_8x32_IDX_FLAGS      2
#define NIOS_PKT_8x32_IDX_RESV1      3
#define NIOS_PKT_8x32_IDX_ADDR       4
#define NIOS_PKT_8x32_IDX_DATA       5
#define NIOS_PKT_8x32_IDX_RESV2      9

/* Target IDs */
#define NIOS_PKT_8x32_TARGET_VERSION 0x00   /* FPGA version (read only) */
#define NIOS_PKT_8x32_TARGET_CONTROL 0x01   /* FPGA control/config register */
#define NIOS_PKT_8x32_TARGET_ADF4351 0x02   /* XB-200 ADF4351 register access
                                             * (write-only) */

/* IDs 0x80 through 0xff will not be assigned by Nuand. These are reserved
 * for user customizations */
#define NIOS_PKT_8x32_TARGET_USR1    0x80
#define NIOS_PKT_8x32_TARGET_USR128  0xff

/* Flag bits */
#define NIOS_PKT_8x32_FLAG_WRITE     (1 << 0)
#define NIOS_PKT_8x32_FLAG_SUCCESS   (1 << 1)

/* Pack the request buffer */
static inline void nios_pkt_8x32_pack(uint8_t *buf, uint8_t target, bool write,
                                      uint8_t addr, uint32_t data)
{
    buf[NIOS_PKT_8x32_IDX_MAGIC]     = NIOS_PKT_8x32_MAGIC;
    buf[NIOS_PKT_8x32_IDX_TARGET_ID] = target;

    if (write) {
        buf[NIOS_PKT_8x32_IDX_FLAGS] = NIOS_PKT_8x32_FLAG_WRITE;
    } else {
        buf[NIOS_PKT_8x32_IDX_FLAGS] = 0x00;
    }

    buf[NIOS_PKT_8x32_IDX_RESV1] = 0x00;

    buf[NIOS_PKT_8x32_IDX_ADDR] = addr;

    buf[NIOS_PKT_8x32_IDX_DATA + 0] = data & 0xff;
    buf[NIOS_PKT_8x32_IDX_DATA + 1] = (data >> 8);
    buf[NIOS_PKT_8x32_IDX_DATA + 2] = (data >> 16);
    buf[NIOS_PKT_8x32_IDX_DATA + 3] = (data >> 24);

    buf[NIOS_PKT_8x32_IDX_RESV2 + 0] = 0x00;
    buf[NIOS_PKT_8x32_IDX_RESV2 + 1] = 0x00;
    buf[NIOS_PKT_8x32_IDX_RESV2 + 2] = 0x00;
    buf[NIOS_PKT_8x32_IDX_RESV2 + 3] = 0x00;
    buf[NIOS_PKT_8x32_IDX_RESV2 + 4] = 0x00;
    buf[NIOS_PKT_8x32_IDX_RESV2 + 5] = 0x00;
    buf[NIOS_PKT_8x32_IDX_RESV2 + 6] = 0x00;
}

/* Unpack the request buffer */
static inline void nios_pkt_8x32_unpack(const uint8_t *buf, uint8_t *target,
                                        bool *write, uint8_t *addr,
                                        uint32_t *data)
{
    if (target != NULL) {
        *target = buf[NIOS_PKT_8x32_IDX_TARGET_ID];
    }

    if (write != NULL) {
        *write  = (buf[NIOS_PKT_8x32_IDX_FLAGS] & NIOS_PKT_8x32_FLAG_WRITE) != 0;
    }

    if (addr != NULL) {
        *addr   = buf[NIOS_PKT_8x32_IDX_ADDR];
    }

    if (data != NULL) {
        *data   = (buf[NIOS_PKT_8x32_IDX_DATA + 0] << 0)  |
                  (buf[NIOS_PKT_8x32_IDX_DATA + 1] << 8)  |
                  (buf[NIOS_PKT_8x32_IDX_DATA + 2] << 16) |
                  (buf[NIOS_PKT_8x32_IDX_DATA + 3] << 24);
    }
}

/* Pack the response buffer */
static inline void nios_pkt_8x32_resp_pack(uint8_t *buf, uint8_t target,
                                           bool write, uint8_t addr,
                                           uint32_t data, bool success)
{
    nios_pkt_8x32_pack(buf, target, write, addr, data);

    if (success) {
        buf[NIOS_PKT_8x32_IDX_FLAGS] |= NIOS_PKT_8x32_FLAG_SUCCESS;
    }
}

/* Unpack the response buffer */
static inline void nios_pkt_8x32_resp_unpack(const uint8_t *buf,
                                             uint8_t *target, bool *write,
                                             uint8_t *addr, uint32_t *data,
                                             bool *success)
{
    nios_pkt_8x32_unpack(buf, target, write, addr, data);

    if ((buf[NIOS_PKT_8x32_IDX_FLAGS] & NIOS_PKT_8x32_FLAG_SUCCESS) != 0) {
        *success = true;
    } else {
        *success = false;
    }
}

#endif
