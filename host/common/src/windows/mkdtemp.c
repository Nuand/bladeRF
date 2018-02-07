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

// _CRT_RAND_S must be defined before including stdlib.h, to enable rand_s
#define _CRT_RAND_S
#include <stdlib.h>

#ifdef WINDOWS_MKDTEMP_TEST_SUITE
// Running in standalone test mode

#include <stdbool.h>
#include <stdio.h>
#define __debug(...) fprintf(stderr, __VA_ARGS__)

#ifndef WIN32
// Running standalone test on non-Windows OS
#include <time.h>
#include <unistd.h>
#define errno_t int
errno_t rand_s(unsigned int *randomValue)
{
    *randomValue = rand();
    return 0;
}
#endif  // WIN32

#else
// Building as part of a library

#ifndef WIN32
#error "This file is intended for use with WIN32 systems only."
#endif  // WIN32

#define __debug(...)

#endif  // WINDOWS_MKDTEMP_TEST_SUITE

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include "host_config.h"

// The concepts of F_OK and S_IRWXU do not exist on Win32
#ifndef F_OK
#define F_OK 0
#endif

#ifndef S_IRWXU
#define S_IRWXU 00700
#endif

/* h/t https://stackoverflow.com/a/14598879 */
static int random_number(int min_num, int max_num)
{
    int result = 0, low_num = 0, hi_num = 0;
    unsigned int randomValue;

    if (min_num < max_num) {
        low_num = min_num;
        hi_num  = max_num + 1;  // include max_num in output
    } else {
        low_num = max_num + 1;  // include max_num in output
        hi_num  = min_num;
    }

    // Use rand_s() on Windows, so that we don't have to deal with srand()
    errno_t err = rand_s(&randomValue);
    if (0 != err) {
        return -1;
    }

    result = (randomValue % (hi_num - low_num)) + low_num;
    return result;
}

char *mkdtemp(char *template)
{
    size_t const TEMPL_LEN = 6;
    char const TEMPL_CHAR  = 'X';

    if (strlen(template) <= TEMPL_LEN) {
        // template is too short
        errno = EINVAL;
        goto error;
    }

    // Loop through the end of the template, replacing 'X' with random char
    for (size_t i = strlen(template) - TEMPL_LEN; i < strlen(template); ++i) {
        // The last TEMPL_LEN characters MUST be 'X'
        if (template[i] != TEMPL_CHAR) {
            errno = EINVAL;
            goto error;
        }

        // Pick a random letter
        if (random_number(0, 1)) {
            template[i] = (char)random_number('A', 'Z');
        } else {
            template[i] = (char)random_number('a', 'z');
        }
    }

    // Error out if the file already exists
    if (access(template, F_OK) != -1) {
        __debug("%s: failed: %s exists\n", __FUNCTION__, template);
        errno = EEXIST;
        goto error;
    }

    // Try to create the directory...
    if (0 != mkdir(template, S_IRWXU)) {
        int errsv = errno;
        __debug("%s: mkdir() failed: %s\n", __FUNCTION__, strerror(errsv));
        goto error;
    }

    // Success!
    errno = 0;
    return template;

error:
    return NULL;
}

#ifdef WINDOWS_MKDTEMP_TEST_SUITE
/**
 * These functions are intended to verify proper operation of the test suite.
 */
static bool test(char *template, bool expect_success)
{
    char *rv = mkdtemp(template);

    if (NULL == rv) {
        int errsv = errno;
        printf("%s: mkdtemp failed: %s\n", __FUNCTION__, strerror(errsv));
        return (false == expect_success);
    } else {
        printf("%s: mkdtemp created: %s\n", __FUNCTION__, rv);

        if (0 != rmdir(rv)) {
            int errsv = errno;
            printf("%s: rmdir failed: %s\n", __FUNCTION__, strerror(errsv));
        }

        return (true == expect_success);
    }
}

int main(int argc, char *argv[])
{
#ifndef WIN32
    srand(time(NULL));
#endif  // WIN32

    int success = 0, failure = 0;

    // Normal: should pass
    char template1[] = "/tmp/asdf.XXXXXX";
    if (test(template1, true)) {
        printf("*** Test case 1: PASS\n");
        ++success;
    } else {
        printf("*** Test case 1: FAIL\n");
        ++failure;
    }

    // Too short: should fail
    char template2[] = "XXXXXX";
    if (test(template2, false)) {
        printf("*** Test case 2: PASS\n");
        ++success;
    } else {
        printf("*** Test case 2: FAIL\n");
        ++failure;
    }

    // Not enough replacement Xs: should fail
    char template3[] = "/tmp/asdf.XXXXX";
    if (test(template3, false)) {
        printf("*** Test case 3: PASS\n");
        ++success;
    } else {
        printf("*** Test case 3: FAIL\n");
        ++failure;
    }

    // Make sure it only replaces the end: should pass
    char template4[] = "/tmp/asdfXXXXXX.XXXXXX";
    if (test(template4, true)) {
        printf("*** Test case 4: PASS\n");
        ++success;
    } else {
        printf("*** Test case 4: FAIL\n");
        ++failure;
    }

    // Really long: should fail
    char template5[] = "/tmp/asdfaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                       "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                       "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                       "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                       "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                       "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                       "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                       "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaXXXXXX";
    if (test(template5, false)) {
        printf("*** Test case 5: PASS\n");
        ++success;
    } else {
        printf("*** Test case 5: FAIL\n");
        ++failure;
    }

    // Unwriteable path: should fail
    char template6[] = "/asdfkjavblkjadv/asdf.XXXX";
    if (test(template6, false)) {
        printf("*** Test case 6: PASS\n");
        ++success;
    } else {
        printf("*** Test case 6: FAIL\n");
        ++failure;
    }

    printf("TEST SUMMARY: Success=%d, Failure=%d\n", success, failure);

    return failure;
}
#endif  // WINDOWS_MKDTEMP_TEST_SUITE
