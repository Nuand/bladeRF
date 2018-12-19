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

#include <stdlib.h>

#include "debug.h"
#include "morse.h"

// clang-format off
struct character const MORSE_MAP[] = {
    {
        .letter = 0x18, /* Error (ASCII "CAN"): mash out 8 dots */
        .sequence = { T_DOT, T_DOT, T_DOT, T_DOT,
                      T_DOT, T_DOT, T_DOT, T_DOT, T_END }
    },
    {
        .letter = 'A',
        .sequence = { T_DOT, T_DASH, T_END }
    },
    {
        .letter = 'B',
        .sequence = { T_DASH, T_DOT, T_DOT, T_DOT, T_END }
    },
    {
        .letter = 'C',
        .sequence = { T_DASH, T_DOT, T_DASH, T_DOT, T_END }
    },
    {
        .letter = 'D',
        .sequence = { T_DASH, T_DOT, T_DOT, T_END }
    },
    {
        .letter = 'E',
        .sequence = { T_DOT, T_END }
    },
    {
        .letter = 'F',
        .sequence = { T_DOT, T_DOT, T_DASH, T_DOT, T_END }
    },
    {
        .letter = 'G',
        .sequence = { T_DASH, T_DASH, T_DOT, T_END }
    },
    {
        .letter = 'H',
        .sequence = { T_DOT, T_DOT, T_DOT, T_DOT, T_END }
    },
    {
        .letter = 'I',
        .sequence = { T_DOT, T_DOT, T_END }
    },
    {
        .letter = 'J',
        .sequence = { T_DOT, T_DASH, T_DASH, T_DASH, T_END }
    },
    {
        .letter = 'K',
        .sequence = { T_DASH, T_DOT, T_DASH, T_END }
    },
    {
        .letter = 'L',
        .sequence = { T_DOT, T_DASH, T_DOT, T_DOT, T_END }
    },
    {
        .letter = 'M',
        .sequence = { T_DASH, T_DASH, T_END }
    },
    {
        .letter = 'N',
        .sequence = { T_DASH, T_DOT, T_END }
    },
    {
        .letter = 'O',
        .sequence = { T_DASH, T_DASH, T_DASH, T_END }
    },
    {
        .letter = 'P',
        .sequence = { T_DOT, T_DASH, T_DASH, T_DOT, T_END }
    },
    {
        .letter = 'Q',
        .sequence = { T_DASH, T_DASH, T_DOT, T_DASH, T_END }
    },
    {
        .letter = 'R',
        .sequence = { T_DOT, T_DASH, T_DOT, T_END }
    },
    {
        .letter = 'S',
        .sequence = { T_DOT, T_DOT, T_DOT, T_END }
    },
    {
        .letter = 'T',
        .sequence = { T_DASH, T_END }
    },
    {
        .letter = 'U',
        .sequence = { T_DOT, T_DOT, T_DASH, T_END }
    },
    {
        .letter = 'V',
        .sequence = { T_DOT, T_DOT, T_DOT, T_DASH, T_END }
    },
    {
        .letter = 'W',
        .sequence = { T_DOT, T_DASH, T_DASH, T_END }
    },
    {
        .letter = 'X',
        .sequence = { T_DASH, T_DOT, T_DOT, T_DASH, T_END }
    },
    {
        .letter = 'Y',
        .sequence = { T_DASH, T_DOT, T_DASH, T_DASH, T_END }
    },
    {
        .letter = 'Z',
        .sequence = { T_DASH, T_DASH, T_DOT, T_DOT, T_END }
    },
    {
        .letter = '"',
        .sequence = { T_DOT, T_DASH, T_DOT, T_DOT, T_DASH, T_DOT, T_END }
    },
    {
        .letter = '\'',
        .sequence = { T_DOT, T_DASH, T_DASH, T_DASH, T_DASH, T_DOT, T_END }
    },
    {
        .letter = '(',
        .sequence = { T_DASH, T_DOT, T_DASH, T_DASH, T_DOT, T_END }
    },
    {
        .letter = ')',
        .sequence = { T_DASH, T_DOT, T_DASH, T_DASH, T_DOT, T_DASH, T_END }
    },
    {
        .letter = '*',
        .sequence = { T_DASH, T_DOT, T_DOT, T_DASH, T_END }
    },
    {
        .letter = '+',
        .sequence = { T_DOT, T_DASH, T_DOT, T_DASH, T_DOT, T_END }
    },
    {
        .letter = ',',
        .sequence = { T_DASH, T_DASH, T_DOT, T_DOT, T_DASH, T_DASH, T_END }
    },
    {
        .letter = '-',
        .sequence = { T_DASH, T_DOT, T_DOT, T_DOT, T_DOT, T_DASH, T_END }
    },
    {
        .letter = '.',
        .sequence = { T_DOT, T_DASH, T_DOT, T_DASH, T_DOT, T_DASH, T_END }
    },
    {
        .letter = '/',
        .sequence = { T_DASH, T_DOT, T_DOT, T_DASH, T_DOT, T_END }
    },
    {
        .letter = '0',
        .sequence = { T_DASH, T_DASH, T_DASH, T_DASH, T_DASH, T_END }
    },
    {
        .letter = '1',
        .sequence = { T_DOT, T_DASH, T_DASH, T_DASH, T_DASH, T_END }
    },
    {
        .letter = '2',
        .sequence = { T_DOT, T_DOT, T_DASH, T_DASH, T_DASH, T_END }
    },
    {
        .letter = '3',
        .sequence = { T_DOT, T_DOT, T_DOT, T_DASH, T_DASH, T_END }
    },
    {
        .letter = '4',
        .sequence = { T_DOT, T_DOT, T_DOT, T_DOT, T_DASH, T_END }
    },
    {
        .letter = '5',
        .sequence = { T_DOT, T_DOT, T_DOT, T_DOT, T_DOT, T_END }
    },
    {
        .letter = '6',
        .sequence = { T_DASH, T_DOT, T_DOT, T_DOT, T_DOT, T_END }
    },
    {
        .letter = '7',
        .sequence = { T_DASH, T_DASH, T_DOT, T_DOT, T_DOT, T_END }
    },
    {
        .letter = '8',
        .sequence = { T_DASH, T_DASH, T_DASH, T_DOT, T_DOT, T_END }
    },
    {
        .letter = '9',
        .sequence = { T_DASH, T_DASH, T_DASH, T_DASH, T_DOT, T_END }
    },
    {
        .letter = ':',
        .sequence = { T_DASH, T_DASH, T_DASH, T_DOT, T_DOT, T_DOT, T_END }
    },
    {
        .letter = ';',
        .sequence = { T_DASH, T_DOT, T_DASH, T_DOT, T_DASH, T_DOT, T_END }
    },
    {
        .letter = '=',
        .sequence = { T_DASH, T_DOT, T_DOT, T_DOT, T_DASH, T_END }
    },
    {
        .letter = '?',
        .sequence = { T_DOT, T_DOT, T_DASH, T_DASH, T_DOT, T_DOT, T_END }
    },
    {
        .letter = '@',
        .sequence = { T_DOT, T_DASH, T_DASH, T_DOT, T_DASH, T_DOT, T_END }
    },
    {
        .letter = '_',
        .sequence = { T_DOT, T_DOT, T_DASH, T_DASH, T_DOT, T_DASH, T_END }
    },
    {
        .letter = ' ', /* inter-word gap */
        .sequence = { T_WORD_GAP, T_END }
    },
    {
        .letter = 0x00, /* End of string */
        .sequence = { T_DONE, T_END }
    },
    {
        .letter = 0x03, /* End of Message (ASCII "ETX") */
        .sequence = { T_DOT, T_DASH, T_DOT, T_DASH, T_DOT, T_END }
    },
    {
        .letter = 0xFF, /* End of array placeholder */
        .sequence = { T_END }
    },
};
// clang-format on

struct character get_letter(char letter)
{
    /* Convert to upper case */
    if (letter >= 'a' && letter <= 'z') {
        letter += 'A' - 'a';
    }

    for (size_t i = 0; i < ARRAY_SIZE(MORSE_MAP); ++i) {
        if (MORSE_MAP[i].letter == letter) {
            return MORSE_MAP[i];
        }
    };

    /* Couldn't find it, error pattern */
    // DBG("%s: Letter not found: '%c'\n", __FUNCTION__, letter);
    return MORSE_MAP[0];
};

size_t to_morse(char const *str, element *out, size_t maxlen)
{
    size_t count = 0;

    for (size_t i = 0; str[i] != 0x00; ++i) {
        struct character ltr = get_letter(str[i]);

        for (size_t j = 0; j < MAX_SEQ_LEN; ++j) {
            if (count < maxlen) {
                out[count] = ltr.sequence[j];
                ++count;
            }

            if (T_END == ltr.sequence[j]) {
                break;
            }
        }
    }

    if (count >= maxlen) {
        count           = maxlen;
        out[maxlen - 1] = T_INVALID;
    } else {
        out[count++] = T_DONE;
    }

    return count;
}

size_t to_tones(element const *elements,
                struct tone *tones,
                unsigned int frequency,
                float dot_length,
                size_t maxlen)
{
    size_t count    = 0;
    size_t next_gap = 0;

    for (size_t i = 0;; ++i) {
        size_t tone = 0;
        size_t gap  = 0;

        /* Map Morse element to tone and gap length */
        switch (elements[i]) {
            case T_INVALID:
                /* Error case: just ditch out */
                // DBG("%s: T_INVALID encountered at i=%x\n", __FUNCTION__, i);
                return 0;
            case T_DOT:
                /* Dot */
                tone = 1;
                gap  = 1;
                break;
            case T_DASH:
                /* Dash (3 dot length) */
                tone = 3;
                gap  = 1;
                break;
            case T_END:
                /* End of a character */
                tone = 0;
                gap  = 3;
                break;
            case T_DONE:
                /* ensure this gets done this round */
                tone     = 1;
                next_gap = 7;
                break;
            case T_WORD_GAP:
                /* Space between words */
                tone = 0;
                gap  = 7;
                if (T_END == elements[i + 1]) {
                    ++i; /* skip the T_END */
                }
                break;
        }

        /* We apply the gap from the last iteration here, so that we only end up
         * with one gap when there's a space, etc */
        if (count < maxlen && next_gap && tone) {
            tones[count].frequency = 0;
            tones[count].duration  = next_gap * dot_length;
            ++count;
        }

        /* Done flag? Out of space? We're done. */
        if (T_DONE == elements[i] || count >= maxlen) {
            break;
        }

        /* Apply the tone here. */
        if (count < maxlen && tone) {
            tones[count].frequency = frequency;
            tones[count].duration  = tone * dot_length;
            ++count;
        }

        next_gap = gap;
    }

    return count;
}
