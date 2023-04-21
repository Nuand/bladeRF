/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2023 Nuand LLC
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
#include <stdio.h>
#include <getopt.h>

char* getopt_str(const struct option* long_options) {
    size_t len = 0;

    for (const struct option* opt = long_options; opt->name; opt++) {
        len++;
        if (opt->has_arg == required_argument) {
            len++;
        }
    }

    char* opt_str = (char*) malloc((len + 1) * sizeof(char));
    if (!opt_str) {
        return NULL;
    }

    int i = 0;
    for (const struct option* opt = long_options; opt->name; opt++) {
        opt_str[i++] = opt->val;
        if (opt->has_arg == required_argument) {
            opt_str[i++] = ':';
        }
    }
    opt_str[i] = '\0';

    return opt_str;
}

static void usage()
{
    printf("TXRX Hardware Loop Test\n\n");

    printf("Test configuration:\n");
    printf("    -b, --buflen <value>      Buffer length. Must be multiple of 1024.\n");
    printf("    -p, --period <value>      Length between timestamps\n");
    printf("    -f, --fill <value>        %% of burst to fill with [2000,2000]\n");
    printf("                                others set to [0,0]\n");
    printf("\n");

    printf("Misc options:\n");
    printf("    -h, --help                  Show this help text\n");
    printf("    -v, --lib-verbosity <level> Set libbladeRF verbosity (Default: warning)\n");
    printf("\n");
}
