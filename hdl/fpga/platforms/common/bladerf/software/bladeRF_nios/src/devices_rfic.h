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

#include <stdbool.h>
#include <stdint.h>

#include "bladerf2_common.h"
#include "debug.h"
#include "devices.h"

#include "devices_rfic_cmds.h"
#include "devices_rfic_queue.h"


/******************************************************************************/
/* Types */
/******************************************************************************/

typedef uint8_t command_bitmask;

#define RFIC_CMD_INIT_REQD 1   /**< Command requires initialized state */
#define RFIC_CMD_CHAN_SYSTEM 2 /**< Command valid for wildcard channels */
#define RFIC_CMD_CHAN_TX 4     /**< Command valid for TX channels */
#define RFIC_CMD_CHAN_RX 8     /**< Command valid for RX channels */

/**
 * RFIC command handler state structure
 */
struct rfic_state {
    /* Initialization state */
    bladerf_rfic_init_state init_state;

    /* AD9361 state structure */
    struct ad9361_rf_phy *phy;

    /* Queue for handling long-running write operations */
    struct rfic_queue write_queue;

    /* RX and TX FIR filter memory */
    bladerf_rfic_rxfir rxfir;
    bladerf_rfic_txfir txfir;

    /* Frequency retrieval is invalid due to fastlock shenanigans */
    bool frequency_invalid[NUM_MODULES];

    /* TX mute state at standby */
    bool tx_mute_state[2];
};


/******************************************************************************/
/* Function declarations */
/******************************************************************************/

/**
 * RFIC command read (immediate)
 *
 * @param   command     Command to execute
 * @param   channel     Channel to apply command to
 * @param   data        Response from command
 *
 * @return bool (true = success)
 */
bool rfic_command_read_immed(bladerf_rfic_command command,
                             bladerf_channel channel,
                             uint64_t *data);

/**
 * RFIC command write (immediate)
 *
 * @param   command     Command to execute
 * @param   channel     Channel to apply command to
 * @param   data        Argument for command
 *
 * @return bool (true = success)
 */
bool rfic_command_write_immed(bladerf_rfic_command command,
                              bladerf_channel channel,
                              uint64_t data);

/**
 * Invalidate the LO frequency state for a given module
 *
 * If anything affecting the tuning frequency of the RFIC is performed
 * without using the ad9361_* functions, call this function to indicate
 * that the state is invalid. Attempts to get the LO frequency will fail,
 * until the LO frequency is set with a BLADERF_RFIC_COMMAND_FREQUENCY
 * command.
 *
 * @param[in]  module  The module
 */
void rfic_invalidate_frequency(bladerf_module module);


/******************************************************************************/
/* Helpers */
/******************************************************************************/

/**
 * @brief      Translate a bladerf_direction and channel number to a
 *             bladerf_channel
 *
 * @param      _dir  Direction
 * @param      _idx  Channel number
 *
 * @return     BLADERF_CHANNEL_TX(_idx) or BLADERF_CHANNEL_RX(_idx)
 */
#define CHANNEL_IDX(_dir, _idx) \
    (BLADERF_TX == _dir) ? BLADERF_CHANNEL_TX(_idx) : BLADERF_CHANNEL_RX(_idx)

/**
 * @brief      Iterate over all bladerf_directions
 *
 * @param      _dir  Direction
 */
#define FOR_EACH_DIRECTION(_dir) \
    for (_dir = BLADERF_RX; _dir <= BLADERF_TX; ++_dir)

/**
 * @brief      Iterate over all channels in a given direction
 *
 * @param      _dir      Direction
 * @param      _count    Number of channels
 * @param[out] _index    Index variable (size_t)
 * @param[out] _channel  Channel variable (bladerf_channel)
 *
 * @return     { description_of_the_return_value }
 */
#define FOR_EACH_CHANNEL(_dir, _count, _index, _channel)                    \
    for (_index = 0, _channel = CHANNEL_IDX(_dir, _index); _index < _count; \
         ++_index, _channel   = CHANNEL_IDX(_dir, _index))

/* Packet decoding debugging output */
static char const RFIC_DBG_FMT[] = "%s: cmd=%s ch=%s state=%s [%x]=%x(-%x)\n";
#define RFIC_DBG(prefix, cmd, ch, state, addr, data) \
    DBG(RFIC_DBG_FMT, prefix, cmd, ch, state, addr, data, -data)

/* Error message output */
static char const RFIC_ERR_FMT[] = "%s: error: %s returned %x (-%x)\n";
#define RFIC_ERR(thing, retval) \
    DBG(RFIC_ERR_FMT, __FUNCTION__, thing, retval, -retval)

/**
 * @brief      Error-catching wrapper
 *
 * @param      _fn   The function to wrap
 *
 * @return     false on error (_fn() < 0), continues otherwise
 */
#define CHECK_BOOL(_fn)         \
    do {                        \
        int _s = _fn;           \
        if (_s < 0) {           \
            RFIC_ERR(#_fn, _s); \
            return false;       \
        }                       \
    } while (0)

#define RFIC_SYSTEM_CHANNEL 0xF

static inline bool _rfic_is_initialized(struct rfic_state *state)
{
    return BLADERF_RFIC_INIT_STATE_ON == state->init_state;
}

static inline bool _rfic_chan_is_system(uint8_t ch)
{
    return RFIC_SYSTEM_CHANNEL == ch;
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
        case RFIC_SYSTEM_CHANNEL:
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

#endif  // BLADERF_NIOS_DEVICES_RFIC_H_
