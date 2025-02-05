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
#include <locale.h>
#include "text.h"
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
    setlocale(LC_ALL, "");  // Set locale for wide character support
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
    const size_t MIN_LINES = 10;
    size_t maxy = getmaxy(win);
    size_t maxx = getmaxx(win);
    size_t start_y = 1;

    werase(win);

    mvwprintw(win, start_y++, 1, "Channel:    %s(%i)\n",
        test->direction == BLADERF_TX ? "TX" : "RX", test->channel);
    mvwprintw(win, start_y++, 1, "Gain Calibration: %s\n", test->gain_cal_enabled ? "enabled" : "disabled");
    mvwprintw(win, start_y++, 1, "Automatic Gain Control: %s\n", test->gain_mode == BLADERF_GAIN_MGC ? "disabled" : "enabled");
    mvwprintw(win, start_y++, 1, "Sample Rate:  %7.3f %sHz\n",
        uint64_to_eng_double(test->samp_rate).value,
        uint64_to_eng_double(test->samp_rate).unit);
    mvwprintw(win, start_y++, 1, "Frequency:  %7.3f %sHz\n",
        uint64_to_eng_double(test->frequency).value,
        uint64_to_eng_double(test->frequency).unit);
    mvwprintw(win, start_y++, 1, "Gain:       %" PRIi32 "dB actual, %" PRIi32 "dB target, range: [%" PRIi32 ", %" PRIi32 "]\n",
            test->gain_actual, test->gain, test->gain_min, test->gain_max);

    if (test->gain_cal_enabled && test->direction == BLADERF_TX) {
        mvwprintw(win, start_y++, 1, "Output Pwr:  %" PRIi32 "dBm, range: [%" PRIi32 ", %" PRIi32 "]\n",
            test->gain-60, test->gain_min-60, test->gain_max-60);
        display_double(win, test->gain-60, start_y+=2, 1, POWER_SUFFIX_DBM);
    } else if (test->gain_cal_enabled && test->direction == BLADERF_RX) {
        mvwprintw(win, start_y++, 1, "Avg Power:   %0.2fdBFS, %0.2fdBm",
            test->rx_power, rx_power_dbfs_to_dbm(test->rx_power, test->gain));
        display_double(win, rx_power_dbfs_to_dbm(test->rx_power, test->gain),
            start_y+=2, 1, POWER_SUFFIX_DBM);
    } else if (test->direction == BLADERF_RX) {
        mvwprintw(win, start_y++, 1, "Avg Power:   %0.2fdBFS", test->rx_power);
        display_double(win, test->rx_power, start_y+=2, 1, POWER_SUFFIX_DBFS);
    }

    if (test->show_messages) {
        int message_start_y = 10;
        mvwprintw(win, message_start_y, 1, "Messages:");

        // Leave space for the box and key instructions
        int available_lines = maxy - message_start_y - 3;

        char *line_start = test->message_buffer;
        char *line_end;
        int line_count = 0;
        while ((line_end = strchr(line_start, '\n')) && line_count < available_lines) {
            *line_end = '\0';  // Temporarily replace newline with null terminator
            mvwprintw(win, message_start_y + 1 + line_count, 1, "%.*s", (int)(maxx - 2), line_start);
            *line_end = '\n';  // Restore newline
            line_start = line_end + 1;
            line_count++;
        }
    }

    mvwprintw(win, maxy-2, 1, "Keys: [q] Quit, [h/l] Frequency, [j/k] Gain, [c] Toggle Calibration, [a] Toggle AGC, [i] Cal info, [m] Toggle messages\n");
    box(win, 0, 0);

    if (maxy < MIN_LINES) {
        const char* WIN_HEIGHT_WARNING = "[Warning] Increase terminal window height to see more info\0";
        int warning_length = strlen(WIN_HEIGHT_WARNING);
        int start_x = (maxx - warning_length) / 2;
        mvwprintw(win, 0, start_x, "%s", WIN_HEIGHT_WARNING);
    }

    wnoutrefresh(win);
    doupdate();
}

static char* truncate_path(const char* path, size_t max_width) {
    static char truncated[256];  // Adjust size as needed
    if (strlen(path) <= max_width) {
        return (char*)path;
    }

    int prefix_len = (max_width - 3) / 2;
    int suffix_len = max_width - 3 - prefix_len;

    snprintf(truncated, sizeof(truncated), "%.*s...%s",
             prefix_len, path, path + strlen(path) - suffix_len);
    return truncated;
}

void display_overlay(WINDOW *main_win, const struct bladerf_gain_cal_tbl *cal_tbl) {
    int main_height, main_width;
    getmaxyx(main_win, main_height, main_width);
    size_t overlay_width = main_width / 2;
    size_t overlay_height = main_height - 3;

    WINDOW *overlay_win = newwin(overlay_height, overlay_width, 1, main_width / 4);

    size_t start_y = 3;
    mvwprintw(overlay_win, 1, 2, "Gain Calibration Table Info");
    mvwprintw(overlay_win, start_y++, 2, "File: %s",
              truncate_path(cal_tbl->file_path, overlay_width - 10));
    mvwprintw(overlay_win, start_y++, 2, "Version: %u.%u.%u",
              cal_tbl->version.major, cal_tbl->version.minor, cal_tbl->version.patch);
    mvwprintw(overlay_win, start_y++, 2, "Channel: %s",
              cal_tbl->ch == BLADERF_CHANNEL_RX(0) ? "RX1" :
              cal_tbl->ch == BLADERF_CHANNEL_TX(0) ? "TX1" :
              cal_tbl->ch == BLADERF_CHANNEL_RX(1) ? "RX2" :
              cal_tbl->ch == BLADERF_CHANNEL_TX(1) ? "TX2" : "Unknown");
    mvwprintw(overlay_win, start_y++, 2, "Num. Entries: %u", cal_tbl->n_entries);
    mvwprintw(overlay_win, start_y++, 2, "First Freq: %.3f MHz",
              (double)cal_tbl->start_freq / 1e6);
    mvwprintw(overlay_win, start_y++, 2, "Last Freq: %.3f MHz",
              (double)cal_tbl->stop_freq / 1e6);
    mvwprintw(overlay_win, start_y++, 2, "Enabled: %s", cal_tbl->enabled ? "Yes" : "No");
    mvwprintw(overlay_win, start_y++, 2, "Gain Target: %d dB", cal_tbl->gain_target);
    mvwprintw(overlay_win, start_y++, 2, "State: %s",
              cal_tbl->state == BLADERF_GAIN_CAL_UNINITIALIZED ? "Uninitialized" :
              cal_tbl->state == BLADERF_GAIN_CAL_LOADED ? "Loaded" :
              cal_tbl->state == BLADERF_GAIN_CAL_UNLOADED ? "Unloaded" : "Unknown");


    box(overlay_win, 0, 0);

    if (overlay_height < start_y + 2) {
        const char* WIN_HEIGHT_WARNING = "[Warning] Increase terminal window height to see more info\0";
        int warning_length = strlen(WIN_HEIGHT_WARNING);
        int start_x = (getmaxx(overlay_win) - warning_length) / 2;
        mvwprintw(overlay_win, 0, start_x, "%s", WIN_HEIGHT_WARNING);
    }

    mvwprintw(overlay_win, main_height - 5, 2, "Press any key to close");
    wnoutrefresh(overlay_win);
    doupdate();
}
