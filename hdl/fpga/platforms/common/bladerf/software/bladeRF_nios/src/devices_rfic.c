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

#include "devices.h"

#if defined(BOARD_BLADERF_MICRO) && defined(BLADERF_NIOS_LIBAD936X)

#include "devices_rfic.h"


/******************************************************************************/
/* Global state */
/******************************************************************************/

static struct rfic_state state;


/******************************************************************************/
/* Helpers */
/******************************************************************************/

void rfic_invalidate_frequency(bladerf_module module)
{
    state.frequency_invalid[module] = true;
}


/******************************************************************************/
/* Dispatching */
/******************************************************************************/

/**
 * @brief      Function pointers for RFIC command dispatching
 */
struct rfic_command_fns {
    bool (*write64)(struct rfic_state *, bladerf_channel, uint64_t);
    bool (*write32)(struct rfic_state *, bladerf_channel, uint32_t);
    bool (*read64)(struct rfic_state *, bladerf_channel, uint64_t *);
    bool (*read32)(struct rfic_state *, bladerf_channel, uint32_t *);
    command_bitmask bitmask;
    uint8_t command;
};

/**
 * Function pointers for command dispatching
 */
struct rfic_command_fns const funcs[] = {
    // clang-format off
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_STATUS),
        FIELD_INIT(.read64, _rfic_cmd_rd_status),
        FIELD_INIT(.bitmask, RFIC_CMD_CHAN_SYSTEM),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_INIT),
        FIELD_INIT(.write32, _rfic_cmd_wr_init),
        FIELD_INIT(.read32, _rfic_cmd_rd_init),
        FIELD_INIT(.bitmask, RFIC_CMD_CHAN_SYSTEM),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_ENABLE),
        FIELD_INIT(.write32, _rfic_cmd_wr_enable),
        FIELD_INIT(.read32, _rfic_cmd_rd_enable),
        FIELD_INIT(.bitmask,
            RFIC_CMD_INIT_REQD | RFIC_CMD_CHAN_TX | RFIC_CMD_CHAN_RX),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_SAMPLERATE),
        FIELD_INIT(.write32, _rfic_cmd_wr_samplerate),
        FIELD_INIT(.read32, _rfic_cmd_rd_samplerate),
        FIELD_INIT(.bitmask,
            RFIC_CMD_INIT_REQD | RFIC_CMD_CHAN_TX | RFIC_CMD_CHAN_RX),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_FREQUENCY),
        FIELD_INIT(.write64, _rfic_cmd_wr_frequency),
        FIELD_INIT(.read64, _rfic_cmd_rd_frequency),
        FIELD_INIT(.bitmask,
            RFIC_CMD_INIT_REQD | RFIC_CMD_CHAN_TX | RFIC_CMD_CHAN_RX),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_BANDWIDTH),
        FIELD_INIT(.write32, _rfic_cmd_wr_bandwidth),
        FIELD_INIT(.read32, _rfic_cmd_rd_bandwidth),
        FIELD_INIT(.bitmask,
            RFIC_CMD_INIT_REQD | RFIC_CMD_CHAN_TX | RFIC_CMD_CHAN_RX),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_GAINMODE),
        FIELD_INIT(.write32, _rfic_cmd_wr_gainmode),
        FIELD_INIT(.read32, _rfic_cmd_rd_gainmode),
        FIELD_INIT(.bitmask, RFIC_CMD_INIT_REQD | RFIC_CMD_CHAN_RX),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_GAIN),
        FIELD_INIT(.write32, _rfic_cmd_wr_gain),
        FIELD_INIT(.read32, _rfic_cmd_rd_gain),
        FIELD_INIT(.bitmask,
            RFIC_CMD_INIT_REQD | RFIC_CMD_CHAN_TX | RFIC_CMD_CHAN_RX),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_RSSI),
        FIELD_INIT(.read64, _rfic_cmd_rd_rssi),
        FIELD_INIT(.bitmask,
            RFIC_CMD_INIT_REQD | RFIC_CMD_CHAN_TX | RFIC_CMD_CHAN_RX),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_FILTER),
        FIELD_INIT(.write32, _rfic_cmd_wr_filter),
        FIELD_INIT(.read32, _rfic_cmd_rd_filter),
        FIELD_INIT(.bitmask,
            RFIC_CMD_INIT_REQD | RFIC_CMD_CHAN_TX | RFIC_CMD_CHAN_RX),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_TXMUTE),
        FIELD_INIT(.write32, _rfic_cmd_wr_txmute),
        FIELD_INIT(.read32, _rfic_cmd_rd_txmute),
        FIELD_INIT(.bitmask, RFIC_CMD_INIT_REQD | RFIC_CMD_CHAN_TX),
    },
    {
        FIELD_INIT(.command, BLADERF_RFIC_COMMAND_FASTLOCK),
        FIELD_INIT(.write32, _rfic_cmd_wr_fastlock),
        FIELD_INIT(.bitmask,
            RFIC_CMD_INIT_REQD | RFIC_CMD_CHAN_TX | RFIC_CMD_CHAN_RX),
    },
    // clang-format on
};

/**
 * @brief      Retrieve command pointer for a given command
 *
 * @param[in]  command  The command
 *
 * @return     The command pointer.
 */
static struct rfic_command_fns const *_get_cmd_ptr(uint8_t command)
{
    for (size_t i = 0; i < ARRAY_SIZE(funcs); ++i) {
        if (command == funcs[i].command) {
            return &funcs[i];
        }
    }

    return NULL;
}

/**
 * @brief      Determines if bitmask contains a given bit.
 *
 * @param[in]  bitmask  The bitmask
 * @param[in]  bit      The bit
 *
 * @return     True if bitmask has bit, False otherwise.
 */
static bool _has_bit(command_bitmask bitmask, command_bitmask bit)
{
    return ((bitmask & bit) > 0);
}

/**
 * @brief      Check validity of command pointer in current state
 *
 * @param      cmd_ptr  The command pointer
 * @param[in]  channel  The channel
 *
 * @return     True if the current state is valid, False otherwise.
 */
static bool _check_validity(struct rfic_command_fns const *cmd_ptr,
                            bladerf_channel channel)
{
    bool is_system = _rfic_chan_is_system(channel);
    bool is_tx     = !is_system && BLADERF_CHANNEL_IS_TX(channel);
    bool is_rx     = !is_system && !BLADERF_CHANNEL_IS_TX(channel);

    /* System channel check */
    if (is_system && !_has_bit(cmd_ptr->bitmask, RFIC_CMD_CHAN_SYSTEM)) {
        DBG("%s: operation not valid for wildcard channel\n", __FUNCTION__);
        return false;
    }

    /* Initialization check */
    if (!_rfic_is_initialized(&state) &&
        _has_bit(cmd_ptr->bitmask, RFIC_CMD_INIT_REQD)) {
        DBG("%s: operation not valid in uninitialized state\n", __FUNCTION__);
        return false;
    }

    /* TX channel check */
    if (is_tx && !_has_bit(cmd_ptr->bitmask, RFIC_CMD_CHAN_TX)) {
        DBG("%s: operation not valid for TX channels\n", __FUNCTION__);
        return false;
    }

    /* RX channel check */
    if (is_rx && !_has_bit(cmd_ptr->bitmask, RFIC_CMD_CHAN_RX)) {
        DBG("%s: operation not valid for RX channels\n", __FUNCTION__);
        return false;
    }

    /* All tests passed */
    return true;
}

/**
 * @brief      Write queue worker
 *
 * Sets e->rv to 0xFE if there is no write handler for the command.
 *
 * @param      q     rfic_queue pointer
 */
static void rfic_command_work_wq(struct rfic_queue *q)
{
    struct rfic_queue_entry *e = rfic_queue_peek(q);

    if (NULL == e) {
        return;
    }

#if 0
    if (ENTRY_STATE_COMPLETE == e->state)
#endif
    {
        RFIC_DBG("WRW", _rfic_cmdstr(e->cmd), _rfic_chanstr(e->ch),
                 _rfic_statestr(e->state), -1, e->value);
    }

    switch (e->state) {
        case ENTRY_STATE_NEW: {
            e->state = ENTRY_STATE_RUNNING;
            break;
        }

        case ENTRY_STATE_RUNNING: {
            struct rfic_command_fns const *f = _get_cmd_ptr(e->cmd);

            e->rv    = 0xFE;
            e->state = ENTRY_STATE_COMPLETE;

            if (NULL != f->write64) {
                e->rv = f->write64(&state, e->ch, e->value);
            } else if (NULL != f->write32) {
                e->rv = f->write32(&state, e->ch, e->value);
            }

            break;
        }

        case ENTRY_STATE_COMPLETE: {
            /* Drop the item from the queue */
            q->last_rv = e->rv;
            rfic_dequeue(q, NULL);
            break;
        }

        default:
            break;
    }
}

bool rfic_command_write(uint16_t addr, uint64_t data)
{
    uint8_t cmd                      = _rfic_unpack_cmd(addr);
    uint8_t const ch                 = _rfic_unpack_chan(addr);
    struct rfic_command_fns const *f = _get_cmd_ptr(cmd);
    bool rv                          = false;

    if (NULL != f && (NULL != f->write64 || NULL != f->write32) &&
        _check_validity(f, ch)) {
        rv = rfic_enqueue(&state.write_queue, ch, cmd, data) !=
             COMMAND_QUEUE_FULL;
    }

    RFIC_DBG("WR ", _rfic_cmdstr(cmd), _rfic_chanstr(ch), rv ? "ENQ" : "BAD",
             addr, data);

    return rv;
}

bool rfic_command_write_immed(bladerf_rfic_command cmd,
                              bladerf_channel ch,
                              uint64_t data)
{
    struct rfic_command_fns const *f = _get_cmd_ptr(cmd);

    bool rv = false;

    if (NULL == f || !_check_validity(f, ch)) {
        rv = false;
    } else if (NULL != f->write64) {
        rv = f->write64(&state, ch, data);
    } else if (NULL != f->write32) {
        rv = f->write32(&state, ch, (uint32_t)data);
    }

    RFIC_DBG("WR!", _rfic_cmdstr(cmd), _rfic_chanstr(ch), rv ? "OK " : "BAD",
             0x0, data);

    return rv;
}

bool rfic_command_read(uint16_t addr, uint64_t *data)
{
    return rfic_command_read_immed(_rfic_unpack_cmd(addr),
                                   _rfic_unpack_chan(addr), data);
}

bool rfic_command_read_immed(bladerf_rfic_command cmd,
                             bladerf_channel ch,
                             uint64_t *data)
{
    struct rfic_command_fns const *f = _get_cmd_ptr(cmd);
    bool rv                          = false;

    // Canary
    *data = 0xDEADBEEF0B4DF00D;

    if (NULL == f || !_check_validity(f, ch)) {
        rv = false;
    } else if (NULL != f->read64) {
        rv = f->read64(&state, ch, data);
    } else if (NULL != f->read32) {
        uint32_t data32;

        rv = f->read32(&state, ch, &data32);

        if (rv) {
            *data = (uint64_t)data32;
        }
    }

    RFIC_DBG("RD!", _rfic_cmdstr(cmd), _rfic_chanstr(ch), rv ? "OK " : "BAD",
             0x0, *data);

    return rv;
}

/**
 * @brief      Startup initialization for RFIC command
 */
void rfic_command_init(void)
{
    rfic_queue_reset(&state.write_queue);
}

/**
 * @brief      Worker wrapper for RFIC command
 */
void rfic_command_work(void)
{
    rfic_command_work_wq(&state.write_queue);
}

#endif  // defined(BOARD_BLADERF_MICRO) && defined(BLADERF_NIOS_LIBAD936X)
