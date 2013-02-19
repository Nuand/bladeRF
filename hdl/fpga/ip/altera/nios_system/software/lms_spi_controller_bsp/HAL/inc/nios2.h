#ifndef __NIOS2_H__
#define __NIOS2_H__

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
******************************************************************************/

/*
 * This header provides processor specific macros for accessing the Nios2
 * control registers.
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/*
 * Macros for accessing selected processor registers
 */

#define NIOS2_READ_ET(et) \
    do { __asm ("mov %0, et" : "=r" (et) ); } while (0)

#define NIOS2_WRITE_ET(et) \
    do { __asm volatile ("mov et, %z0" : : "rM" (et)); } while (0)

#define NIOS2_READ_SP(sp) \
    do { __asm ("mov %0, sp" : "=r" (sp) ); } while (0)

/*
 * Macros for useful processor instructions
 */

#define NIOS2_BREAK() \
    do { __asm volatile ("break"); } while (0)

#define NIOS2_REPORT_STACK_OVERFLOW() \
    do { __asm volatile("break 3"); } while (0)

/*
 * Macros for accessing the control registers.
 */

#define NIOS2_READ_STATUS(dest) \
    do { dest = __builtin_rdctl(0); } while (0)

#define NIOS2_WRITE_STATUS(src) \
    do { __builtin_wrctl(0, src); } while (0)

#define NIOS2_READ_ESTATUS(dest) \
    do { dest = __builtin_rdctl(1); } while (0)

#define NIOS2_READ_BSTATUS(dest) \
    do { dest = __builtin_rdctl(2); } while (0)

#define NIOS2_READ_IENABLE(dest) \
    do { dest = __builtin_rdctl(3); } while (0)

#define NIOS2_WRITE_IENABLE(src) \
    do { __builtin_wrctl(3, src); } while (0)

#define NIOS2_READ_IPENDING(dest) \
    do { dest = __builtin_rdctl(4); } while (0)

#define NIOS2_READ_CPUID(dest) \
    do { dest = __builtin_rdctl(5); } while (0)


/*
 * Macros for accessing extra exception registers. These
 * are always enabled wit the MPU or MMU, and optionally
 * with other advanced exception types/
 */
#define NIOS2_READ_EXCEPTION(dest) \
    do { dest = __builtin_rdctl(7); } while (0)

#define NIOS2_READ_BADADDR(dest) \
    do { dest = __builtin_rdctl(12); } while (0)


/*
 * Macros for accessing control registers for MPU
 * operation. These should not be used unless the
 * MPU is enabled.
 *
 * The config register may be augmented for future
 * enhancements. For now, only MPU support is provided.
 */
/* Config register */
#define NIOS2_WRITE_CONFIG(src) \
    do { __builtin_wrctl(13, src); } while (0)

#define NIOS2_READ_CONFIG(dest) \
    do { dest = __builtin_rdctl(13); } while (0)

/* MPU Base Address Register */
#define NIOS2_WRITE_MPUBASE(src) \
    do { __builtin_wrctl(14, src); } while (0)

#define NIOS2_READ_MPUBASE(dest) \
    do { dest = __builtin_rdctl(14); } while (0)

/* MPU Access Register */
#define NIOS2_WRITE_MPUACC(src) \
    do { __builtin_wrctl(15, src); } while (0)

#define NIOS2_READ_MPUACC(dest) \
    do { dest = __builtin_rdctl(15); } while (0)


/*
 * Nios II control registers that are always present
 */
#define NIOS2_STATUS   status
#define NIOS2_ESTATUS  estatus
#define NIOS2_BSTATUS  bstatus
#define NIOS2_IENABLE  ienable
#define NIOS2_IPENDING ipending
#define NIOS2_CPUID cpuid

/*
 * STATUS, BSTATUS, ESTATUS, and SSTATUS fields.
 * The presence of fields is a function of the Nios II configuration.
 */
#define NIOS2_STATUS_PIE_MSK  (0x00000001)
#define NIOS2_STATUS_PIE_OFST (0)
#define NIOS2_STATUS_U_MSK    (0x00000002)
#define NIOS2_STATUS_U_OFST   (1)
#define NIOS2_STATUS_EH_MSK   (0x00000004)
#define NIOS2_STATUS_EH_OFST  (2)
#define NIOS2_STATUS_IH_MSK     (0x00000008)
#define NIOS2_STATUS_IH_OFST    (3)
#define NIOS2_STATUS_IL_MSK     (0x000003f0)
#define NIOS2_STATUS_IL_OFST    (4)
#define NIOS2_STATUS_CRS_MSK    (0x0000fc00)
#define NIOS2_STATUS_CRS_OFST   (10)
#define NIOS2_STATUS_PRS_MSK    (0x003f0000)
#define NIOS2_STATUS_PRS_OFST   (16)
#define NIOS2_STATUS_NMI_MSK    (0x00400000)
#define NIOS2_STATUS_NMI_OFST   (22)
#define NIOS2_STATUS_RSIE_MSK   (0x00800000)
#define NIOS2_STATUS_RSIE_OFST  (23)
#define NIOS2_STATUS_SRS_MSK    (0x80000000)
#define NIOS2_STATUS_SRS_OFST   (31)

/*
 * Bit masks & offsets available with extra exceptions support
 */

/* Exception register */
#define NIOS2_EXCEPTION_REG_CAUSE_MASK (0x0000007c)
#define NIOS2_EXCEPTION_REG_CAUSE_OFST (2)

/*
 * Bit masks & offsets for MPU support
 *
 * All bit-masks are expressed relative to the position
 * of the data with a register. To read data that is LSB-
 * aligned, the register read data should be masked, then
 * right-shifted by the designated "OFST" macro value. The
 * opposite should be used for register writes when starting
 * with LSB-aligned data.
 */

/* Config register */
#define NIOS2_CONFIG_REG_PE_MASK (0x00000001)
#define NIOS2_CONFIG_REG_PE_OFST (0)
#define NIOS2_CONFIG_REG_ANI_MASK (0x00000002)
#define NIOS2_CONFIG_REG_ANI_OFST (1)

/* MPU Base Address Register */
#define NIOS2_MPUBASE_D_MASK         (0x00000001)
#define NIOS2_MPUBASE_D_OFST         (0)
#define NIOS2_MPUBASE_INDEX_MASK     (0x0000003e)
#define NIOS2_MPUBASE_INDEX_OFST     (1)
#define NIOS2_MPUBASE_BASE_ADDR_MASK (0xffffffc0)
#define NIOS2_MPUBASE_BASE_ADDR_OFST (6)

/* MPU Access Register */
#define NIOS2_MPUACC_LIMIT_MASK (0xffffffc0)
#define NIOS2_MPUACC_LIMIT_OFST (6)
#define NIOS2_MPUACC_MASK_MASK  (0xffffffc0)
#define NIOS2_MPUACC_MASK_OFST  (6)
#define NIOS2_MPUACC_C_MASK     (0x00000020)
#define NIOS2_MPUACC_C_OFST     (5)
#define NIOS2_MPUACC_PERM_MASK  (0x0000001c)
#define NIOS2_MPUACC_PERM_OFST  (2)
#define NIOS2_MPUACC_RD_MASK    (0x00000002)
#define NIOS2_MPUACC_RD_OFST    (1)
#define NIOS2_MPUACC_WR_MASK    (0x00000001)
#define NIOS2_MPUACC_WR_OFST    (0)

/*
 * Number of available IRQs in internal interrupt controller.
 */
#define NIOS2_NIRQ 32


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __NIOS2_H__ */
