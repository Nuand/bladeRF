/* This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (c) 2018 Nuand LLC
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

#ifndef FOXHUNT_H_
#define FOXHUNT_H_

#include "libbladeRF_nios_compat.h"

/**
 * @brief      A tone, of a given frequency and length
 */
struct tone {
    unsigned int frequency; /**< Tone frequency, Hz (0 = silence) */
    float duration;         /**< Length of tone, seconds */
};

/**
 * @brief      Message contents
 */
struct message {
    char const *text;          /**< Message text, null-terminated */
    float wpm;                 /**< Code speed, words per minute */
    unsigned int tone;         /**< Tone frequency, Hz */
    bladerf_frequency carrier; /**< Carrier frequency, Hz */
    bladerf_gain backoff_dB;   /**< Backoff from full power, dB */
};

#define TONE_GEN_OFFSET_STATUS 0x0
#define TONE_GEN_OFFSET_DPHASE 0x4
#define TONE_GEN_OFFSET_DURATION 0x8

#define TONE_GEN_OFFSET_CTRL_APPEND 0x1
#define TONE_GEN_OFFSET_CTRL_CLEAR 0x1

#endif  // FOXHUNT_H_
