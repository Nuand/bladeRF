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

#include <sys/types.h>

#include "os/alt_syscall.h"

#include "sys/alt_irq.h"
#include "sys/alt_stack.h"

#include "system.h"

/*
 * sbrk() is called to dynamically extend the data segment for the application.
 * Thie input argument "incr" is the size of the block to allocate.
 *
 * This simple implementation does not perform any bounds checking. Memory will
 * be allocated, even if the request region colides with the stack or overflows
 * the available physical memory. 
 *
 * ALT_SBRK is mapped onto the sbrk() system call in alt_syscall.h
 *
 * This function is called by the profiling code to allocate memory so must be
 * safe if called from an interrupt context.  It must also not be instrumented
 * because that would lead to an infinate loop.
 */

extern char __alt_heap_start[]; /* set by linker */
extern char __alt_heap_limit[]; /* set by linker */

static char *heap_end = __alt_heap_start;

#if defined(ALT_EXCEPTION_STACK) && defined(ALT_STACK_CHECK)
char * alt_exception_old_stack_limit = NULL;
#endif
 
caddr_t ALT_SBRK (int incr) __attribute__ ((no_instrument_function ));

caddr_t ALT_SBRK (int incr)
{ 
  alt_irq_context context;
  char *prev_heap_end; 

  context = alt_irq_disable_all();

  /* Always return data aligned on a word boundary */
  heap_end = (char *)(((unsigned int)heap_end + 3) & ~3);

#ifdef ALT_MAX_HEAP_BYTES
  /*  
   * User specified a maximum heap size.  Return -1 if it would
   * be exceeded by this sbrk call.
   */
  if (((heap_end + incr) - __alt_heap_start) > ALT_MAX_HEAP_BYTES) {
    alt_irq_enable_all(context);
    return (caddr_t)-1;
  }
#else
  if ((heap_end + incr) > __alt_heap_limit) {
    alt_irq_enable_all(context);
    return (caddr_t)-1;
  }
#endif

  prev_heap_end = heap_end; 
  heap_end += incr; 

#ifdef ALT_STACK_CHECK
  /*
   * If the stack and heap are contiguous then extending the heap reduces the
   * space available for the stack.  If we are still using the default stack
   * then adjust the stack limit to note this, while checking for stack
   * pointer overflow. 
   * If the stack limit isn't pointing at the top of the heap then the code
   * is using a different stack so none of this needs to be done.
   */

  if (alt_stack_limit() == prev_heap_end)
  {
    if (alt_stack_pointer() <= heap_end)
      alt_report_stack_overflow();

    alt_set_stack_limit(heap_end);
  }

#ifdef ALT_EXCEPTION_STACK
  /*
   * If we are executing from the exception stack then compare against the
   * stack we switched away from as well.  The exception stack is a fixed
   * size so doesn't need to be checked.
   */

  if (alt_exception_old_stack_limit == prev_heap_end)
  {
    if (alt_exception_old_stack_limit <= heap_end)
      alt_report_stack_overflow();

    alt_exception_old_stack_limit = heap_end;
  }
#endif

#endif

  alt_irq_enable_all(context);

  return (caddr_t) prev_heap_end; 
} 
