/**
 * @file have_cap.h
 *
 * @brief Convenience wrapper for testing capabilities mask
 *
 * Copyright (C) 2020 Nuand LLC
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */
#ifndef HAVE_CAP_H_
#define HAVE_CAP_H_

#include <stdint.h>

/**
 * Convenience wrapper for testing capabilities mask
 */
static inline bool have_cap(uint64_t capabilities, uint64_t capability)
{
    return (capabilities & capability) != 0;
}

/**
 * Convenience wrapper for testing capabilities mask
 */
static inline bool have_cap_dev(struct bladerf *dev, uint64_t capability)
{
    uint64_t capabilities = dev->board->get_capabilities(dev);
    return (capabilities & capability) != 0;
}
#endif  // HAVE_CAP_H_
