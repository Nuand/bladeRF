/**
 * @brief These static inline routines are preferred over the use of macros,
 *        as they provide type safety.
 */

#ifndef MINMAX_H__
#define MINMAX_H__


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


#endif
