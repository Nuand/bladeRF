/*
 * Copyright (c) 2003-2004 Altera Corporation, San Jose, California, USA.  
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to 
 * deal in the Software without restriction, including without limitation the 
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is 
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 * 
 * ------------
 *
 * Altera does not recommend, suggest or require that this reference design 
 * file be used in conjunction or combination with any other product.
 *
 * alt_busy_sleep.c - Microsecond delay routine which uses a calibrated busy
 *                    loop to perform the delay. This is used to implement
 *                    usleep for both uC/OS-II and the standalone HAL.  
 *
 * Author PRR
 *
 * Calibrated delay with no timer required
 * 
 * The ASM instructions in the routine are equivalent to 
 *
 * for (i=0;i<us*(ALT_CPU_FREQ/3);i++);
 * 
 * and takes three cycles each time around the loop
 *
 */

#include <limits.h>
#include <string.h>

#include "system.h"
#include "alt_types.h"

#include "priv/alt_busy_sleep.h"

unsigned int alt_busy_sleep (unsigned int us)
{
/*
 * Only delay if ALT_SIM_OPTIMIZE is not defined; i.e., if software
 * is built targetting ModelSim RTL simulation, the delay will be
 * skipped to speed up simulation.
 */
#ifndef ALT_SIM_OPTIMIZE
  int i;
  int big_loops;
  alt_u32 cycles_per_loop;
  
  if (!strcmp(NIOS2_CPU_IMPLEMENTATION,"tiny"))
  {
    cycles_per_loop = 9;
  }
  else  
  {
    cycles_per_loop = 3;
  }
  

  big_loops = us / (INT_MAX/
  (ALT_CPU_FREQ/(cycles_per_loop * 1000000)));

  if (big_loops)
  {
    for(i=0;i<big_loops;i++)
    {
      /*
      * Do NOT Try to single step the asm statement below 
      * (single step will never return)
      * Step out of this function or set a breakpoint after the asm statements
      */
      __asm__ volatile (
        "\n0:"
        "\n\taddi %0,%0, -1"
        "\n\tbne %0,zero,0b"
        "\n1:"
        "\n\t.pushsection .debug_alt_sim_info"
        "\n\t.int 4, 0, 0b, 1b"
        "\n\t.popsection"
        :: "r" (INT_MAX));
      us -= (INT_MAX/(ALT_CPU_FREQ/
      (cycles_per_loop * 1000000)));
    }

    /*
    * Do NOT Try to single step the asm statement below 
    * (single step will never return)
    * Step out of this function or set a breakpoint after the asm statements
    */
    __asm__ volatile (
      "\n0:"
      "\n\taddi %0,%0, -1"
      "\n\tbne %0,zero,0b"
      "\n1:"
      "\n\t.pushsection .debug_alt_sim_info"
      "\n\t.int 4, 0, 0b, 1b"
      "\n\t.popsection"
      :: "r" (us*(ALT_CPU_FREQ/(cycles_per_loop * 1000000))));
  }
  else
  {
    /*
    * Do NOT Try to single step the asm statement below 
    * (single step will never return)
    * Step out of this function or set a breakpoint after the asm statements
    */
    __asm__ volatile (
      "\n0:"
      "\n\taddi %0,%0, -1"
      "\n\tbgt %0,zero,0b"
      "\n1:"
      "\n\t.pushsection .debug_alt_sim_info"
      "\n\t.int 4, 0, 0b, 1b"
      "\n\t.popsection"
      :: "r" (us*(ALT_CPU_FREQ/(cycles_per_loop * 1000000))));
  }
#endif /* #ifndef ALT_SIM_OPTIMIZE */
  return 0;
}
