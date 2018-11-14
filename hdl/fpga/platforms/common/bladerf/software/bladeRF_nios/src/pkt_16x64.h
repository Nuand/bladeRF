/* This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (c) 2017 Nuand LLC
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
#ifndef PKT_16x64_H_
#define PKT_16x64_H_

#include <stdint.h>
#include "pkt_handler.h"
#include "nios_pkt_16x64.h"

void pkt_16x64_init(void);

void pkt_16x64(struct pkt_buf *b);

void pkt_16x64_work(void);

#ifdef BLADERF_NIOS_LIBAD936X

#define PKT_16x64 { \
    .magic          = NIOS_PKT_16x64_MAGIC, \
    .init           = pkt_16x64_init, \
    .exec           = pkt_16x64, \
    .do_work        = pkt_16x64_work, \
}

#else

#define PKT_16x64 { \
    .magic          = NIOS_PKT_16x64_MAGIC, \
    .init           = NULL, \
    .exec           = pkt_16x64, \
    .do_work        = NULL, \
}

#endif  // BLADERF_NIOS_LIBAD936X
#endif
