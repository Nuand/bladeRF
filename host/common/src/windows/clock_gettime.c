/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (c) 2013 Nuand LLC
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
#ifndef WIN32
#   error "This file is intended for use with WIN32 systems only."
#endif

#include <Windows.h>
#include <errno.h>
#include "clock_gettime.h"
#include "ptw32_timespec.h"

int clock_gettime(clockid_t clk_id, struct timespec *tp)
{
    BOOL success;
    FILETIME file_time;
    SYSTEMTIME system_time;

    if (clk_id != CLOCK_REALTIME) {
        errno = EINVAL;
        return -1;
    }

    GetSystemTime(&system_time);
    success = SystemTimeToFileTime(&system_time, &file_time);

    if (!success) {
        /* For lack of a better or more compliant return value... */
        errno = EINVAL;
        return -1;
    }

    ptw32_filetime_to_timespec(&file_time, tp);

    return 0;
}

