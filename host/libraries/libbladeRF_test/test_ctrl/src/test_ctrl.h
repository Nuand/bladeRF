/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014 Nuand LLC
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
#ifndef TEST_CTRL_H_
#define TEST_CTRL_H_

#include "conversions.h"
#include "host_config.h"
#include "rel_assert.h"
#include "test_common.h"
#include <libbladeRF.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef size_t failure_count;

#define DECLARE_TEST(name_)                                                   \
    extern failure_count test_##name_(struct bladerf *, struct app_params *p, \
                                      bool quiet);                            \
    extern const struct test_case test_case_##name_;
#endif

#define DECLARE_TEST_CASE(name_) \
    const struct test_case test_case_##name_ = { #name_, &test_##name_ }

#define PRINT(...)               \
    do {                         \
        if (!quiet) {            \
            printf(__VA_ARGS__); \
            fflush(stdout);      \
        }                        \
    } while (0)

#define PR_ERROR(...) fprintf(stderr, "\n(!) " __VA_ARGS__)

#define DEFAULT_BUF_LEN 16384
#define DEFAULT_NUM_BUFFERS 16
#define DEFAULT_NUM_XFERS 8
#define DEFAULT_TIMEOUT_MS 10000
#define DEFAULT_SAMPLERATE 1000000

struct app_params {
    bool use_xb200;
    bladerf_tuning_mode tuning_mode;
    char *device_str;
    char *test_name;
    uint64_t randval_seed;
    uint64_t randval_state;
    bool module_enabled;
};


struct test_case {
    const char *name;
    failure_count (*fn)(struct bladerf *dev, struct app_params *p, bool quiet);
};

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

DECLARE_TEST(bandwidth);
DECLARE_TEST(correction);
DECLARE_TEST(enable_module);
DECLARE_TEST(gain);
DECLARE_TEST(frequency);
DECLARE_TEST(loopback);
DECLARE_TEST(rx_mux);
DECLARE_TEST(lpf_mode);
DECLARE_TEST(samplerate);
DECLARE_TEST(sampling);
DECLARE_TEST(threads);
DECLARE_TEST(xb200);
