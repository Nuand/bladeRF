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
#include <stdint.h>
#include <stdbool.h>
#include "pkt_handler.h"
#include "pkt_retune.h"
#include "devices.h"
#include "debug.h"

/*
 * Retune word looks a little something like this:
 *  Bytes       Description
 *  0           Magic
 *  8:1         64-bit unsigned time
 *  12:9        32-bit unsigned frequency
 *  13          VCOCAP Hint
 *  14          Status flags
 *  15          Reserved
 */

struct retune_config {
    uint64_t    timestamp ;
    uint32_t    frequency ;
    uint8_t     vcocap_hint ;
    uint8_t     status_flags ;
} ;

void pkt_retune(struct pkt_buf *b)
{
    /* Fill in the retune config structure */
    struct retune_config config = { .timestamp = 0, .frequency = 0, .vcocap_hint = 0, .status_flags = 0 } ;
    int i ;
    uint8_t const *data = b->req;

    /* Timestamp */
    for( i = 1 ; i < 9 ; i++ ) {
        config.timestamp |= ((uint64_t)data[i]) << ((i-1)*8) ;
    }

    /* Frequency */
    for( i = 9 ; i < 13 ; i++ ) {
        config.frequency |= ((uint32_t)data[i]) << ((i-9)*8) ;
    }

    /* VCOCAP Hint */
    config.vcocap_hint = data[13] ;

    /* TODO: Schedule the retune here */

    /* Response is the same as the request */
    memcpy(b->resp, data, PKT_BUFLEN) ;

    return;
}
