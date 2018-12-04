/* This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
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

#ifndef BLADERF_NIOS_DEVICES_RFIC_H_
#define BLADERF_NIOS_DEVICES_RFIC_H_

#ifdef BOARD_BLADERF_MICRO

#include <stdbool.h>
#include <stdint.h>

#include "bladerf2_common.h"

#define COMMAND_QUEUE_MAX 16
#define COMMAND_QUEUE_FULL 0xff
#define COMMAND_QUEUE_EMPTY 0xfe

/* Packet decoding debugging output */
static char const RFIC_DBG_FMT[] = "%s: cmd=%s ch=%s state=%s [%x]=%x(-%x)\n";
#define RFIC_DBG(prefix, cmd, ch, state, addr, data) \
    DBG(RFIC_DBG_FMT, prefix, cmd, ch, state, addr, data, -data)

/* Error message output */
static char const RFIC_ERR_FMT[] = "%s: error: %s returned %x (-%x)\n";
#define RFIC_ERR(thing, retval) \
    DBG(RFIC_ERR_FMT, __FUNCTION__, thing, retval, -retval)

/* State of items in the command queue */
enum rfic_entry_state {
    ENTRY_STATE_INVALID = 0, /* Marks entry invalid and not in use */
    ENTRY_STATE_NEW,         /* We have a new command request to satisfy */
    ENTRY_STATE_RUNNING,     /* Currently executing a job */
    ENTRY_STATE_COMPLETE,    /* The job is complete */
};

/* An entry in the command queue */
struct rfic_queue_entry {
    volatile enum rfic_entry_state state; /* State of this queue entry */
    uint8_t rv;                           /* Return value */
    uint8_t ch;                           /* Channel */
    uint8_t cmd;                          /* Command */
    uint64_t value;                       /* Value to pass to command */
};

/* A queue itself */
struct rfic_queue {
    uint8_t count;   /* Total number of items in the queue */
    uint8_t ins_idx; /* Insertion index */
    uint8_t rem_idx; /* Removal index */
    uint8_t last_rv; /* Returned value from executing last command */

    struct rfic_queue_entry entries[COMMAND_QUEUE_MAX];
};

struct rfic_state {
    /* AD9361 state structure */
    struct ad9361_rf_phy *phy;

    /* Queue for handling long-running write operations */
    struct rfic_queue write_queue;

    /* RX and TX FIR filter memory */
    bladerf_rfic_rxfir rxfir;
    bladerf_rfic_txfir txfir;
};

uint8_t rfic_enqueue(struct rfic_queue *q,
                     uint8_t ch,
                     uint8_t cmd,
                     uint64_t value);

uint8_t rfic_dequeue(struct rfic_queue *q, struct rfic_queue_entry *e);

struct rfic_queue_entry *rfic_queue_peek(struct rfic_queue *q);

void rfic_queue_reset(struct rfic_queue *q);

static inline bool _rfic_chan_is_system(uint8_t ch)
{
    return 0xF == ch;
}

static inline uint8_t _rfic_unpack_chan(uint16_t addr)
{
    return (addr >> 8) & 0xF;
}

static inline uint8_t _rfic_unpack_cmd(uint16_t addr)
{
    return addr & 0xF;
}

static inline char const *_rfic_cmdstr(uint8_t cmd)
{
    switch (cmd) {
        case BLADERF_RFIC_COMMAND_STATUS:
            return "STATUS  ";

        case BLADERF_RFIC_COMMAND_INIT:
            return "INIT    ";

        case BLADERF_RFIC_COMMAND_ENABLE:
            return "ENABLE  ";

        case BLADERF_RFIC_COMMAND_SAMPLERATE:
            return "SAMPRATE";

        case BLADERF_RFIC_COMMAND_FREQUENCY:
            return "FREQ    ";

        case BLADERF_RFIC_COMMAND_BANDWIDTH:
            return "BNDWDTH ";

        case BLADERF_RFIC_COMMAND_GAINMODE:
            return "GAINMD  ";

        case BLADERF_RFIC_COMMAND_GAIN:
            return "GAIN    ";

        case BLADERF_RFIC_COMMAND_RSSI:
            return "RSSI    ";

        case BLADERF_RFIC_COMMAND_FILTER:
            return "FILTER  ";

        case BLADERF_RFIC_COMMAND_TXMUTE:
            return "TXMUTE  ";

        case BLADERF_RFIC_COMMAND_FASTLOCK:
            return "FASTLOCK";

        default:
            return "        ";
    }
}

static inline char const *_rfic_chanstr(uint8_t ch)
{
    switch (ch) {
        case BLADERF_CHANNEL_TX(0):
            return "TX1";
        case BLADERF_CHANNEL_TX(1):
            return "TX2";
        case BLADERF_CHANNEL_RX(0):
            return "RX1";
        case BLADERF_CHANNEL_RX(1):
            return "RX2";
        case 0xF:
            return "SYS";
        default:
            return "   ";
    }
}

static inline char const *_rfic_statestr(enum rfic_entry_state state)
{
    switch (state) {
        case ENTRY_STATE_INVALID:
            return "BAD";
        case ENTRY_STATE_NEW:
            return "NEW";
        case ENTRY_STATE_RUNNING:
            return "RUN";
        case ENTRY_STATE_COMPLETE:
            return "OK ";
        default:
            return "   ";
    }
}

#endif  // BOARD_BLADERF_MICRO
#endif  // BLADERF_NIOS_DEVICES_RFIC_H_
