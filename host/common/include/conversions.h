/**
 * @file conversions.h
 *
 * @brief Miscellaneous conversion routines
 */
#ifndef CONVERSIONS_H__
#define CONVERSIONS_H__

#include <stdint.h>

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
unsigned int str2uint64(const char *str, uint64_t min, uint64_t max, bool *ok);

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

#endif
