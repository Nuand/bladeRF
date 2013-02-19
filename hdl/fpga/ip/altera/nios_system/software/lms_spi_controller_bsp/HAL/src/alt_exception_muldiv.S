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
         * This is the software multiply/divide handler for Nios2.
         */ 

        /*
         * Provide a label which can be used to pull this file in.
         */

        .section .exceptions.start
        .globl alt_exception_muldiv
alt_exception_muldiv:

        /*
         * Pull in the entry/exit code.
         */
        .globl alt_exception


        .section .exceptions.soft, "xa"


        /* INSTRUCTION EMULATION
        *  ---------------------
        *
        * Nios II processors generate exceptions for unimplemented instructions.
        * The routines below emulate these instructions.  Depending on the
        * processor core, the only instructions that might need to be emulated
        * are div, divu, mul, muli, mulxss, mulxsu, and mulxuu.
        *
        * The emulations match the instructions, except for the following
        * limitations:
        *
        * 1) The emulation routines do not emulate the use of the exception
        *    temporary register (et) as a source operand because the exception
        *    handler already has modified it.
        *
        * 2) The routines do not emulate the use of the stack pointer (sp) or the
        *    exception return address register (ea) as a destination because
        *    modifying these registers crashes the exception handler or the
        *    interrupted routine.
        *
        * 3) To save code size, the routines do not emulate the use of the
        *    breakpoint registers (ba and bt) as operands.
        *
        * Detailed Design
        * ---------------
        *
        * The emulation routines expect the contents of integer registers r0-r31
        * to be on the stack at addresses sp, 4(sp), 8(sp), ... 124(sp).  The
        * routines retrieve source operands from the stack and modify the
        * destination register's value on the stack prior to the end of the
        * exception handler.  Then all registers except the destination register
        * are restored to their previous values.
        *
        * The instruction that causes the exception is found at address -4(ea).
        * The instruction's OP and OPX fields identify the operation to be
        * performed.
        *
        * One instruction, muli, is an I-type instruction that is identified by
        * an OP field of 0x24.
        *
        * muli   AAAAA,BBBBB,IIIIIIIIIIIIIIII,-0x24-
        *           27    22                6      0    <-- LSB of field
        *
        * The remaining emulated instructions are R-type and have an OP field
        * of 0x3a.  Their OPX fields identify them.
        *
        * R-type AAAAA,BBBBB,CCCCC,XXXXXX,NNNNN,-0x3a-
        *           27    22    17     11     6      0  <-- LSB of field
        * 
        * 
        */


        /*
         * Split the instruction into its fields.  We need 4*A, 4*B, and 4*C as
         * offsets to the stack pointer for access to the stored register values.
         */
                             /* r2 = AAAAA,BBBBB,IIIIIIIIIIIIIIII,PPPPPP    */
        roli  r3, r2, 7      /* r3 = BBB,IIIIIIIIIIIIIIII,PPPPPP,AAAAA,BB   */
        roli  r4, r3, 3      /* r4 = IIIIIIIIIIIIIIII,PPPPPP,AAAAA,BBBBB    */
        roli  r6, r4, 2      /* r6 = IIIIIIIIIIIIII,PPPPPP,AAAAA,BBBBB,II   */
        srai  r4, r4, 16     /* r4 = (sign-extended) IMM16                  */
        xori  r6, r6, 0x42   /* r6 = CCC,XXXXXX,NNNNN,PPPPPP,AAAAA,bBBBB,cC */
        roli  r7, r6, 5      /* r7 = XXXX,NNNNN,PPPPPP,AAAAA,bBBBB,cCCCC,XX */
        andi  r5, r2, 0x3f   /* r5 = 00000000000000000000000000,PPPPPP      */
        xori  r3, r3, 0x40
        andi  r3, r3, 0x7c   /* r3 = 0000000000000000000000000,aAAAA,00     */
        andi  r6, r6, 0x7c   /* r6 = 0000000000000000000000000,bBBBB,00     */
        andi  r7, r7, 0x7c   /* r7 = 0000000000000000000000000,cCCCC,00     */

        /* Now either
         *  r5 = OP
         *  r3 = 4*(A^16)
         *  r4 = IMM16 (sign extended)
         *  r6 = 4*(B^16)
         *  r7 = 4*(C^16)
         * or
         *  r5 = OP
         */


        /*
         * Save everything on the stack to make it easy for the emulation routines
         * to retrieve the source register operands.  The exception entry code has
         * already saved some of this so we don't need to do it all again.
         */

        addi  sp, sp, -60
        stw   zero, 64(sp)   /* Save zero on stack to avoid special case for r0. */
                             /* Register at and r2-r15 have already been saved.  */

        stw   r16,  0(sp)
        stw   r17,  4(sp)
        stw   r18,  8(sp)
        stw   r19, 12(sp)
        stw   r20, 16(sp)
        stw   r21, 20(sp)
        stw   r22, 24(sp)
        stw   r23, 28(sp)
                            /* et @ 32 - Has already been changed.*/
                            /* bt @ 36 - Usually isn't an operand.   */
        stw   gp,  40(sp)
        stw   sp,  44(sp)
        stw   fp,  48(sp)
                            /* ea @ 52 - Don't bother to save - it's already been changed */
                            /* ba @ 56 - Breakpoint register usually isn't an operand */
                            /* ra @ 60 - Has already been saved */


        /*
         *  Prepare for either multiplication or division loop.
         *  They both loop 32 times.
         */
        movi   r14, 32


        /*
         * Get the operands.
         *
         * It is necessary to check for muli because it uses an I-type instruction
         * format, while the other instructions are have an R-type format.
         */
        add    r3, r3, sp     /* r3 = address of A-operand. */
        ldw    r3, 0(r3)      /* r3 = A-operand. */
        movi   r15, 0x24      /* muli opcode (I-type instruction format) */
        beq    r5, r15, .Lmul_immed /* muli doesn't use the B register as a source */

        add    r6, r6, sp     /* r6 = address of B-operand.               */
        ldw    r6, 0(r6)      /* r6 = B-operand.                          */
                              /* r4 = SSSSSSSSSSSSSSSS,-----IMM16------   */
                              /* IMM16 not needed, align OPX portion      */
                              /* r4 = SSSSSSSSSSSSSSSS,CCCCC,-OPX--,00000 */
        srli   r4, r4, 5      /* r4 = 00000,SSSSSSSSSSSSSSSS,CCCCC,-OPX-- */
        andi   r4, r4, 0x3f   /* r4 = 00000000000000000000000000,-OPX--   */

        /* Now
         * r5 = OP
         * r3 = src1
         * r6 = src2
         * r4 = OPX (no longer can be muli)
         * r7 = 4*(C^16)
         * r14 = loop counter
         */

        /* ILLEGAL-INSTRUCTION EXCEPTION
         *  -----------------------------
         *
         *  This code is for Nios II cores that generate exceptions when attempting
         *  to execute illegal instructions.  Nios II cores that support an
         *  illegal-instruction exception are identified by the presence of the
         *  macro definition NIOS2_HAS_ILLEGAL_INSTRUCTION_EXCEPTION in system.h .
         *
         *  Remember that illegal instructions are different than unimplemented
         *  instructions.  Illegal instructions are instruction encodings that
         *  have not been defined by the Nios II ISA.  Unimplemented instructions
         *  are legal instructions that must be emulated by some Nios II cores.
         *
         *  If we get here, all instructions except multiplies and divides
         *  are illegal.
         *
         *  This code assumes that OP is not muli (because muli was tested above).
         *  All other multiplies and divides are legal.  Anything else is illegal.
         */

        movi  r8, 0x3a                        /* OP for R-type mul* and div* */
        bne   r5, r8, .Lnot_muldiv

        /* r15 already is 0x24 */            /* OPX of divu */
        beq   r4, r15, .Ldivide

        movi  r15,0x27                        /* OPX of mul */
        beq   r4, r15, .Lmultiply

        movi  r15,0x07                        /* OPX of mulxuu */
        beq   r4, r15, .Lmultiply

        movi  r15,0x17                        /* OPX of mulxsu */
        beq   r4, r15, .Lmultiply

        movi  r15,0x1f                        /* OPX of mulxss */
        beq   r4, r15, .Lmultiply

        movi  r15,0x25                        /* OPX of div */
        bne   r4, r15, .Lnot_muldiv


        /* DIVISION
         *
         * Divide an unsigned dividend by an unsigned divisor using
         * a shift-and-subtract algorithm.  The example below shows
         * 43 div 7 = 6 for 8-bit integers.  This classic algorithm uses a
         * single register to store both the dividend and the quotient,
         * allowing both values to be shifted with a single instruction.
         *
         *                               remainder dividend:quotient
         *                               --------- -----------------
         *   initialize                   00000000     00101011:
         *   shift                        00000000     0101011:_
         *   remainder >= divisor? no     00000000     0101011:0
         *   shift                        00000000     101011:0_
         *   remainder >= divisor? no     00000000     101011:00
         *   shift                        00000001     01011:00_
         *   remainder >= divisor? no     00000001     01011:000
         *   shift                        00000010     1011:000_
         *   remainder >= divisor? no     00000010     1011:0000
         *   shift                        00000101     011:0000_
         *   remainder >= divisor? no     00000101     011:00000
         *   shift                        00001010     11:00000_
         *   remainder >= divisor? yes    00001010     11:000001
         *       remainder -= divisor   - 00000111
         *                              ----------
         *                                00000011     11:000001
         *   shift                        00000111     1:000001_
         *   remainder >= divisor? yes    00000111     1:0000011
         *       remainder -= divisor   - 00000111
         *                              ----------
         *                                00000000     1:0000011
         *   shift                        00000001     :0000011_
         *   remainder >= divisor? no     00000001     :00000110
         *
         * The quotient is 00000110.
         */

.Ldivide:
        /*
         *  Prepare for division by assuming the result
         *  is unsigned, and storing its "sign" as 0.
         */
        movi   r17, 0


        /* Which division opcode? */
        xori   r15, r4, 0x25         /* OPX of div */
        bne    r15, zero, .Lunsigned_division


        /*
         *  OPX is div.  Determine and store the sign of the quotient.
         *  Then take the absolute value of both operands.
         */
        xor   r17, r3, r6      /* MSB contains sign of quotient */
        bge   r3, zero, 0f
        sub   r3, zero, r3     /* -r3 */
0:
        bge   r6, zero, 0f
        sub   r6, zero, r6     /* -r6 */
0:


.Lunsigned_division:
        /* Initialize the unsigned-division loop. */
        movi  r13, 0          /* remainder = 0 */

        /* Now
        * r3 = dividend : quotient
        * r4 = 0x25 for div, 0x24 for divu
        * r6 = divisor
        * r13 = remainder
        * r14 = loop counter (already initialized to 32)
        * r17 = MSB contains sign of quotient
        */


        /*
        *   for (count = 32; count > 0; --count)
        *   {
        */
.Ldivide_loop:

        /*
        *       Division:
        *
        *       (remainder:dividend:quotient) <<= 1;
        */
        slli  r13, r13, 1
        cmplt r15, r3, zero        /* r15 = MSB of r3 */
        or    r13, r13, r15
        slli  r3, r3, 1


        /*
        *       if (remainder >= divisor)
        *       {
        *           set LSB of quotient
        *           remainder -= divisor;
        *       }
        */
        bltu  r13, r6, .Ldiv_skip
        ori   r3, r3, 1
        sub   r13, r13, r6
.Ldiv_skip:

        /*
        *   }
        */
        subi  r14, r14, 1
        bne   r14, zero, .Ldivide_loop

        mov   r9, r3


        /* Now
        * r9 = quotient
        * r4 = 0x25 for div, 0x24 for divu
        * r7 = 4*(C^16)
        * r17 = MSB contains sign of quotient
        */

    
        /*
        *  Conditionally negate signed quotient.  If quotient is unsigned,
        *  the sign already is initialized to 0.
        */
        bge   r17, zero, .Lstore_result
        sub   r9, zero, r9     /* -r9 */

        br    .Lstore_result




        /* MULTIPLICATION
        *
        * A "product" is the number that one gets by summing a "multiplicand"
        * several times.  The "multiplier" specifies the number of copies of the
        * multiplicand that are summed.
        *
        * Actual multiplication algorithms don't use repeated addition, however.
        * Shift-and-add algorithms get the same answer as repeated addition, and
        * they are faster.  To compute the lower half of a product (pppp below)
        * one shifts the product left before adding in each of the partial products
        * (a * mmmm) through (d * mmmm).
        *
        * To compute the upper half of a product (PPPP below), one adds in the
        * partial products (d * mmmm) through (a * mmmm), each time following the
        * add by a right shift of the product.
        *
        *     mmmm
        *   * abcd
        *   ------
        *     ####  = d * mmmm
        *    ####   = c * mmmm
        *   ####    = b * mmmm
        *  ####     = a * mmmm
        * --------
        * PPPPpppp
        *
        * The example above shows 4 partial products.  Computing actual Nios II
        * products requires 32 partials.
        *
        * It is possible to compute the result of mulxsu from the result of mulxuu
        * because the only difference between the results of these two opcodes is
        * the value of the partial product associated with the sign bit of rA.
        *
        *   mulxsu = mulxuu - ((rA < 0) ? rB : 0);
        *
        * It is possible to compute the result of mulxss from the result of mulxsu
        * because the only difference between the results of these two opcodes is
        * the value of the partial product associated with the sign bit of rB.
        *
        *   mulxss = mulxsu - ((rB < 0) ? rA : 0);
        *
        */

.Lmul_immed:
        /* Opcode is muli.  Change it into mul for remainder of algorithm. */
        mov   r7, r6         /* Field B is dest register, not field C. */
        mov   r6, r4         /* Field IMM16 is src2, not field B. */
        movi  r4, 0x27       /* OPX of mul is 0x27 */

.Lmultiply:
        /* Initialize the multiplication loop. */
        movi  r9, 0          /* mul_product    = 0 */
        movi  r10, 0         /* mulxuu_product = 0 */
        mov   r11, r6        /* save original multiplier for mulxsu and mulxss */
        mov   r12, r6        /* mulxuu_multiplier (will be shifted) */
        movi  r16, 1         /* used to create "rori B,A,1" from "ror B,A,r16" */

        /* Now
        * r3 = multiplicand
        * r6 = mul_multiplier
        * r7 = 4 * dest_register (used later as offset to sp)
        * r9 = mul_product
        * r10 = mulxuu_product
        * r11 = original multiplier
        * r12 = mulxuu_multiplier
        * r14 = loop counter (already initialized)
        * r15 = temp
        * r16 = 1
        */


        /*
        *   for (count = 32; count > 0; --count)
        *   {
        */
.Lmultiply_loop:

        /*
        *       mul_product <<= 1;
        *       lsb = multiplier & 1;
        */
        slli   r9, r9, 1
        andi   r15, r12, 1

        /*
        *       if (lsb == 1)
        *       {
        *           mulxuu_product += multiplicand;
        *       }
        */
        beq   r15, zero, .Lmulx_skip
        add   r10, r10, r3
        cmpltu r15, r10, r3  /* Save the carry from the MSB of mulxuu_product. */
        ror   r15, r15, r16  /* r15 = 0x80000000 on carry, or else 0x00000000 */
.Lmulx_skip:

        /*
        *       if (MSB of mul_multiplier == 1)
        *       {
        *           mul_product += multiplicand;
        *       }
        */
        bge   r6, zero, .Lmul_skip
        add   r9, r9, r3
.Lmul_skip:

        /*
        *       mulxuu_product >>= 1;           logical shift
        *       mul_multiplier <<= 1;           done with MSB
        *       mulx_multiplier >>= 1;          done with LSB
        */
        srli   r10, r10, 1
        or     r10, r10, r15           /* OR in the saved carry bit. */
        slli   r6, r6, 1
        srli   r12, r12, 1


        /*
        *   }
        */
        subi   r14, r14, 1
        bne    r14, zero, .Lmultiply_loop


        /*
        *  Multiply emulation loop done.
        */

        /* Now
        * r3 = multiplicand
        * r4 = OPX
        * r7 = 4 * dest_register (used later as offset to sp)
        * r9 = mul_product
        * r10 = mulxuu_product
        * r11 = original multiplier
        * r15 = temp
        */


        /*
        *  Select/compute the result based on OPX.
        */


        /* OPX == mul?  Then store. */
        xori  r15, r4, 0x27
        beq   r15, zero, .Lstore_result

        /* It's one of the mulx.. opcodes.  Move over the result. */
        mov   r9, r10

        /* OPX == mulxuu?  Then store. */
        xori  r15, r4, 0x07
        beq   r15, zero, .Lstore_result

        /* Compute mulxsu
         *
         * mulxsu = mulxuu - ((rA < 0) ? rB : 0);
         */
        bge   r3, zero, .Lmulxsu_skip
        sub   r9, r9, r11
.Lmulxsu_skip:

        /* OPX == mulxsu?  Then store. */
        xori  r15, r4, 0x17
        beq   r15, zero, .Lstore_result

        /* Compute mulxss
         *
         * mulxss = mulxsu - ((rB < 0) ? rA : 0);
         */
        bge   r11, zero, .Lmulxss_skip
        sub   r9, r9, r3
.Lmulxss_skip:
        /* At this point, assume that OPX is mulxss, so store */


.Lstore_result:
        add   r7, r7, sp
        stw   r9, 0(r7)

        ldw   r16,  0(sp)
        ldw   r17,  4(sp)
        ldw   r18,  8(sp)
        ldw   r19, 12(sp)
        ldw   r20, 16(sp)
        ldw   r21, 20(sp)
        ldw   r22, 24(sp)
        ldw   r23, 28(sp)

                            /* bt @ 32 - Breakpoint register usually isn't an operand. */
                            /* et @ 36 - Don't corrupt et. */
                            /* gp @ 40 - Don't corrupt gp. */
                            /* sp @ 44 - Don't corrupt sp. */
        ldw   fp,  48(sp)
                            /* ea @ 52 - Don't corrupt ea. */
                            /* ba @ 56 - Breakpoint register usually isn't an operand. */

        addi  sp, sp, 60

        br    .Lexception_exit


.Lnot_muldiv:

        addi  sp, sp, 60


        .section .exceptions.exit.label
.Lexception_exit:

