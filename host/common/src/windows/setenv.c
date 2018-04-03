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

#ifndef WIN32
#error "This file is intended for use with WIN32 systems only."
#endif  // WIN32

#include <stdlib.h>

int setenv(const char *name, const char *value, int overwrite)
{
    errno_t rv     = 0;
    size_t envsize = 0;

    if (!overwrite) {
        // Test for existence
        rv = getenv_s(&envsize, NULL, 0, name);
        if (rv != 0 || envsize != 0) {
            return rv;
        }
    }
    return _putenv_s(name, value);
}

int unsetenv(const char *name)
{
    return _putenv_s(name, "");
}
