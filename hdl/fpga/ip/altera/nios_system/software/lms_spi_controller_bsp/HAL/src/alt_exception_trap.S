/******************************************************************************
*                                                                             *
* License Agreement                                                           *
*                                                                             *
* Copyright (c) 2003-2005 Altera Corporation, San Jose, California, USA.      *
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

        /*
         * This is the trap exception handler for Nios2.
         */ 

        /*
         * Provide a label which can be used to pull this file in.
         */

        .section .exceptions.start
        .globl alt_exception_trap
alt_exception_trap:

        /*
         * Pull in the entry/exit code.
         */
        .globl alt_exception

        .section .exceptions.soft, "xa"

.Ltrap_handler:

        /*
         * Did a trap instruction cause the exception?
         *
         * The instruction which the exception occurred on has been loaded
         * into r2 by code in alt_exception_entry.S
         *
         */

        movhi r3,0x003b              /* upper half of trap opcode */
        ori   r3,r3,0x683a           /* lower half of trap opcode */
        bne   r2,r3,.Lnot_trap

        /*
         *  There is no trap handler defined here, and so executing a trap
         *  instruction causes a software break.  If you provide a trap handler,
         *  then you must replace the break instruction below with your handler.
         *  Your handler must preserve ea and the usual callee saved registers.
         */

        break

        br   .Lexception_exit

.Lnot_trap:


        .section .exceptions.exit.label
.Lexception_exit:


