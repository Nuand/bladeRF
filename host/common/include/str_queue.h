/**
 * @file str_queue.h
 *
 * @brief Simple string queue
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
#ifndef STR_QUEUE_H_
#define STR_QUEUE_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct str_queue_entry;

struct str_queue {
    struct str_queue_entry *head;
    struct str_queue_entry *tail;
};

/**
 * Allocate and initialize a string queue
 *
 * @param[in]   q   Queue to initialize
 */
void str_queue_init(struct str_queue *q);

/**
 * Deallocate all resources used by the specified string queue
 *
 * @param[in]   q   Queue to deinitialize
 */
void str_queue_deinit(struct str_queue *q);

/** Deinitialization simply clears out the queue - same operation */
#define str_queue_clear str_queue_deinit

/**
 * Add a string to the end of the provided string queue
 *
 * @param   q           String queue to append to
 * @param   cmd_str     String to append. This string will be copied
 *                      to a heap-allocated buffer.
 *
 * @return 0 on success, -1 on memory allocation failure
 */
int str_queue_enq(struct str_queue *q, const char *str);

/**
 * Remove a string from the front of the specified string queue.
 *
 * @param[in]   q       Queue to remove from
 *
 * @return Heap-allocated string removed from queue, or NULL if the queue is
 *         empty. The caller is responsible for free()-ing the returned string.
 */
char * str_queue_deq(struct str_queue *q);

/**
 * @return true if the queue is empty
 */
bool str_queue_empty(struct str_queue *q);

#ifdef __cplusplus
} /* extern "C" */
#endif


#endif
