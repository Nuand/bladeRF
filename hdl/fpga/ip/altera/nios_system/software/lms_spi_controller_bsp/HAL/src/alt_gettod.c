/******************************************************************************
*                                                                             *
* License Agreement                                                           *
*                                                                             *
* Copyright (c) 2004 Altera Corporation, San Jose, California, USA.           *
* All rights reserved.                                                        *
*                                                                             *
* Permission is hereby granted, free of charge, to any person obtaining a     *
* copy of this software and associated documentation files (the "Software"),  *
* to deal in the Software without restriction, including without limitation   *
* the rights to use, copy, modify, merge, publish, distribute, sublicense,    *
* and/or sell copies of the Software, and to permit persons to whom the       *
* Software is furnished to do so, subject to the following conditions:        *
*                                                                             *
* The above copyright notice and this permission notice shall be included in  *
* all copies or substantial portions of the Software.                         *
*                                                                             *
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR  *
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,    *
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE *
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER      *
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING     *
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER         *
* DEALINGS IN THE SOFTWARE.                                                   *
*                                                                             *
* This agreement shall be governed in all respects by the laws of the State   *
* of California and by the laws of the United States of America.              *
*                                                                             *
* Altera does not recommend, suggest or require that this reference design    *
* file be used in conjunction or combination with any other product.          *
******************************************************************************/

#include <sys/time.h>
#include <sys/times.h>
#include <errno.h>

#include "sys/alt_alarm.h"
#include "alt_types.h"
#include "os/alt_syscall.h"

/*
 * Macro defining the number of micoseconds in a second.
 */

#define ALT_US (1000000)

/*
 * "alt_timezone" and "alt_resettime" are the values of the the reset time and
 * time zone set through the last call to settimeofday(). By default they are
 * zero initialised.
 */

struct timezone alt_timezone = {0, 0};
struct timeval  alt_resettime = {0, 0};

/*
 * gettimeofday() can be called to obtain a time structure which indicates the
 * current "wall clock" time. This is calculated using the elapsed number of
 * system clock ticks, and the value of "alt_resettime" and "alt_timezone" set
 * through the last call to settimeofday().  
 *
 * Warning: if this function is called concurrently with a call to 
 * settimeofday(), the value returned by gettimeofday() will be unreliable. 
 *
 * ALT_GETTIMEOFDAY is mapped onto the gettimeofday() system call in 
 * alt_syscall.h
 */
 

#if defined (__GNUC__) && (__GNUC__ >= 4)
int ALT_GETTIMEOFDAY (struct timeval  *ptimeval, void *ptimezone_vptr)
{
  struct timezone *ptimezone = (struct timezone*)ptimezone_vptr;
#else
int ALT_GETTIMEOFDAY (struct timeval  *ptimeval, struct timezone *ptimezone)
{
#endif
  
  alt_u32 nticks = alt_nticks (); 
  alt_u32 tick_rate = alt_ticks_per_second ();

  /* 
   * Check to see if the system clock is running. This is indicated by a 
   * non-zero system clock rate. If the system clock is not running, an error
   * is generated and the contents of "ptimeval" and "ptimezone" are not
   * updated.
   */

  if (tick_rate)
  {
    ptimeval->tv_sec  = alt_resettime.tv_sec  + nticks/tick_rate;
    ptimeval->tv_usec = alt_resettime.tv_usec +
     (alt_u32)(((alt_u64)nticks*(ALT_US/tick_rate))%ALT_US);
      
    while(ptimeval->tv_usec < 0) {
      if (ptimeval->tv_sec <= 0)
      {
          ptimeval->tv_sec = 0;
          ptimeval->tv_usec = 0;
          break;
      }
      else
      {
          ptimeval->tv_sec--;
          ptimeval->tv_usec += ALT_US;
      }
    }
    
    while(ptimeval->tv_usec >= ALT_US) {
      ptimeval->tv_sec++;
      ptimeval->tv_usec -= ALT_US;
    }
      
    if (ptimezone)
    { 
      ptimezone->tz_minuteswest = alt_timezone.tz_minuteswest;
      ptimezone->tz_dsttime     = alt_timezone.tz_dsttime;
    }

    return 0;
  }

  return -ENOTSUP;
}

