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
#include <curses.h>

#define DIGIT_HEIGHT 6
#define DIGIT_WIDTH 11
#define MAX_DIGITS 20
#define POWER_SUFFIX_DBM 0
#define POWER_SUFFIX_DBFS 1
#define DEFCON_ART_HEIGHT 5
#define DEFCON_ART_WIDTH 56
#define DEFCON_ART_VARIATIONS 6

/**
 * @brief Display a double in the form of ASCII art
 *
 * @param win       Window to display the ASCII art in
 * @param number    Number to display
 * @param start_y   Starting y coordinate
 * @param start_x   Starting x coordinate
 * @param suffix_type   Type of suffix to display
 *
 * @note This function will display a double in the form of ASCII art
 */
void display_double(WINDOW *win, double number, int start_y, int start_x, int suffix_type);


/**
 * @brief Display DEFCON ASCII art
 *
 * @param win Window to display the art in
 * @param start_y Starting y coordinate
 * @param start_x Starting x coordinate
 * @param variation Variation of the art to display
 *
 * @note The variation parameter is used to select a specific DEFCON ASCII art
 */
void display_defcon_art(WINDOW *win, int start_y, int start_x, int variation);

