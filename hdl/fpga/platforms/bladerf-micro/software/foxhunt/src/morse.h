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

#ifndef MORSE_H_
#define MORSE_H_

#include "foxhunt.h"

#define ARRAY_SIZE(n) (sizeof(n) / sizeof(n[0]))

#define MAX_SEQ_LEN 10

typedef enum {
    T_DONE = 0, /**< End of transmission */
    T_DOT,      /**< Dot */
    T_DASH,     /**< Dash (3 dot lengths) */
    T_WORD_GAP, /**< Gap of 7 dot lengths */
    T_END,      /**< End of character */
    T_INVALID,  /**< Error condition (overflow?) */
} element;

struct character {
    char const letter;                   /**< A letter, in ASCII */
    element const sequence[MAX_SEQ_LEN]; /**< A sequence of elements */
};

struct character get_letter(char);

size_t to_morse(char const *, element *, size_t);

size_t to_tones(element const *, struct tone *, unsigned int, float, size_t);

#endif  // MORSE_H_
