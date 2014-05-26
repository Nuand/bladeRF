/*
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
#include <string.h>
#include <stdlib.h>
#include "str_queue.h"
#include "host_config.h"

struct str_queue_entry {
    char *str;
    struct str_queue_entry *next;
};

void str_queue_init(struct str_queue *q)
{
    memset(q, 0, sizeof(q[0]));
}

void str_queue_deinit(struct str_queue *q)
{
    char *str;

    do {
        str = str_queue_deq(q);
        free(str);
    } while (str != NULL);

    q->head = NULL;
    q->tail = NULL;
}

int str_queue_enq(struct str_queue *q, const char *str)
{
    struct str_queue_entry *entry = malloc(sizeof(entry[0]));
    if (entry == NULL) {
        return -1;
    }

    entry->str = strdup(str);
    if (entry->str == NULL) {
        free(entry);
        return -1;
    }
    entry->next = NULL;

    if (q->head == NULL) {
        q->head = entry;
    }

    if (q->tail != NULL) {
        q->tail->next = entry;
    }

    q->tail = entry;
    return 0;
}

char * str_queue_deq(struct str_queue *q)
{
    char *ret;
    struct str_queue_entry *entry;

    entry = q->head;
    if (entry == NULL) {
        ret = NULL;
    } else {
        q->head = entry->next;
        if (q->head == NULL) {
            q->tail = NULL;
        }

        ret = entry->str;
        free(entry);
    }

    return ret;
}

bool str_queue_empty(struct str_queue *q)
{
    return q->head == NULL || q->tail == NULL;
}

#if TEST_STR_QUEUE
#include <stdio.h>

#define STR_NULL    "(null)"
#define STR_1       "This is test string one."
#define STR_2       "A second string queue test string"
#define STR_3       "String thr33, this be"

#define CHECK_EMPTY(q) do { \
        if (str_queue_empty(q) != true) { \
            fprintf(stderr, "Queue not empty!\n"); \
            return EXIT_FAILURE; \
        } \
    } while (0)

#define CHECK_NONEMPTY(q) do { \
        if (str_queue_empty(q) != false) { \
            fprintf(stderr, "Queue unexpectedly empty!\n"); \
            return EXIT_FAILURE; \
        } \
    } while (0)

#define ENQ(q, str) do { \
        int status_ = str_queue_enq(q, str); \
        if (status_ != 0) { \
            fprintf(stderr, "Failed to enqueue %s\n", str); \
            return EXIT_FAILURE; \
        } else { \
            printf("Enqueued: %s\n", str); \
        } \
    } while (0)

#define DEQ(q, exp) do { \
        char * str_; \
        bool failed_; \
        if (!strcmp(exp, STR_NULL)) { \
            CHECK_EMPTY(q); \
        } else { \
            CHECK_NONEMPTY(q); \
        } \
        str_ = str_queue_deq(q); \
        printf("Dequeued: %s\n", str_); \
        if (str_ == NULL) { \
            failed_ = strcmp(exp, STR_NULL) != 0; \
        } else { \
            failed_ = strcmp(str_, exp) != 0; \
        } \
        if (failed_) { \
            fprintf(stderr, "Dequeue failed.\n"); \
            return EXIT_FAILURE; \
        } \
    } while (0)

int main(void)
{
    char *str;
    struct str_queue q;

    str_queue_init(&q);
    str = str_queue_deq(&q);


    DEQ(&q, STR_NULL);
    ENQ(&q, STR_1);
    DEQ(&q, STR_1);
    DEQ(&q, STR_NULL);
    DEQ(&q, STR_NULL);

    ENQ(&q, STR_1);
    ENQ(&q, STR_2);
    DEQ(&q, STR_1);
    DEQ(&q, STR_2);
    DEQ(&q, STR_NULL);

    ENQ(&q, STR_1);
    ENQ(&q, STR_2);
    DEQ(&q, STR_1);
    ENQ(&q, STR_3);
    DEQ(&q, STR_2);
    ENQ(&q, STR_1);
    DEQ(&q, STR_3);
    DEQ(&q, STR_1);
    DEQ(&q, STR_NULL);
    DEQ(&q, STR_NULL);

    str_queue_deinit(&q);
    return 0;
}
#endif
