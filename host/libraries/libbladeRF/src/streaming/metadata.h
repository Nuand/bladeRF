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
