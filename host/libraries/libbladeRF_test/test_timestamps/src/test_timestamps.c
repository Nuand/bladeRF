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
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <libbladeRF.h>
#include "test_timestamps.h"

int perform_sync_init(struct bladerf *dev, bladerf_module module,
                      unsigned int buf_size, struct app_params *p)
{
    int status;

    if (buf_size == 0) {
        buf_size = p->buf_size;
    }

    status = bladerf_sync_config(dev,
                                 module,
                                 BLADERF_FORMAT_SC16_Q11_META,
                                 p->num_buffers,
                                 buf_size,
                                 p->num_xfers,
                                 p->timeout_ms);

    if (status != 0) {
        fprintf(stderr, "Failed to configure %s sync i/f: %s\n",
                module == BLADERF_MODULE_RX ? "RX" : "TX",
                bladerf_strerror(status));
    } else {
        status = bladerf_enable_module(dev, module, true);
        if (status != 0) {
        fprintf(stderr, "Failed to enable %s module: %s\n",
                module == BLADERF_MODULE_RX ? "RX" : "TX",
                bladerf_strerror(status));
        }
    }

    return status;
}

int enable_counter_mode(struct bladerf *dev, bool enable)
{
    int status;
    uint32_t regval;

    status = bladerf_config_gpio_read(dev, &regval);
    if (status != 0) {
        fprintf(stderr, "Failed to read FPGA GPIO config: %s\n",
                bladerf_strerror(status));
    } else {
        if (enable) {
            regval |= BLADERF_GPIO_COUNTER_ENABLE;
        } else {
            regval &= ~BLADERF_GPIO_COUNTER_ENABLE;
        }

        status = bladerf_config_gpio_write(dev, regval);
        if (status != 0) {
        fprintf(stderr, "Failed to write FPGA GPIO config: %s\n",
                bladerf_strerror(status));
        }
    }

    return status;
}

bool counter_data_is_valid(int16_t *samples, size_t n_samples, uint32_t ctr)
{
    size_t i;

    for (i = 0; i < 2 * n_samples; i += 2, ctr++) {
        const uint32_t val = extract_counter_val((uint8_t*) &samples[i]);
        if (val != ctr) {
            fprintf(stderr, "Invalid counter value @ sample %"PRIu64". "
                    "Expected 0x%"PRIx32", got 0x%"PRIx32"\n",
                    (uint64_t) i, ctr, val);

            return false;
        }
    }

    return true;
}

