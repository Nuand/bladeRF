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
#include <stdio.h>
#include <ncurses.h>
#include <window.h>
#include "helpers.h"
#include "libbladeRF.h"

typedef struct {
    double value;
    const char* unit;
} eng_double_t;

static eng_double_t uint64_to_eng_double(uint64_t value) {
    static const char* units[] = {"", "K", "M", "G", "T", "P"};
    eng_double_t value_eng = { .value = (double)value, .unit = "" };
    int unit_index = 0;

    while (value_eng.value >= 1000.0 && unit_index < 5) {
        value_eng.value /= 1000.0;
        unit_index++;
    }

    value_eng.unit = units[unit_index];
    return value_eng;
}

/**
 * @brief Converts an RX power calculation from dBFS to dBm.
 *
 * The calibration procedure references to 0dB. Once gain calibration is
 * enabled, we can directly convert the dBFS value to dBm if the gain is
 * set to 0dB. If the gain is not set to 0dB, the current gain can simply
 * be subtracted from the dBFS value to get the dBm value.
 *
 * @param power_dbfs The power in dBFS.
 * @param target_gain The current gain setting.
 * @return The power in dBm.
 */
static double rx_power_dbfs_to_dbm(double power_dbfs, bladerf_gain target_gain) {
    const double CALIBRATION_REFERENCE_GAIN = 0.0;
    return power_dbfs + (CALIBRATION_REFERENCE_GAIN - target_gain);
}

void init_curses(WINDOW **win) {
    initscr();    // Start curses mode
    noecho();     // Don't echo input characters
    timeout(0);   // Non-blocking getch() returns ERR if no input is waiting
    curs_set(0);  // Hide the cursor
    keypad(stdscr, TRUE);   // Enable keyboard mapping, such as arrow keys
    nodelay(stdscr, TRUE);  // Make getch() non-blocking

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    *win = newwin(max_y, max_x, 0, 0);
}

void update_window(WINDOW *win, struct test_params *test) {
    size_t maxy = getmaxy(win);
    size_t start_y = 1;

    werase(win);

    mvwprintw(win, start_y++, 1, "Gain Calibration: %s\n", test->gain_cal_enabled ? "enabled" : "disabled");
    mvwprintw(win, start_y++, 1, "Automatic Gain Control: %s\n", test->gain_mode == BLADERF_GAIN_MGC ? "disabled" : "enabled");
    mvwprintw(win, start_y++, 1, "Bandwidth:  %7.3f %sHz\n",
        uint64_to_eng_double(test->bandwidth).value,
        uint64_to_eng_double(test->bandwidth).unit);
    mvwprintw(win, start_y++, 1, "Frequency:  %7.3f %sHz\n",
        uint64_to_eng_double(test->frequency).value,
        uint64_to_eng_double(test->frequency).unit);
    mvwprintw(win, start_y++, 1, "Gain:       %" PRIi32 "dB actual, %" PRIi32 "dB target, range: [%" PRIi32 ", %" PRIi32 "]\n",
            test->gain_actual, test->gain, test->gain_min, test->gain_max);

    if (test->gain_cal_enabled && test->direction == BLADERF_TX) {
        mvwprintw(win, start_y++, 1, "Output Pwr:  %" PRIi32 "dBm, range: [%" PRIi32 ", %" PRIi32 "]\n",
            test->gain-60, test->gain_min-60, test->gain_max-60);
    } else if (test->gain_cal_enabled && test->direction == BLADERF_RX) {
        mvwprintw(win, start_y++, 1, "Avg Power:   %0.2fdBFS, %0.2fdBm",
            test->rx_power, rx_power_dbfs_to_dbm(test->rx_power, test->gain));
    } else if (test->direction == BLADERF_RX) {
        mvwprintw(win, start_y++, 1, "Avg Power:   %0.2fdBFS", test->rx_power);
    }

    wprintw(win, "\n");
    mvwprintw(win, maxy-2, 1, "Keys: [h/l] Frequency, [j/k] Gain, [c] Toggle Calibration, [a] Toggle AGC, [q] Quit\n");
    box(win, 0, 0);
    wrefresh(win);
}
