/**
 * @file test_parse/src/main.c
 *
 * @brief Unit test suite for host/common/parse.c
 *
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2018 Nuand LLC
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

#include "host_config.h"
#include "parse.h"

struct csv2int_test_string {
    char *input;
    int outlen;
    int output[128];
};

// clang-format off
static const struct csv2int_test_string csv2int_test_strings[] = {
    {
        FIELD_INIT(.input, "42"),
        FIELD_INIT(.outlen, 1),
        FIELD_INIT(.output, { 42 }),
    },
    {
        FIELD_INIT(.input, "0,1,2,3"),
        FIELD_INIT(.outlen, 4),
        FIELD_INIT(.output, { 0, 1, 2, 3 }),
    },
    {
        FIELD_INIT(.input, "-22,13,000,-19,2235"),
        FIELD_INIT(.outlen, 5),
        FIELD_INIT(.output, { -22, 13, 0, -19, 2235 }),
    },
    {
        FIELD_INIT(.input, "-22, 13, 000, -19, 2235"),
        FIELD_INIT(.outlen, 5),
        FIELD_INIT(.output, { -22, 13, 0, -19, 2235 }),
    },
    {
        FIELD_INIT(.input, "20 54 56 61 64 07"),
        FIELD_INIT(.outlen, 6),
        FIELD_INIT(.output, {20, 54, 56, 61, 64, 7}),
    },
    {
        FIELD_INIT(.input, " 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,"
                           "16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27"),
        FIELD_INIT(.outlen, 27),
        FIELD_INIT(.output, { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                             16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27}),
    },
    {
        FIELD_INIT(.input, "expecting,str2int,failure"),
        FIELD_INIT(.outlen, -1),
        FIELD_INIT(.output, { 0 }),
    },
    {
        FIELD_INIT(.input, "1,2,3,expecting_str2int_failure"),
        FIELD_INIT(.outlen, -1),
        FIELD_INIT(.output, { 0 }),
    },
    /* The next few test cases test the arglen adjustments */
    {
        FIELD_INIT(.input, "0,1,2,3"),
        FIELD_INIT(.outlen, 4),
        FIELD_INIT(.output, { 0, 1, 2, 3 }),
    },
    {
        FIELD_INIT(.input, "0,1,2,3"),
        FIELD_INIT(.outlen, 4),
        FIELD_INIT(.output, { 0, 1, 2, 3 }),
    },
    {
        FIELD_INIT(.input, "0,1,2,3"),
        FIELD_INIT(.outlen, 4),
        FIELD_INIT(.output, { 0, 1, 2, 3 }),
    },
    {
        FIELD_INIT(.input, "0,1,2,3"),
        FIELD_INIT(.outlen, 4),
        FIELD_INIT(.output, { 0, 1, 2, 3 }),
    },
    {
        FIELD_INIT(.input, "0,1,2,3"),
        FIELD_INIT(.outlen, 4),
        FIELD_INIT(.output, { 0, 1, 2, 3 }),
    },
    {
        FIELD_INIT(.input, "0,1,2,3"),
        FIELD_INIT(.outlen, 4),
        FIELD_INIT(.output, { 0, 1, 2, 3 }),
    },
    {
        FIELD_INIT(.input, "0,1,2,3"),
        FIELD_INIT(.outlen, 4),
        FIELD_INIT(.output, { 0, 1, 2, 3 }),
    },
    {
        FIELD_INIT(.input, ""),
        FIELD_INIT(.outlen, 0),
        FIELD_INIT(.output, { 0 }),
    },
};
// clang-format on

bool test_csv2int()
{
    int i, j;
    size_t good = 0, bad = 0;

    i = 0;

    while (csv2int_test_strings[i].outlen != 0) {
        int rv;
        int **args   = NULL;
        bool failure = false;

        const struct csv2int_test_string *tv = &csv2int_test_strings[i];

        printf("%s: test case %d: '%s'\n", __FUNCTION__, i, tv->input);

        rv = csv2int(tv->input, &args);

        if (rv != tv->outlen) {
            printf("%s: outlen mismatch (expected %d, got %d)\n", __FUNCTION__,
                   tv->outlen, rv);
            failure = true;
            goto out;
        }

        for (j = 0; j < tv->outlen; ++j) {
            if (*args[j] != tv->output[j]) {
                printf("%s: output mismatch (arg %d, expected %d, got %d)\n",
                       __FUNCTION__, j, tv->output[j], *args[j]);
                failure = true;
                goto out;
            }
        }

    out:
        if (failure) {
            ++bad;
        } else {
            ++good;
        }

        free_csv2int(rv, args);

        ++i;
    }

    printf("%s: good = %zu, bad = %zu\n", __FUNCTION__, good, bad);

    return (0 == bad);
}

int main(int argc, char *argv[])
{
    size_t bad = 0;

    printf("*** testing csv2int ***\n");
    if (test_csv2int()) {
        printf("*** testing csv2int: PASSED ***\n");
    } else {
        printf("*** testing csv2int: FAILED ***\n");
        ++bad;
    }

    return bad;
}
