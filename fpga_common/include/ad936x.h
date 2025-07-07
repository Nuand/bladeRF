/**
 * @file ad936x.h
 *
 * @brief Interface to the library for the AD936X RFIC family
 *
 * Copyright (c) 2018 Nuand LLC.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef AD936X_H_
#define AD936X_H_

#include <inttypes.h>
#include <stdbool.h>

#include <ad9361_api.h>

/**
 * The purpose of this header file is to allow the use of libad9361 without
 * including all of the unnecessary defines, etc, used during compilation.
 *
 * This file is largely copied from the files named in each section.  Only
 * necessary declarations are present.
 *
 * In general, defines are prefixed with AD936X_ to avoid conflicts.
 *
 * Comments have been removed for brevity.  Please see the original header
 * files from the third-party ADI library for further details.
 */

/******************************************************************************
 * From common.h
 ******************************************************************************/

#define AD936X_REG_TX1_OUT_1_PHASE_CORR 0x08E
#define AD936X_REG_TX1_OUT_1_GAIN_CORR 0x08F
#define AD936X_REG_TX2_OUT_1_PHASE_CORR 0x090
#define AD936X_REG_TX2_OUT_1_GAIN_CORR 0x091
#define AD936X_REG_TX1_OUT_1_OFFSET_I 0x092
#define AD936X_REG_TX1_OUT_1_OFFSET_Q 0x093
#define AD936X_REG_TX2_OUT_1_OFFSET_I 0x094
#define AD936X_REG_TX2_OUT_1_OFFSET_Q 0x095
#define AD936X_REG_TX1_OUT_2_PHASE_CORR 0x096
#define AD936X_REG_TX1_OUT_2_GAIN_CORR 0x097
#define AD936X_REG_TX2_OUT_2_PHASE_CORR 0x098
#define AD936X_REG_TX2_OUT_2_GAIN_CORR 0x099
#define AD936X_REG_TX1_OUT_2_OFFSET_I 0x09A
#define AD936X_REG_TX1_OUT_2_OFFSET_Q 0x09B
#define AD936X_REG_TX2_OUT_2_OFFSET_I 0x09C
#define AD936X_REG_TX2_OUT_2_OFFSET_Q 0x09D
#define AD936X_REG_TX_FORCE_BITS 0x09F

#define AD936X_REG_RX1_INPUT_A_PHASE_CORR 0x170
#define AD936X_REG_RX1_INPUT_A_GAIN_CORR 0x171
#define AD936X_REG_RX2_INPUT_A_PHASE_CORR 0x172
#define AD936X_REG_RX2_INPUT_A_GAIN_CORR 0x173
#define AD936X_REG_RX1_INPUT_A_Q_OFFSET 0x174
#define AD936X_REG_RX1_INPUT_A_OFFSETS 0x175
#define AD936X_REG_INPUT_A_OFFSETS_1 0x176
#define AD936X_REG_RX2_INPUT_A_OFFSETS 0x177
#define AD936X_REG_RX2_INPUT_A_I_OFFSET 0x178
#define AD936X_REG_RX1_INPUT_BC_PHASE_CORR 0x179
#define AD936X_REG_RX1_INPUT_BC_GAIN_CORR 0x17A
#define AD936X_REG_RX2_INPUT_BC_PHASE_CORR 0x17B
#define AD936X_REG_RX2_INPUT_BC_GAIN_CORR 0x17C
#define AD936X_REG_RX1_INPUT_BC_Q_OFFSET 0x17D
#define AD936X_REG_RX1_INPUT_BC_OFFSETS 0x17E
#define AD936X_REG_INPUT_BC_OFFSETS_1 0x17F
#define AD936X_REG_RX2_INPUT_BC_OFFSETS 0x180
#define AD936X_REG_RX2_INPUT_BC_I_OFFSET 0x181
#define AD936X_REG_FORCE_BITS 0x182

#define AD936X_READ (0 << 15)
#define AD936X_WRITE (1 << 15)
#define AD936X_CNT(x) ((((x)-1) & 0x7) << 12)
#define AD936X_ADDR(x) ((x)&0x3FF)

#define AD936X_A_BALANCED 0
#define AD936X_B_BALANCED 1
#define AD936X_C_BALANCED 2
#define AD936X_A_N 3
#define AD936X_A_P 4
#define AD936X_B_N 5
#define AD936X_B_P 6
#define AD936X_C_N 7
#define AD936X_C_P 8
#define AD936X_TX_MON1 9
#define AD936X_TX_MON2 10
#define AD936X_TX_MON1_2 11

#define AD936X_TXA 0
#define AD936X_TXB 1


#endif  // AD936X_H_
