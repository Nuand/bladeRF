#ifndef WIN32
#   error "This file is intended for use with WIN32 systems only."
#endif

#include <Windows.h>
#include <errno.h>
#include "clock_gettime.h"
#include "ptw32_timespec.h"

int clock_gettime(clockid_t clk_id, struct timespec *tp)
{
    BOOL success;
    FILETIME file_time;
    SYSTEMTIME system_time;

    if (clk_id != CLOCK_REALTIME) {
        errno = EINVAL;
        return -1;
    }

    GetSystemTime(&system_time);
    success = SystemTimeToFileTime(&system_time, &file_time);

    if (!success) {
        /* For lack of a better or more compliant return value... */
        errno = EINVAL;
        return -1;
    }

    ptw32_filetime_to_timespec(&file_time, tp);

    return 0;
}

