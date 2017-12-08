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
#include "log.h"

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

    unsigned i;
    size_t len;

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

static struct config_options *add_opt(struct config_options *optv, int optc,
                                            char *key, char *val, int lineno)
{

    struct config_options *rv;
    char *ptr;
    rv = (struct config_options *)
                realloc(optv, sizeof(struct config_options) * optc);
    if (rv == NULL)
        return NULL;

    ptr = (char *)malloc(strlen(key) + 1);
    if (!ptr) {
        return NULL;
    }
    strcpy(ptr, key);
    rv[optc - 1].key = ptr;

    ptr = (char *)malloc(strlen(val) + 1);
    if (!ptr) {
        return NULL;
    }
    strcpy(ptr, val);
    rv[optc - 1].value = ptr;

    rv[optc - 1].lineno = lineno;

    return rv;
}

bool update_match(struct bladerf *dev, char *str)
{
    size_t len;
    int status;
    struct bladerf_devinfo info;
    bladerf_fpga_size fpga_size;

    status = bladerf_get_devinfo(dev, &info);
    if (status < 0)
        return false;

    bladerf_get_fpga_size(dev, &fpga_size);
    if (status < 0)
        return false;

    str++;
    len = strlen(str);
    if (str[len - 1] == ']')
        str[len - 1] = '\0';

    if (!strcmp(str, "x40")) {
        return fpga_size == BLADERF_FPGA_40KLE;
    } else if (!strcmp(str, "x115")) {
        return fpga_size == BLADERF_FPGA_115KLE;
    }

    status = bladerf_devstr_matches(str, &info);

    return status == 1;
}

int str2options(struct bladerf *dev, const char *buf, size_t buf_sz,
                    struct config_options **opts)
{
    char *line;
    char *d_ptr;
    int   line_num;
    char  c;
    unsigned i;

    struct config_options *optv;
    int    optc;

    char **line_argv;
    int  line_argc;

    bool match;

    match = true;

    optv = NULL;
    optc = 0;

    line_num = 1;

    line = malloc(buf_sz + 1);
    if (!line)
        return BLADERF_ERR_MEM;

    d_ptr = line;

    for (i = 0; i < buf_sz; i++) {
        c = buf[i];
        if (c == '\n') {
            /* deal with the old line */
            *d_ptr = 0;
            line_argc = str2args(line, '#', &line_argv);
            if (line_argc < 0)
                goto out;

            /* handle line */
            if (line_argc > 3) {
                log_error("Too many arguments in bladeRF.conf on line %d\n",
                                line_num);
                goto out;
            } else if (match && line_argc == 2) {
                optc++;
                optv = add_opt(optv, optc, line_argv[0], line_argv[1], line_num);
                if (!optv) {
                    optc = -1;
                    goto out;
                }
            } else if (line_argc == 1) {
                if (*line_argv[0] != '[') {
                    log_error("Expecting scoping line (requires [ and ]) on line %d\n",
                                line_num);
                }
                match = update_match(dev, line_argv[0]);
            }

            /* free line */
            free_args(line_argc, line_argv);

            /* setup to capture the next line */
            line_num++;
            d_ptr = line;
        } else {
            *d_ptr++ = c;
        }
    }

    if (opts)
        *opts = optv;

out:
    free(line);
    return optc;
}

void free_opts(struct config_options *optv, int optc)
{
    int i;

    for (i = 0; i < optc; i++) {
        free(optv[i].key);
        free(optv[i].value);
    }
    free(optv);
}
