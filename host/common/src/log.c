#include <log.h>
#include <stdio.h>
#include <stdarg.h>

static bladerf_log_level filter_level = BLADERF_LOG_LEVEL_INFO;

int bladerf_log(bladerf_log_level level, const char *format, ...)
{
    int ret = 0;

    // Only process this message if its level exceeds the current threshold
    if (level >= filter_level)
    {
        FILE *out_stream;
        va_list args;

        // Select output stream or error stream based on severity
        if (level >= BLADERF_LOG_LEVEL_WARNING)
            out_stream = stderr;
        else
            out_stream = stdout;

        va_start(args, format);

        // Write the log message
        ret = vfprintf(out_stream, format, args);

        va_end(args);

        // As a precaution, flush the stream if this is a warning priority
        // or higher
        if (level >= BLADERF_LOG_LEVEL_WARNING)
            fflush(out_stream);
    }

    return ret;
}

bladerf_log_level bladerf_log_set_verbosity(bladerf_log_level level)
{
    // Replace the old level with the new level and return the old level
    bladerf_log_level old_level = filter_level;
    filter_level = level;
    
    return old_level;
}

