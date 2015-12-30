/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (c) 2013 Nuand LLC
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
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include "conversions.h"

enum str2args_parse_state {
    PARSE_STATE_IN_SPACE,
    PARSE_STATE_START_ARG,
    PARSE_STATE_IN_ARG,
    PARSE_STATE_IN_QUOTE,
    PARSE_STATE_ERROR
};


int str2int(const char *str, int min, int max, bool *ok)
{
    long value;
    char *endptr;

    errno = 0;
    value = strtol(str, &endptr, 0);

    if (errno != 0 || value < (long)min || value > (long)max ||
        endptr == str || *endptr != '\0') {

        if (ok) {
            *ok = false;
        }

        return 0;
    }

    if (ok) {
        *ok = true;
    }
    return (int)value;
}

unsigned int str2uint(const char *str, unsigned int min, unsigned int max, bool *ok)
{
    unsigned long value;
    char *endptr;

    errno = 0;
    value = strtoul(str, &endptr, 0);

    if (errno != 0 ||
        value < (unsigned long)min || value > (unsigned long)max ||
        endptr == str || *endptr != '\0') {

        if (ok) {
            *ok = false;
        }

        return 0;
    }

    if (ok) {
        *ok = true;
    }
    return (unsigned int)value;
}


uint64_t str2uint64(const char *str, uint64_t min, uint64_t max, bool *ok)
{
    unsigned long long value;
    char *endptr;

    errno = 0;
    value = strtoull(str, &endptr, 0);

    if (errno != 0 || endptr == str || *endptr != '\0' ||
        value < (unsigned long long)min || value > (unsigned long long)max) {

        if (ok) {
            *ok = false;
        }

        return 0;
    }

    if (ok) {
        *ok = true;
    }

    return (uint64_t)value;
}

double str2double(const char *str, double min, double max, bool *ok)
{
    double value;
    char *endptr;

    errno = 0;
    value = strtod(str, &endptr);

    if (errno != 0 || value < min || value > max ||
        endptr == str || *endptr != '\0') {

        if (ok) {
            *ok = false;
        }

        return 0;
    }

    if (ok) {
        *ok = true;
    }

    return value;
}

/* MSVC workarounds */

#if _MSC_VER   
    /* This appears to be missing <= 2012 */
#   ifndef INFINITY
#       define INFINITY (_HUGE * _HUGE)
#   endif

   /* As of MSVC 2013, INFINITY appears to be available, but
    * the compiler emits a warning for every usage, despite it
    * causing an overflow by design.
    *  https://msdn.microsoft.com/en-us/library/cwt7tyxx.aspx
    *
    * Oddly, we see warning 4056, despite math.h noting that 4756
    * will be induced (with the same description).
    */
#   pragma warning (push)
#   pragma warning (disable:4056)
#endif

double str2dbl_suffix(const char *str,
                      double min, double max,
                      const struct numeric_suffix suffixes[],
                      size_t num_suffixes, bool *ok)
{
    double value;
    char *endptr;
    size_t i;

    errno = 0;
    value = strtod(str, &endptr);

    /* If a number could not be parsed at the beginning of the string */
    if (errno != 0 || endptr == str) {
        if (ok) {
            *ok = false;
        }

        return 0;
    }

    /* Loop through each available suffix */
    for (i = 0; i < num_suffixes; i++) {
        /* If the suffix appears at the end of the number */
        if (!strcasecmp(endptr, suffixes[i].suffix)) {
            /* Apply the multiplier */
            value *= suffixes[i].multiplier;
            break;
        }
    }

    /* Test for overflow */
    if (value == INFINITY || value == -INFINITY) {
        if (ok) {
            *ok = false;
        }

        return 0;
    }

    /* Check that the resulting value is in bounds */
    if (value > max || value < min) {
        if (ok) {
            *ok = false;
        }

        return 0;
    }

    if (ok) {
        *ok = true;
    }

    return value;
}
#if _MSC_VER
#   pragma warning(pop)
#endif

unsigned int str2uint_suffix(const char *str,
                             unsigned int min, unsigned int max,
                             const struct numeric_suffix suffixes[],
                             size_t num_suffixes, bool *ok)
{
    return (unsigned int) str2dbl_suffix(str, min, max,
                                         suffixes, num_suffixes, ok);
}

uint64_t str2uint64_suffix(const char *str,
                           uint64_t min, uint64_t max,
                           const struct numeric_suffix suffixes[],
                           size_t num_suffixes, bool *ok)
{
    /* FIXME: Potential loss of precision on min/max here */
    return (uint64_t) str2dbl_suffix(str, (double) min, (double) max,
                                     suffixes, num_suffixes, ok);
}
int str2version(const char *str, struct bladerf_version *version)
{
    unsigned long tmp;
    const char *start = str;
    char *end;

    /* Major version */
    errno = 0;
    tmp = strtoul(start, &end, 10);
    if (errno != 0 || tmp > UINT16_MAX || end == start || *end != '.') {
        return -1;
    }
    version->major = (uint16_t)tmp;

    /* Minor version */
    if (end[0] == '\0' || end[1] == '\0') {
        return -1;
    }
    errno = 0;
    start = &end[1];
    tmp = strtoul(start, &end, 10);
    if (errno != 0 || tmp > UINT16_MAX || end == start || *end != '.') {
        return -1;
    }
    version->minor = (uint16_t)tmp;

    /* Patch version */
    if (end[0] == '\0' || end[1] == '\0') {
        return -1;
    }
    errno = 0;
    start = &end[1];
    tmp = strtoul(start, &end, 10);
    if (errno != 0 || tmp > UINT16_MAX || end == start ||
            (*end != '-' && *end != '\0')) {
        return -1;
    }
    version->patch = (uint16_t)tmp;

    version->describe = str;

    return 0;
}

void free_args(int argc, char **argv)
{
    int i;

    if (argc >= 0 && argv != NULL) {

        for (i = 0; i < argc; i++) {
            free(argv[i]);
        }
        free(argv);
    }
}

static void zero_argvs(int start, int end, char **argv)
{
    int i;
    for (i = start; i <= end; i++) {
        argv[i] = NULL;
    }
}

/* Returns 0 on success, -1 on failure */
static int append_char(char **arg, int *arg_size, int *arg_i, char c)
{
    char *tmp;

    if (*arg_i >= *arg_size) {
        tmp = (char *)realloc(*arg, *arg_size * 2);

        if (!tmp) {
            return -1;
        } else {
            memset(tmp + *arg_size, 0, *arg_size);
            *arg = tmp;
            *arg_size = *arg_size * 2;
        }
    }

    (*arg)[*arg_i] = c;
    *arg_i += 1;

    return 0;
}

const char * devspeed2str(bladerf_dev_speed speed)
{
    switch (speed) {
        case BLADERF_DEVICE_SPEED_HIGH:
            /* Yeah, the USB IF actually spelled it "Hi" instead of "High".
             * I know. It hurts me too. */
            return "Hi-Speed";

        case BLADERF_DEVICE_SPEED_SUPER:
            /* ...and no hyphen :( */
            return "SuperSpeed";

        default:
            return "Unknown";
    }
}

bladerf_log_level str2loglevel(const char *str, bool *ok)
{
    bladerf_log_level level = BLADERF_LOG_LEVEL_ERROR;
    bool valid = true;

    if (!strcasecmp(str, "critical")) {
        level = BLADERF_LOG_LEVEL_CRITICAL;
    } else if (!strcasecmp(str, "error")) {
        level = BLADERF_LOG_LEVEL_ERROR;
    } else if (!strcasecmp(str, "warning")) {
        level = BLADERF_LOG_LEVEL_WARNING;
    } else if (!strcasecmp(str, "info")) {
        level = BLADERF_LOG_LEVEL_INFO;
    } else if (!strcasecmp(str, "debug")) {
        level = BLADERF_LOG_LEVEL_DEBUG;
    } else if (!strcasecmp(str, "verbose")) {
        level = BLADERF_LOG_LEVEL_VERBOSE;
    } else {
        valid = false;
    }

    *ok = valid;
    return level;
}

const char * module2str(bladerf_module m)
{
    switch (m) {
        case BLADERF_MODULE_RX:
            return "RX";
        case BLADERF_MODULE_TX:
            return "TX";
        default:
            return "Unknown";
    }
}

bladerf_module str2module(const char *str)
{
    if (!strcasecmp(str, "RX")) {
        return BLADERF_MODULE_RX;
    } else if (!strcasecmp(str, "TX")) {
        return BLADERF_MODULE_TX;
    } else {
        return BLADERF_MODULE_INVALID;
    }
}

int str2loopback(const char *str, bladerf_loopback *loopback)
{
    int status = 0;

    if (!strcasecmp("bb_txlpf_rxvga2", str)) {
        *loopback = BLADERF_LB_BB_TXLPF_RXVGA2;
    } else if (!strcasecmp("bb_txlpf_rxlpf", str)) {
        *loopback = BLADERF_LB_BB_TXLPF_RXLPF;
    } else if (!strcasecmp("bb_txvga1_rxvga2", str)) {
        *loopback = BLADERF_LB_BB_TXVGA1_RXVGA2;
    } else if (!strcasecmp("bb_txvga1_rxlpf", str)) {
        *loopback = BLADERF_LB_BB_TXVGA1_RXLPF;
    } else if (!strcasecmp("rf_lna1", str)) {
        *loopback = BLADERF_LB_RF_LNA1;
    } else if (!strcasecmp("rf_lna2", str)) {
        *loopback = BLADERF_LB_RF_LNA2;
    } else if (!strcasecmp("rf_lna3", str)) {
        *loopback = BLADERF_LB_RF_LNA3;
    } else if (!strcasecmp("none", str)) {
        *loopback = BLADERF_LB_NONE;
    } else {
        status = -1;
    }

    return status;
}

int str2args(const char *line, char comment_char, char ***argv_ret)
{
    int line_i, arg_i;      /* Index into line and current argument */
    int argv_size = 10;     /* Initial # of allocated args */
    int arg_size;           /* Allocated size of the curr arg */
    char **argv;
    int argc;
    enum str2args_parse_state state = PARSE_STATE_IN_SPACE;
    const size_t line_len = strlen(line);
    bool got_eol_comment = false;


    argc = arg_i = 0;
    argv = (char **)malloc(argv_size * sizeof(char *));
    if (!argv) {
        return -1;
    }

    zero_argvs(0, argv_size - 1, argv);
    arg_size = 0;
    line_i = 0;

    while ( ((size_t)line_i < line_len) &&
            state != PARSE_STATE_ERROR  &&
            !got_eol_comment) {

        switch (state) {
            case PARSE_STATE_IN_SPACE:
                /* Found the start of the next argument */
                if (!isspace((unsigned char) line[line_i])) {
                    if (line[line_i] == comment_char) {
                        got_eol_comment = true;
                    } else {
                        state = PARSE_STATE_START_ARG;
                    }
                } else {
                    /* Gobble up space */
                    line_i++;
                }
                break;

            case PARSE_STATE_START_ARG:
                /* Increase size of argv, if needed */
                if (argc >= argv_size) {
                    void *tmp;
                    argv_size = argv_size + argv_size / 2;
                    tmp = realloc(argv, argv_size);

                    if (tmp) {
                        argv = (char **)tmp;
                        zero_argvs(argc, argv_size - 1, argv);
                    } else {
                        state = PARSE_STATE_ERROR;
                    }
                }

                /* Record start of word (unless we failed to realloc() */
                if (state != PARSE_STATE_ERROR) {

                    /* Reset per-arg variables */
                    arg_i = 0;
                    arg_size = 32;

                    /* Allocate this argument. This will be
                     * realloc'd as necessary by append_char() */
                    argv[argc] = (char *)calloc(arg_size, 1);
                    if (!argv[argc]) {
                        state = PARSE_STATE_ERROR;
                        break;
                    }

                    if (line[line_i] == '"') {
                        /* Gobble up quote */
                        state = PARSE_STATE_IN_QUOTE;
                    } else {
                        /* Append this character to the argument begin
                         * fetching up a word */
                        if (append_char(&argv[argc], &arg_size,
                                    &arg_i, line[line_i])) {
                            state = PARSE_STATE_ERROR;
                        } else {
                            state = PARSE_STATE_IN_ARG;
                        }
                    }

                    argc++;
                    line_i++;
                }

                break;

            case PARSE_STATE_IN_ARG:
                if (isspace((unsigned char) line[line_i])) {
                    state = PARSE_STATE_IN_SPACE;
                } else if (line[line_i] == '"') {
                    state = PARSE_STATE_IN_QUOTE;
                } else if (line[line_i] == comment_char) {
                    got_eol_comment = true;
                } else {
                    /* Append this character to the argument and remain in
                     * PARSE_STATE_IN_ARG state */
                    if (append_char(&argv[argc - 1], &arg_size,
                                    &arg_i, line[line_i])) {
                        state = PARSE_STATE_ERROR;
                    }
                }

                line_i++;
                break;


            case PARSE_STATE_IN_QUOTE:
                if (line[line_i] == '"') {
                    /* Return to looking for more of the word */
                    state = PARSE_STATE_IN_ARG;
                } else {
                    /* Append this character to the argumen */
                  if (append_char(&argv[argc - 1], &arg_size,
                                  &arg_i, line[line_i])) {
                      state = PARSE_STATE_ERROR;
                  }
                }

                line_i++;
                break;

            case PARSE_STATE_ERROR:
                break;
        }
    }

    /* Print PARSE_STATE_ERROR message if hit the EOL in an invalid state */
    switch (state) {
        case PARSE_STATE_IN_SPACE:
        case PARSE_STATE_IN_ARG:
            *argv_ret = argv;
            break;

        case PARSE_STATE_IN_QUOTE:
            free_args(argc, argv);
            argc = -2;
            break;

        default:
            free_args(argc, argv);
            argc = -1;
            break;
    }

    return argc;
}

int str2lnagain(const char *str, bladerf_lna_gain *gain)
{
    *gain = BLADERF_LNA_GAIN_MAX;

    if (!strcasecmp("max", str) ||
        !strcasecmp("BLADERF_LNA_GAIN_MAX", str)) {
        *gain = BLADERF_LNA_GAIN_MAX;
        return 0;
    } else if (!strcasecmp("mid", str) ||
               !strcasecmp("BLADERF_LNA_GAIN_MID", str)) {
        *gain = BLADERF_LNA_GAIN_MID;
        return 0;
    } else if (!strcasecmp("bypass", str) ||
               !strcasecmp("BLADERF_LNA_GAIN_BYPASS", str)) {
        *gain = BLADERF_LNA_GAIN_BYPASS;
        return 0;
    } else {
        *gain = BLADERF_LNA_GAIN_UNKNOWN;
        return -1;
    }
}

const char *backend_description(bladerf_backend b)
{
    switch (b) {
        case BLADERF_BACKEND_ANY:
            return "Any";

        case BLADERF_BACKEND_LINUX:
            return "Linux kernel driver";

        case BLADERF_BACKEND_LIBUSB:
            return "libusb";

        case BLADERF_BACKEND_CYPRESS:
            return "Cypress driver";

        case BLADERF_BACKEND_DUMMY:
            return "Dummy";

        default:
            return "Unknown";
    }
}

void sc16q11_to_float(const int16_t *in, float *out, unsigned int n)
{
    unsigned int i;

    for (i = 0; i < (2 * n); i += 2) {
        out[i]   = (float) in[i]   * (1.0f / 2048.0f);
        out[i+1] = (float) in[i+1] * (1.0f / 2048.0f);
    }
}

void float_to_sc16q11(const float *in, int16_t *out, unsigned int n)
{
    unsigned int i;

    for (i = 0; i < (2 * n); i += 2) {
        out[i]   = (int16_t) (in[i]   * 2048.0f);
        out[i+1] = (int16_t) (in[i+1] * 2048.0f);
    }
}

bladerf_cal_module str_to_bladerf_cal_module(const char *str)
{
    bladerf_cal_module module = BLADERF_DC_CAL_INVALID;

    if (!strcasecmp(str, "lpf_tuning") || !strcasecmp(str, "lpftuning") ||
        !strcasecmp(str, "tuning")) {
        module = BLADERF_DC_CAL_LPF_TUNING;
    } else if (!strcasecmp(str, "tx_lpf")  || !strcasecmp(str, "txlpf")) {
        module = BLADERF_DC_CAL_TX_LPF;
    } else if (!strcasecmp(str, "rx_lpf")  || !strcasecmp(str, "rxlpf")) {
        module = BLADERF_DC_CAL_RX_LPF;
    } else if (!strcasecmp(str, "rx_vga2") || !strcasecmp(str, "rxvga2")) {
        module = BLADERF_DC_CAL_RXVGA2;
    }

    return module;
}
