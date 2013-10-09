/**
 * @file conversions.h
 *
 * @brief Miscellaneous conversion routines
 */
#ifndef CONVERSIONS_H__
#define CONVERSIONS_H__

#include <stdint.h>
#include <libbladeRF.h>
#include "host_config.h"

/**
 * Represents an association between a string suffix for a numeric value and
 * its multiplier. For example, "k" might correspond to 1000.
 */
typedef struct numeric_suffix {
    const char *suffix;
    int multiplier;
} numeric_suffix;

/**
 * String to integer conversion with range and error checking
 *
 *  @param  str     String to convert
 *  @param  min     Inclusive minimum allowed value
 *  @param  max     Inclusive maximum allowed value
 *  @param  ok      If non-NULL, this will be set to true to denote that
 *                  the conversion succeeded. If this value is not true,
 *                  then the return value should not be used.
 *
 * @return 0 on success, undefined on failure
 */
int str2int(const char *str, int min, int max, bool *ok);

/**
 * Convert a string to an unsigned inteager with range and error checking
 *
 * @param[in]   str     String to conver
 * @param[in]   min     Minimum allowed value (inclusive)
 * @param[in]   max     Maximum allowed value (inclusive)
 * @param[out]  ok      If non-NULL, this is set to true if the conversion was
 *                      successful, and false for an invalid or out of range
 *                      value.
 *
 * @return  Converted value on success, 0 on failure
 */
unsigned int str2uint(const char *str,
                      unsigned int min, unsigned int max, bool *ok);

/**
 * Convert a string to an unsigned 64-bit integer with range and error checking
 *
 * @param[in]   str     String to conver
 * @param[in]   min     Minimum allowed value (inclusive)
 * @param[in]   max     Maximum allowed value (inclusive)
 * @param[out]  ok      If non-NULL, this is set to true if the conversion was
 *                      successful, and false for an invalid or out of range
 *                      value.
 *
 * @return  Converted value on success, 0 on failure
 */
uint64_t str2uint64(const char *str, uint64_t min, uint64_t max, bool *ok);

/**
 * Convert a string to a double with range and error checking
 *
 * @param[in]   str     String to conver
 * @param[in]   min     Minimum allowed value (inclusive)
 * @param[in]   max     Maximum allowed value (inclusive)
 * @param[out]  ok      If non-NULL, this is set to true if the conversion was
 *                      successful, and false for an invalid or out of range
 *                      value.
 *
 * @return  Converted value on success, 0 on failure
 */
double str2double(const char *str, double min, double max, bool *ok);

/**
 * Convert a string to an unsigned integer with range and error checking.
 * Supports the use of decimal representations and suffixes in the string.
 * For example, a string "2.4G" might be converted to 2400000000.
 *
 * @param[in]   str     String to convert
 * @param[in]   min     Minimum allowed value (inclusive)
 * @param[in]   max     Maximum allowed value (inclusive)
 * @param[in]   suffixes    List of allowed suffixes and their multipliers
 * @param[in]   num_suffixes    Number of suffixes in the list
 * @param[out]  ok      If non-NULL, this is set to true if the conversion was
 *                      successful, and false for an invalid or out of range
 *                      value.
 *
 * @return  Converted value on success, 0 on failure
 */
unsigned int str2uint_suffix(const char *str, unsigned int min,
        unsigned int max, const struct numeric_suffix suffixes[],
        int num_suffixes, bool *ok);

/**
 * Convert a string to a bladerf_version
 *
 * Accepted inputs are in the form: X.Y.Z or X.Y.Z-<extra text>
 *
 * @param[in]   string  Version string to convert
 * @param[out]  version Version structure to populate. For a valid string,
 *                      the describe member will point to the provided
 *                      string argument. The contents of this structure
 *                      are undefined when this function returns -1.
 *
 * @return 0 on success, -1 if provided string is invalid
 */
int str2version(const char *str, struct bladerf_version *version);

#endif
