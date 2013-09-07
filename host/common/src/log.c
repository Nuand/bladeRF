#include <log.h>
#include <stdio.h>
#include <stdarg.h>

static bladerf_log_level filter_level = BLADERF_LOG_LEVEL_VERBOSE;

int bladerf_log(bladerf_log_level level, const char *format, ...)
{
    int ret = 0;

    /* Only process this message if its level exceeds the current threshold */
    if (level >= filter_level)
    {
        va_list args;

        /* Write the log message */
        va_start(args, format);
        ret = vfprintf(stderr, format, args);
        va_end(args);
    }

    return ret;
}

bladerf_log_level bladerf_log_set_verbosity(bladerf_log_level level)
{
    /* Replace the old level with the new level and return the old level */
    bladerf_log_level old_level = filter_level;
    filter_level = level;

    return old_level;
}

