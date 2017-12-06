/*
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

#include "parse.h"

#include <stdio.h>

static char **add_arg(char **argv, int argc, const char *buf,
                        int start, int end, int quote_count)
{
    char **rv;
    char *d_ptr;
    int i;
    int len;

    char c;
    char quote_char;

    quote_char = 0;

    rv = (char **)realloc(argv, sizeof(char *) * (argc + 1));
    if (rv == NULL)
        return NULL;

    rv[argc] = NULL;

    len = end - start + 1;

    d_ptr = (char *)malloc(len + 1 - quote_count * 2);
    if (d_ptr == NULL)
        return NULL;

    rv[argc - 1] = d_ptr;

    for (i = 0; i < len; i++) {
        c = buf[start + i];

        if (!quote_char) {
            /* We are not in a quote, copy everything but quote chars */
            if (c == '"' || c == '\'') {
                quote_char = c;
            } else {
                *d_ptr++ = c;
            }
        } else {
            /* We are in a quote, copy everything but the current quote char */
            if (c == quote_char) {
                quote_char = 0;
            } else {
                *d_ptr++ = c;
            }
        }
    }
    *d_ptr = '\0';

    return rv;

}

int str2args(const char *line, char comment_char, char ***argv)
{
    char **rv;
    int argc;

    int i, len;

    bool in_arg;
    char c;
    char quote_char;

    int arg_start;
    int quote_count;

    rv = NULL;
    argc = 0;

    quote_char = 0;

    arg_start = 0;
    quote_count = 0;

    len = strlen(line);

    in_arg = false;

    for (i = 0; i < len; i++) {
        c = line[i];

        if (in_arg) {
            /* Found the end of a quote! */
            if (quote_char) {
                if (quote_char == c) {
                    quote_char = 0;
                }
                continue;
            }

            /* Found the beginning of a quote! */
            if (c == '\'' || c == '"') {
                quote_count++;
                quote_char = c;
                continue;
            }

            /* Found whitespace outside of quote */
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                in_arg = false;
                argc++;
                rv = add_arg(rv, argc, line, arg_start, i - 1, quote_count);
                if (rv == NULL)
                    return -1;
            }
        } else {
            if (c == comment_char) {
                break;
            }
            /* Enter a new argument */
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
                /* If first argument is a tick it means we're in a quote */
                if (c == '\'' || c == '"') {
                    quote_char = c;
                } else {
                    quote_char = 0;
                }
                quote_count = 0;
                arg_start = i;
                in_arg = true;
            }
            /* else this is still whitespace */
        }
    }

    /* reached the end of the line, check to see if current arg needs to
     * be closed */
    if (in_arg) {
        if (quote_char) {
            return -2;
        } else {
            argc++;
            rv = add_arg(rv, argc, line, arg_start, i - 1, quote_count);
            if (rv == NULL) {
                return -1;
            }
        }
    }

    *argv = rv;

    return argc;
}

void free_args(int argc, char **argv)
{
    int i;
    for (i = 0; i < argc; i++) {
        free(argv[i]);
    }
    free(argv);
}
