/*
 
 author: jbenet
 os x, compile with: gcc -o testo test.c
 linux, compile with: gcc -o testo test.c -lrt
 
 source: https://gist.github.com/jbenet/1087739
 change for bladerf: maggo1404 (Marco Schwan)
 
 */

#ifndef __MACH__
#   error "This file is intended for use with Mac OS X systems only."
#endif

#include <time.h>
#include <sys/time.h>
#include <stdio.h>

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

#include "clock_gettime.h"


int clock_gettime(clockid_t clk_id, struct timespec *tp)
{
    if (clk_id != CLOCK_REALTIME) {
        return -1;
    }

    clock_serv_t cclock;
    mach_timespec_t mts;
    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);
    tp->tv_sec = mts.tv_sec;
    tp->tv_nsec = mts.tv_nsec;

    return 0;
}

