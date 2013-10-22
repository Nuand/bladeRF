#ifdef LOGGING_ENABLED
#include <log.h>
#include <stdio.h>
#include <stdarg.h>

static bladerf_log_level filter_level = BLADERF_LOG_LEVEL_INFO;

void log_write(bladerf_log_level level, const char *format, ...)
{
    /* Only process this message if its level exceeds the current threshold */
    if (level >= filter_level)
    {
        va_list args;

        /* Write the log message */
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
    }
}

void log_set_verbosity(bladerf_log_level level)
{
    filter_level = level;
}
#endif
