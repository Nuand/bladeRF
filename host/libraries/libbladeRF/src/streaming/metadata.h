/*
 * Copyright (C) 2014 Nuand LLC
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef STREAMING_METADATA_H_
#define STREAMING_METADATA_H_

/*
 *  Metadata layout
 * ~~~~~~~~~~~~~~~~~~~~~~~
 *
 * The FPGA handles data in units of "messages."  These messages are
 * 2048 or 8192 bytes for USB 2.0 (Hi-Speed) or USB 3.0 (SuperSpeed),
 * respectively.
 *
 * The first 16 bytes of the message form a header, which includes metadata
 * for the samples within the message. This header is shown below:
 *
 *       +-----------------+
 *  0x00 |  Packet length  |    2 bytes, Little-endian uint16_t
 *       +-----------------+
 *  0x02 |   Packet flags  |    1 byte
 *       +-----------------+
 *  0x03 |  Packet core ID |    1 byte
 *       +-----------------+
 *  0x04 |    Timestamp    |    8 bytes, Little-endian uint64_t
 *       +-----------------+
 *  0x0c |      Flags      |    4 bytes, Little-endian uint32_t
 *       +-----------------+
 *
 * The first 4 bytes are only packet length/flags/core ID in
 * BLADERF_FORMAT_PACKET_META. In BLADERF_FORMAT_SC16_Q11_META they are a
 * reserved word that the host historically ignored (the FPGA filled it with the
 * constant 0x12344321). FPGA v0.17.0 and later instead put the RFIC gain tag
 * there, so a receive can be paired with the gain the RFIC's AGC had applied:
 *
 *      31:16  magic 0x9361 (distinguishes this from the old 0x12344321)
 *      15:11  reserved, zero
 *         10  CTRL_OUT snapshot passed the FPGA's stability filter
 *          9  CTRL_OUT changed during the *previous* message (see below)
 *          8  gain lock (copy of bit 7)
 *        7:0  raw CTRL_OUT snapshot. When RFIC register 0x035 is 0x16, bits
 *             [6:0] are the RX1 full gain-table index and bit 7 is gain lock.
 *
 * The "changed" bit necessarily lags by one message: the header is emitted
 * before its own samples, so the FPGA cannot yet know whether the gain will
 * move partway through them. It reports the flag in the following header
 * instead, meaning a set bit in header N marks the samples of message N-1 as
 * spanning a gain change.
 *
 * The term "buffer" is used to describe a block of of data received from or
 * sent to the device. The size of a "buffer" (in bytes) is always a multiple
 * of the size of a "message." Said another way, a buffer will always evenly
 * divide into multiple messages.  Messages are *not* fragmented across
 * consecutive buffers.
 *
 *       +-----------------+ <-.  <-.
 *       | header          |   |    |
 *       +-----------------+   |    |
 *       |                 |   |    |
 *       | samples         |   |    |
 *       |                 |   |    |
 *       +-----------------+   |  <-+---- message
 *       | header          |   |
 *       +-----------------+   |
 *       |                 |   |
 *       | samples         |   |
 *       |                 |   |
 *       +-----------------+   |
 *       | header          |   |
 *       +-----------------+   |
 *       |                 |   |
 *       | samples         |   |
 *       |                 |   |
 *       +-----------------+   |
 *       | header          |   |
 *       +-----------------+   |
 *       |                 |   |
 *       | samples         |   |
 *       |                 |   |
 *       +-----------------+ <-+---------- buffer
 *
 *
 * When intentionally transmitting discontinuous groups of samples (such
 * as bursts), it is important that the last two samples within a message
 * be (0 + 0j). Otherwise, the DAC will not properly hold its output
 * at (0 + 0j) for the duration of the discontinuity.
 */

/* Components of the metadata header */
#define METADATA_RESV_SIZE (sizeof(uint32_t))
#define METADATA_TIMESTAMP_SIZE (sizeof(uint64_t))
#define METADATA_FLAGS_SIZE (sizeof(uint32_t))
#define METADATA_PACKET_LEN_SIZE (sizeof(uint16_t))
#define METADATA_PACKET_CORE_SIZE (sizeof(uint8_t))
#define METADATA_PACKET_FLAGS_SIZE (sizeof(uint8_t))

#define METADATA_RESV_OFFSET 0
#define METADATA_PACKET_LEN_OFFSET 0
#define METADATA_PACKET_FLAGS_OFFSET 2
#define METADATA_PACKET_CORE_OFFSET 3
#define METADATA_TIMESTAMP_OFFSET (METADATA_RESV_SIZE)
#define METADATA_FLAGS_OFFSET \
    (METADATA_TIMESTAMP_OFFSET + METADATA_TIMESTAMP_SIZE)

#define METADATA_HEADER_SIZE (METADATA_FLAGS_OFFSET + METADATA_FLAGS_SIZE)

/* RFIC gain tag, carried in the reserved word in SC16_Q11_META mode */
#define METADATA_GAIN_TAG_MAGIC 0x9361
#define METADATA_GAIN_TAG_MAGIC_SHIFT 16
#define METADATA_GAIN_TAG_STABLE (1 << 10)
#define METADATA_GAIN_TAG_CHANGED (1 << 9)
#define METADATA_GAIN_TAG_CTRL_OUT_MASK 0xff

/* Decoded form of the gain tag. Fields are only meaningful when
 * metadata_get_gain_tag() returned true. */
struct metadata_gain_tag {
    uint8_t ctrl_out; /**< Raw CTRL_OUT byte */
    bool stable;      /**< Snapshot passed the FPGA's stability filter */
    bool changed;     /**< CTRL_OUT moved during the previous message */
};

static inline uint64_t metadata_get_timestamp(const uint8_t *header)
{
    uint64_t ret;
    assert(sizeof(ret) == METADATA_TIMESTAMP_SIZE);
    memcpy(&ret, &header[METADATA_TIMESTAMP_OFFSET], METADATA_TIMESTAMP_SIZE);

    ret = LE64_TO_HOST(ret);

    return ret;
}

static inline uint32_t metadata_get_flags(const uint8_t *header)
{
    uint32_t ret;
    assert(sizeof(ret) == METADATA_FLAGS_SIZE);
    memcpy(&ret, &header[METADATA_FLAGS_OFFSET], METADATA_FLAGS_SIZE);
    return LE32_TO_HOST(ret);
}

static inline uint16_t metadata_get_packet_len(const uint8_t *header)
{
    uint16_t ret;
    assert(sizeof(ret) == METADATA_PACKET_LEN_SIZE);
    memcpy(&ret, &header[METADATA_PACKET_LEN_OFFSET], METADATA_PACKET_LEN_SIZE);
    return LE16_TO_HOST(ret);
}

static inline uint8_t metadata_get_packet_core(const uint8_t *header)
{
    uint8_t ret;
    assert(sizeof(ret) == METADATA_PACKET_CORE_SIZE);
    memcpy(&ret, &header[METADATA_PACKET_CORE_OFFSET], METADATA_PACKET_CORE_SIZE);
    return ret;
}

static inline uint8_t metadata_get_packet_flags(const uint8_t *header)
{
    uint8_t ret;
    assert(sizeof(ret) == METADATA_PACKET_FLAGS_SIZE);
    memcpy(&ret, &header[METADATA_PACKET_FLAGS_OFFSET], METADATA_PACKET_FLAGS_SIZE);
    return ret;
}

/* Decode the RFIC gain tag from an RX message header.
 *
 * Returns true and fills in *tag if this header carries a gain tag. Returns
 * false for FPGA images that predate it (which leave the constant 0x12344321
 * in the reserved word), leaving *tag untouched.
 *
 * Only valid in SC16_Q11_META mode; in PACKET_META mode those bytes are the
 * packet length/flags/core ID instead.
 */
static inline bool metadata_get_gain_tag(const uint8_t *header,
                                         struct metadata_gain_tag *tag)
{
    uint32_t word;
    assert(sizeof(word) == METADATA_RESV_SIZE);
    memcpy(&word, &header[METADATA_RESV_OFFSET], METADATA_RESV_SIZE);
    word = LE32_TO_HOST(word);

    if ((word >> METADATA_GAIN_TAG_MAGIC_SHIFT) != METADATA_GAIN_TAG_MAGIC) {
        return false;
    }

    tag->ctrl_out = (uint8_t)(word & METADATA_GAIN_TAG_CTRL_OUT_MASK);
    tag->stable   = (word & METADATA_GAIN_TAG_STABLE) != 0;
    tag->changed  = (word & METADATA_GAIN_TAG_CHANGED) != 0;

    return true;
}

static inline void metadata_set_packet(uint8_t *header,
                                uint64_t timestamp,
                                uint32_t flags,
                                uint16_t length,
                                uint8_t core,
                                uint8_t pkt_flags)
{
    timestamp = HOST_TO_LE64(timestamp);

    flags = HOST_TO_LE32(flags);

    length = HOST_TO_LE16(length);

    assert(sizeof(timestamp) == METADATA_TIMESTAMP_SIZE);
    assert(sizeof(flags) == METADATA_FLAGS_SIZE);

    memset(&header[METADATA_RESV_OFFSET], 0, METADATA_RESV_SIZE);

    memcpy(&header[METADATA_PACKET_LEN_OFFSET],   &length,    METADATA_PACKET_LEN_SIZE);
    memcpy(&header[METADATA_PACKET_CORE_OFFSET],  &core,      METADATA_PACKET_CORE_SIZE);
    memcpy(&header[METADATA_PACKET_FLAGS_OFFSET], &pkt_flags, METADATA_PACKET_FLAGS_SIZE);

    memcpy(&header[METADATA_TIMESTAMP_OFFSET], &timestamp,
           METADATA_TIMESTAMP_SIZE);

    memcpy(&header[METADATA_FLAGS_OFFSET], &flags, METADATA_FLAGS_SIZE);
}

static inline void metadata_set(uint8_t *header,
                                uint64_t timestamp,
                                uint32_t flags)
{
    metadata_set_packet(header, timestamp, flags, 0, 0, 0);
}

#endif
