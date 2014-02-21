/**
 * @file conversions.h
 *
 * @brief Miscellaneous conversion routines
 *
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (c) 2013 Nuand LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
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

/**
 * Convert a bladerf_dev_speed to a string suitable for printing
 *
 * @note The caller should not attempt to modify or free() the returned string.
 *
 * @param   speed   Device speed
 * @return  Const string describing the provided speed
 */
const char * devspeed2str(bladerf_dev_speed speed);

/**
 * Convert a string to libbladeRF log verbosity level
 *
 * @param[in]   str     Input string
 * @param[out]  ok      Value is updated to true if the input string was valid
 *
 * @return Log level if ok is true, undefined otherwise
 */
bladerf_log_level str2loglevel(const char *str, bool *ok);

/**
 * Convert a module enumeration to a string
 *
 * @note The caller should not attempt to modify or free() the returned string.
 *
 * @param   module      Module to convert to string
 * @return  String representation of module
 */
const char * module2str(bladerf_module m);

/**
 * Convert a string to a loopback mode
 *
 * @param[in]   str         String to convert
 * @param[out]  loopback    Corresponding loopback mode. Only valid when
 *                          this function returns successfully
 *
 * @return 0 on success, -1 on invalid string
 */
int str2loopback(const char *str, bladerf_loopback *loopback);

/**
 * Convert a string to an argc/argv-style argument list. Arguments are split
 * on whitespace. Double-quotes may be used to include whitespace in an
 * argument.  Backescapes are not supported.
 *
 * @param[in]   str     String to break into args
 * @param[out]  argv    Will be updated to point to heap-allocated argv list
 *
 * @return argc on success, -1 on failure
 */
int str2args(const char *line, char ***argv);

/**
 * Free argument list previously allocated by str2args
 *
 * @param       argc    Number of arguments
 * @param       argv    Argument list
 */
void free_args(int argc, char **argv);

#endif
