/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014-2018 Nuand LLC
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
#ifndef CONVERSIONS_H_
#define CONVERSIONS_H_

#include <errno.h>
#include <libbladeRF.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "host_config.h"
#include "rel_assert.h"

#ifdef __cplusplus
extern "C" {
#endif

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
const char *devspeed2str(bladerf_dev_speed speed);

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
const char *module2str(bladerf_module m);

/**
 * Convert a string to a module enumeration value.
 *
 * This is case-insensitive.
 *
 * @param   str         Module as a string. Should be "rx" or "tx".
 *
 * @return  BLADERF_MODULE_RX, BLADERF_MODULE_TX, or BLADERF_MODULE_INVALID
 */
bladerf_module str2module(const char *str);

/**
 * Convert a channel index to a string
 *
 * @note The caller should not attempt to modify or free() the returned string.
 *
 * @param   ch          Channel
 * @return  String representation of channel
 */
const char *channel2str(bladerf_channel ch);

/**
 * Convert a string to a channel index
 *
 * This is case-insensitive.
 *
 * @param   str         Channel as a string.
 * @return  BLADERF_CHANNEL_RX(n) or BLADERF_CHANNEL_TX(n),
 * 			or BLADERF_CHANNEL_INVALID if not recognized
 */
bladerf_channel str2channel(char const *str);

/**
 * Convert a direction enumeration to a string
 *
 * @note The caller should not attempt to modify or free() the returned string.
 *
 * @param   dir         direction
 * @return  String representation of direction
 */
const char *direction2str(bladerf_direction dir);

/**
 * Convert a trigger signal enumeration value to a string
 *
 * @param   trigger     Trigger item
 *
 * @return  String representation or "Unknown"
 */
const char *trigger2str(bladerf_trigger_signal trigger);

/**
 * Conver a string to a trigger signal enumeration value.
 *
 * This is case-insensitive.
 *
 * @param   str         Trigger as a string. Valid values include `Miniexp-1`,
 *                      `J51-1`, `J71-4`, or `User-0` through `User-7`.
 *
 * @return valid bladerf_trigger_signal value, or BLADERF_TRIGGER_INVALID
 */
bladerf_trigger_signal str2trigger(const char *str);

/**
 * Convert a trigger role enumeration value to a string
 *
 * @param   role    Role value
 *
 * @return  String representation or "Unknown"
 */
const char *triggerrole2str(bladerf_trigger_role role);

/**
 * Convert a string to a trigger role enumeration value
 *
 * @param   role    Role value
 *
 * @return  String representation or "Unknown"
 */
bladerf_trigger_role str2triggerrole(const char *str);

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
 * @brief      Convert a loopback mode to a string const
 *
 * @param[in]  loopback  The loopback mode
 *
 * @return     NUL-terminated string
 */
char const *loopback2str(bladerf_loopback loopback);

/**
 * Convert RX LNA gain strings to their associated enum values
 *
 * @param[in]   str         Gain string to convert
 * @param[out]  gain        Associated LNA gain string. Set to
 *                          BLADERF_LNA_GAIN_UNKNOWN on error.
 *
 * @return 0 on success, -1 on invalid string
 */
int str2lnagain(const char *str, bladerf_lna_gain *gain);

/**
 * Get a string description of the specified bladeRF backend
 *
 * @param  b       Backend to get a string for
 *
 * @return NUL-terminated string
 */
const char *backend_description(bladerf_backend b);

/**
 * Convert bladeRF SC16Q11 DAC/ADC samples to floats
 *
 * Note that the both the input and output buffers contain interleaved, where
 * 1 sample is associated with two array elements:
 *                       [I, Q, I, Q, ... I, Q]
 *
 * Therefore, the caller must ensure the output buffer large enough to contain
 * 2*n floats (or 2*n*sizeof(float) bytes).
 *
 * @param[in]   in      Input buffer containing SC16Q11 samples
 * @param[out]  out     Output buffer of float values
 * @param[in]   n       Number of samples to convert
 */
void sc16q11_to_float(const int16_t *in, float *out, unsigned int n);

/**
 * Convert float samples to bladeRF SC16Q11 DAC/ADC format
 *
 * Note that the both the input and output buffers contain interleaved, where
 * 1 sample is associated with two array elements:
 *                       [I, Q, I, Q, ... I, Q]
 *
 * Therefore, the caller must ensure the output buffer large enough to contain
 * 2*n int16_t's (or 2*n*sizeof(int16_t) bytes).
 *
 * @param[in]   in      Input buffer containing float samples
 * @param[out]  out     Output buffer of int16_t values
 * @param[in]   n       Number of samples to convert
 */
void float_to_sc16q11(const float *in, int16_t *out, unsigned int n);

/**
 * Convert a string to a bladerf_cal_module value
 *
 * @param[in]   str     String to convert
 *
 * @return A bladerf_cal_module value. This will be set to
 *         BLADERF_DC_CAL_INVALID if the provided string is invalid.
 */
bladerf_cal_module str_to_bladerf_cal_module(const char *str);

/**
 * Convert a bladerf_smb_mode enumeration value to a string
 *
 * @param[in]   mode        Mode enum value
 *
 * @return      String representation of enumeration, or "Unknown" for an
 *              invalid value.
 */
const char *smb_mode_to_str(bladerf_smb_mode mode);

/**
 * Convert a string to bladerf_smb_mode value
 *
 * @param[in]   str         String to convert
 *
 * @return      A BLADERF_SMB_MODE_* value. BLADERF_SMB_MODE_INVALID will
 *              returned for an invalid string.
 */
bladerf_smb_mode str_to_smb_mode(const char *str);

/**
 * Convert an ASCII char string to an unsigned integer and check its bounds
 *
 * @param[in]   str         String to convert
 * @param[in]   min         Value below this is bad
 * @param[in]   max         Value above this is bad
 * @param[out]  ok          True if conversion and bounds check did not fail
 *
 * @return An unsigned integer converted from an ASCII string
 */
unsigned int str2uint(const char *str,
                      unsigned int min,
                      unsigned int max,
                      bool *ok);

/**
 * Convert an ASCII char string to an integer and check its bounds
 *
 * @param[in]   str         String to convert
 * @param[in]   min         Value below this is bad
 * @param[in]   max         Value above this is bad
 * @param[out]  ok          True if conversion and bounds check did not fail
 *
 * @return A signed integer converted from an ASCII string
 */
int str2int(const char *str, int min, int max, bool *ok);

/**
 * Convert an ASCII char string to a 64bit unsigned long long integer and check
 * its bounds
 *
 * @param[in]   str         String to convert
 * @param[in]   min         Value below this is bad
 * @param[in]   max         Value above this is bad
 * @param[out]  ok          True if conversion and bounds check did not fail
 *
 * @return An unsigned long long integer converted from an ASCII string
 */
uint64_t str2uint64(const char *str, uint64_t min, uint64_t max, bool *ok);

/**
 * Convert an ASCII char string to a double and check its bounds
 *
 * @param[in]   str         String to convert
 * @param[in]   min         Value below this is bad
 * @param[in]   max         Value above this is bad
 * @param[out]  ok          True if conversion and bounds check did not fail
 *
 * @return A double converted from an ASCII string
 */
double str2double(const char *str, double min, double max, bool *ok);
struct numeric_suffix {
    const char *suffix;
    uint64_t multiplier;
};
typedef struct numeric_suffix numeric_suffix;

/**
 * Convert an ASCII char string that has a suffix multipler to an unsigned
 * integer and check its bounds
 *
 * @param[in]   str         String to convert
 * @param[in]   min         Value below this is bad
 * @param[in]   max         Value above this is bad
 * @param[in]   suffixes    Array of numeric_suffix
 * @param[in]   num_suff    Total number of numeric_suffix in suffixes array
 * @param[out]  ok          True if conversion and bounds check did not fail
 *
 * @return An unsigned integer converted from an ASCII string
 */
unsigned int str2uint_suffix(const char *str,
                             unsigned int min,
                             unsigned int max,
                             const struct numeric_suffix *suffixes,
                             const size_t num_suff,
                             bool *ok);

/**
 * Convert an ASCII char string that has a suffix multipler to an unsigned
 * integer and check its bounds
 *
 * @param[in]   str         String to convert
 * @param[in]   min         Value below this is bad
 * @param[in]   max         Value above this is bad
 * @param[in]   suffixes    Array of numeric_suffix
 * @param[in]   num_suff    Total number of numeric_suffix in suffixes array
 * @param[out]  ok          True if conversion and bounds check did not fail
 *
 * @return An unsigned long long integer converted from an ASCII string
 */
uint64_t str2uint64_suffix(const char *str,
                           uint64_t min,
                           uint64_t max,
                           const struct numeric_suffix *suffixes,
                           const size_t num_suff,
                           bool *ok);

/**
 * Convert a string to a boolean
 *
 * @param[in]	str	String to convert
 * @param[out]  val	Boolean value
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
int str2bool(const char *str, bool *val);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
