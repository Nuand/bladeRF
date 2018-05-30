/**
 * @file parse.h
 *
 * @brief String and file parsing functions
 *
 * Copyright (c) 2017 Nuand LLC.
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
#ifndef PARSE_H_
#define PARSE_H_

#include "libbladeRF.h"
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Convert ASCII string into arguments
 *
 * @param[in]   line           Command line string
 * @param[in]   comment_char   Everything after this charater is discarded
 * @param[out]  argv           A pointer to a double pointer of successfully
 *                             extracted arguments
 *
 * @return -2 on unterminated quote, -1 on error, otherwise number of arguments
 */
int str2args(const char *line, char comment_char, char ***argv);

/**
 * Free arguments returned by str2args()
 *
 * @param[in]   argc    Number of arguments
 * @param[in]   argv    Pointer to table of pointers to arguments
 */
void free_args(int argc, char **argv);

struct config_options {
    char *key;
    char *value;
    int lineno;
};

/**
 * Convert a bladeRF options file to an array of strings
 *
 * @param[in]   dev     Pointer to bladerf device
 * @param[in]   buf     String buffer containing entire file
 * @param[in]   buf_sz  String buffer containing entire file
 * @param[out]  opts    A pointer to an array of strings containing options for
 *                      this device
 *
 * @return  number of options on success, BLADERF_ERR_* values on other failures
 */
int str2options(struct bladerf *dev,
                const char *buf,
                size_t buf_sz,
                struct config_options **opts);

/**
 * Free an array of previously returned options
 *
 * @param[in]   optv    Pointer to array of config_options
 * @param[in]   optc    Number of config_options
 */
void free_opts(struct config_options *optv, int optc);

/**
 * Convert comma-delimited string into integer arguments
 *
 * @param[in]   line    Line to split
 * @param[out]  args    Pointer to double-pointer of successfully extracted
 *                      integers
 *
 * @see free_csv2int()
 *
 * @return -1 on error, otherwise number of arguments
 */
int csv2int(const char *line, int ***args);

/**
 * Free array of previously-returned csv2int arguments
 *
 * @param[in]   argc    Number of arguments
 * @param[in]   args    Pointer to array of pointers to ints
 */
void free_csv2int(int argc, int **args);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
