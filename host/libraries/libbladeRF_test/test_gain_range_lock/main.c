/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2026 Nuand LLC
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

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board/board.h"

static bool callback_saw_lock;

static int fake_get_gain_range(struct bladerf *dev,
                               bladerf_channel ch,
                               struct bladerf_range const **range)
{
    static struct bladerf_range const test_range;
    int status;

    (void)ch;

    status = pthread_mutex_trylock(&dev->lock);
    if (status == 0) {
        pthread_mutex_unlock(&dev->lock);
    } else if (status == EBUSY) {
        callback_saw_lock = true;
    } else {
        return BLADERF_ERR_UNEXPECTED;
    }

    *range = &test_range;
    return 0;
}

int main(void)
{
    static struct board_fns const fake_board = {
        .get_gain_range = fake_get_gain_range,
    };
    struct bladerf_range const *range;
    struct bladerf dev;
    int status;

    memset(&dev, 0, sizeof(dev));
    dev.board = &fake_board;
    MUTEX_INIT(&dev.lock);

    status = bladerf_get_gain_range(&dev, BLADERF_CHANNEL_RX(0), &range);

    MUTEX_DESTROY(&dev.lock);

    if (status != 0) {
        fprintf(stderr, "gain range query failed: %s\n",
                bladerf_strerror(status));
        return EXIT_FAILURE;
    }

    if (!callback_saw_lock) {
        fputs("gain range callback entered without the device lock\n", stderr);
        return EXIT_FAILURE;
    }

    puts("PASS: gain range callback entered with the device lock");
    return EXIT_SUCCESS;
}
