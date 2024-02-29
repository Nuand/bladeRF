#ifndef TEST_COMMON_H_
#define TEST_COMMON_H_

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "libbladeRF.h"
#include "host_config.h"
#include "rel_assert.h"

#if BLADERF_OS_WINDOWS
#   include "clock_gettime.h"
#endif

/**
 * @brief Checks if any of the provided arguments are NULL.
 *
 * This macro iterates over each argument provided to it and checks if it is NULL.
 * If a NULL argument is found, it logs an error message indicating the argument
 * position that is NULL and returns a `BLADERF_ERR_INVAL` error code to indicate
 * an invalid parameter was passed.
 *
 * @param[in] __VA_ARGS__ A list of pointers to check for NULL. This list must be
 *                        terminated by a NULL pointer.
 */
#define CHECK_NULL(...) do { \
    const void* _args[] = { __VA_ARGS__, NULL }; \
    for (size_t _i = 0; _args[_i] != NULL; ++_i) { \
        if (_args[_i] == NULL) { \
            log_error("%s:%d: Argument %zu is a NULL pointer\n", __FILE__, __LINE__, _i + 1); \
            return BLADERF_ERR_INVAL; \
        } \
    } \
} while (0)

/**
 * @brief Checks the status returned by a function and logs an error if it indicates failure.
 *
 * This macro invokes a function and checks its return value. If the return value
 * indicates a failure (non-zero), it logs an error message including the file name,
 * line number, function name, and a string describing the error based on the status code.
 * After logging the error, it jumps to a label `error` for error handling.
 *
 * @param[in] fn The function to invoke, which should return an integer status code.
 */
#define CHECK_STATUS(fn) do {                                            \
        status = fn;                                                     \
        if (status != 0) {                                               \
            log_error("%s:%d: %s failed: %s\n", __FILE__, __LINE__, #fn, \
                      bladerf_strerror(status));                         \
            goto error;                                                  \
        }                                                                \
    } while (0)

/**
 * @brief Converts bytes to double words.
 *
 * Calculates the number of 32-bit double words (DWORDs) required to hold a given number of bytes.
 * It assumes that 1 DWORD is equivalent to 4 bytes (32 bits).
 *
 * @param bytes The number of bytes to convert to DWORDs. The value should be non-negative.
 *
 * @return The minimum number of DWORDs required to hold the specified number of bytes.
 */
static inline size_t bytes_to_dwords(int bytes) {
    return (bytes + 3) / 4;
}

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
