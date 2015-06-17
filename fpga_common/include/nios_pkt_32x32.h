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

#ifndef BLADERF_NIOS_PKT_32x32_H_
#define BLADERF_NIOS_PKT_32x32_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/*
 * This file defines the Host <-> FPGA (NIOS II) packet formats for accesses
 * to devices/blocks with 32-bit addresses and 32-bit data
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
 * |       7:4      | 32-bit address                                           |
 * +----------------+---------------------------------------------------------+
 * |      11:8      | 32-bit data, little-endian                              |
 * +----------------+---------------------------------------------------------+
 * |      15:12     | Reserved. Set to 0.                                     |
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
 *  See the NIOS_PKT_32x32_TARGET_* values.
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

#define NIOS_PKT_32x32_MAGIC         ((uint8_t) 'K')

/* Request packet indices */
#define NIOS_PKT_32x32_IDX_MAGIC      0
#define NIOS_PKT_32x32_IDX_TARGET_ID  1
#define NIOS_PKT_32x32_IDX_FLAGS      2
#define NIOS_PKT_32x32_IDX_RESV1      3
#define NIOS_PKT_32x32_IDX_ADDR       4
#define NIOS_PKT_32x32_IDX_DATA       8
#define NIOS_PKT_32x32_IDX_RESV2      12

/* Target IDs */

/* For the EXP and EXP_DIR targets, the address is a bitmask of values
 * to read/write */
#define NIOS_PKT_32x32_TARGET_EXP     0x00 /* Expansion I/O */
#define NIOS_PKT_32x32_TARGET_EXP_DIR 0x01 /* Expansion I/O Direction reg */

/* IDs 0x80 through 0xff will not be assigned by Nuand. These are reserved
 * for user customizations */
#define NIOS_PKT_32x32_TARGET_USR1    0x80
#define NIOS_PKT_32x32_TARGET_USR128  0xff

/* Flag bits */
#define NIOS_PKT_32x32_FLAG_WRITE     (1 << 0)
#define NIOS_PKT_32x32_FLAG_SUCCESS   (1 << 1)


/* Pack the request buffer */
static inline void nios_pkt_32x32_pack(uint8_t *buf, uint8_t target, bool write,
                                       uint32_t addr, uint32_t data)
{
    buf[NIOS_PKT_32x32_IDX_MAGIC]     = NIOS_PKT_32x32_MAGIC;
    buf[NIOS_PKT_32x32_IDX_TARGET_ID] = target;

    if (write) {
        buf[NIOS_PKT_32x32_IDX_FLAGS] = NIOS_PKT_32x32_FLAG_WRITE;
    } else {
        buf[NIOS_PKT_32x32_IDX_FLAGS] = 0x00;
    }

    buf[NIOS_PKT_32x32_IDX_RESV1] = 0x00;

    buf[NIOS_PKT_32x32_IDX_ADDR + 0] = (addr >> 0);
    buf[NIOS_PKT_32x32_IDX_ADDR + 1] = (addr >> 8);
    buf[NIOS_PKT_32x32_IDX_ADDR + 2] = (addr >> 16);
    buf[NIOS_PKT_32x32_IDX_ADDR + 3] = (addr >> 24);

    buf[NIOS_PKT_32x32_IDX_DATA + 0] = (data >> 0);
    buf[NIOS_PKT_32x32_IDX_DATA + 1] = (data >> 8);
    buf[NIOS_PKT_32x32_IDX_DATA + 2] = (data >> 16);
    buf[NIOS_PKT_32x32_IDX_DATA + 3] = (data >> 24);

    buf[NIOS_PKT_32x32_IDX_RESV2 + 0] = 0x00;
    buf[NIOS_PKT_32x32_IDX_RESV2 + 1] = 0x00;
    buf[NIOS_PKT_32x32_IDX_RESV2 + 2] = 0x00;
    buf[NIOS_PKT_32x32_IDX_RESV2 + 3] = 0x00;
}

/* Unpack the request buffer */
static inline void nios_pkt_32x32_unpack(const uint8_t *buf, uint8_t *target,
                                        bool *write, uint32_t *addr,
                                        uint32_t *data)
{
    if (target != NULL) {
        *target = buf[NIOS_PKT_32x32_IDX_TARGET_ID];
    }

    if (write != NULL) {
        *write  = (buf[NIOS_PKT_32x32_IDX_FLAGS] & NIOS_PKT_32x32_FLAG_WRITE) != 0;
    }

    if (addr != NULL) {
        *addr   = (buf[NIOS_PKT_32x32_IDX_ADDR + 0] << 0)  |
                  (buf[NIOS_PKT_32x32_IDX_ADDR + 1] << 8)  |
                  (buf[NIOS_PKT_32x32_IDX_ADDR + 2] << 16) |
                  (buf[NIOS_PKT_32x32_IDX_ADDR + 3] << 24);
    }


    if (data != NULL) {
        *data   = (buf[NIOS_PKT_32x32_IDX_DATA + 0] << 0)  |
                  (buf[NIOS_PKT_32x32_IDX_DATA + 1] << 8)  |
                  (buf[NIOS_PKT_32x32_IDX_DATA + 2] << 16) |
                  (buf[NIOS_PKT_32x32_IDX_DATA + 3] << 24);
    }
}

/* Pack the response buffer */
static inline void nios_pkt_32x32_resp_pack(uint8_t *buf, uint8_t target,
                                            bool write, uint32_t addr,
                                            uint32_t data, bool success)
{
    nios_pkt_32x32_pack(buf, target, write, addr, data);

    if (success) {
        buf[NIOS_PKT_32x32_IDX_FLAGS] |= NIOS_PKT_32x32_FLAG_SUCCESS;
    }
}

/* Unpack the response buffer */
static inline void nios_pkt_32x32_resp_unpack(const uint8_t *buf,
                                             uint8_t *target, bool *write,
                                             uint32_t *addr, uint32_t *data,
                                             bool *success)
{
    nios_pkt_32x32_unpack(buf, target, write, addr, data);

    if ((buf[NIOS_PKT_32x32_IDX_FLAGS] & NIOS_PKT_32x32_FLAG_SUCCESS) != 0) {
        *success = true;
    } else {
        *success = false;
    }
}

#endif
