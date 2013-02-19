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

#include <sys/times.h>

#include "sys/alt_errno.h"
#include "sys/alt_alarm.h"
#include "os/alt_syscall.h"

/*
 * The times() function is used by newlib to obtain elapsed time information.
 * The return value is the elapsed time since reset in system clock ticks. Note
 * that this is distinct from the strict Posix version of times(), which should
 * return the time since: 0 hours, 0 minutes, 0 seconds, January 1, 1970, GMT.
 *
 * The input structure is filled in with time accounting information. This 
 * implementation attributes all cpu time to the system.
 *
 * ALT_TIMES is mapped onto the times() system call in alt_syscall.h
 */
 
clock_t ALT_TIMES (struct tms *buf)
{
  clock_t ticks = alt_nticks(); 

  /* If there is no system clock present, generate an error */

  if (!alt_ticks_per_second())
  {
    ALT_ERRNO = ENOSYS;
    return 0;
  }

  /* Otherwise return the elapsed time */

  buf->tms_utime  = 0;
  buf->tms_stime  = ticks;
  buf->tms_cutime = 0;
  buf->tms_cstime = 0;

  return ticks;
}
