/******************************************************************************
*                                                                             *
* License Agreement                                                           *
*                                                                             *
* Copyright (c) 2003-2008 Altera Corporation, San Jose, California, USA.      *
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
 * This is the exception entry point code, which saves all the caller saved
 * registers and then handles the appropriate exception.  It should be pulled
 * in using a .globl from all the exception handler routines.  This scheme is
 * used so that if an interrupt is never registered, then this code will not
 * appear in the generated executable, thereby improving code footprint.
 *
 * If an external interrpt controller (EIC) is present, it will supply an 
 * interrupt vector address to the processor when an interrupt occurs. For 
 * The Altera Vectored Interrupt Controller (VIC) driver will establish a 
 * vector table and the processor will jump directly to the appropriate 
 * table entry, funnel routine, and then user ISR. This will bypass this code 
 * in entirety. This code might still be linked into a system with an EIC, 
 * but would then be used only for non-interrupt exceptions.
 */

        /*
         * Explicitly allow the use of r1 (the assembler temporary register)
         * within this code. This register is normally reserved for the use of
         * the assembler.
         */
        .set noat

        /*
         * The top and bottom of the exception stack
         */
#ifdef ALT_EXCEPTION_STACK

        .globl __alt_exception_stack_pointer

#ifdef ALT_STACK_CHECK

        .globl __alt_exception_stack_limit

        /*
         * We need to store the value of the stack limit after interrupt somewhere.
         */
        .globl  alt_exception_old_stack_limit

#endif
#endif

        .section .exceptions.entry.label, "xa"

        .globl alt_exception
        .type alt_exception, @function
alt_exception:

        .section .exceptions.entry, "xa"

#ifdef ALT_EXCEPTION_STACK

#ifdef ALT_STACK_CHECK
        stw   et, %gprel(alt_exception_old_stack_limit)(gp)
#endif

        movhi et, %hiadj(__alt_exception_stack_pointer - 80)
        addi  et, et, %lo(__alt_exception_stack_pointer - 80) 
        stw   sp, 76(et)
        mov   sp, et

#ifdef ALT_STACK_CHECK
        movhi et, %hiadj(__alt_exception_stack_limit)
        addi  et, et, %lo(__alt_exception_stack_limit) 
        stw   et, %gprel(alt_stack_limit_value)(gp)
#endif

#else
        /* 
         * Process an exception.  For all exceptions we must preserve all
         * caller saved registers on the stack (See the Nios2 ABI
         * documentation for details).
         */

        addi  sp, sp, -76

#ifdef ALT_STACK_CHECK

        bltu  sp, et, .Lstack_overflow

#endif

#endif

        stw   ra,  0(sp)

        /*
         * Leave a gap in the stack frame at 4(sp) for the muldiv handler to
         * store zero into.
         */

        stw   r1,   8(sp)
        stw   r2,  12(sp)
        stw   r3,  16(sp)
        stw   r4,  20(sp)
        stw   r5,  24(sp)
        stw   r6,  28(sp)
        stw   r7,  32(sp)

        rdctl r5, estatus

        stw   r8,  36(sp)
        stw   r9,  40(sp)
        stw   r10, 44(sp)
        stw   r11, 48(sp)
        stw   r12, 52(sp)
        stw   r13, 56(sp)
        stw   r14, 60(sp)
        stw   r15, 64(sp)

        /*
         * ea-4 contains the address of the instruction being executed
         * when the exception occured. For interrupt exceptions, we will
         * will be re-issue the isntruction. Store it in 72(sp)
         */
        stw   r5,  68(sp)  /* estatus */
        addi  r15, ea, -4  /* instruction that caused exception */
        stw   r15,  72(sp)

        /*
         * The interrupt testing code (.exceptions.irqtest) will be
         * linked here. If the Internal Interrupt Controller (IIC) is
         * present (an EIC is not present), the presense of an interrupt
         * is determined by examining CPU control registers or an interrupt
         * custom instruction, if present. 
         * 
         * If the IIC is used and an interrupt is active, the code linked 
         * here will call the HAL IRQ handler (alt_irq_handler()) which 
         * successively calls registered interrupt handler(s) until no 
         * interrupts remain pending. It then jumps to .exceptions.exit. If 
         * there is no interrupt then it continues to .exception.notirq, below.
         */

        .section .exceptions.notirq, "xa"

        /*
         * Prepare to service unimplemtned instructions or traps,
         * each of which is optionally inked into section .exceptions.soft,
         * which will preceed .exceptions.unknown below.
         *
         * Unlike interrupts, we want to skip the exception-causing instructon
         * upon completion, so we write ea (address of instruction *after*
         * the one where the exception occured) into 72(sp). The actual
         * instruction that caused the exception is written in r2, which these
         * handlers will utilize.
         */
        stw   ea,  72(sp)  /* Don't re-issue */
        ldw   r2, -4(ea)   /* Instruction that caused exception */

        /*
         * Other exception handling code, if enabled, will be linked here.
         * This includes unimplemted (multiply/divide) instruction support
         * (a BSP generaton option), and a trap handler (that would typically
         * be augmented with user-specific code). These are not linked in by
         * default.
         */

        /*
         * In the context of linker sections, "unknown" are all exceptions
         * not handled by the built-in handlers above (interupt, and trap or
         * unimplemented instruction decoding, if enabled).
         *
         * Advanced exception types can be serviced by registering a handler.
         * To do so, enable the "Enable Instruction-related Exception API" HAL
         * BSP setting. If this setting is disabled, this handler code will
         * either break (if the debug core is present) or enter an infinite
         * loop because we don't how how to handle the exception.
         */
        .section .exceptions.unknown
#ifdef ALT_INCLUDE_INSTRUCTION_RELATED_EXCEPTION_API
        /*
         * The C-based HAL routine alt_instruction_exception_entry() will
         * attempt to service the exception by calling a user-registered
         * exception handler using alt_instruction_exception_register().
         * If no handler was registered it will either break (if the
         * debugger is present) or go into an infinite loop since the
         * handling behavior is undefined; in that case we will not return here.
         */

        /* Load exception-causing address as first argument (r4) */
        addi   r4, ea, -4

        /* Call the instruction-exception entry */
        call   alt_instruction_exception_entry

        /*
         * If alt_instruction_exception_entry() returned, the exception was
         * serviced by a user-registered routine. Its return code (now in r2)
         * indicates whether to re-issue or skip the exception-causing
         * instruction
         *
         * Return code was 0: Skip. The instruction after the exception is
         * already stored in 72(sp).
         */
        bne   r2, r0, .Lexception_exit

        /*
         * Otherwise, modify 72(sp) to re-issue the instruction that caused the
         * exception.
         */
        addi  r15, ea, -4  /* instruction that caused exception */
        stw   r15,  72(sp)

#else /* ALT_INCLUDE_INSTRUCTION_RELATED_EXCEPTION_API disabled */

        /*
         * We got here because an instruction-related exception occured, but the
         * handler API was not compiled in. We do not presume to know how to
         * handle it. If the debugger is present, break, otherwise hang.
         *
         *  If you get here then one of the following could have happened:
         *
         *  - An instruction-generated exception occured, and the processor
         *    does not have the extra exceptions feature enabled, or you
         *    have not registered a handler using
         *    alt_instruction_exception_register()
         *
         *  Some examples of instruction-generated exceptions and why they
         *  might occur:
         *
         *  - Your program could have been compiled for a full-featured
         *    Nios II core, but it is running on a smaller core, and
         *    instruction emulation has been disabled by defining
         *    ALT_NO_INSTRUCTION_EMULATION.
         *
         *    You can work around the problem by re-enabling instruction
         *    emulation, or you can figure out why your program is being
         *    compiled for a system other than the one that it is running on.
         *
         *  - Your program has executed a trap instruction, but has not
         *    implemented a handler for this instruction.
         *
         *  - Your program has executed an illegal instruction (one which is
         *    not defined in the instruction set).
         *
         *  - Your processor includes an MMU or MPU, and you have enabled it
         *    before registering an exception handler to service exceptions it
         *    generates.
         *
         * The problem could also be hardware related:
         *  - If your hardware is broken and is generating spurious interrupts
         *    (a peripheral which negates its interrupt output before its
         *    interrupt handler has been executed will cause spurious
         *    interrupts)
         */
#ifdef NIOS2_HAS_DEBUG_STUB
       /*
        *  Either tell the user now (if there is a debugger attached) or go into
        *  the debug monitor which will loop until a debugger is attached.
        */
        break
#else
       /*
        *  If there is no debug stub then a BREAK will probably cause a reboot.
        *  An infinate loop will probably be more useful.
        */
0:
        br   0b
#endif /* NIOS2_HAS_DEBUG_STUB */

#endif /* ALT_INCLUDE_INSTRUCTION_RELATED_EXCEPTION_API */

        .section .exceptions.exit.label
.Lexception_exit:

        .section .exceptions.exit, "xa"

        /* 
         * Restore the saved registers, so that all general purpose registers 
         * have been restored to their state at the time the interrupt occured.
         */

        ldw   r5,  68(sp)
        ldw   ea,  72(sp)  /* This becomes the PC once eret is executed */
        ldw   ra,   0(sp)

        wrctl estatus, r5

        ldw   r1,   8(sp)
        ldw   r2,  12(sp)
        ldw   r3,  16(sp)
        ldw   r4,  20(sp)
        ldw   r5,  24(sp)
        ldw   r6,  28(sp)
        ldw   r7,  32(sp)

#ifdef ALT_EXCEPTION_STACK
#ifdef ALT_STACK_CHECK
        ldw   et, %gprel(alt_exception_old_stack_limit)(gp)
#endif
#endif

        ldw   r8,  36(sp)
        ldw   r9,  40(sp)
        ldw   r10, 44(sp)
        ldw   r11, 48(sp)
        ldw   r12, 52(sp)
        ldw   r13, 56(sp)
        ldw   r14, 60(sp)
        ldw   r15, 64(sp)

#ifdef ALT_EXCEPTION_STACK

#ifdef ALT_STACK_CHECK
        stw   et, %gprel(alt_stack_limit_value)(gp)
        stw   zero, %gprel(alt_exception_old_stack_limit)(gp)
#endif

        ldw   sp,  76(sp)

#else
        addi  sp, sp, 76

#endif

        /*
         * Return to the interrupted instruction.
         */

        eret

#ifdef ALT_STACK_CHECK

.Lstack_overflow:
        break 3

#endif

