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
#define log_verbose(...) \
    LOG_WRITE(BLADERF_LOG_LEVEL_VERBOSE, "[VERBOSE", __VA_ARGS__)

/** Logs a debug message. Does not include function/line information */
#define log_debug(...) \
    LOG_WRITE(BLADERF_LOG_LEVEL_DEBUG, "[DEBUG", __VA_ARGS__)

/** Logs an info message. Does not include function/line information*/
#define log_info(...) \
    LOG_WRITE(BLADERF_LOG_LEVEL_INFO, "[INFO", __VA_ARGS__)

/** Logs a warning message. Includes function/line information */
#define log_warning(...) \
    LOG_WRITE(BLADERF_LOG_LEVEL_WARNING, "[WARNING", __VA_ARGS__)

/** Logs an error message. Includes function/line information */
#define log_error(...) \
    LOG_WRITE(BLADERF_LOG_LEVEL_ERROR, "[ERROR", __VA_ARGS__)

/** Logs a critical error message. Includes function/line information */
#define log_critical(...) \
    LOG_WRITE(BLADERF_LOG_LEVEL_CRITICAL, "[CRITICAL", __VA_ARGS__)

/** @} */

#ifdef LOG_INCLUDE_FILE_INFO
#   define LOG_WRITE(LEVEL, LEVEL_STRING, ...) \
    do { log_write(LEVEL, LEVEL_STRING  \
                     " @ "  __FILE__ ":" _LOG_STRINGIFY_(__LINE__) "] " \
                     __VA_ARGS__); \
    } while (0);
#else
#   define LOG_WRITE(LEVEL, LEVEL_STRING, ...) \
    do { log_write(LEVEL, LEVEL_STRING "] " __VA_ARGS__); } while (0);
#endif

/**
 * Writes a message to the logger with the specified log level. This function
 * should only be invoked indirectly through one of the log_*
 * convenience macros above to ensure consistent formatting of log messages.
 *
 * @param   level       The severity level of the message
 * @param   format      The printf-style format string
 * @param   ...         Optional values for format specifier substitution
 *
 * @return The number of characters written to the log
 */
int log_write(bladerf_log_level level, const char *format, ...);


/**
 * Sets the filter level for displayed log messages. Messages that are at
 * or above the specified log level will be written to the log output, while
 * messages with a lower log level will be suppressed. This function returns
 * the previous log level.
 *
 * @param   level       The new log level filter value
 *
 * @return The previous log level
 */
bladerf_log_level log_set_verbosity(bladerf_log_level level);

#endif
