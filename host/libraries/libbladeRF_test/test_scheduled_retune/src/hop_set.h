/*
 * This file is part of the bladeRF project:
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
#ifndef HOP_SET_H_
#define HOP_SET_H_

#include <stdlib.h>
#include <libbladeRF.h>

struct hop_params {
    unsigned int f;                 /* 32-bit frequency, in Hz */
    struct bladerf_quick_tune qt;   /* Quick tune parameters */
};

struct hop_set {
    size_t idx;                     /* Current index into `hops` */
    size_t count;                   /* Number of hop entries */
    struct hop_params *params;
};

/**
 * Load a hop set from the specified file. The file should contain a list
 * of frequencies, with one item per line.
 *
 * @param   filename    File to load hop set from
 *
 * @return Hop set on success, NULL on failure
 */
struct hop_set * hop_set_load(const char *filename);

/**
 * Load the quick tune parameters for all the entries in a hop set
 *
 * @param   dev     Handle to bladeRF device
 * @param   m       Module to retune in order to find the desired quick tune
 *                  parameters.
 * @param   h       Hop set
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int hop_set_load_quick_tunes(struct bladerf *dev,
                             bladerf_module m,
                             struct hop_set *h);
/**
 * Get the next entry in the hop set table and advance the hop index
 *
 * @param[in]   h   Hop set
 * @param[out]  p   If non-NULL, the hop's parameters, including quick tune
 *                  information, is written this address.
 *
 * @return The frequency (Hz) of the next hop set entry
 */
unsigned int hop_set_next(struct hop_set *h, struct hop_params *p);

/**
 * Deallocate the specified hop set
 *
 * @param   h   Hop set
 */
void hop_set_free(struct hop_set *h);

#endif
