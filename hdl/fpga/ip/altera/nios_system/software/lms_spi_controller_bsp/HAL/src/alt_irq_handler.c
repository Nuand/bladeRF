/******************************************************************************
*                                                                             *
* License Agreement                                                           *
*                                                                             *
* Copyright (c) 2009      Altera Corporation, San Jose, California, USA.      *
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
#include "system.h"

/*
 * This interrupt handler only works with an internal interrupt controller
 * (IIC). Processors with an external interrupt controller (EIC) use an 
 * implementation provided by an EIC driver.
 */
#ifndef ALT_CPU_EIC_PRESENT

#include "sys/alt_irq.h"
#include "os/alt_hooks.h"

#include "alt_types.h"

/*
 * A table describing each interrupt handler. The index into the array is the
 * interrupt id associated with the handler. 
 *
 * When an interrupt occurs, the associated handler is called with
 * the argument stored in the context member.
 */
struct ALT_IRQ_HANDLER
{
#ifdef ALT_ENHANCED_INTERRUPT_API_PRESENT
  void (*handler)(void*);
#else
  void (*handler)(void*, alt_u32);
#endif
  void *context;
} alt_irq[ALT_NIRQ];

/*
 * alt_irq_handler() is called by the interrupt exception handler in order to 
 * process any outstanding interrupts. 
 *
 * It is defined here since it is linked in using weak linkage. 
 * This means that if there is never a call to alt_irq_register() (above) then
 * this function will not get linked in to the executable. This is acceptable
 * since if no handler is ever registered, then an interrupt can never occur.
 *
 * If Nios II interrupt vector custom instruction exists, use it to accelerate
 * the dispatch of interrupt handlers.  The Nios II interrupt vector custom 
 * instruction is present if the macro ALT_CI_INTERRUPT_VECTOR defined.
 */

void alt_irq_handler (void) __attribute__ ((section (".exceptions")));
void alt_irq_handler (void)
{
#ifdef ALT_CI_INTERRUPT_VECTOR
  alt_32 offset;
  char*  alt_irq_base = (char*)alt_irq;
#else
  alt_u32 active;
  alt_u32 mask;
  alt_u32 i;
#endif /* ALT_CI_INTERRUPT_VECTOR */
  
  /*
   * Notify the operating system that we are at interrupt level.
   */ 
  
  ALT_OS_INT_ENTER();

#ifdef ALT_CI_INTERRUPT_VECTOR
  /*
   * Call the interrupt vector custom instruction using the 
   * ALT_CI_INTERRUPT_VECTOR macro.
   * It returns the offset into the vector table of the lowest-valued pending
   * interrupt (corresponds to highest priority) or a negative value if none.
   * The custom instruction assumes that each table entry is eight bytes.
   */
  while ((offset = ALT_CI_INTERRUPT_VECTOR) >= 0) {
    struct ALT_IRQ_HANDLER* handler_entry = 
      (struct ALT_IRQ_HANDLER*)(alt_irq_base + offset);
#ifdef ALT_ENHANCED_INTERRUPT_API_PRESENT
    handler_entry->handler(handler_entry->context);
#else
    handler_entry->handler(handler_entry->context, offset >> 3);
#endif
  }
#else /* ALT_CI_INTERRUPT_VECTOR */
  /* 
   * Obtain from the interrupt controller a bit list of pending interrupts,
   * and then process the highest priority interrupt. This process loops, 
   * loading the active interrupt list on each pass until alt_irq_pending() 
   * return zero.
   * 
   * The maximum interrupt latency for the highest priority interrupt is
   * reduced by finding out which interrupts are pending as late as possible.
   * Consider the case where the high priority interupt is asserted during
   * the interrupt entry sequence for a lower priority interrupt to see why
   * this is the case.
   */

  active = alt_irq_pending ();

  do
  {
    i = 0;
    mask = 1;

    /*
     * Test each bit in turn looking for an active interrupt. Once one is 
     * found, the interrupt handler asigned by a call to alt_irq_register() is
     * called to clear the interrupt condition.
     */

    do
    {
      if (active & mask)
      { 
#ifdef ALT_ENHANCED_INTERRUPT_API_PRESENT
        alt_irq[i].handler(alt_irq[i].context); 
#else
        alt_irq[i].handler(alt_irq[i].context, i); 
#endif
        break;
      }
      mask <<= 1;
      i++;

    } while (1);

    active = alt_irq_pending ();
    
  } while (active);
#endif /* ALT_CI_INTERRUPT_VECTOR */

  /*
   * Notify the operating system that interrupt processing is complete.
   */ 

  ALT_OS_INT_EXIT();
}

#endif /* ALT_CPU_EIC_PRESENT */
