/******************************************************************************
*                                                                             *
* License Agreement                                                           *
*                                                                             *
* Copyright (c) 2009 Altera Corporation, San Jose, California, USA.           *
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
******************************************************************************/
#include "system.h"

/*
 * This file implements the HAL Enhanced interrupt API for Nios II processors
 * with an internal interrupt controller (IIC). For most routines, this serves 
 * as a wrapper layer over the legacy interrupt API (which must be used with 
 * the IIC only). 
 *
 * Use of the enhanced API is recommended so that application and device
 * drivers are compatible with a Nios II system configured with an external 
 * interrupt controller (EIC), or IIC. This will afford maximum portability. 
 *
 * If an EIC is present, the EIC device driver must provide these routines, 
 * because their operation will be specific to that EIC type. 
 */
#ifndef NIOS2_EIC_PRESENT
#ifdef ALT_ENHANCED_INTERRUPT_API_PRESENT

#include "sys/alt_irq.h"
#include "priv/alt_iic_isr_register.h"
#include "priv/alt_legacy_irq.h"
                            
/** @Function Description:  This function registers an interrupt handler. 
  * If the function is succesful, then the requested interrupt will be enabled upon 
  * return. Registering a NULL handler will disable the interrupt.
  * @API Type:              External
  * @param ic_id            Ignored.
  * @param irq              IRQ number
  * @return                 0 if successful, else error (-1)
  */
int alt_ic_isr_register(alt_u32 ic_id, alt_u32 irq, alt_isr_func isr, 
  void *isr_context, void *flags)
{
    return alt_iic_isr_register(ic_id, irq, isr, isr_context, flags);
}  

/** @Function Description:  This function enables a single interrupt.
  * @API Type:              External
  * @param ic_id            Ignored.
  * @param irq              IRQ number
  * @return                 0 if successful, else error (-1)
  */
int alt_ic_irq_enable (alt_u32 ic_id, alt_u32 irq)
{
    return alt_irq_enable(irq);
}

/** @Function Description:  This function disables a single interrupt.
  * @API Type:              External
  * @param ic_id            Ignored.
  * @param irq              IRQ number
  * @return                 0 if successful, else error (-1)
  */
int alt_ic_irq_disable(alt_u32 ic_id, alt_u32 irq)
{
    return alt_irq_disable(irq);
}

/** @Function Description:  This function to determine if corresponding
  *                         interrupt is enabled.
  * @API Type:              External
  * @param ic_id            Ignored.
  * @param irq              IRQ number
  * @return                 Zero if corresponding interrupt is disabled and
  *                         non-zero otherwise.
  */
alt_u32 alt_ic_irq_enabled(alt_u32 ic_id, alt_u32 irq)
{
    alt_u32 irq_enabled;

    NIOS2_READ_IENABLE(irq_enabled);

    return (irq_enabled & (1 << irq)) ? 1: 0;
}

#endif /* ALT_ENHANCED_INTERRUPT_API_PRESENT */
#endif /* NIOS2_EIC_PRESENT */
