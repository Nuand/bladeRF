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

#ifndef BLADERF_NIOS_DEVICES_RFIC_QUEUE_H_
#define BLADERF_NIOS_DEVICES_RFIC_QUEUE_H_

#define COMMAND_QUEUE_MAX 16
#define COMMAND_QUEUE_FULL 0xff
#define COMMAND_QUEUE_EMPTY 0xfe


/******************************************************************************/
/* Structures */
/******************************************************************************/

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


/******************************************************************************/
/* Functions */
/******************************************************************************/

/**
 * @brief       Enqueue a command
 *
 * @param       q       Queue
 * @param[in]   ch      Channel
 * @param[in]   cmd     Command
 * @param[in]   value   Value to pass to command
 *
 * @return  queue size after the enqueue operation, or COMMAND_QUEUE_FULL if
 *          the queue was full.
 */
uint8_t rfic_enqueue(struct rfic_queue *q,
                     uint8_t ch,
                     uint8_t cmd,
                     uint64_t value);

/**
 * @brief       Dequeue a command
 *
 * @param       q   Queue
 * @param[out]  e   Pointer to a queue entry struct
 *
 * @return  queue size after the dequeue operation, or COMMAND_QUEUE_EMPTY if
 *          the queue was empty.
 */
uint8_t rfic_dequeue(struct rfic_queue *q, struct rfic_queue_entry *e);

/**
 * @brief       Get the state of the next item in the queue
 *
 * @param[in]   q   Queue
 *
 * @return      pointer to the next item in the queue, or NULL if the
 *              queue is empty
 */
struct rfic_queue_entry *rfic_queue_peek(struct rfic_queue *q);

/**
 * @brief      Reset/initialize the queue
 *
 * @param      q    Queue
 */
void rfic_queue_reset(struct rfic_queue *q);

#endif  // BLADERF_NIOS_DEVICES_RFIC_QUEUE_H_
