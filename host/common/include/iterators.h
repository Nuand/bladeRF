/**
 * @file iterators.h
 *
 * @brief Useful iterators for working with channels, etc
 *
 * Copyright (c) 2018 Nuand LLC.
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
