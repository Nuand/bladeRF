/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2015 Nuand LLC
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
#ifndef BAND_SELECT_H_
#define BAND_SELECT_H_

#include <stdbool.h>

#if !defined(BLADERF_NIOS_BUILD) && !defined(BLADERF_NIOS_PC_SIMULATION)
#   include <libbladeRF.h>
#   include "bladerf_priv.h"
#   include "log.h"
#else
#   include "libbladeRF_nios_compat.h"
#   include "devices.h"
#endif

/**
 * Select the bladeRF's low or high band
 *
 * @param   dev         Device handle
 * @param   module      Module to configure
 * @param   low_band    Configure for low band (true) or high band (false)
 *
 * @return 0 on succes, BLADERF_ERR_* value on failure
 */
int band_select(struct bladerf *dev, bladerf_module module, bool low_band);

#endif
