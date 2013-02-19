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

#include <errno.h>

#include "sys/alt_alarm.h"
#include "sys/alt_irq.h"

/*
 * alt_alarm_start is called to register an alarm with the system. The 
 * "alarm" structure passed as an input argument does not need to be 
 * initialised by the user. This is done within this function.
 *
 * The remaining input arguments are:
 *
 * nticks - The time to elapse until the alarm executes. This is specified in
 *          system clock ticks.
 * callback - The function to run when the indicated time has elapsed.
 * context  - An opaque value, passed to the callback function. 
*
 * Care should be taken when defining the callback function since it is 
 * likely to execute in interrupt context. In particular, this mean that 
 * library calls like printf() should not be made, since they can result in 
 * deadlock.
 *
 * The interval to be used for the next callback is the return
 * value from the callback function. A return value of zero indicates that the
 * alarm should be unregistered. 
 * 
 * alt_alarm_start() will fail if  the timer facility has not been enabled 
 * (i.e. there is no system clock). Failure is indicated by a negative return 
 * value.
 */ 

int alt_alarm_start (alt_alarm* alarm, alt_u32 nticks,
                     alt_u32 (*callback) (void* context),
                     void* context)
{
  alt_irq_context irq_context;
  alt_u32 current_nticks = 0;
  
  if (alt_ticks_per_second ())
  {
    if (alarm)
    {
      alarm->callback = callback;
      alarm->context  = context;
 
      irq_context = alt_irq_disable_all ();
      
      current_nticks = alt_nticks();
      
      alarm->time = nticks + current_nticks + 1; 
      
      /* 
       * If the desired alarm time causes a roll-over, set the rollover
       * flag. This will prevent the subsequent tick event from causing
       * an alarm too early.
       */
      if(alarm->time < current_nticks)
      {
        alarm->rollover = 1;
      }
      else
      {
        alarm->rollover = 0;
      }
    
      alt_llist_insert (&alt_alarm_list, &alarm->llist);
      alt_irq_enable_all (irq_context);

      return 0;
    }
    else
    {
      return -EINVAL;
    }
  }
  else
  {
    return -ENOTSUP;
  }
}
