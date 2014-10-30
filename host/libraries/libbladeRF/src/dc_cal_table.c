/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014 Nuand LLC
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/* The binary DC calibration data is stored as follows. All values are
 * little-endian byte order.
 *
 * 0x0000 [uint16_t: Fixed value of 0x9a51]
 * 0x0002 [uint32_t: Reserved. Set to 0x00000000]
 * 0x0006 [uint32_t: Table format version]
 * 0x000a [uint32_t: Number of entries]
 * 0x000e [uint8_t:  LMS LPF tuning register value]
 * 0x000f [uint8_t:  LMS TX LPF I register value]
 * 0x0010 [uint8_t:  LMS TX LPF Q register value]
 * 0x0011 [uint8_t:  LMS RX LPF I register value]
 * 0x0012 [uint8_t:  LMS RX LPF Q register value]
 * 0x0013 [uint8_t:  LMS DC REF register value]
 * 0x0014 [uint8_t:  LMS RX VGA2a I register value]
 * 0x0015 [uint8_t:  LMS RX VGA2a Q register value]
 * 0x0016 [uint8_t:  LMS RX VGA2b I register value]
 * 0x0017 [uint8_t:  LMS RX VGA2b Q register value]
 * 0x0018 [Start of table entries]
 *
 * Where a table entry is:
 *        [uint32_t: Frequency]
 *        [int16_t:  DC I correction value]
 *        [int16_t:  DC Q correction value]
 */

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <float.h>

#include "dc_cal_table.h"
#include "host_config.h"
#include "minmax.h"

#ifdef TEST_DC_CAL_TABLE
#   include <stdio.h>
#   define SHORT_SEARCH 4
#   define WARN(str) fprintf(stderr, str)
#else
#   include "log.h"
#   define SHORT_SEARCH 10
#   define WARN(str) log_warning(str)
#endif

#define DC_CAL_TBL_MAGIC        0x1ab1

#define DC_CAL_TBL_META_SIZE    0x18
#define DC_CAL_TBL_ENTRY_SIZE   (sizeof(uint32_t) + 2 * sizeof(int16_t))
#define DC_CAL_TBL_MIN_SIZE     (DC_CAL_TBL_META_SIZE + DC_CAL_TBL_ENTRY_SIZE)

static inline bool entry_matches(const struct dc_cal_tbl *tbl,
                                 unsigned int entry_idx, unsigned int freq)
{
    if (entry_idx >= (tbl->n_entries - 1)) {
        return freq >= tbl->entries[entry_idx].freq;
    } else {
        return freq >= tbl->entries[entry_idx].freq &&
               freq <  tbl->entries[entry_idx + 1].freq;
    }
}

static unsigned int find_entry(const struct dc_cal_tbl *tbl,
                               unsigned int curr_idx,
                               unsigned int min_idx, unsigned int max_idx,
                               unsigned int freq, bool *hit_limit)
{
    /* Converged to a single entry - this is the best we can do */
    if ((max_idx < min_idx) || (max_idx == min_idx && max_idx == curr_idx)) {
        *hit_limit = true;
        return curr_idx;
    }

    if (!entry_matches(tbl, curr_idx, freq)) {
        if (tbl->entries[curr_idx].freq > freq) {
            if (curr_idx > 0) {
                max_idx = (curr_idx - 1);
            } else {
                /* Lower limit hit - return first entry */
                *hit_limit = true;
                return 0;
            }
        } else {
            if (curr_idx < (tbl->n_entries - 1)) {
                min_idx = curr_idx + 1;
            } else {
                /* Upper limit hit - return last entry */
                *hit_limit = true;
                return tbl->n_entries - 1;
            }
        }

        curr_idx = min_idx + (max_idx - min_idx) / 2;

        return find_entry(tbl, curr_idx, min_idx, max_idx, freq, hit_limit);
    } else {
        return curr_idx;
    }
}


unsigned int dc_cal_tbl_lookup(const struct dc_cal_tbl *tbl, unsigned int freq)
{
    unsigned int ret = 0;
    bool limit = false; /* Hit a limit before finding a match */

    /* First check if we're at a nearby change. This is generally the case
     * when the frequecy change */
    if (tbl->n_entries > SHORT_SEARCH) {
        const unsigned int min_idx =
            (unsigned int) i64_max(0, tbl->curr_idx - (int64_t)SHORT_SEARCH / 2);

        const unsigned int max_idx =
            (unsigned int) i64_min(tbl->n_entries - 1, tbl->curr_idx + SHORT_SEARCH / 2);

        ret = find_entry(tbl, tbl->curr_idx, min_idx, max_idx, freq, &limit);
        if (!limit) {
            return ret;
        }
    }

    return find_entry(tbl, tbl->curr_idx, 0, tbl->n_entries - 1, freq, &limit);
}

struct dc_cal_tbl * dc_cal_tbl_load(uint8_t *buf, size_t buf_len)
{
    struct dc_cal_tbl *ret;
    uint32_t i;
    uint16_t magic;

    if (buf_len < DC_CAL_TBL_MIN_SIZE) {
        return NULL;
    }

    memcpy(&magic, buf, sizeof(magic));
    if (LE16_TO_HOST(magic) != DC_CAL_TBL_MAGIC) {
        log_debug("Invalid magic value in cal table: %d\n", magic);
        return NULL;
    }
    buf += sizeof(magic);

    ret = malloc(sizeof(ret[0]));
    if (ret == NULL) {
        return NULL;
    }

    buf += sizeof(uint32_t); /* Skip reserved bytes */

    memcpy(&ret->version, buf, sizeof(ret->version));
    ret->version = LE32_TO_HOST(ret->version);
    buf += sizeof(ret->version);

    memcpy(&ret->n_entries, buf, sizeof(ret->n_entries));
    ret->n_entries = LE32_TO_HOST(ret->n_entries);
    buf += sizeof(ret->n_entries);

    if (buf_len <
         (DC_CAL_TBL_META_SIZE + DC_CAL_TBL_ENTRY_SIZE * ret->n_entries) ) {

        free(ret);
        return NULL;
    }

    ret->entries = malloc(sizeof(ret->entries[0]) * ret->n_entries);
    if (ret->entries == NULL) {
        free(ret);
        return NULL;
    }

    ret->reg_vals.lpf_tuning = *buf++;
    ret->reg_vals.tx_lpf_i = *buf++;
    ret->reg_vals.tx_lpf_q = *buf++;
    ret->reg_vals.rx_lpf_i = *buf++;
    ret->reg_vals.rx_lpf_q = *buf++;
    ret->reg_vals.dc_ref = *buf++;
    ret->reg_vals.rxvga2a_i = *buf++;
    ret->reg_vals.rxvga2a_q = *buf++;
    ret->reg_vals.rxvga2b_i = *buf++;
    ret->reg_vals.rxvga2b_q = *buf++;

    ret->curr_idx = ret->n_entries / 2;
    for (i = 0; i < ret->n_entries; i++) {
        memcpy(&ret->entries[i].freq, buf, sizeof(uint32_t));
        buf += sizeof(uint32_t);

        memcpy(&ret->entries[i].dc_i, buf, sizeof(int16_t));
        buf += sizeof(int16_t);

        memcpy(&ret->entries[i].dc_q, buf, sizeof(int16_t));
        buf += sizeof(int16_t);

        ret->entries[i].freq = LE32_TO_HOST(ret->entries[i].freq);
        ret->entries[i].dc_i = LE32_TO_HOST(ret->entries[i].dc_i);
        ret->entries[i].dc_q = LE32_TO_HOST(ret->entries[i].dc_q);
    }

    return ret;
}

/* Interpolate a y value given two points and a desired x value
 *
 * y = interp( (x0, y0), (x1, y1), x )
 *
 * Returns
 */
static inline unsigned int interp(unsigned int x0, unsigned int y0,
                           unsigned int x1, unsigned int y1,
                           unsigned int x)
{
    const float num = (float) y1 - y0;
    const float den = (float) x1 - x0;
    const float m = den == 0 ? FLT_MAX : num / den;
    const float y = (x - x0) * m + y0;

    return (unsigned int) y;
}

static inline void dc_cal_interp(const struct dc_cal_tbl *tbl,
                                 unsigned int idx_low,
                                 unsigned int idx_high,
                                 unsigned int freq,
                                 int16_t *dc_i, int16_t *dc_q)
{
    const unsigned int f_low = tbl->entries[idx_low].freq;
    const unsigned int f_high = tbl->entries[idx_high].freq;

    *dc_i = (int16_t) interp(f_low, tbl->entries[idx_low].dc_i,
                             f_high, tbl->entries[idx_low].dc_i,
                             freq);

    *dc_q = (int16_t) interp(f_low, tbl->entries[idx_low].dc_q,
                             f_high, tbl->entries[idx_low].dc_q,
                             freq);
}

void dc_cal_tbl_vals(const struct dc_cal_tbl *tbl, unsigned int freq,
                     int16_t *dc_i, int16_t *dc_q)
{
    const unsigned int idx = dc_cal_tbl_lookup(tbl, freq);

    if (tbl->entries[idx].freq == freq) {
        /* Exact match */
        *dc_i = tbl->entries[idx].dc_i;
        *dc_q = tbl->entries[idx].dc_q;
        return;
    } else if (idx == (tbl->n_entries - 1)) {
        dc_cal_interp(tbl, idx - 1, idx, freq, dc_i, dc_q);
    } else {
        dc_cal_interp(tbl, idx, idx + 1, freq, dc_i, dc_q);
    }
}

void dc_cal_tbl_free(struct dc_cal_tbl **tbl)
{
    if (*tbl != NULL) {
        free((*tbl)->entries);
        free(*tbl);
        *tbl = NULL;
    }
}

#ifdef TEST_DC_CAL_TABLE

#define ENTRY(f) { f, 0, 0 }

#define TBL(entries, curr_idx) { \
    entries != NULL ? sizeof(entries) / sizeof(entries[0]) : 0, \
    curr_idx, entries \
}

#define TEST_CASE(exp_idx, entries, default_idx, freq) { \
    TBL(entries, default_idx), \
    freq, \
    exp_idx, \
    exp_idx > -2,  \
}


struct dc_cal_entry unsorted_entries[] = {
    ENTRY(300e6), ENTRY(400e6), ENTRY(320e6),
    ENTRY(310e6), ENTRY(550e6), ENTRY(500e6)
};

struct dc_cal_entry single_entry[] = { ENTRY(2.4e9) };

struct dc_cal_entry three_entries[] = {
    ENTRY(300e6), ENTRY(1.5e9), ENTRY(2.4e9)
};

struct dc_cal_entry entries[] = {
    ENTRY(300e6), ENTRY(400e6), ENTRY(500e6), ENTRY(600e6), ENTRY(700e6),
    ENTRY(800e6), ENTRY(900e6), ENTRY(1.0e9), ENTRY(1.1e9), ENTRY(1.2e9),
    ENTRY(1.3e9), ENTRY(1.4e9), ENTRY(1.5e9), ENTRY(1.6e9), ENTRY(1.7e9),
    ENTRY(1.8e9), ENTRY(1.9e9), ENTRY(2.0e9), ENTRY(2.1e9), ENTRY(2.2e9),
    ENTRY(2.3e9), ENTRY(2.4e9), ENTRY(2.5e9), ENTRY(2.6e9), ENTRY(2.7e9),
    ENTRY(2.8e9), ENTRY(2.9e9), ENTRY(3.0e9), ENTRY(3.1e9), ENTRY(3.2e9),
    ENTRY(3.3e9), ENTRY(3.4e9), ENTRY(3.5e9), ENTRY(3.6e9), ENTRY(3.7e9),
    ENTRY(3.8e9),
};

struct test {
    const struct dc_cal_tbl tbl;
    unsigned int freq;
    int expected_idx;
    bool check_result;
} tests[] = {
    /* Invalid due to unsorted entries. These won't neccessarily work,
     * but shouldn't crash */
    TEST_CASE(-2, unsorted_entries, 0, 300e6),
    TEST_CASE(-2, unsorted_entries, 1, 300e6),
    TEST_CASE(-2, unsorted_entries, 2, 300e6),
    TEST_CASE(-2, unsorted_entries, 3, 300e6),
    TEST_CASE(-2, unsorted_entries, 4, 300e6),
    TEST_CASE(-2, unsorted_entries, 5, 300e6),
    TEST_CASE(-2, unsorted_entries, 0, 310e6),
    TEST_CASE(-2, unsorted_entries, 1, 401e6),
    TEST_CASE(-2, unsorted_entries, 2, 550e6),
    TEST_CASE(-2, unsorted_entries, 3, 100e5),
    TEST_CASE(-2, unsorted_entries, 4, 3.8e9),
    TEST_CASE(-2, unsorted_entries, 5, 321e6),

    /* Single entry - should just return whatever is availble */
    TEST_CASE(0, single_entry, 0, 300e6),
    TEST_CASE(0, single_entry, 0, 2.4e9),
    TEST_CASE(0, single_entry, 0, 3.8e9),

    /* Three entries, exact matches */
    TEST_CASE(0, three_entries, 0, 300e6),
    TEST_CASE(0, three_entries, 1, 300e6),
    TEST_CASE(0, three_entries, 2, 300e6),
    TEST_CASE(1, three_entries, 0, 1.5e9),
    TEST_CASE(1, three_entries, 1, 1.5e9),
    TEST_CASE(1, three_entries, 2, 1.5e9),
    TEST_CASE(2, three_entries, 0, 2.4e9),
    TEST_CASE(2, three_entries, 1, 2.4e9),
    TEST_CASE(2, three_entries, 2, 2.4e9),

    /* Three entries, non-exact matches */
    TEST_CASE(0, three_entries, 0, 435e6),
    TEST_CASE(0, three_entries, 1, 435e6),
    TEST_CASE(0, three_entries, 2, 435e6),
    TEST_CASE(1, three_entries, 0, 2.0e9),
    TEST_CASE(1, three_entries, 1, 2.0e9),
    TEST_CASE(1, three_entries, 2, 2.0e9),
    TEST_CASE(2, three_entries, 0, 3.8e9),
    TEST_CASE(2, three_entries, 1, 3.8e9),
    TEST_CASE(2, three_entries, 2, 3.8e9),

    /* Larger table, lower limits */
    TEST_CASE(0, entries, 0, 0),
    TEST_CASE(0, entries, 0, 300e6),
    TEST_CASE(0, entries, 0, 350e6),
    TEST_CASE(0, entries, 17, 0),
    TEST_CASE(0, entries, 17, 300e6),
    TEST_CASE(0, entries, 17, 350e6),
    TEST_CASE(0, entries, 35, 0),
    TEST_CASE(0, entries, 35, 300e6),
    TEST_CASE(0, entries, 35, 350e6),

    /* Larger table, upper limits */
    TEST_CASE(35, entries, 0, 3.8e9),
    TEST_CASE(35, entries, 0, 4e9),
    TEST_CASE(35, entries, 17, 3.8e9),
    TEST_CASE(35, entries, 17, 4e9),
    TEST_CASE(35, entries, 35, 3.8e9),
    TEST_CASE(35, entries, 35, 4e9),

    /* Larger table, exact matches */
    TEST_CASE(4, entries, 0, 700e6),
    TEST_CASE(4, entries, 4, 700e6),
    TEST_CASE(4, entries, 15, 700e6),
    TEST_CASE(4, entries, 30, 700e6),
    TEST_CASE(4, entries, 35, 700e6),

    TEST_CASE(12, entries, 0, 1.5e9),
    TEST_CASE(12, entries, 12, 1.5e9),
    TEST_CASE(12, entries, 15, 1.5e9),
    TEST_CASE(12, entries, 30, 1.5e9),
    TEST_CASE(12, entries, 35, 1.5e9),

    TEST_CASE(30, entries, 0, 3.3e9),
    TEST_CASE(30, entries, 10, 3.3e9),
    TEST_CASE(30, entries, 20, 3.3e9),
    TEST_CASE(30, entries, 30, 3.3e9),
    TEST_CASE(30, entries, 35, 3.3e9),

    /* Larger table, approximate matches */
    TEST_CASE(4, entries, 0, 701e6),
    TEST_CASE(4, entries, 4, 701e6),
    TEST_CASE(4, entries, 15, 701e6),
    TEST_CASE(4, entries, 30, 701e6),
    TEST_CASE(4, entries, 35, 701e6),

    TEST_CASE(12, entries, 0, 1.59e9),
    TEST_CASE(12, entries, 12, 1.59e9),
    TEST_CASE(12, entries, 15, 1.59e9),
    TEST_CASE(12, entries, 30, 1.59e9),
    TEST_CASE(12, entries, 35, 1.59e9),

    TEST_CASE(30, entries, 0, 3.35e9),
    TEST_CASE(30, entries, 10, 3.35e9),
    TEST_CASE(30, entries, 20, 3.35e9),
    TEST_CASE(30, entries, 30, 3.35e9),
    TEST_CASE(30, entries, 35, 3.35e9),
};

static inline void print_entry(const struct dc_cal_tbl *t,
                               const char *prefix, int idx)
{
    if (idx >= 0) {
        fprintf(stderr, "%s: %u Hz\n", prefix, t->entries[idx].freq);
    } else {
        fprintf(stderr, "%s: None (%d)\n", prefix, idx);
    }
}

int main(void)
{
    unsigned int i;
    unsigned int num_failures = 0;

    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        const int expected_idx = tests[i].expected_idx;
        const int entry_idx = dc_cal_tbl_lookup(&tests[i].tbl, tests[i].freq);

        if (tests[i].check_result && entry_idx != expected_idx) {
            fprintf(stderr, "Test case %u: failed.\n", i);
            print_entry(&tests[i].tbl, "  Got", entry_idx);
            print_entry(&tests[i].tbl, "  Expected", expected_idx);
            num_failures++;
        } else {
            printf("Test case %u: passed.\n", i);
        }
    }

    return num_failures;
}
#endif
