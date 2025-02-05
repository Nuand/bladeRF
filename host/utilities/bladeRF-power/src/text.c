/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2024 Nuand LLC
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
 *
 * This program is intended to verify that C programs build against
 * libbladeRF without any unintended dependencies.
 */
#include <math.h>
#include <curses.h>
#include <locale.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "text.h"

const char* power_suffix[2][DIGIT_HEIGHT] = {
    {
        "██████╗ ██████╗ ███╗   ███╗",
        "██╔══██╗██╔══██╗████╗ ████║",
        "██║  ██║██████╔╝██╔████╔██║",
        "██║  ██║██╔══██╗██║╚██╔╝██║",
        "██████╔╝██████╔╝██║ ╚═╝ ██║",
        "╚═════╝ ╚═════╝ ╚═╝     ╚═╝",
    },
    {
        "██████╗ ██████╗ ███████╗███████╗",
        "██╔══██╗██╔══██╗██╔════╝██╔════╝",
        "██║  ██║██████╔╝█████╗  ███████╗",
        "██║  ██║██╔══██╗██╔══╝  ╚════██║",
        "██████╔╝██████╔╝██║     ███████║",
        "╚═════╝ ╚═════╝ ╚═╝     ╚══════╝",
    }
};

const char* ascii_art_digits[12][DIGIT_HEIGHT] = {
    {
        " ██████╗ ",
        "██╔═══██╗",
        "██║   ██║",
        "██║   ██║",
        "╚██████╔╝",
        " ╚═════╝ "
    },
    {
        " ██╗",
        "███║",
        "╚██║",
        " ██║",
        " ██║",
        " ╚═╝"
    },
    {
        "██████╗ ",
        "╚════██╗",
        " █████╔╝",
        "██╔═══╝ ",
        "███████╗",
        "╚══════╝"
    },
    {
        "██████╗ ",
        "╚════██╗",
        " █████╔╝",
        " ╚═══██╗",
        "██████╔╝",
        "╚═════╝ "
    },
    {
        "██╗  ██╗",
        "██║  ██║",
        "███████║",
        "╚════██║",
        "     ██║",
        "     ╚═╝"
    },
    {
        "███████╗",
        "██╔════╝",
        "███████╗",
        "╚════██║",
        "███████║",
        "╚══════╝"
    },
    {
        " ██████╗",
        "██╔════╝",
        "███████╗",
        "██╔═══██╗",
        "╚██████╔╝",
        " ╚═════╝ "
    },
    {
        "███████╗",
        "╚════██║",
        "    ██╔╝",
        "   ██╔╝ ",
        "  ██╔╝  ",
        "  ╚═╝   "
    },
    {
        " █████╗ ",
        "██╔══██╗",
        "╚█████╔╝",
        "██╔══██╗",
        "╚█████╔╝",
        " ╚════╝ "
    },
    {
        " █████╗ ",
        "██╔══██╗",
        "╚██████║",
        " ╚═══██║",
        " █████╔╝",
        " ╚════╝ "
    },
    {   // Negative symbol (index 10)
        "        ",
        "        ",
        "███████╗",
        "        ",
        "        ",
        "        "
    },
    {   // Decimal point (index 11)
        "    ",
        "    ",
        "    ",
        "    ",
        " ██╗",
        " ╚═╝"
    }
};

void display_digit(WINDOW *win, int digit, int start_y, int start_x) {
    for (int i = 0; i < DIGIT_HEIGHT; i++) {
        mvwaddstr(win, start_y + i, start_x, ascii_art_digits[digit][i]);
    }
}

void display_double(WINDOW *win, double number, int start_y, int start_x, int suffix_type) {
    int x_offset = 0;
    char number_str[MAX_DIGITS];
    snprintf(number_str, MAX_DIGITS, "%.2f", number);
    for (int i = 0; number_str[i] != '\0'; i++) {
        if (number_str[i] >= '0' && number_str[i] <= '9') {
            display_digit(win, number_str[i] - '0', start_y, start_x + x_offset);
            x_offset += DIGIT_WIDTH;
        } else if (number_str[i] == '.') {
            display_digit(win, 11, start_y, start_x + x_offset);
            x_offset += 4;  // Width of decimal point
        } else if (number_str[i] == '-') {
            display_digit(win, 10, start_y, start_x + x_offset);
            x_offset += DIGIT_WIDTH;
        }
    }

    x_offset += 2;  // Width of decimal point

    for (size_t i = 0; i < DIGIT_HEIGHT; i++) {
        mvwaddstr(win, start_y + i, start_x + x_offset, power_suffix[suffix_type][i]);
    }
}
