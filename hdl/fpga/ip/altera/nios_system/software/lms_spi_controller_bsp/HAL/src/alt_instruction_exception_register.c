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
#include "alt_types.h"
#include "system.h"

/*
 * This file implements support for calling user-registered handlers for
 * instruction-generated exceptions.
 *
 * The registry API is optionally enabled through the "Enable
 * Instruction-related Exception API" HAL BSP setting, which will
 * define the macro below.
 */
#ifdef ALT_INCLUDE_INSTRUCTION_RELATED_EXCEPTION_API

/*
 * The header, alt_exception_handler_registry.h contains a struct describing
 * the registered exception handler
 */
#include "priv/alt_exception_handler_registry.h"

/*
 * Pull in the exception entry assembly code. This will not be linked in 
 * unless this object is linked into the executable (i.e. only if 
 * alt_instruction_exception_register() is called).
 */
__asm__( "\n\t.globl alt_exception" );

/*
 * alt_instruction_exception_register() is called to register a handler to
 * service instruction-generated exceptions that are not handled by the
 * default exception handler code (interrupts, and optionally unimplemented
 * instructions and traps). 
 * 
 * Passing null (0x0) in the handler argument will disable a previously-
 * registered handler.
 *
 * Note that if no handler is registered, exceptions that are not processed
 * using the built-in handler (interrupts, and optionally unimplemented
 * instructions and traps) are treated as unknown exceptions, resulting
 * in either a break or an infinite loop.
 */
void alt_instruction_exception_register (
  alt_exception_result (*exception_handler)(
    alt_exception_cause cause,
    alt_u32 exception_pc,
    alt_u32 bad_addr) )
{
  alt_instruction_exception_handler = exception_handler;
}

#endif /* ALT_INCLUDE_INSTRUCTION_RELATED_EXCEPTION_API */
