#ifndef TEST_COMMON_H_
#define TEST_COMMON_H_

#include <stdint.h>
#include "host_config.h"
#include "rel_assert.h"

#if BLADERF_OS_WINDOWS
#   include "clock_gettime.h"
#endif

/**
 * Initialize a seed for use with randval_update
 *
 * @param[out]   state      PRNG state
 * @param[in]    seed       PRNG seed value. Should not be 0.
 *
 */
static inline void randval_init(uint64_t *state, uint64_t seed) {
    if (seed == 0) {
        seed = UINT64_MAX;
    }

    *state = seed;
}

/**
 * Get the next PRNG value, using a simple xorshift implementation
 *
 * @param[inout] state      PRNG state
 *
 * @return  next PRNG value
 */
static inline uint64_t randval_update(uint64_t *state)
{
    assert(*state != 0);
    (*state) ^= (*state) >> 12;
    (*state) ^= (*state) << 25;
    (*state) ^= (*state) >> 27;
    (*state) *= 0x2545f4914f6cdd1d;
    return *state;
}

/**
 * Calculate an average duration
 *
 * @param   start           Start time
 * @param   end             End time
 * @param   iteratations    Number of iterations performed timed
 *
 * @return average duration, in seconds
 */
double calc_avg_duration(const struct timespec *start,
                         const struct timespec *end,
                         double iterations);

/**
 * Get a random frequency
 *
 * @param[in]
 * @param[inout]    This function updates this PRNG state it with randval_update()
 */
unsigned int get_rand_freq(uint64_t *prng_state, bool xb200_enabled);

/**
 * Wait for the specified timestamp to occur/pass
 *
 * @param[in]       dev         Device handle
 * @param[in]       module      Module to wait on
 * @param[in]       timestamp   Time to wait for
 * @param[in]       timeout_ms  Approximate max time to wait, in milliseconds
 *
 * @return 0 on success, BLADERF_ERR_TIMEOUT if timeout_ms expires,
 *           and return values from bladerf_get_timestamp() if non-zero.
 */
int wait_for_timestamp(struct bladerf *dev, bladerf_module module,
                       uint64_t timestamp, unsigned int timeout_ms);

#endif
