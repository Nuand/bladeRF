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

#include "sys/alt_errno.h"
#include "sys/alt_alarm.h"
#include "os/alt_syscall.h"

/*
 * "alt_timezone" and "alt_resettime" are the values of the the reset time and
 * time zone set through the last call to settimeofday(). By default they are
 * zero initialised.
 */

extern struct timezone alt_timezone;
extern struct timeval  alt_resettime;

/*
 * Macro defining the number of micoseconds in a second.
 */

#define ALT_US (1000000)


/*
 * settimeofday() can be called to calibrate the system clock, so that 
 * subsequent calls to gettimeofday() will return the elapsed "wall clock" 
 * time.
 *
 * This is done by updating the global structures "alt_resettime" and 
 * "alt_timezone" so that an immediate call to gettimeofday() would return
 * the value specified by "t" and "tz". 
 *
 * Warning: if this function is called concurrently with a call to 
 * gettimeofday(), the value returned by gettimeofday() will be unreliable.  
 *
 * ALT_SETTIMEOFDAY is mapped onto the settimeofday() system call in 
 * alt_syscall.h
 */
 
int ALT_SETTIMEOFDAY (const struct timeval  *t,
                      const struct timezone *tz)
{
  alt_u32 nticks    = alt_nticks ();
  alt_u32 tick_rate = alt_ticks_per_second ();

  /* If there is a system clock available, update the current time */

  if (tick_rate)
  {
    alt_resettime.tv_sec  = t->tv_sec - nticks/tick_rate;
    alt_resettime.tv_usec = t->tv_usec - 
      ((nticks*(ALT_US/tick_rate))%ALT_US);

    alt_timezone.tz_minuteswest = tz->tz_minuteswest;
    alt_timezone.tz_dsttime     = tz->tz_dsttime;
    
    return 0;
  }
  
  /* There's no system clock available */

  ALT_ERRNO = ENOSYS;
  return -1;
}
