/******************************************************************************
*                                                                             *
* License Agreement                                                           *
*                                                                             *
* Copyright (c) 2008 Altera Corporation, San Jose, California, USA.           *
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
#include "sys/alt_exceptions.h"
#include "nios2.h"
#include "alt_types.h"
#include "system.h"

/*
 * This file implements support for calling user-registered handlers for
 * instruction-generated exceptions. This handler could also be reached
 * in the event of a spurious interrupt.
 *
 * The handler code is optionally enabled through the "Enable
 * Instruction-related Exception API" HAL BSP setting, which will
 * define the macro below.
 */
#ifdef ALT_INCLUDE_INSTRUCTION_RELATED_EXCEPTION_API

/* Function pointer to exception callback routine */
alt_exception_result (*alt_instruction_exception_handler)
  (alt_exception_cause, alt_u32, alt_u32) = 0x0;

/* Link entry routine to .exceptions section */
int alt_instruction_exception_entry (alt_u32 exception_pc)
  __attribute__ ((section (".exceptions")));

/*
 * This is the entry point for instruction-generated exceptions handling.
 * This routine will be called by alt_exceptions_entry.S, after it determines
 * that an exception could not be handled by handlers that preceed that
 * of instruction-generated exceptions (such as interrupts).
 *
 * For this to function properly, you must register an exception handler
 * using alt_instruction_exception_register(). This routine will call
 * that handler if it has been registered. Absent a handler, it will
 * break break or hang as discussed below.
 */
int alt_instruction_exception_entry (alt_u32 exception_pc)
{
  alt_u32 cause, badaddr;

/*
 * If the processor hardware has the optional EXCEPTIONS & BADADDR registers,
 * read them and pass their content to the user handler. These are always
 * present if the MMU or MPU is enabled, and optionally for other advanced
 * exception types via the "Extra exceptions information" setting in the
 * processor (hardware) configuration.
 *
 * If these registers are not present, the cause field will be set to
 * NIOS2_EXCEPTION_CAUSE_NOT_PRESENT. Your handling routine should
 * check the validity of the cause argument before proceeding.
 */
#ifdef NIOS2_HAS_EXTRA_EXCEPTION_INFO
  /* Get exception cause & "badaddr" */
  NIOS2_READ_EXCEPTION(cause);
  cause = ( (cause & NIOS2_EXCEPTION_REG_CAUSE_MASK) >>
              NIOS2_EXCEPTION_REG_CAUSE_OFST );

  NIOS2_READ_BADADDR(badaddr);
#else
  cause = NIOS2_EXCEPTION_CAUSE_NOT_PRESENT;
  badaddr = 0;
#endif /* NIOS2_HAS_EXTRA_EXCEPTION_INFO */

  if(alt_instruction_exception_handler) {
    /*
     * Call handler. Its return value indicates whether the exception-causing
     * instruction should be re-issued. The code that called us,
     * alt_eceptions_entry.S, will look at this value and adjust the ea
     * register as necessary
     */
    return alt_instruction_exception_handler(cause, exception_pc, badaddr);
  }
  /*
   * We got here because an instruction-generated exception occured, but no
   * handler is present. We do not presume to know how to handle it. If the
   * debugger is present, break, otherwise hang.
   *
   * If you've reached here in the debugger, consider examining the
   * EXCEPTIONS register cause bit-field, which was read into the 'cause'
   * variable above, and compare it against the exceptions-type enumeration
   * in alt_exceptions.h. This register is availabe if the MMU or MPU is
   * present, or if the "Extra exceptions information" hardware option is
   * selected.
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
   *    interrupt handler has been executed will cause spurious interrupts)
   */
  else {
#ifdef NIOS2_HAS_DEBUG_STUB
    NIOS2_BREAK();
#else
    while(1)
      ;
#endif /* NIOS2_HAS_DEBUG_STUB */
  }

  /* // We should not get here. Remove compiler warning. */
  return NIOS2_EXCEPTION_RETURN_REISSUE_INST;
}

#endif /* ALT_INCLUDE_INSTRUCTION_RELATED_EXCEPTION_API */

/*
 * This routine indicates whether a particular exception cause will have
 * set a valid address into the BADADDR register, which is included
 * in the arguments to a user-registered instruction-generated exception
 * handler. Many exception types do not set valid contents in BADADDR;
 * this is a convenience routine to easily test the validity of that
 * argument in your handler.
 *
 * Note that this routine will return false (0) for cause '12',
 * TLB miss. This is because there are four exception types that
 * share that cause, two of which do not have a valid BADADDR. You
 * must determine BADADDR's validity for these.
 *
 * Arguments:
 * cause:  The 5-bit exception cause field of the EXCEPTIONS register,
 *         shifted to the LSB position. You may pass the 'cause' argument
 *         in a handler you registered directy to this routine.
 *
 * Return: 1: BADADDR (bad_addr argument to handler) is valid
 *         0: BADADDR is not valid
 */
int alt_exception_cause_generated_bad_addr(alt_exception_cause cause)
{
  switch (cause) {
  case NIOS2_EXCEPTION_SUPERVISOR_ONLY_DATA_ADDR:
    return 1;
  case NIOS2_EXCEPTION_MISALIGNED_DATA_ADDR:
    return 1;
  case NIOS2_EXCEPTION_MISALIGNED_TARGET_PC:
    return 1;
  case NIOS2_EXCEPTION_TLB_READ_PERM_VIOLATION:
    return 1;
  case NIOS2_EXCEPTION_TLB_WRITE_PERM_VIOLATION:
    return 1;
  case NIOS2_EXCEPTION_MPU_DATA_REGION_VIOLATION:
    return 1;
  default:
    return 0;
  }
}
