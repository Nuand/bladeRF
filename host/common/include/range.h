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

#ifndef RANGE_H_
#define RANGE_H_

#if !defined(BLADERF_NIOS_BUILD) && !defined(BLADERF_NIOS_PC_SIMULATION)
#include <libbladeRF.h>
#else
#include "libbladeRF_nios_compat.h"
#endif

#include "bladerf2_common.h"

#ifdef BLADERF_NIOS_BUILD
// don't do scaling on the Nios, since multiplication and division suck
#define __scale(r, v) (v)
#define __unscale(r, v) (v)
#else
#define __scale(r, v) ((float)(v) / (r)->scale)
#define __unscale(r, v) ((float)(v) * (r)->scale)
#endif  // BLADERF_NIOS_BUILD

#define __scale_int(r, v) (__round_int(__scale(r, v)))
#define __scale_int64(r, v) (__round_int64(__scale(r, v)))

#define __unscale_int(r, v) (__round_int(__unscale(r, v)))
#define __unscale_int64(r, v) (__round_int64(__unscale(r, v)))

bool is_within_range(struct bladerf_range const *range, int64_t value);
int64_t clamp_to_range(struct bladerf_range const *range, int64_t value);

#endif  // RANGE_H_
