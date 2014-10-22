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

#ifndef EXAMPLE_COMMON_H__
#define EXAMPLE_COMMON_H__

/* Some portability macros */
#include "host_config.h"

/* assert() that is kept enabled in non-debug builds */
#include "rel_assert.h"

/* Settings used in examples */
#define EXAMPLE_SAMPLERATE  2000000
#define EXAMPLE_BANDWIDTH   BLADERF_BANDWIDTH_MIN

#define EXAMPLE_RX_FREQ     910000000
#define EXAMPLE_RX_LNA      BLADERF_LNA_GAIN_MAX
#define EXAMPLE_RX_VGA1     20
#define EXAMPLE_RX_VGA2     BLADERF_RXVGA2_GAIN_MIN

#define EXAMPLE_TX_FREQ     920000000
#define EXAMPLE_TX_VGA1     (-20)
#define EXAMPLE_TX_VGA2     BLADERF_TXVGA2_GAIN_MIN

/**
 * Device initialization function for example snippets in this directory
 *
 * @param   devstr      Optional device specifier string. Use NULL to open
 *                      any available device.
 *
 * @return  A bladeRF device handle on success, NULL on failure.
 */
struct bladerf *example_init(const char *devstr);

#endif
