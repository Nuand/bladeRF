#ifndef LOG_H__
#define LOG_H__

#include <stdio.h>
#include "libbladeRF.h"

#define _LOG_STRINGIFY__(x)         #x
#define _LOG_STRINGIFY_(x)          _LOG_STRINGIFY__(x)

/**
 * @defgroup LOG_MACROS Logging macros
 * @{
 */

/** Logs a verbose message. Does not include function/line information */
#define bladerf_log_verbose(...) \
    BLADERF_LOG(BLADERF_LOG_LEVEL_VERBOSE, "VERBOSE", __VA_ARGS__)

/** Logs a debug message. Does not include function/line information */
#define bladerf_log_debug(...) \
    BLADERF_LOG(BLADERF_LOG_LEVEL_DEBUG, "DEBUG", __VA_ARGS__)

/** Logs an info message. Does not include function/line information*/
#define bladerf_log_info(...) \
    BLADERF_LOG(BLADERF_LOG_LEVEL_INFO, "INFO", __VA_ARGS__)

/** Logs a warning message. Includes function/line information */
#define bladerf_log_warning(...) \
    BLADERF_LOG_LOCATION(BLADERF_LOG_LEVEL_WARNING, "WARNING", __VA_ARGS__)

/** Logs an error message. Includes function/line information */
#define bladerf_log_error(...) \
    BLADERF_LOG_LOCATION(BLADERF_LOG_LEVEL_ERROR, "ERROR", __VA_ARGS__)

/** Logs a critical error message. Includes function/line information */
#define bladerf_log_critical(...) \
    BLADERF_LOG_LOCATION(BLADERF_LOG_LEVEL_CRITICAL, "CRITICAL", __VA_ARGS__)

/** @} */


#define BLADERF_LOG_LOCATION(LEVEL, LEVEL_STRING, ...)                      \
    BLADERF_LOG(LEVEL, LEVEL_STRING,                                        \
            "[" __FILE__ ":" _LOG_STRINGIFY_(__LINE__) "] " __VA_ARGS__)

#define BLADERF_LOG(LEVEL, LEVEL_STRING, ...)                               \
    do { bladerf_log(LEVEL, LEVEL_STRING " - " __VA_ARGS__); } while (0);


/**
 * Writes a message to the logger with the specified log level. This function
 * should only be invoked indirectly through one of the bladerf_log_*
 * convenience macros above to ensure consistent formatting of log messages.
 *
 * @param   level       The severity level of the message
 * @param   format      The printf-style format string
 * @param   ...         Optional values for format specifier substitution
 *
 * @return The number of characters written to the log
 */
int bladerf_log(bladerf_log_level level, const char *format, ...);

#endif
