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
#include <math.h>
#include "window.h"
#include "helpers.h"
#include "libbladeRF.h"
#include "log.h"
#include "filter.h"

#define INT12_MAX 2047

#define CHECK(fn) do { \
    status = fn; \
    if (status != 0) { \
        fprintf(stderr, "[Error] %s:%d: %s - %s\n", __FILE__, __LINE__, #fn, bladerf_strerror(status)); \
        fprintf(stderr, "Exiting.\n"); \
        goto error; \
    } \
} while (0)

#define CHECK_NULL(...) do { \
    const void* _args[] = { __VA_ARGS__, NULL }; \
    for (size_t _i = 0; _args[_i] != NULL; ++_i) { \
        if (_args[_i] == NULL) { \
            log_error("%s:%d: Argument %zu is a NULL pointer\n", __FILE__, __LINE__, _i + 1); \
            return BLADERF_ERR_INVAL; \
        } \
    } \
} while (0)

bladerf_direction ask_direction() {
    char direction;
    printf("Enter direction (tx/rx/q[uit]): ");

    int ret = scanf("%c", &direction);
    if (ret == EOF || ret == 0) {
        printf("Error reading direction. Exiting...\n");
        exit(1);
    }
    while (getchar() != '\n');

    if (direction == 't' || direction == 'r') {
        return (direction == 't') ? BLADERF_TX : BLADERF_RX;
    } else if (direction == 'q') {
        printf("Exiting...\n");
        exit(0);
    } else {
        bladerf_direction dir = ask_direction();
        return dir;
    }
}

static int calculate_power(struct bladerf *dev, const int16_t *samples, size_t num_samples, double *power) {
    CHECK_NULL(dev, samples, power);
    int status = 0;
    double sum = 0;
    int16_t *samples_filtered = NULL;

    samples_filtered = malloc(2 * num_samples * sizeof(int16_t));
    CHECK_NULL(samples_filtered);

    CHECK(flatten_noise_figure(dev, samples, num_samples, samples_filtered));

    for (size_t i = 0; i < num_samples; i++) {
        sum += (samples_filtered[2*i] * samples_filtered[2*i]) + (samples_filtered[2*i+1] * samples_filtered[2*i+1]);
    }

    const double avg_power = sum / num_samples;
    const double max_power = INT12_MAX * INT12_MAX;
    const double avg_power_dbfs = 10 * log10(avg_power / max_power);
    *power = avg_power_dbfs;

error:
    if (samples_filtered != NULL)
        free(samples_filtered);

    return status;
}

int start_streaming(struct bladerf *dev, struct test_params *test) {
    int status = 0;
    WINDOW *main_win = NULL;
    bladerf_channel ch = (test->direction == BLADERF_TX) ? BLADERF_CHANNEL_TX(0) : BLADERF_CHANNEL_RX(0);
    const size_t num_samples = test->samp_rate / 30; // 30Hz
    const struct bladerf_gain_cal_tbl *gain_tbl = NULL;

    int16_t *samples = malloc(2 * num_samples * sizeof(int16_t));
    if (samples == NULL) {
        fprintf(stderr, "Error allocating memory for samples. Exiting...\n");
        return BLADERF_ERR_INVAL;
    }

    if (test->direction == BLADERF_TX) {
        for (size_t i = 0; i < num_samples; i++) {
            samples[2*i]   = INT12_MAX;
            samples[2*i+1] = INT12_MAX;
        }
    }

    CHECK(bladerf_get_gain_calibration(dev, ch, &gain_tbl));
    test->gain_cal_enabled = gain_tbl->enabled;

    CHECK(bladerf_enable_module(dev, ch, true));
    CHECK(bladerf_get_gain(dev, ch, &test->gain_actual));
    CHECK(bladerf_get_frequency(dev, ch, &test->frequency_actual));

    int cmd = 0;
    init_curses(&main_win);
    while (status == 0 && cmd != 'q') {
        cmd = getch();

        if (cmd == 'l' || cmd == KEY_RIGHT) {
            bladerf_frequency next_freq = test->frequency + 5e6;
            test->frequency = (next_freq > test->freq_max) ? test->freq_max : next_freq;
            CHECK(bladerf_set_frequency(dev, ch, test->frequency));
            CHECK(bladerf_get_frequency(dev, ch, &test->frequency_actual));
        }

        if (cmd == 'h' || cmd == KEY_LEFT) {
            bladerf_frequency next_freq = test->frequency - 5e6;
            test->frequency = (next_freq < test->freq_min) ? test->freq_min : next_freq;
            CHECK(bladerf_set_frequency(dev, ch, test->frequency));
            CHECK(bladerf_get_frequency(dev, ch, &test->frequency_actual));
        }

        if ((cmd == 'k' || cmd == KEY_UP) && test->gain + 1 <= test->gain_max) {
            CHECK(bladerf_set_gain(dev, ch, test->gain + 1));
            CHECK(bladerf_get_gain(dev, ch, &test->gain_actual));
        }

        if ((cmd == 'j' || cmd == KEY_DOWN) && test->gain - 1 >= test->gain_min) {
            CHECK(bladerf_set_gain(dev, ch, test->gain - 1));
            CHECK(bladerf_get_gain(dev, ch, &test->gain_actual));
        }

        if (cmd == 'c') {
            CHECK(bladerf_get_gain_calibration(dev, ch, &gain_tbl));
            CHECK(bladerf_enable_gain_calibration(dev, ch, !gain_tbl->enabled));
            CHECK(bladerf_get_gain_calibration(dev, ch, &gain_tbl));
            test->gain_cal_enabled = gain_tbl->enabled;
        }

        if (cmd == 'a' && BLADERF_CHANNEL_IS_TX(ch) == false) {
            bladerf_gain_mode next_mode;

            next_mode = (test->gain_mode == BLADERF_GAIN_MGC)
                            ? BLADERF_GAIN_DEFAULT
                            : BLADERF_GAIN_MGC;

            CHECK(bladerf_set_gain_mode(dev, ch, next_mode));
            CHECK(bladerf_get_gain_mode(dev, ch, &test->gain_mode));
        }

        if (BLADERF_CHANNEL_IS_TX(ch) == false) {
            CHECK(bladerf_get_gain_mode(dev, ch, &test->gain_mode));
        }

        CHECK(bladerf_get_gain(dev, ch, &test->gain_actual));
        CHECK(bladerf_get_gain_target(dev, ch, &test->gain));

        update_window(main_win, test);

        if (test->direction == BLADERF_RX) {
            CHECK(bladerf_sync_rx(dev, samples, num_samples, NULL, 1000));
            CHECK(calculate_power(dev, samples, num_samples, &test->rx_power));
        } else {
            CHECK(bladerf_sync_tx(dev, samples, num_samples, NULL, 1000));
        }
    }

error:
    bladerf_enable_module(dev, ch, false);
    free(samples);

    delwin(main_win);
    endwin();
    return status;
}

struct numeric_suffix const freq_suffixes[NUM_FREQ_SUFFIXES] = {
    { "G",      1000 * 1000 * 1000 },
    { "GHz",    1000 * 1000 * 1000 },
    { "M",      1000 * 1000 },
    { "MHz",    1000 * 1000 },
    { "k",      1000 },
    { "kHz",    1000 }
};
