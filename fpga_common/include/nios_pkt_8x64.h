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

#ifndef BLADERF_NIOS_PKT_8x64_H_
#define BLADERF_NIOS_PKT_8x64_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/*
 * This file defines the Host <-> FPGA (NIOS II) packet formats for accesses
 * to devices/blocks with 8-bit addresses and 64-bit data
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
 * |      12:5      | 64-bit data, little-endian                              |
 * +----------------+---------------------------------------------------------+
 * |      15:13     | Reserved. Set to 0.                                     |
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
 *  See the NIOS_PKT_8x64_TARGET_* values.
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

#define NIOS_PKT_8x64_MAGIC         ((uint8_t) 'D')

/* Request packet indices */
#define NIOS_PKT_8x64_IDX_MAGIC      0
#define NIOS_PKT_8x64_IDX_TARGET_ID  1
#define NIOS_PKT_8x64_IDX_FLAGS      2
#define NIOS_PKT_8x64_IDX_RESV1      3
#define NIOS_PKT_8x64_IDX_ADDR       4
#define NIOS_PKT_8x64_IDX_DATA       5
#define NIOS_PKT_8x64_IDX_RESV2      13

/* Target IDs */

#define NIOS_PKT_8x64_TARGET_TIMESTAMP 0x00 /* Timestamp readback (read only) */

/* IDs 0x80 through 0xff will not be assigned by Nuand. These are reserved
 * for user customizations */
#define NIOS_PKT_8x64_TARGET_USR1    0x80
#define NIOS_PKT_8x64_TARGET_USR128  0xff

/* Flag bits */
#define NIOS_PKT_8x64_FLAG_WRITE     (1 << 0)
#define NIOS_PKT_8x64_FLAG_SUCCESS   (1 << 1)

/* Sub-addresses for timestamp target */
#define NIOS_PKT_8x64_TIMESTAMP_RX  0x00
#define NIOS_PKT_8x64_TIMESTAMP_TX  0x01

/* Pack the request buffer */
static inline void nios_pkt_8x64_pack(uint8_t *buf, uint8_t target, bool write,
                                      uint8_t addr, uint64_t data)
{
    buf[NIOS_PKT_8x64_IDX_MAGIC]     = NIOS_PKT_8x64_MAGIC;
    buf[NIOS_PKT_8x64_IDX_TARGET_ID] = target;

    if (write) {
        buf[NIOS_PKT_8x64_IDX_FLAGS] = NIOS_PKT_8x64_FLAG_WRITE;
    } else {
        buf[NIOS_PKT_8x64_IDX_FLAGS] = 0x00;
    }

    buf[NIOS_PKT_8x64_IDX_RESV1] = 0x00;

    buf[NIOS_PKT_8x64_IDX_ADDR] = addr;

    buf[NIOS_PKT_8x64_IDX_DATA + 0] = (data >> 0)  & 0xff;
    buf[NIOS_PKT_8x64_IDX_DATA + 1] = (data >> 8)  & 0xff;
    buf[NIOS_PKT_8x64_IDX_DATA + 2] = (data >> 16) & 0xff;
    buf[NIOS_PKT_8x64_IDX_DATA + 3] = (data >> 24) & 0xff;
    buf[NIOS_PKT_8x64_IDX_DATA + 4] = (data >> 32) & 0xff;
    buf[NIOS_PKT_8x64_IDX_DATA + 5] = (data >> 40) & 0xff;
    buf[NIOS_PKT_8x64_IDX_DATA + 6] = (data >> 48) & 0xff;
    buf[NIOS_PKT_8x64_IDX_DATA + 7] = (data >> 56) & 0xff;

    buf[NIOS_PKT_8x64_IDX_RESV2 + 0] = 0x00;
    buf[NIOS_PKT_8x64_IDX_RESV2 + 1] = 0x00;
    buf[NIOS_PKT_8x64_IDX_RESV2 + 2] = 0x00;
}

/* Unpack the request buffer */
static inline void nios_pkt_8x64_unpack(const uint8_t *buf, uint8_t *target,
                                        bool *write, uint8_t *addr,
                                        uint64_t *data)
{
    if (target != NULL) {
        *target = buf[NIOS_PKT_8x64_IDX_TARGET_ID];
    }

    if (write != NULL) {
        *write  = (buf[NIOS_PKT_8x64_IDX_FLAGS] & NIOS_PKT_8x64_FLAG_WRITE) != 0;
    }

    if (addr != NULL) {
        *addr   = buf[NIOS_PKT_8x64_IDX_ADDR];
    }

    if (data != NULL) {
        *data   = ((uint64_t) buf[NIOS_PKT_8x64_IDX_DATA + 0] << 0)  |
                  ((uint64_t) buf[NIOS_PKT_8x64_IDX_DATA + 1] << 8)  |
                  ((uint64_t) buf[NIOS_PKT_8x64_IDX_DATA + 2] << 16) |
                  ((uint64_t) buf[NIOS_PKT_8x64_IDX_DATA + 3] << 24) |
                  ((uint64_t) buf[NIOS_PKT_8x64_IDX_DATA + 4] << 32) |
                  ((uint64_t) buf[NIOS_PKT_8x64_IDX_DATA + 5] << 40) |
                  ((uint64_t) buf[NIOS_PKT_8x64_IDX_DATA + 6] << 48) |
                  ((uint64_t) buf[NIOS_PKT_8x64_IDX_DATA + 7] << 56);
    }
}

/* Pack the response buffer */
static inline void nios_pkt_8x64_resp_pack(uint8_t *buf, uint8_t target,
                                           bool write, uint8_t addr,
                                           uint64_t data, bool success)
{
    nios_pkt_8x64_pack(buf, target, write, addr, data);

    if (success) {
        buf[NIOS_PKT_8x64_IDX_FLAGS] |= NIOS_PKT_8x64_FLAG_SUCCESS;
    }
}

/* Unpack the response buffer */
static inline void nios_pkt_8x64_resp_unpack(const uint8_t *buf,
                                             uint8_t *target, bool *write,
                                             uint8_t *addr, uint64_t *data,
                                             bool *success)
{
    nios_pkt_8x64_unpack(buf, target, write, addr, data);

    if ((buf[NIOS_PKT_8x64_IDX_FLAGS] & NIOS_PKT_8x64_FLAG_SUCCESS) != 0) {
        *success = true;
    } else {
        *success = false;
    }
}

#endif
