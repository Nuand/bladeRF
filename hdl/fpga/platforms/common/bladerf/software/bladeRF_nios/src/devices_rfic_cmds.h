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

#ifndef BLADERF_NIOS_DEVICES_RFIC_CMDS_H_
#define BLADERF_NIOS_DEVICES_RFIC_CMDS_H_


/******************************************************************************/
/* Command handlers */
/******************************************************************************/

struct rfic_state;

bool _rfic_cmd_rd_status(struct rfic_state *, bladerf_channel, uint64_t *);

bool _rfic_cmd_wr_init(struct rfic_state *, bladerf_channel, uint32_t);

bool _rfic_cmd_rd_init(struct rfic_state *, bladerf_channel, uint32_t *);

bool _rfic_cmd_wr_enable(struct rfic_state *, bladerf_channel, uint32_t);

bool _rfic_cmd_rd_enable(struct rfic_state *, bladerf_channel, uint32_t *);

bool _rfic_cmd_wr_samplerate(struct rfic_state *, bladerf_channel, uint32_t);

bool _rfic_cmd_rd_samplerate(struct rfic_state *, bladerf_channel, uint32_t *);

bool _rfic_cmd_wr_frequency(struct rfic_state *, bladerf_channel, uint64_t);

bool _rfic_cmd_rd_frequency(struct rfic_state *, bladerf_channel, uint64_t *);

bool _rfic_cmd_wr_bandwidth(struct rfic_state *, bladerf_channel, uint32_t);

bool _rfic_cmd_rd_bandwidth(struct rfic_state *, bladerf_channel, uint32_t *);

bool _rfic_cmd_wr_gainmode(struct rfic_state *, bladerf_channel, uint32_t);

bool _rfic_cmd_rd_gainmode(struct rfic_state *, bladerf_channel, uint32_t *);

bool _rfic_cmd_wr_gain(struct rfic_state *, bladerf_channel, uint32_t);

bool _rfic_cmd_rd_gain(struct rfic_state *, bladerf_channel, uint32_t *);

bool _rfic_cmd_rd_rssi(struct rfic_state *, bladerf_channel, uint64_t *);

bool _rfic_cmd_wr_filter(struct rfic_state *, bladerf_channel, uint32_t);

bool _rfic_cmd_rd_filter(struct rfic_state *, bladerf_channel, uint32_t *);

bool _rfic_cmd_wr_txmute(struct rfic_state *, bladerf_channel, uint32_t);

bool _rfic_cmd_rd_txmute(struct rfic_state *, bladerf_channel, uint32_t *);

bool _rfic_cmd_wr_fastlock(struct rfic_state *, bladerf_channel, uint32_t);

#endif  // BLADERF_NIOS_DEVICES_RFIC_CMDS_H_
