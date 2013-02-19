/******************************************************************************
*                                                                             *
* License Agreement                                                           *
*                                                                             *
* Copyright (c) 2003-2010 Altera Corporation, San Jose, California, USA.      *
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

/*  mcount or _mcount is inserted by GCC before the function prologue of every 
 *  function when a program is compiled for profiling. At the start of mcount,
 *  we guarantee that:
 *  ra = self_pc (an address in the function which called mcount)
 *  r8 = from_pc (an address in the function which called mcount's caller)
 *
 *  Because this is always called at the start of a function we can corrupt
 *  r2,r3 and r11-r15.  We must not corrupt r4-r7 (because they might contain
 *  function arguments for the instrumented function) or r8 (which holds ra
 *  for the instrumented function).
 */

        .global __mcount_fn_head

        .global mcount

        /* _mcount is used by gcc4 */
        .global _mcount

_mcount:        
mcount:
        /* Use a hash to speed up locating fn_entry.  We use bits 5 upwards to choose
         * the bucket because bits 1:0 will always be 0, and because the distribution
         * of values for bits 4:2 won't be even (aligning on cache line boundaries
         * will skew it).  Higher bits should be fairly random.
         */
        /* fn_head = mcount_fn_head + (((unsigned int)self_pc >> 5) & (HASH_BUCKETS - 1)); */

        srli    r2, ra, 3
        movhi   r3, %hiadj(__mcount_fn_head)
        addi    r3, r3, %lo(__mcount_fn_head)
        andi    r2, r2, 0xFC
        add     r11, r2, r3

        /* The fast case is where we have already allocated a function arc, and so
         * also a function pointer.
         */

        /* First find the function being called (using self_pc) */
        mov     r10, r11
0:
        ldw     r10, 0(r10)
        beq     r10, zero, .Lnew_arc
        ldw     r2, 4(r10)
        bne     r2, ra, 0b

        /* Found a function entry for this PC. Now look for an arc with a matching
         * from_pc value. There will always be at least one arc. */
        ldw     r3, 8(r10)
0:
        ldw     r2, 4(r3)
        beq     r2, r8, .Lfound_arc
        ldw     r3, 0(r3)
        bne     r3, zero, 0b

.Lnew_arc:
        addi    sp, sp, -24

.LCFI0:
        stw     ra, 0(sp)
        stw     r4, 4(sp)
        stw     r5, 8(sp)
        stw     r6, 12(sp)
        stw     r7, 16(sp)
        stw     r8, 20(sp)

.LCFI1:
        /* __mcount_record(orig_ra, orig_r8, fn_entry, *fn_head); */
        mov     r4, ra
        mov     r5, r8
        mov     r6, r10
        mov     r7, r11
        call     __mcount_record
        
        /* restore registers from the stack */
        ldw     ra, 0(sp)
        ldw     r4, 4(sp)
        ldw     r5, 8(sp)
        ldw     r6, 12(sp)
        ldw     r7, 16(sp)
        ldw     r8, 20(sp)

        addi    sp, sp, 24

.LCFI2:
        ret

.Lfound_arc:
        /* We've found the correct arc record.  Increment the count and return */
        ldw     r2, 8(r3)
        addi    r2, r2, 1
        stw     r2, 8(r3)
        ret

.Lmcount_end:



/*
 * Dwarf2 debug information for the function.  This provides GDB with the
 * information it needs to backtrace out of this function.
 */

        .section    .debug_frame,"",@progbits
.LCIE:
        .4byte   2f - 1f               /* Length */
1:
        .4byte   0xffffffff            /* CIE id */
        .byte    0x1                   /* Version */
        .string  ""                    /* Augmentation */
        .uleb128 0x1                   /* Code alignment factor */
        .sleb128 -4                    /* Data alignment factor */
        .byte    0x1f                  /* Return address register */

        .byte    0xc                   /* Define CFA */
        .uleb128 0x1b                  /*   Register 27 (sp) */
        .uleb128 0x0                   /*   Offset 0 */

        .align   2                     /* Padding */
2:

.LFDE_mcount:
        .4byte   2f - 1f               /* Length */
1:
        .4byte   .LCIE                 /* Pointer to CIE */
        .4byte   mcount                /* Start of table entry */
        .4byte   .Lmcount_end - mcount /* Size of table entry */

        .byte    0x4                   /* Advance location */
        .4byte   .LCFI0 - mcount       /*   to .LCFI0 */
        .byte    0xe                   /* Define CFA offset */
        .uleb128 24                    /*   to 24     */

        .byte    0x4                   /* Advance location */
        .4byte   .LCFI1 - .LCFI0       /*   to .LCFI1 */
        .byte    0x9f                  /* Store ra    */
        .uleb128 0x6                   /*   at CFA-24 */
        .byte    0x84                  /* Store r4    */
        .uleb128 0x5                   /*   at CFA-20 */
        .byte    0x85                  /* Store r5    */
        .uleb128 0x4                   /*   at CFA-16 */
        .byte    0x86                  /* Store r6    */
        .uleb128 0x3                   /*   at CFA-12 */
        .byte    0x87                  /* Store r7    */
        .uleb128 0x2                   /*   at CFA-8  */
        .byte    0x88                  /* Store r8    */
        .uleb128 0x1                   /*   at CFA-4  */

        .byte    0x4                   /* Advance location */
        .4byte   .LCFI2 - .LCFI1       /*   to .LCFI2 */
        .byte    0xe                   /* Define CFA offset */
        .uleb128 0                     /*   to 0      */
        .byte    0x8                   /* Same value  */
        .uleb128 31                    /*   for ra    */
        .byte    0x8                   /* Same value  */
        .uleb128 4                     /*   for r4    */
        .byte    0x8                   /* Same value  */
        .uleb128 5                     /*   for r5    */
        .byte    0x8                   /* Same value  */
        .uleb128 6                     /*   for r6    */
        .byte    0x8                   /* Same value  */
        .uleb128 7                     /*   for r7    */
        .byte    0x8                   /* Same value  */
        .uleb128 8                     /*   for r8    */

        .align   2
2:

