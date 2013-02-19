/******************************************************************************
*                                                                             *
* License Agreement                                                           *
*                                                                             *
* Copyright (c) 2003-2004 Altera Corporation, San Jose, California, USA.      *
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
 * This is the interrupt exception entry point code, which saves all the
 * registers and calls the interrupt handler.  It should be pulled in using
 * a .globl from alt_irq_register.c.  This scheme is used so that if an
 * interrupt is never registered, then this code will not appear in the
 * generated executable, thereby improving code footprint.
 */

        /*
         * Explicitly allow the use of r1 (the assembler temporary register)
         * within this code. This register is normally reserved for the use of
         * the compiler.
         */
        .set noat

        /*
         * Pull in the exception handler register save code.
         */
        .globl alt_exception

        .globl alt_irq_entry
        .section .exceptions.entry.label, "xa"
alt_irq_entry:

        /*
         * Section .exceptions.entry is in alt_exception_entry.S
         * This saves all the caller saved registers and reads estatus into r5
         */

        .section .exceptions.irqtest, "xa"

#ifdef ALT_CI_INTERRUPT_VECTOR_N
        /*
         * Use the interrupt vector custom instruction if present to accelerate
         * this code.
         * If the interrupt vector custom instruction returns a negative
         * value, there are no interrupts active (estatus.pie is 0
         * or ipending is 0) so assume it is a software exception.
         */
        custom ALT_CI_INTERRUPT_VECTOR_N, r4, r0, r0
        blt r4, r0, .Lnot_irq
#else
        /*
         * Test to see if the exception was a software exception or caused 
         * by an external interrupt, and vector accordingly.
         */
        rdctl r4, ipending
        andi  r2, r5, 1
        beq   r2, zero, .Lnot_irq
        beq   r4, zero, .Lnot_irq
#endif /* ALT_CI_INTERRUPT_VECTOR_N */

        .section .exceptions.irqhandler, "xa"
        /*
         * Now that all necessary registers have been preserved, call 
         * alt_irq_handler() to process the interrupts.
         */

        call alt_irq_handler

        .section .exceptions.irqreturn, "xa"

        br    .Lexception_exit

        .section .exceptions.notirq.label, "xa"

.Lnot_irq:

        /*
         * Section .exceptions.exit is in alt_exception_entry.S
         * This restores all the caller saved registers
         */

        .section .exceptions.exit.label
.Lexception_exit:

