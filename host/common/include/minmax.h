/**
 * @file minmax.h
 *
 * @brief These static inline routines are preferred over the use of macros,
 *        as they provide type safety.
 */

#ifndef MINMAX_H__
#define MINMAX_H__

#include <stdlib.h>
#include <stdint.h>
#include "host_config.h"

static inline size_t min_sz(size_t x, size_t y)
{
    return x < y ? x : y;
}

static inline size_t max_sz(size_t x, size_t y)
{
    return x > y ? x : y;
}

static inline unsigned int uint_min(unsigned int x, unsigned int y)
{
    return x < y ? x : y;
}

static inline unsigned int uint_max(unsigned int x, unsigned int y)
{
    return x > y ? x : y;
}

static inline uint32_t u32_min(uint32_t x, uint32_t y)
{
    return x < y ? x : y;
}

static inline uint32_t u32_max(uint32_t x, uint32_t y)
{
    return x > y ? x : y;
}

static inline uint64_t u64_min(uint64_t x, uint64_t y)
{
    return x < y ? x : y;
}

#endif
