#ifndef WIN_CLOCK_GETTIME_H_
#define WIN_CLOCK_GETTIME_H_

#include <pthread.h>

#ifndef WIN32
#   error "This file is intended for use with WIN32 systems only."
#endif

typedef int clockid_t;
#define CLOCK_REALTIME 0

int clock_gettime(clockid_t clk_id, struct timespec *tp);

#endif
