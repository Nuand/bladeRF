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
#include <stdint.h>
#include "band_select.h"
#include "lms.h"

int band_select(struct bladerf *dev, bladerf_module module, bool low_band)
{
    int status;
    uint32_t gpio;
    const uint32_t band = low_band ? 2 : 1;

    log_debug("Selecting %s band.\n", low_band ? "low" : "high");

    status = lms_select_band(dev, module, low_band);
    if (status != 0) {
        return status;
    }

    status = CONFIG_GPIO_READ(dev, &gpio);
    if (status != 0) {
        return status;
    }

    gpio &= ~(module == BLADERF_MODULE_TX ? (3 << 3) : (3 << 5));
    gpio |= (module == BLADERF_MODULE_TX ? (band << 3) : (band << 5));

    return CONFIG_GPIO_WRITE(dev, gpio);
}
