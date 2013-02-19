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
#include "sys/alt_sim.h"
#include "os/alt_hooks.h"
#include "os/alt_syscall.h"

#include "alt_types.h"
#include "sys/alt_log_printf.h"
/*
 * _exit() is called by exit() in order to terminate the current process. 
 * Typically this is called when main() completes. It should never return. 
 * Since there is nowhere to go once this process completes, this 
 * implementation simply blocks forever.
 *
 * Note that interrupts are not disabled so that execution outside of this
 * thread is allowed to continue. 
 *
 * ALT_EXIT is mapped onto the _exit() system call in alt_syscall.h
 */

void ALT_EXIT (int exit_code)
{
  /* ALT_LOG - please see HAL/inc/alt_log_printf.h for details */
  ALT_LOG_PRINT_BOOT("[alt_exit.c] Entering _exit() function.\r\n");
  ALT_LOG_PRINT_BOOT("[alt_exit.c] Exit code from main was %d.\r\n",exit_code);
  /* Stop all other threads */

  ALT_LOG_PRINT_BOOT("[alt_exit.c] Calling ALT_OS_STOP().\r\n");
  ALT_OS_STOP();

  /* Provide notification to the simulator that we've stopped */

  ALT_LOG_PRINT_BOOT("[alt_exit.c] Calling ALT_SIM_HALT().\r\n");
  ALT_SIM_HALT(exit_code);

  /* spin forever, since there's no where to go back to */

  ALT_LOG_PRINT_BOOT("[alt_exit.c] Spinning forever.\r\n");
  while (1);
}
