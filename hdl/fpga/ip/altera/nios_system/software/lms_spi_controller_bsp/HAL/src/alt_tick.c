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

#include "sys/alt_irq.h"
#include "sys/alt_alarm.h"
#include "os/alt_hooks.h"
#include "alt_types.h"

/*
 * "_alt_tick_rate" is used to store the value of the system clock frequency
 * in ticks per second. It is initialised to zero, which corresponds to there
 * being no system clock facility available. 
 */

alt_u32 _alt_tick_rate = 0;

/*
 * "_alt_nticks" is the number of system clock ticks that have elapsed since
 * reset. 
 */

volatile alt_u32 _alt_nticks = 0;

/*
 * "alt_alarm_list" is the head of a linked list of registered alarms. This is
 * initialised to be an empty list.
 */

ALT_LLIST_HEAD(alt_alarm_list);

/*
 * alt_alarm_stop() is called to remove an alarm from the list of registered 
 * alarms. Alternatively an alarm can unregister itself by returning zero when 
 * the alarm executes.
 */

void alt_alarm_stop (alt_alarm* alarm)
{
  alt_irq_context irq_context;

  irq_context = alt_irq_disable_all();
  alt_llist_remove (&alarm->llist);
  alt_irq_enable_all (irq_context);
}

/*
 * alt_tick() is periodically called by the system clock driver in order to
 * process the registered list of alarms. Each alarm is registed with a
 * callback interval, and a callback function, "callback". 
 *
 * The return value of the callback function indicates how many ticks are to
 * elapse until the next callback. A return value of zero indicates that the
 * alarm should be deactivated. 
 * 
 * alt_tick() is expected to run at interrupt level.
 */

void alt_tick (void)
{
  alt_alarm* next;
  alt_alarm* alarm = (alt_alarm*) alt_alarm_list.next;

  alt_u32    next_callback;

  /* update the tick counter */

  _alt_nticks++;

  /* process the registered callbacks */

  while (alarm != (alt_alarm*) &alt_alarm_list)
  {
    next = (alt_alarm*) alarm->llist.next;

    /* 
     * Upon the tick-counter rolling over it is safe to clear the 
     * roll-over flag; once the flag is cleared this (or subsequnt)
     * tick events are enabled to generate an alarm event. 
     */
    if ((alarm->rollover) && (_alt_nticks == 0))
    {
      alarm->rollover = 0;
    }
    
    /* if the alarm period has expired, make the callback */    
    if ((alarm->time <= _alt_nticks) && (alarm->rollover == 0))
    {
      next_callback = alarm->callback (alarm->context);

      /* deactivate the alarm if the return value is zero */

      if (next_callback == 0)
      {
        alt_alarm_stop (alarm);
      }
      else
      {
        alarm->time += next_callback;
        
        /* 
         * If the desired alarm time causes a roll-over, set the rollover
         * flag. This will prevent the subsequent tick event from causing
         * an alarm too early.
         */
        if(alarm->time < _alt_nticks)
        {
          alarm->rollover = 1;
        }
      }
    }
    alarm = next;
  }

  /* 
   * Update the operating system specific timer facilities.
   */

  ALT_OS_TIME_TICK();
}

