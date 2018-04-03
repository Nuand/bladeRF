/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2013 Nuand LLC
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "host_config.h"

#if BLADERF_OS_WINDOWS || BLADERF_OS_OSX
#include "clock_gettime.h"
#else
#include <time.h>
#endif

#include <libbladeRF.h>

int populate_abs_timeout(struct timespec *t, unsigned int timeout_ms)
{
    static const int nsec_per_sec = 1000 * 1000 * 1000;
    const unsigned int timeout_sec = timeout_ms / 1000;
    int status;

    status = clock_gettime(CLOCK_REALTIME, t);
    if (status != 0) {
        return BLADERF_ERR_UNEXPECTED;
    } else {
        t->tv_sec += timeout_sec;
        t->tv_nsec += (timeout_ms % 1000) * 1000 * 1000;

        if (t->tv_nsec >= nsec_per_sec) {
            t->tv_sec += t->tv_nsec / nsec_per_sec;
            t->tv_nsec %= nsec_per_sec;
        }

        return 0;
    }
}
