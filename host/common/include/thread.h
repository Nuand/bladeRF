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
#include "rel_assert.h"


//#define USE_PTHREADS
#ifdef USE_PTHREADS

/* Use pthreads */

#include <pthread.h>
#include <time.h>
#define CLOCK_REALTIME 0

#define THREAD pthread_t
#define MUTEX pthread_mutex_t
#define COND pthread_cond_t
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

#   define MUTEX_DESTROY(m) do { \
        int status = pthread_mutex_destroy(m); \
        assert(status == 0 && "Mutex destroy failure");\
    } while (0)
#else /* ENABLE_LOCK_CHECKS */
#   define THREAD_CREATE(x,y,z) pthread_create(x, NULL, y, z)
#   define THREAD_SUCCESS 0
#   define THREAD_TIMEOUT ETIMEDOUT
#   define THREAD_CANCEL(m) pthread_cancel(m)
#   define THREAD_JOIN(t, s) pthread_join(t, s)

#   define COND_INIT(m) pthread_cond_init(m, NULL)
#   define COND_SIGNAL(m) pthread_cond_signal(m)
// POSIX implementation as a function
static inline int posix_cond_timedwait(pthread_cond_t *c,
                                       pthread_mutex_t *m,
                                       unsigned int t)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += t / 1000;
    ts.tv_nsec += (t % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }
    return pthread_cond_timedwait(c, m, &ts) == ETIMEDOUT ? -1 : 0;
}
#define COND_TIMED_WAIT(c, m, t) posix_cond_timedwait(c, m, t)

#define COND_WAIT(c, m) pthread_cond_wait(c, m)

#   define MUTEX_INIT(m) pthread_mutex_init(m, NULL)
#   define MUTEX_LOCK(m) pthread_mutex_lock(m)
#   define MUTEX_UNLOCK(m) pthread_mutex_unlock(m)
#   define MUTEX_DESTROY(m) pthread_mutex_destroy(m)
#endif  /* ENABLE_LOCK_CHECKS */

#else /* USE_PTHREADS */

/* Use C11 threads */
//#include <threads.h>

#define THREAD HANDLE            // For thread handles
#define MUTEX CRITICAL_SECTION   // For mutexes
#define COND CONDITION_VARIABLE  // For condition variables


#ifdef ENABLE_LOCK_CHECKS
#   define MUTEX_INIT(m) do { \
        int status; \
        status = mtx_init(m, mtx_plain | mtx_recursive); \
        assert(status == thrd_success && "Mutex init failure"); \
    } while (0)

#   define MUTEX_LOCK(m) do { \
        int status = mtx_lock(m); \
        assert(status == thrd_success && "Mutex lock failure");\
    } while (0)

#   define MUTEX_UNLOCK(m) do { \
        int status = mtx_unlock(m); \
        assert(status == thrd_success && "Mutex unlock failure");\
    } while (0)

#   define MUTEX_DESTROY(m) do { \
        mtx_destroy(m); \
    } while (0)
#else /* ENABLE_LOCK_CHECKS */

#include <windows.h>

#   define THREAD_CREATE(x, y, z) \
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)y, z, 0, x)
#   define THREAD_SUCCESS 0
#   define THREAD_TIMEOUT WAIT_TIMEOUT
#   define THREAD_CANCEL(m) TerminateThread(m, 0)
#   define THREAD_JOIN(t, s) WaitForSingleObject(t, INFINITE)

#   define COND_INIT(m) (InitializeConditionVariable(m), 0)
#   define COND_SIGNAL(m) WakeConditionVariable(m)
#   define COND_TIMED_WAIT(c, m, t) \
        (SleepConditionVariableCS(c, m, t) ? 0 : GetLastError())
#   define COND_WAIT(c, m) SleepConditionVariableCS(c, m, INFINITE)

#   define MUTEX_INIT(m) InitializeCriticalSection(m)
#   define MUTEX_LOCK(m) EnterCriticalSection(m)
#   define MUTEX_UNLOCK(m) LeaveCriticalSection(m)
#   define MUTEX_DESTROY(m) DeleteCriticalSection(m)

#endif /* ENABLE_LOCK_CHECKS */

#endif /* USE_PTHREADS */

#endif
