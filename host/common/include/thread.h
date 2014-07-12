/**
 * @file thread.h
 *
 * @brief Threading portability and debug wrappers
 *
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (c) 2014 Nuand LLC
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
#ifndef BLADERF_THREAD_H_
#define BLADERF_THREAD_H_

/* Currently, only pthreads is supported. In the future, native windows threads
 * may be used; one of the objectives of this file is to ease that transistion.
 */
#include <pthread.h>
#include "rel_assert.h"

#define MUTEX pthread_mutex_t

#ifdef ENABLE_LOCK_CHECKS
#   define MUTEX_INIT(m) do { \
        int status; \
        pthread_mutexattr_t mutex_attr; \
        status = pthread_mutexattr_init(&mutex_attr); \
        assert(status == 0 && "Mutex attr init failure"); \
        status = pthread_mutexattr_settype(&mutex_attr, \
                                           PTHREAD_MUTEX_ERRORCHECK); \
        assert(status == 0 && "Mutex attr setype failure"); \
        status = pthread_mutex_init(m, &mutex_attr); \
        assert(status == 0 && "Mutex init failure"); \
    } while (0)

#   define MUTEX_LOCK(m) do { \
        int status = pthread_mutex_lock(m); \
        assert(status == 0 && "Mutex lock failure");\
    } while (0)

#   define MUTEX_UNLOCK(m) do { \
        int status = pthread_mutex_unlock(m); \
        assert(status == 0 && "Mutex unlock failure");\
    } while (0)

#else
#   define MUTEX_INIT(m) pthread_mutex_init(m, NULL)
#   define MUTEX_LOCK(m) pthread_mutex_lock(m)
#   define MUTEX_UNLOCK(m) pthread_mutex_unlock(m)
#endif

#endif
