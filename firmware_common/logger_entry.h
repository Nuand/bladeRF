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
#ifndef LOGGER_ENTRY
#define LOGGER_ENTRY

#include <stdint.h>

/* A log entry word is laid out as follows. All values are little endian.
 *
 * +--------+--------+-------------------------------------------------------+
 * |  Bit   | Length | Description                                           |
 * +========+========+=======================================================+
 * |   0    |   16   | Data to include with the logged event                 |
 * +--------+--------+-------------------------------------------------------+
 * |   16   |   11   | Source line number where the event was logged         |
 * +--------+--------+-------------------------------------------------------+
 * |   27   |   5    | Source file ID where the event was logged             |
 * +--------+--------+-------------------------------------------------------+
 *
 */
typedef uint32_t logger_entry;

#define LOG_DATA_SHIFT  0
#define LOG_DATA_MASK   0xffff

#define LOG_LINE_SHIFT  16
#define LOG_LINE_MASK   0x7ff

#define LOG_FILE_SHIFT  27
#define LOG_FILE_MASK   0x1f

#define LOG_EOF 0x00000000
#define LOG_ERR 0xffffffff

static inline logger_entry logger_entry_pack(uint8_t file, uint16_t line,
                                             uint16_t data)
{
    logger_entry e;

    e  = (data & LOG_DATA_MASK) << LOG_DATA_SHIFT;
    e |= (line & LOG_LINE_MASK) << LOG_LINE_SHIFT;
    e |= (file & LOG_FILE_MASK) << LOG_FILE_SHIFT;

    return e;
}

static inline void logger_entry_unpack(logger_entry e, uint8_t *file,
                                       uint16_t *line, uint16_t *data)
{
    *data = (e >> LOG_DATA_SHIFT) & LOG_DATA_MASK;
    *line = (e >> LOG_LINE_SHIFT) & LOG_LINE_MASK;
    *file = (e >> LOG_FILE_SHIFT) & LOG_FILE_MASK;
}

#endif
