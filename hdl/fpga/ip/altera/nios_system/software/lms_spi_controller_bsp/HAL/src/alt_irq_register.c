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
 * This interrupt registry mechanism works with the Nios II internal interrupt 
 * controller (IIC) only. Systems with an external interrupt controller (EIC),
 * or those with the IIC who are using the enhanced interrupt API will
 * utilize the alt_ic_isr_register() routine to register an interrupt. 
 */
#ifndef NIOS2_EIC_PRESENT

#include "sys/alt_irq.h"
#include "priv/alt_legacy_irq.h"
#include "os/alt_hooks.h"

#include "alt_types.h"

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

/*
 * alt_irq_handler() is called to register an interrupt handler. If the 
 * function is succesful, then the requested interrupt will be enabled upon 
 * return. Registering a NULL handler will disable the interrupt.
 *
 * The return value is 0 if the interrupt handler was registered and the 
 * interrupt was enabled, otherwise it is negative.
 */
 
int alt_irq_register (alt_u32 id, 
                      void* context, 
                      alt_isr_func handler)
{
  int rc = -EINVAL;  
  alt_irq_context status;

  if (id < ALT_NIRQ)
  {
    /* 
     * interrupts are disabled while the handler tables are updated to ensure
     * that an interrupt doesn't occur while the tables are in an inconsistant
     * state.
     */

    status = alt_irq_disable_all ();

    alt_irq[id].handler = handler;
    alt_irq[id].context = context;

    rc = (handler) ? alt_irq_enable (id): alt_irq_disable (id);

    alt_irq_enable_all(status);
  }
  return rc; 
}
#endif /* NIOS2_EIC_PRESENT */

