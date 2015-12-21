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

#ifndef BLADERF_NIOS_PKT_RETUNE_H_
#define BLADERF_NIOS_PKT_RETUNE_H_

#ifndef BLADERF_NIOS_BUILD
#   include <libbladeRF.h>
#else
#   include "libbladeRF_nios_compat.h"
#endif

#include <stdint.h>

/* Specify this value instead of a timestamp to clear the retune queue */
#define NIOS_PKT_RETUNE_CLEAR_QUEUE ((uint64_t) -1)

/* This file defines the Host <-> FPGA (NIOS II) packet formats for
 * retune messages. This packet is formatted, as follows. All values are
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
 * |        9       | 32-bit LMS6002D n_int & n_frac register values (Note 2) |
 * +----------------+---------------------------------------------------------+
 * |       13       | RX/TX bit, FREQSEL LMS6002D reg value  (Note 3)         |
 * +----------------+---------------------------------------------------------+
 * |       14       | Bit 7:        Band-selection (Note 4)                   |
 * |                | Bit 6:        1=Quick tune, 0=Normal tune               |
 * |                | Bits [5:0]    VCOCAP[5:0] Hint                          |
 * +----------------+---------------------------------------------------------+
 * |       15       | 8-bit reserved word. Should be set to 0x00.             |
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
 * |       0        |        NINT[8:1]      |
 * +----------------+-----------------------+
 * |       1        | NINT[0], NFRAC[22:16] |
 * +----------------+-----------------------+
 * |       2        |       NFRAC[15:8]     |
 * +----------------+-----------------------+
 * |       3        |       NFRAC[7:0]      |
 * +----------------+-----------------------+
 *
 * (Note 3) Packed as follows:
 *
 * +================+=======================+
 * |      Bit(s)    |         Value         |
 * +================+=======================+
 * |        7       |          TX           |
 * +----------------+-----------------------+
 * |        6       |          RX           |
 * +----------------+-----------------------+
 * |      [5:0]     |        FREQSEL        |
 * +----------------+-----------------------+
 *
 * (Notes 4) Band-selection bit = 1 implies "Low band". 0 = "High band"
 */

#define NIOS_PKT_RETUNE_IDX_MAGIC    0
#define NIOS_PKT_RETUNE_IDX_TIME     1
#define NIOS_PKT_RETUNE_IDX_INTFRAC  9
#define NIOS_PKT_RETUNE_IDX_FREQSEL  13
#define NIOS_PKT_RETUNE_IDX_BANDSEL  14
#define NIOS_PKT_RETUNE_IDX_RESV     15

#define NIOS_PKT_RETUNE_MAGIC        'T'


#define FLAG_QUICK_TUNE (1 << 6)
#define FLAG_RX (1 << 6)
#define FLAG_TX (1 << 7)
#define FLAG_LOW_BAND (1 << 7)


/* Denotes no tune word is supplied. */
#define NIOS_PKT_RETUNE_NO_HINT      0xff

/* Denotes that the retune should not be scheduled - it should occur "now" */
#define NIOS_PKT_RETUNE_NOW          ((uint64_t) 0x00)

#define PACK_TXRX_FREQSEL(module_, freqsel_) \
    (freqsel_ & 0x3f)

/* Pack the retune request buffer with the provided parameters */
static inline void nios_pkt_retune_pack(uint8_t *buf,
                                        bladerf_module module,
                                        uint64_t timestamp,
                                        uint16_t nint,
                                        uint32_t nfrac,
                                        uint8_t  freqsel,
                                        uint8_t  vcocap,
                                        bool     low_band,
                                        bool     quick_tune)
{
    buf[NIOS_PKT_RETUNE_IDX_MAGIC] = NIOS_PKT_RETUNE_MAGIC;

    buf[NIOS_PKT_RETUNE_IDX_TIME + 0] =  timestamp        & 0xff;
    buf[NIOS_PKT_RETUNE_IDX_TIME + 1] = (timestamp >>  8) & 0xff;
    buf[NIOS_PKT_RETUNE_IDX_TIME + 2] = (timestamp >> 16) & 0xff;
    buf[NIOS_PKT_RETUNE_IDX_TIME + 3] = (timestamp >> 24) & 0xff;
    buf[NIOS_PKT_RETUNE_IDX_TIME + 4] = (timestamp >> 32) & 0xff;
    buf[NIOS_PKT_RETUNE_IDX_TIME + 5] = (timestamp >> 40) & 0xff;
    buf[NIOS_PKT_RETUNE_IDX_TIME + 6] = (timestamp >> 48) & 0xff;
    buf[NIOS_PKT_RETUNE_IDX_TIME + 7] = (timestamp >> 56) & 0xff;

    buf[NIOS_PKT_RETUNE_IDX_INTFRAC + 0]  = (nint >> 1) & 0xff;
    buf[NIOS_PKT_RETUNE_IDX_INTFRAC + 1]  = (nint & 0x1) << 7;
    buf[NIOS_PKT_RETUNE_IDX_INTFRAC + 1] |= ((nfrac >> 16) & 0x7f);
    buf[NIOS_PKT_RETUNE_IDX_INTFRAC + 2]  = (nfrac >> 8) & 0xff;
    buf[NIOS_PKT_RETUNE_IDX_INTFRAC + 3]  = nfrac & 0xff;

    buf[NIOS_PKT_RETUNE_IDX_FREQSEL] = freqsel & 0xff;

    switch (module) {
        case BLADERF_MODULE_TX:
            buf[NIOS_PKT_RETUNE_IDX_FREQSEL] |= FLAG_TX;
            break;

        case BLADERF_MODULE_RX:
            buf[NIOS_PKT_RETUNE_IDX_FREQSEL] |= FLAG_RX;
            break;

        default:
            /* Erroneous case - should not occur */
            break;
    }

    if (low_band) {
        buf[NIOS_PKT_RETUNE_IDX_BANDSEL] = FLAG_LOW_BAND;
    } else {
        buf[NIOS_PKT_RETUNE_IDX_BANDSEL] = 0x00;
    }

    if (quick_tune) {
        buf[NIOS_PKT_RETUNE_IDX_BANDSEL] |= FLAG_QUICK_TUNE;
    }

    buf[NIOS_PKT_RETUNE_IDX_BANDSEL] |= vcocap;

    buf[NIOS_PKT_RETUNE_IDX_RESV]     = 0x00;
}

/* Unpack a retune request */
static inline void nios_pkt_retune_unpack(const uint8_t *buf,
                                          bladerf_module *module,
                                          uint64_t *timestamp,
                                          uint16_t *nint,
                                          uint32_t *nfrac,
                                          uint8_t  *freqsel,
                                          uint8_t  *vcocap,
                                          bool     *low_band,
                                          bool     *quick_tune)
{
    *timestamp  = ( ((uint64_t) buf[NIOS_PKT_RETUNE_IDX_TIME + 0]) <<  0);
    *timestamp |= ( ((uint64_t) buf[NIOS_PKT_RETUNE_IDX_TIME + 1]) <<  8);
    *timestamp |= ( ((uint64_t) buf[NIOS_PKT_RETUNE_IDX_TIME + 2]) << 16);
    *timestamp |= ( ((uint64_t) buf[NIOS_PKT_RETUNE_IDX_TIME + 3]) << 24);
    *timestamp |= ( ((uint64_t) buf[NIOS_PKT_RETUNE_IDX_TIME + 4]) << 32);
    *timestamp |= ( ((uint64_t) buf[NIOS_PKT_RETUNE_IDX_TIME + 5]) << 40);
    *timestamp |= ( ((uint64_t) buf[NIOS_PKT_RETUNE_IDX_TIME + 6]) << 48);
    *timestamp |= ( ((uint64_t) buf[NIOS_PKT_RETUNE_IDX_TIME + 7]) << 56);

    *nint  = buf[NIOS_PKT_RETUNE_IDX_INTFRAC + 0] << 1;
    *nint |= buf[NIOS_PKT_RETUNE_IDX_INTFRAC + 1] >> 7;

    *nfrac  = (buf[NIOS_PKT_RETUNE_IDX_INTFRAC + 1] & 0x7f) << 16;
    *nfrac |= buf[NIOS_PKT_RETUNE_IDX_INTFRAC + 2] << 8;
    *nfrac |= buf[NIOS_PKT_RETUNE_IDX_INTFRAC + 3];

    *freqsel = buf[NIOS_PKT_RETUNE_IDX_FREQSEL] & 0x3f;

    *module = -1;

    if (buf[NIOS_PKT_RETUNE_IDX_FREQSEL] & FLAG_TX) {
        *module = BLADERF_MODULE_TX;
    } else if (buf[NIOS_PKT_RETUNE_IDX_FREQSEL] & FLAG_RX) {
        *module = BLADERF_MODULE_RX;
    }

    *low_band = (buf[NIOS_PKT_RETUNE_IDX_BANDSEL] & FLAG_LOW_BAND) != 0;
    *quick_tune = (buf[NIOS_PKT_RETUNE_IDX_BANDSEL] & FLAG_QUICK_TUNE) != 0;
    *vcocap = buf[NIOS_PKT_RETUNE_IDX_BANDSEL] & 0x3f;
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
 * |        9       | Bits [7:6]    Reserved, set to 0.                       |
 * |                | Bits [5:0]    VCOCAP value used. (Note 2)               |
 * +----------------+---------------------------------------------------------+
 * |       10       | Status Flags (Note 3)                                   |
 * +----------------+---------------------------------------------------------+
 * |      11-15     | Reserved. All bits set to 0.                            |
 * +----------------+---------------------------------------------------------+
 *
 * (Note 1) This value will be zero if timestamps are not running for the
 *          associated module.
 *
 * (Note 2) This field's value should be ignored when reading a response for
 *          a request to clear the retune queue.
 *
 * (Note 3) Description of Status Flags:
 *
 *      flags[0]: 1 = Timestamp and VCOCAP are valid. This is only the case for
 *                    "Tune NOW" requests. It is not possible to return this
 *                    information for scheduled retunes, as the event generally
 *                    does not occur before the response is set.
 *
 *                0 = This was a scheduled retune. Timestamp and VCOCAP Fields
 *                    should be ignored.
 *
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

#define NIOS_PKT_RETUNERESP_IDX_MAGIC   0
#define NIOS_PKT_RETUNERESP_IDX_TIME    1
#define NIOS_PKT_RETUNERESP_IDX_VCOCAP  9
#define NIOS_PKT_RETUNERESP_IDX_FLAGS   10
#define NIOS_PKT_RETUNERESP_IDX_RESV    11

#define NIOS_PKT_RETUNERESP_FLAG_TSVTUNE_VALID (1 << 0)
#define NIOS_PKT_RETUNERESP_FLAG_SUCCESS       (1 << 1)

static inline void nios_pkt_retune_resp_pack(uint8_t *buf,
                                             uint64_t duration,
                                             uint8_t vcocap,
                                             uint8_t flags)
{
    buf[NIOS_PKT_RETUNERESP_IDX_MAGIC] = NIOS_PKT_RETUNE_MAGIC;

    buf[NIOS_PKT_RETUNERESP_IDX_TIME + 0] =  duration        & 0xff;
    buf[NIOS_PKT_RETUNERESP_IDX_TIME + 1] = (duration >> 8)  & 0xff;
    buf[NIOS_PKT_RETUNERESP_IDX_TIME + 2] = (duration >> 16) & 0xff;
    buf[NIOS_PKT_RETUNERESP_IDX_TIME + 3] = (duration >> 24) & 0xff;
    buf[NIOS_PKT_RETUNERESP_IDX_TIME + 4] = (duration >> 32) & 0xff;
    buf[NIOS_PKT_RETUNERESP_IDX_TIME + 5] = (duration >> 40) & 0xff;
    buf[NIOS_PKT_RETUNERESP_IDX_TIME + 6] = (duration >> 48) & 0xff;
    buf[NIOS_PKT_RETUNERESP_IDX_TIME + 7] = (duration >> 56) & 0xff;

    buf[NIOS_PKT_RETUNERESP_IDX_VCOCAP] = vcocap;

    buf[NIOS_PKT_RETUNERESP_IDX_FLAGS] = flags;

    buf[NIOS_PKT_RETUNERESP_IDX_RESV + 0] = 0x00;
    buf[NIOS_PKT_RETUNERESP_IDX_RESV + 1] = 0x00;
    buf[NIOS_PKT_RETUNERESP_IDX_RESV + 2] = 0x00;
    buf[NIOS_PKT_RETUNERESP_IDX_RESV + 3] = 0x00;
    buf[NIOS_PKT_RETUNERESP_IDX_RESV + 4] = 0x00;
}

static inline void nios_pkt_retune_resp_unpack(const uint8_t *buf,
                                               uint64_t      *duration,
                                               uint8_t       *vcocap,
                                               uint8_t       *flags)
{
    *duration  = buf[NIOS_PKT_RETUNERESP_IDX_TIME + 0];
    *duration |= ((uint64_t) buf[NIOS_PKT_RETUNERESP_IDX_TIME + 1]) << 8;
    *duration |= ((uint64_t) buf[NIOS_PKT_RETUNERESP_IDX_TIME + 2]) << 16;
    *duration |= ((uint64_t) buf[NIOS_PKT_RETUNERESP_IDX_TIME + 3]) << 24;
    *duration |= ((uint64_t) buf[NIOS_PKT_RETUNERESP_IDX_TIME + 4]) << 32;
    *duration |= ((uint64_t) buf[NIOS_PKT_RETUNERESP_IDX_TIME + 5]) << 40;
    *duration |= ((uint64_t) buf[NIOS_PKT_RETUNERESP_IDX_TIME + 6]) << 48;
    *duration |= ((uint64_t) buf[NIOS_PKT_RETUNERESP_IDX_TIME + 7]) << 56;

    *vcocap = buf[NIOS_PKT_RETUNERESP_IDX_VCOCAP];

    *flags = buf[NIOS_PKT_RETUNERESP_IDX_FLAGS];
}

#endif
