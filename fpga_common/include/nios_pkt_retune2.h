/*
 * Copyright (c) 2018 Nuand LLC
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

#ifndef BLADERF_NIOS_PKT_RETUNE2_H_
#define BLADERF_NIOS_PKT_RETUNE2_H_

#ifndef BLADERF_NIOS_BUILD
#   include <libbladeRF.h>
#else
#   include "libbladeRF_nios_compat.h"
#endif

#include <stdint.h>

/* This file defines the Host <-> FPGA (NIOS II) packet formats for
 * retune2 messages. This packet is formatted, as follows. All values are
 * little-endian.
 *
 *                              Request
 *                      ----------------------
 *
 * +================+=========================================================+
 * |  Byte offset   |                       Description                       |
 * +================+=========================================================+
 * |        0       | Magic Value                                             |
 * +----------------+---------------------------------------------------------+
 * |        1       | 64-bit timestamp denoting when to retune. (Note 1)      |
 * +----------------+---------------------------------------------------------+
 * |        9       | 16-bit Nios fast lock profile number to load (Note 2)   |
 * +----------------+---------------------------------------------------------+
 * |       11       | 8-bit RFFE fast lock profile slot to use                |
 * +----------------+---------------------------------------------------------+
 * |       12       | Bit  7:     RX bit (set if this is an RX profile        |
 * |                | Bits 6:     TX output port selection                    |
 * |                | Bits [5:0]: RX input port selection                     |
 * +----------------+---------------------------------------------------------+
 * |       13       | Bits [7:6]: External TX2 SPDT switch setting            |
 * |                | Bits [5:4]: External TX1 SPDT switch setting            |
 * |                | Bits [3:2]: External RX2 SPDT switch setting            |
 * |                | Bits [1:0]: External RX1 SPDT switch setting            |
 * +----------------+---------------------------------------------------------+
 * |       14-15    | 8-bit reserved words. Should be set to 0x00.            |
 * +----------------+---------------------------------------------------------+
 *
 * (Note 1) Special Timestamp Values:
 *
 * Tune "Now":          0x0000000000000000
 * Clear Retune Queue:  0xffffffffffffffff
 *
 * When the "Clear Retune Queue" value is used, all of the other tuning
 * parameters are ignored.
 *
 * (Note 2) Packed as follows:
 *
 * +================+=======================+
 * |   Byte offset  | (MSB)   Value    (LSB)|
 * +================+=======================+
 * |       0        |  NIOS_PROFILE[7:0]    |
 * +----------------+-----------------------+
 * |       1        |  NIOS_PROFILE[15:8]   |
 * +----------------+-----------------------+
 *
 */

#define NIOS_PKT_RETUNE2_IDX_MAGIC        0
#define NIOS_PKT_RETUNE2_IDX_TIME         1
#define NIOS_PKT_RETUNE2_IDX_NIOS_PROFILE 9
#define NIOS_PKT_RETUNE2_IDX_RFFE_PROFILE 11
#define NIOS_PKT_RETUNE2_IDX_RFFE_PORT    12
#define NIOS_PKT_RETUNE2_IDX_SPDT         13
#define NIOS_PKT_RETUNE2_IDX_RESV         14

#define NIOS_PKT_RETUNE2_MAGIC            'U'

/* Specify this value instead of a timestamp to clear the retune2 queue */
#define NIOS_PKT_RETUNE2_CLEAR_QUEUE      ((uint64_t) -1)

/* Denotes that the retune2 should not be scheduled - it should occur "now" */
#define NIOS_PKT_RETUNE2_NOW              ((uint64_t) 0x00)

/* The IS_RX bit embedded in the 'port' parameter of the retune2 packet */
#define NIOS_PKT_RETUNE2_PORT_IS_RX_MASK  (0x1 << 7)

/* Pack the retune2 request buffer with the provided parameters */
static inline void nios_pkt_retune2_pack(uint8_t *buf,
                                         bladerf_module module,
                                         uint64_t timestamp,
                                         uint16_t nios_profile,
                                         uint8_t rffe_profile,
                                         uint8_t port,
                                         uint8_t spdt)
{
    uint8_t pkt_port;

    /* Clear the IS_RX bit of the port parameter */
    pkt_port = (port & (~NIOS_PKT_RETUNE2_PORT_IS_RX_MASK));

    /* Set the IS_RX bit (if needed) */
    pkt_port = (pkt_port | (BLADERF_CHANNEL_IS_TX(module) ? 0x0 :
                            NIOS_PKT_RETUNE2_PORT_IS_RX_MASK)) & 0xff;

    buf[NIOS_PKT_RETUNE2_IDX_MAGIC] = NIOS_PKT_RETUNE2_MAGIC;

    buf[NIOS_PKT_RETUNE2_IDX_TIME + 0] = (timestamp >>  0) & 0xff;
    buf[NIOS_PKT_RETUNE2_IDX_TIME + 1] = (timestamp >>  8) & 0xff;
    buf[NIOS_PKT_RETUNE2_IDX_TIME + 2] = (timestamp >> 16) & 0xff;
    buf[NIOS_PKT_RETUNE2_IDX_TIME + 3] = (timestamp >> 24) & 0xff;
    buf[NIOS_PKT_RETUNE2_IDX_TIME + 4] = (timestamp >> 32) & 0xff;
    buf[NIOS_PKT_RETUNE2_IDX_TIME + 5] = (timestamp >> 40) & 0xff;
    buf[NIOS_PKT_RETUNE2_IDX_TIME + 6] = (timestamp >> 48) & 0xff;
    buf[NIOS_PKT_RETUNE2_IDX_TIME + 7] = (timestamp >> 56) & 0xff;

    buf[NIOS_PKT_RETUNE2_IDX_NIOS_PROFILE + 0] = (nios_profile >> 0) & 0xff;
    buf[NIOS_PKT_RETUNE2_IDX_NIOS_PROFILE + 1] = (nios_profile >> 8) & 0xff;

    buf[NIOS_PKT_RETUNE2_IDX_RFFE_PROFILE] = rffe_profile & 0xff;

    buf[NIOS_PKT_RETUNE2_IDX_RFFE_PORT] = pkt_port;

    buf[NIOS_PKT_RETUNE2_IDX_SPDT] = spdt & 0xff;

    buf[NIOS_PKT_RETUNE2_IDX_RESV + 0] = 0x00;
    buf[NIOS_PKT_RETUNE2_IDX_RESV + 1] = 0x00;
}

/* Unpack a retune request */
static inline void nios_pkt_retune2_unpack(const uint8_t *buf,
                                           bladerf_module *module,
                                           uint64_t *timestamp,
                                           uint16_t *nios_profile,
                                           uint8_t *rffe_profile,
                                           uint8_t *port,
                                           uint8_t *spdt)
{
    *timestamp  = ( ((uint64_t)buf[NIOS_PKT_RETUNE2_IDX_TIME + 0]) <<  0 );
    *timestamp |= ( ((uint64_t)buf[NIOS_PKT_RETUNE2_IDX_TIME + 1]) <<  8 );
    *timestamp |= ( ((uint64_t)buf[NIOS_PKT_RETUNE2_IDX_TIME + 2]) << 16 );
    *timestamp |= ( ((uint64_t)buf[NIOS_PKT_RETUNE2_IDX_TIME + 3]) << 24 );
    *timestamp |= ( ((uint64_t)buf[NIOS_PKT_RETUNE2_IDX_TIME + 4]) << 32 );
    *timestamp |= ( ((uint64_t)buf[NIOS_PKT_RETUNE2_IDX_TIME + 5]) << 40 );
    *timestamp |= ( ((uint64_t)buf[NIOS_PKT_RETUNE2_IDX_TIME + 6]) << 48 );
    *timestamp |= ( ((uint64_t)buf[NIOS_PKT_RETUNE2_IDX_TIME + 7]) << 56 );

    *nios_profile  = ( ((uint16_t)buf[NIOS_PKT_RETUNE2_IDX_NIOS_PROFILE + 0])
                       << 0 );
    *nios_profile |= ( ((uint16_t)buf[NIOS_PKT_RETUNE2_IDX_NIOS_PROFILE + 1])
                       << 8 );

    *rffe_profile  = buf[NIOS_PKT_RETUNE2_IDX_RFFE_PROFILE];

    *port = buf[NIOS_PKT_RETUNE2_IDX_RFFE_PORT];

    *spdt = buf[NIOS_PKT_RETUNE2_IDX_SPDT];

    *module = ( (buf[NIOS_PKT_RETUNE2_IDX_RFFE_PORT] &
                 NIOS_PKT_RETUNE2_PORT_IS_RX_MASK) ? BLADERF_MODULE_RX :
                BLADERF_MODULE_TX );

}


/*
 *                             Response
 *                      ----------------------
 *
 * +================+=========================================================+
 * |  Byte offset   |                       Description                       |
 * +================+=========================================================+
 * |        0       | Magic Value                                             |
 * +----------------+---------------------------------------------------------+
 * |        1       | 64-bit duration denoting how long the operation took to |
 * |                | complete, in units of timestamp ticks. (Note 1)         |
 * +----------------+---------------------------------------------------------+
 * |        9       | Status Flags (Note 2)                                   |
 * +----------------+---------------------------------------------------------+
 * |      10-15     | Reserved. All bits set to 0.                            |
 * +----------------+---------------------------------------------------------+
 *
 * (Note 1) This value will be zero if timestamps are not running for the
 *          associated module.
 *
 * (Note 2) Description of Status Flags:
 *
 *      flags[0]: 1 = Timestamp is valid. This is only the case for "Tune NOW"
 *                    requests. It is not possible to return this information
 *                    for scheduled retunes, as the event generally does not
 *                    occur before the response is set.
 *
 *                0 = This was a scheduled retune. Timestamp fields should be
 *                    ignored.
 *
 *      flags[1]: 1 = Operation completed successfully.
 *                0 = Operation failed.
 *
 *                For "Tune NOW" requests, a failure may occur as the result
 *                of the tuning algorithm failing to occur, and such other
 *                unexpected failurs.
 *
 *                The scheduled tune request will failure if the retune queue
 *                is full.
 *
 *      flags[7:2]    Reserved. Set to 0.
 */

#define NIOS_PKT_RETUNE2_RESP_IDX_MAGIC   0
#define NIOS_PKT_RETUNE2_RESP_IDX_TIME    1
#define NIOS_PKT_RETUNE2_RESP_IDX_FLAGS   9
#define NIOS_PKT_RETUNE2_RESP_IDX_RESV    10

#define NIOS_PKT_RETUNE2_RESP_FLAG_TSVTUNE_VALID (1 << 0)
#define NIOS_PKT_RETUNE2_RESP_FLAG_SUCCESS       (1 << 1)

static inline void nios_pkt_retune2_resp_pack(uint8_t *buf,
                                              uint64_t duration,
                                              uint8_t flags)
{
    buf[NIOS_PKT_RETUNE2_RESP_IDX_MAGIC] = NIOS_PKT_RETUNE2_MAGIC;

    buf[NIOS_PKT_RETUNE2_RESP_IDX_TIME + 0] =  duration        & 0xff;
    buf[NIOS_PKT_RETUNE2_RESP_IDX_TIME + 1] = (duration >> 8)  & 0xff;
    buf[NIOS_PKT_RETUNE2_RESP_IDX_TIME + 2] = (duration >> 16) & 0xff;
    buf[NIOS_PKT_RETUNE2_RESP_IDX_TIME + 3] = (duration >> 24) & 0xff;
    buf[NIOS_PKT_RETUNE2_RESP_IDX_TIME + 4] = (duration >> 32) & 0xff;
    buf[NIOS_PKT_RETUNE2_RESP_IDX_TIME + 5] = (duration >> 40) & 0xff;
    buf[NIOS_PKT_RETUNE2_RESP_IDX_TIME + 6] = (duration >> 48) & 0xff;
    buf[NIOS_PKT_RETUNE2_RESP_IDX_TIME + 7] = (duration >> 56) & 0xff;

    buf[NIOS_PKT_RETUNE2_RESP_IDX_FLAGS] = flags;

    buf[NIOS_PKT_RETUNE2_RESP_IDX_RESV + 0] = 0x00;
    buf[NIOS_PKT_RETUNE2_RESP_IDX_RESV + 1] = 0x00;
    buf[NIOS_PKT_RETUNE2_RESP_IDX_RESV + 2] = 0x00;
    buf[NIOS_PKT_RETUNE2_RESP_IDX_RESV + 3] = 0x00;
    buf[NIOS_PKT_RETUNE2_RESP_IDX_RESV + 4] = 0x00;
    buf[NIOS_PKT_RETUNE2_RESP_IDX_RESV + 5] = 0x00;
}

static inline void nios_pkt_retune2_resp_unpack(const uint8_t *buf,
                                                uint64_t      *duration,
                                                uint8_t       *flags)
{
    *duration  = buf[NIOS_PKT_RETUNE2_RESP_IDX_TIME + 0];
    *duration |= ((uint64_t) buf[NIOS_PKT_RETUNE2_RESP_IDX_TIME + 1]) << 8;
    *duration |= ((uint64_t) buf[NIOS_PKT_RETUNE2_RESP_IDX_TIME + 2]) << 16;
    *duration |= ((uint64_t) buf[NIOS_PKT_RETUNE2_RESP_IDX_TIME + 3]) << 24;
    *duration |= ((uint64_t) buf[NIOS_PKT_RETUNE2_RESP_IDX_TIME + 4]) << 32;
    *duration |= ((uint64_t) buf[NIOS_PKT_RETUNE2_RESP_IDX_TIME + 5]) << 40;
    *duration |= ((uint64_t) buf[NIOS_PKT_RETUNE2_RESP_IDX_TIME + 6]) << 48;
    *duration |= ((uint64_t) buf[NIOS_PKT_RETUNE2_RESP_IDX_TIME + 7]) << 56;

    *flags = buf[NIOS_PKT_RETUNE2_RESP_IDX_FLAGS];
}

#endif
