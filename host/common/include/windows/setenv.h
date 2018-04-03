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

#ifndef WIN_SETENV_H_
#define WIN_SETENV_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WIN32
#error "This file is intended for use with WIN32 systems only."
#endif

/**
 * @brief      Change or add an environment variable
 *
 * Functionally equivalent to glibc's setenv.
 *
 * @param[in]  name       The name of the environmental variable
 * @param[in]  value      The value to set it to
 * @param[in]  overwrite  If zero, do nothing if the environment variable is
 *                        already set (and return success); if non-zero,
 *                        replace its value.
 *
 * @return     the return value of the _putenv_s() call (zero if successful)
 */
int setenv(const char *name, const char *value, int overwrite);

/**
 * @brief      Delete an environmental variable from the environment
 *
 * Functionally equivalent to glibc's setenv.
 *
 * @param[in]  name  The name of the environmental variable
 *
 * @return     the return value of the _putenv_s() call (zero if successful)
 */

int unsetenv(const char *name);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  // WIN_SETENV_H_
