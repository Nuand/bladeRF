/*
 * clock_gettime() wrapper for OSX based upon jbenet's github "gist":
 *   https://gist.github.com/jbenet/1087739
 *
 * Copyright (c) 2011 Juan Batiz-Benet (https://github.com/jbenet)
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
#include "clock_gettime.h"
#include <errno.h>
#include <mach/clock.h>
#include <mach/mach.h>

int clock_gettime(clockid_t clk_id, struct timespec *tp)
{
    kern_return_t ret;
    clock_serv_t cclock;
    mach_timespec_t mts;

    ret = host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
    if (ret != 0) {
        errno = EINVAL;
        return -1;
    }

    ret = clock_get_time(cclock, &mts);
    if (ret != 0) {
        goto clock_gettime_out;
    }

    tp->tv_sec = mts.tv_sec;
    tp->tv_nsec = mts.tv_nsec;

clock_gettime_out:
    if (mach_port_deallocate(mach_task_self(), cclock) != 0 || ret != 0) {
        errno = EINVAL;
        return -1;
    } else {
        return 0;
    }
}
