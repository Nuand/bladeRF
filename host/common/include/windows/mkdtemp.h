/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/Nuand/bladeRF
 *
 * Copyright (c) 2018 Nuand LLC
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
#ifndef WIN_MKDTEMP_H_
#define WIN_MKDTEMP_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WIN32
#error "This file is intended for use with WIN32 systems only."
#endif

/**
 * Create a unique temporary directory
 *
 * Functionally equivalent to glibc's mkdtemp.
 *
 * The last six characters of template must be XXXXXX, and these are replaced
 * with a string that makes the directory name unique. The directory is then
 * created with 0700 permissions.
 *
 * The `template` parameter is modified in-place, and thus it should be
 * declared as a character array and not a string constant.
 *
 * @param   template    filename template, ending in "XXXXXX"
 *
 * @returns pointer to modified `template` string on success, NULL on failure,
 *          in which case `errno` is set appropriately.
 */
char *mkdtemp(char *template);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
