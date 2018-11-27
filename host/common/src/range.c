/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2018 Nuand LLC
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

#ifdef BLADERF_NIOS_BUILD
#include "devices.h"
#endif  // BLADERF_NIOS_BUILD

/* Avoid building this in low-memory situations */
#if !defined(BLADERF_NIOS_BUILD) || defined(BLADERF_NIOS_LIBAD936X)

#include "range.h"

#if !defined(BLADERF_NIOS_BUILD) && !defined(BLADERF_NIOS_PC_SIMULATION)
#include "log.h"
#endif

bool is_within_range(struct bladerf_range const *range, int64_t value)
{
    if (NULL == range) {
        log_error("%s: range is null\n", __FUNCTION__);
        return false;
    }

    return (__scale(range, value) >= range->min &&
            __scale(range, value) <= range->max);
}

int64_t clamp_to_range(struct bladerf_range const *range, int64_t value)
{
    if (NULL == range) {
        log_error("%s: range is null\n", __FUNCTION__);
        return value;
    }

    if (__scale(range, value) < range->min) {
        log_debug("%s: Requested value %" PRIi64
                  " is below range [%g,%g], clamping to %" PRIi64 "\n",
                  __FUNCTION__, value, __unscale(range, range->min),
                  __unscale(range, range->max),
                  __unscale_int64(range, range->min));
        value = __unscale_int64(range, range->min);
    }

    if (__scale(range, value) > range->max) {
        log_debug("%s: Requested value %" PRIi64
                  " is above range [%g,%g], clamping to %" PRIi64 "\n",
                  __FUNCTION__, value, __unscale(range, range->min),
                  __unscale(range, range->max),
                  __unscale_int64(range, range->max));
        value = __unscale_int64(range, range->max);
    }

    return value;
}

#endif  // !defined(BLADERF_NIOS_BUILD) || defined(BLADERF_NIOS_LIBAD936X)
