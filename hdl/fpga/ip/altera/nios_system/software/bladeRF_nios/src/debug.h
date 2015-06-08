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
#ifndef BLADERF_NIOS_DEBUG_H_
#define BLADERF_NIOS_DEBUG_H_

//#define BLADERF_NIOS_DEBUG

#ifdef BLADERF_NIOS_PC_SIMULATION
#   include <stdio.h>

#   define DBG(...) fprintf(stderr,  __VA_ARGS__)

    /* Always keep assert() enabled for PC simulation */
#   ifdef NDEBUG
#       define NDEBUG_UNDEF
#   endif
#   undef NDEBUG

#   include <assert.h>

#   ifdef NDEBUG_UNDEF
#       define NDEBUG
#       undef NDEBUG_UNDEF
#   endif

#   define ASSERT assert

static inline void print_bytes(const char *msg, const uint8_t *bytes, size_t n)
{
    size_t i;

    if (msg != NULL) {
        puts(msg);
    }

    for (i = 0; i < n; i++) {
        if (i == 0) {
            printf("  0x%02x", bytes[i]);
        } else if ((i + 1) % 8 == 0) {
            printf(" 0x%02x\n ", bytes[i]);
        } else {
            printf(" 0x%02x", bytes[i]);
        }
    }

    putchar('\n');
}

#else
#   ifdef BLADERF_NIOS_DEBUG
#   include "sys/alt_stdio.h"
#       define DBG(...) alt_printf(__VA_ARGS__)
#   else
#       define DBG(...)
#   endif

#   define ASSERT(x)
#   define print_bytes(m, b, n)
#endif

#endif
