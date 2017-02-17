/**
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014-2015 Nuand LLC
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

#include "helpers/version.h"

bool version_equal(const struct bladerf_version *v1,
                   const struct bladerf_version *v2)
{
    return v1->major == v2->major &&
           v1->minor == v2->minor &&
           v1->patch == v2->patch;
}

bool version_greater_or_equal(const struct bladerf_version *v1,
                              const struct bladerf_version *v2)
{
    return version_fields_greater_or_equal(v1, v2->major, v2->minor, v2->patch);
}

bool version_less_than(const struct bladerf_version *v1,
                       const struct bladerf_version *v2)
{
    return version_fields_less_than(v1, v2->major, v2->minor, v2->patch);
}

bool version_fields_greater_or_equal(const struct bladerf_version *version,
                                     unsigned int major, unsigned int minor,
                                     unsigned int patch)
{
    if (version->major > major) {
        return true;
    } else if ( (version->major == major) && (version->minor > minor) ) {
        return true;
    } else if ((version->major == major) &&
               (version->minor == minor) &&
               (version->patch >= patch) ) {
        return true;
    } else {
        return false;
    }
}

bool version_fields_less_than(const struct bladerf_version *version,
                              unsigned int major, unsigned int minor,
                              unsigned int patch)
{
    return !version_fields_greater_or_equal(version, major, minor, patch);
}
