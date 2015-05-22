/* This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
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
#ifndef PKT_HANDLER_H_
#define PKT_HANDLER_H_

#include <stdint.h>
#include <string.h>
#include "nios_pkt_formats.h"

#ifndef ARRAY_SIZE
#   define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

#define PKT_MAGIC_IDX 0
#define PKT_CFG_IDX   1

struct pkt_buf {
    const uint8_t req[NIOS_PKT_LEN];      /* Request */
    uint8_t       resp[NIOS_PKT_LEN];     /* Response */
};


struct pkt_handler {
    /**
     * Associates a message from the host with a particular packet handler
     * This may encode information that's meaningful to the packet handler.
     */
    const uint8_t magic;

    /**
     * Number of bytes that need to be a provided buffer before the state
     * machine can begin executing on the data.
     *
     * This allows a packet handler to begin responding with data while
     * the host is still providing dummy bytes at the end of a message
     * (due to DMA alignment constraints).
     */
    const uint8_t bytes_required;

    /**
     * Perform any required initializations
     */
    void (*init)(void);

    /**
     * Execute packet handler actions, provided a buffer containing the request
     * data. The packet handler is to fill out the entirety of the response
     * data.
     */
    void (*exec)(struct pkt_buf *b);

    /**
     * Perform any deferred work from a previous request
     */
    void (*do_work)(void);
};

static inline void reset_response_buf(struct pkt_buf *b)
{
    memset(b->resp, 0xff, sizeof(b->resp));
}

#endif
