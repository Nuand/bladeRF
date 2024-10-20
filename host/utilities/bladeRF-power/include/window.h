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
#ifndef _HELPERS_H_
#define _HELPERS_H_

#include <ncurses.h>
#include "helpers.h"


/**
 * @brief Initializes ncurses mode and creates two windows: main and log.
 *
 * This function starts the ncurses mode, sets various ncurses properties,
 * and creates two separate windows for main content and log/status display.
 *
 * @param main_win Pointer to a pointer to the main window, which will be initialized by this function.
 */
void init_curses(WINDOW **win);

/**
 * @brief Updates the main window with current test parameters.
 *
 * Clears the main window, draws a box around it, and then prints the current
 * status of the test parameters including gain calibration status, frequency,
 * and gain values.
 *
 * @param win Pointer to the main ncurses window to be updated.
 * @param test Struct containing test parameters to display.
 */
void update_window(WINDOW *win, struct test_params *test);

void display_overlay(WINDOW *main_win, const struct bladerf_gain_cal_tbl *cal_tbl);

#endif // HELPERS_H_
