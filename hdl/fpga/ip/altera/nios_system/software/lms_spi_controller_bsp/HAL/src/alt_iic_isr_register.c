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
 * Provides an interrupt registry mechanism for the any CPUs internal interrupt
 * controller (IIC) when the enhanced interrupt API is active.
 */
#ifndef ALT_CPU_EIC_PRESENT
#ifdef ALT_ENHANCED_INTERRUPT_API_PRESENT

#include "alt_types.h"
#include "sys/alt_irq.h"
#include "priv/alt_iic_isr_register.h"

/*
 * The header, alt_irq_entry.h, contains the exception entry point, and is
 * provided by the processor component. It is included here, so that the code
 * will be added to the executable only if alt_irq_register() is present, i.e.
 * if no interrupts are registered - there's no need to provide any 
 * interrupt handling.
 */

#include "sys/alt_irq_entry.h"

/*
 * The header, alt_irq_table.h contains a table describing which function
 * handles each interrupt.
 */

#include "priv/alt_irq_table.h"

/** @Function Description:  This function registers an interrupt handler. 
  * If the function is succesful, then the requested interrupt will be enabled
  * upon return. Registering a NULL handler will disable the interrupt.
  *
  * @API Type:              External
  * @param ic_id            Interrupt controller ID
  * @param irq              IRQ ID number
  * @param isr              Pointer to interrupt service routine
  * @param isr_context      Opaque pointer passed to ISR
  * @param flags            
  * @return                 0 if successful, else error (-1)
  */
int alt_iic_isr_register(alt_u32 ic_id, alt_u32 irq, alt_isr_func isr, 
  void *isr_context, void *flags)
{
  int rc = -EINVAL;  
  int id = irq;             /* IRQ interpreted as the interrupt ID. */
  alt_irq_context status;

  if (id < ALT_NIRQ)
  {
    /* 
     * interrupts are disabled while the handler tables are updated to ensure
     * that an interrupt doesn't occur while the tables are in an inconsistant
     * state.
     */

    status = alt_irq_disable_all();

    alt_irq[id].handler = isr;
    alt_irq[id].context = isr_context;

    rc = (isr) ? alt_ic_irq_enable(ic_id, id) : alt_ic_irq_disable(ic_id, id);

    alt_irq_enable_all(status);
  }

  return rc; 
}

#endif /* ALT_ENHANCED_INTERRUPT_API_PRESENT */
#endif /* ALT_CPU_EIC_PRESENT */
