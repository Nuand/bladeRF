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
#include <errno.h>
#include "conversions.h"

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

unsigned int str2uint_suffix(const char *str, unsigned int min,
        unsigned int max, const struct numeric_suffix suffixes[],
        int num_suffixes, bool *ok)
{
    double value;
    char *endptr;
    int i;

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

    /* Truncate the floating point value to an integer and return it */
    return (unsigned int)value;
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
