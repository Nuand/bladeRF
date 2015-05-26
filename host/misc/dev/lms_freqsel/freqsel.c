/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (c) 2015 Nuand LLC
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
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <inttypes.h>
#include <assert.h>
#include <libbladeRF.h>

/* Just for easy copy/paste back into lms.c code */
#define FREQ_RANGE(l, h, v) {l, h, v}

struct freq_range {
    uint32_t    low;
    uint32_t    high;
    uint8_t     value;
};

struct vco_range {
    uint64_t low;
    uint64_t high;
};

/* It appears that VCO1-4 are mislabeled in the LMS6002D FAQ 5.24 plot.
 * Note that the below are labeled "correctly," so they will not match the plot.
 *
 * Additionally, these have been defined a bit more conservatively, rounding
 * to the nearest 100 MHz contained within a band.
 */
static const struct vco_range vco[] = {
    { 0,                       0},            /* Dummy entry */
    { 6300000000u,             7700000000u }, /* VCO1 */
    { 5300000000u,             6900000000u },
    { 4500000000u,             5600000000u },
    { 3700000000u,             4700000000u }, /* VCO4 */
};

/* Here we define more conservative band ranges than those in the
 * LMS FAQ (5.24), with the intent of avoiding the use of "edges" that might
 * cause the PLLs to lose lock over temperature changes */
#define VCO4_LOW    3800000000ull
#define VCO4_HIGH   4590000000ull

#define VCO3_LOW    VCO4_HIGH
#define VCO3_HIGH   5408000000ull

#define VCO2_LOW    VCO3_HIGH
#define VCO2_HIGH   6480000000ull

#define VCO1_LOW    VCO2_HIGH
#define VCO1_HIGH   7600000000ull

#if VCO4_LOW/16 != BLADERF_FREQUENCY_MIN
#   error "BLADERF_FREQUENCY_MIN is not actual VCO4_LOW/16 minimum"
#endif

#if VCO1_HIGH/2 != BLADERF_FREQUENCY_MAX
#   error "BLADERF_FREQUENCY_MAX is not actual VCO1_HIGH/2 maximum"
#endif

/* SELVCO values */
#define VCO4 (4 << 3)
#define VCO3 (5 << 3)
#define VCO2 (6 << 3)
#define VCO1 (7 << 3)

/* FRANGE values */
#define DIV2  0x4
#define DIV4  0x5
#define DIV8  0x6
#define DIV16 0x7

/* Additional changes made after tightening "keepout" percentage */
static const struct freq_range bands[] = {
    FREQ_RANGE(BLADERF_FREQUENCY_MIN,   VCO4_HIGH/16,           VCO4 | DIV16),
    FREQ_RANGE(VCO3_LOW/16,             VCO3_HIGH/16,           VCO3 | DIV16),
    FREQ_RANGE(VCO2_LOW/16,             VCO2_HIGH/16,           VCO2 | DIV16),
    FREQ_RANGE(VCO1_LOW/16,             VCO1_HIGH/16,           VCO1 | DIV16),
    FREQ_RANGE(VCO4_LOW/8,              VCO4_HIGH/8,            VCO4 | DIV8),
    FREQ_RANGE(VCO3_LOW/8,              VCO3_HIGH/8,            VCO3 | DIV8),
    FREQ_RANGE(VCO2_LOW/8,              VCO2_HIGH/8,            VCO2 | DIV8),
    FREQ_RANGE(VCO1_LOW/8,              VCO1_HIGH/8,            VCO1 | DIV8),
    FREQ_RANGE(VCO4_LOW/4,              VCO4_HIGH/4,            VCO4 | DIV4),
    FREQ_RANGE(VCO3_LOW/4,              VCO3_HIGH/4,            VCO3 | DIV4),
    FREQ_RANGE(VCO2_LOW/4,              VCO2_HIGH/4,            VCO2 | DIV4),
    FREQ_RANGE(VCO1_LOW/4,              VCO1_HIGH/4,            VCO1 | DIV4),
    FREQ_RANGE(VCO4_LOW/2,              VCO4_HIGH/2,            VCO4 | DIV2),
    FREQ_RANGE(VCO3_LOW/2,              VCO3_HIGH/2,            VCO3 | DIV2),
    FREQ_RANGE(VCO2_LOW/2,              VCO2_HIGH/2,            VCO2 | DIV2),
    FREQ_RANGE(VCO1_LOW/2,              BLADERF_FREQUENCY_MAX,  VCO1 | DIV2),
};

static const size_t num_bands = sizeof(bands) / sizeof(bands[0]);

static const char *get_selvco(uint8_t freqsel, unsigned int *vco)
{
    const char *ret;
    unsigned int vco_num = 0;

    freqsel >>= 3;

    switch (freqsel) {
        case 0x0:
            ret = "000 - All VCOs PD    ";
            break;

        case 0x4:
            vco_num = 4;
            ret = "100 - VCO4 (Low)     ";
            break;

        case 0x5:
            vco_num = 3;
            ret = "101 - VCO3 (Mid Low) ";
            break;

        case 0x6:
            vco_num = 2; ret =  "110 - VCO2 (Mid High)";
            break;

        case 0x7:
            vco_num = 1;
            ret = "111 - VCO1 (High)    ";
            break;

        default:
            ret = "XXX - Invalid SELVCO ";
            break;
    }

    if (vco != NULL) {
        *vco = vco_num;
    }

    return ret;
}

static const char *get_frange(uint8_t freqsel, unsigned int *div)
{
    const char *ret;
    unsigned int div_val = 0;

    freqsel &= 0x7;

    switch (freqsel) {
        case 0x0:
            ret = "000 - Divs PD";
            break;

        case 0x4:
            div_val = 2;
            ret = "100 - Fvco/2 ";
            break;

        case 0x5:
            div_val = 4;
            ret = "101 - Fvco/4 ";
            break;

        case 0x6:
            div_val = 8;
            ret = "110 - Fvco/8 ";
            break;

        case 0x7:
            div_val = 16;
            ret = "111 - Fvco/16";
            break;

        default:
            ret = "XXX - Invalid FRANGE";
            break;

    }

    if (div != NULL) {
        *div = div_val;
    }

    return ret;
}


static void print_table()
{
    size_t i;

    putchar('\n');
    puts("         Range                   SELVCO                  FRANGE");
    puts("-------------------------------------------------------------------");

    for (i = 0; i < num_bands; i++) {
        printf("%10u - %10u     %s     %s\n",
               bands[i].low, bands[i].high,
               get_selvco(bands[i].value, NULL),
               get_frange(bands[i].value, NULL));
    }

    putchar('\n');
}

static void check_vco_range(uint8_t freqsel, uint64_t low, uint64_t high,
                            float keepout_percent)
{
    unsigned int n, div;
    uint64_t vco_width, keepout;
    uint64_t scaled_low, scaled_high;;
    uint64_t low_limit, high_limit;

    get_selvco(freqsel, &n);
    get_frange(freqsel, &div);

    vco_width = vco[n].high - vco[n].low;
    keepout = (float)vco_width * keepout_percent;

    scaled_low = low * div;
    low_limit = vco[n].low + (keepout);

    scaled_high = high * div;
    high_limit = vco[n].high - (keepout);

    printf("  Fscaled,low:  %10"PRIu64"    Flimit,low:  %10"PRIu64"\n",
            scaled_low, low_limit);

    printf("  Fscaled,high: %10"PRIu64"    Flimit,high: %10"PRIu64"\n\n",
            scaled_high, high_limit);

    if (scaled_low < low_limit) {
        printf("  Error: F=%"PRIu64" is too low using VCO%u with div=%u\n",
               low, n, div);
    } else if (scaled_low > high_limit) {
        printf("  Error: F=%"PRIu64" is too *high* using VCO%u with div=%u\n",
               low, n, div);
    } else {
        printf("  Low  OK\n");
    }

    if (scaled_high > high_limit) {
        printf("  Error: F=%"PRIu64" is too high using VCO%u with div=%u\n",
               high, n, div);
    } else if (scaled_high < low_limit) {
        printf("  Error: F=%"PRIu64" is too *low* using VCO%u with div=%u\n",
                high, n, div);
    } else {
        printf("  High OK\n");
    }

    putchar('\n');
}

static void check_vco_ranges(float keepout_percent)
{
    size_t i;

    keepout_percent = keepout_percent / 100.0;

    printf("Checking VCO ranges with keepout = %f%% on either end of a band\n\n",
           100.0 * keepout_percent);

    for (i = 0; i < num_bands; i++) {
        printf("Checking %10u - %10u...\n", bands[i].low, bands[i].high);

        check_vco_range(bands[i].value,
                        bands[i].low, bands[i].high,
                        keepout_percent);
    }
}

int main(void)
{
    print_table();

    /* Check the ranges, with 6.5% on either end of a band marked "keep out" */
    check_vco_ranges(6.5);

    return 0;
}
