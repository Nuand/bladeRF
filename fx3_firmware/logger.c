/*
 * Copyright (c) 2015 Nuand LLC
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

#include <stdint.h>
#include <stdbool.h>

#include "logger.h"


#ifndef LOGGER_HOST
#   define THIS_FILE LOGGER_ID_LOGGER_C

#   include <cyu3error.h>
#   include <cyu3os.h>
#   include "bladeRF.h"

#   define MUTEX_INIT(log)  CyU3PMutexCreate(&log.lock, CYU3P_INHERIT)
#   define LOCK(log)        CyU3PMutexGet(&log.lock, CYU3P_WAIT_FOREVER)
#   define UNLOCK(log)      (void) CyU3PMutexPut(&log.lock)
#else
#   define THIS_FILE        1
#   define CY_U3P_SUCCESS   0
#   define MUTEX_INIT(log)  CY_U3P_SUCCESS
#   define LOCK(log)        CY_U3P_SUCCESS
#   define UNLOCK(log)
typedef int CyU3PReturnStatus_t;
#endif

#ifndef LOGGER_NUM_ENTRIES
#   define LOGGER_NUM_ENTRIES 256
#endif

static struct {
    uint16_t size;
    uint16_t insert;
    uint16_t remove;

    logger_entry entries[LOGGER_NUM_ENTRIES];

#ifndef LOGGER_HOST
    CyU3PMutex lock;
#endif

} log;


bool logger_init() {
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;

    log.size = 0;
    log.insert = 0;
    log.remove = 0;

    status = MUTEX_INIT(log);
    return status == CY_U3P_SUCCESS;
}

bool logger_record(uint8_t file, uint16_t line, uint16_t data)
{
    bool success = true;
    bool fill_warning = false;
    CyU3PReturnStatus_t status;

    status = LOCK(log);
    if (status != CY_U3P_SUCCESS) {
        /* Can't acquire the log lock. Nothing we can do but bail out. */
        return false;
    }

    if (log.size == LOGGER_NUM_ENTRIES) {
        /* Log is full - nothing we can do */
        success = false;
        goto out;
    }

    /* We're about to fill the second-to-last entry. Add a fill warning
     * to the last entry so we can know that we might have dropped future
     * events. */
    fill_warning = (log.size == LOGGER_NUM_ENTRIES - 2);

    log.entries[log.insert] = logger_entry_pack(file, line, data);
    log.insert = (log.insert + 1) % LOGGER_NUM_ENTRIES;
    log.size++;

    if (fill_warning) {
        log.entries[log.insert] = logger_entry_pack(THIS_FILE, __LINE__, 0);
        log.insert = (log.insert + 1) % LOGGER_NUM_ENTRIES;
        log.size++;
    }

out:
    UNLOCK(log);
    return success;
}

logger_entry logger_read()
{
    CyU3PReturnStatus_t status;
    logger_entry e;

    status = LOCK(log);
    if (status != CY_U3P_SUCCESS) {
        return LOG_ERR;
    }

    if (log.size == 0) {
        e = LOG_EOF;
        goto out;
    }

    e = log.entries[log.remove];
    log.size--;
    log.remove = (log.remove + 1) % LOGGER_NUM_ENTRIES;

out:
    UNLOCK(log);
    return e;
}





/* Logger test program. Build with
 * $(CC) -Wall -Wextra -DLOGGER_HOST -DLOGGER_TEST logger.c -o logger_test
 */
#if defined LOGGER_HOST && defined LOGGER_TEST

#include <stdlib.h>
#include <stdio.h>

#define NO_FAILURE_EXPECTED     0xffff
#define NO_EOF_EXPECTED         0xffff
#define NO_MISMATCH_EXPECTED    0xffff

static void gen_test_values(uint8_t *file_id, uint16_t *line, uint16_t *data,
                            uint16_t i)
{
    logger_entry test;

    *file_id = (LOG_FILE_MASK - i) & LOG_FILE_MASK;
    *line    = (LOG_LINE_MASK - i) & LOG_LINE_MASK;
    *data    = (LOG_DATA_MASK - i) & LOG_LINE_MASK;

    test = logger_entry_pack(*file_id, *line, *data);
    if (test == LOG_EOF || test == LOG_ERR) {
        *data = 42;
    }
}

static bool add_log_entries(uint16_t start, uint16_t end,
                            uint16_t expected_failure)
{
    uint16_t i;
    uint8_t file_id;
    uint16_t line;
    uint16_t data;
    bool success;

    for (i = start; i < end; i++) {
        gen_test_values(&file_id, &line, &data, i);
        success = logger_record(file_id, line, data);
        if (!success) {
            if (i == expected_failure) {
                /* We expected to see this happen; it's part of the test */
                return true;
            } else {
                fprintf(stderr, "Logger reported unexpected failure @ i = %u\n", i);
                return false;
            }
        }
    }

    if (expected_failure != NO_FAILURE_EXPECTED) {
        fprintf(stderr, "Expected failure did not occurr @ i = %u\n",
                expected_failure);
        return false;
    } else {
        return true;
    }
}

static bool remove_log_entries(uint16_t start, uint16_t end,
                               uint16_t expected_eof,
                               uint16_t expected_mismatch)
{
    logger_entry e;
    uint16_t i;
    uint8_t file_id, exp_file_id;
    uint16_t line, exp_line;
    uint16_t data, exp_data;
    bool entry_valid = true;

    for (i = start; i < end; i++) {
        gen_test_values(&exp_file_id, &exp_line, &exp_data, i);

        e = logger_read();

        if (e == LOG_ERR) {
            fprintf(stderr, "Logger read failed @ i = %u\n", i);
            return false;
        }

        if (e == LOG_EOF && expected_eof != i) {
            fprintf(stderr, "Logger reported unexpected EOF @ i = %u\n", i);
            return false;
        }

        if (e != LOG_EOF && expected_eof == i) {
            fprintf(stderr, "Logger did not reporte expected EOF @ i = %u\n", i);
            return false;
        }

        if (e == LOG_EOF && expected_eof == i) {
            return true;
        }

        logger_entry_unpack(e, &file_id, &line, &data);

        if (file_id != exp_file_id) {
            entry_valid = false;
        }

        if (line != exp_line) {
            entry_valid = false;
        }

        if (data != exp_data) {
            entry_valid = false;
        }

        if (!entry_valid && expected_mismatch != i) {
            fprintf(stderr, "Mismatch @ %u. "
                            "Expected { 0x%02x, 0x%04x, 0x%04x }. "
                            "Got { 0x%02x, 0x%04x, 0x%04x }\n",
                            i,
                            file_id, line, data,
                            exp_file_id, exp_line, exp_data);
            return false;
        } else if (entry_valid && expected_mismatch == i) {
            fprintf(stderr, "Entry @ %u doesn't mismatch as expected.\n", i);
        }
    }

    return true;
}


static void print_status(const char *str, bool pass, int *retval) {
    const char *result_str = pass ? "Pass\n" : "Fail\n";
    printf("%s: %s\n", str, result_str);

    if (!pass) {
        *retval = EXIT_FAILURE;
    }
}

int main(void)
{
    int retval = 0;
    bool status;

    logger_init();

    status = remove_log_entries(0, 1, 0, NO_FAILURE_EXPECTED);
    print_status("Remove from empty log", status, &retval);

    status = add_log_entries(0, 1, NO_FAILURE_EXPECTED);
    print_status("Add single entry", status, &retval);

    status = remove_log_entries(0, 1, NO_EOF_EXPECTED, NO_MISMATCH_EXPECTED);
    print_status("Remove single entry", status, &retval);

    status = add_log_entries(0, LOGGER_NUM_ENTRIES - 1, NO_FAILURE_EXPECTED);
    print_status("Fill log", status, &retval);

    status = add_log_entries(0, 1, 0);
    print_status("Overfill log", status, &retval);

    status = remove_log_entries(0, LOGGER_NUM_ENTRIES - 1,
                                NO_EOF_EXPECTED, NO_MISMATCH_EXPECTED);
    print_status("Remove all entries", status, &retval);

    status = remove_log_entries(0, 1, NO_EOF_EXPECTED, 0);
    print_status("Remove fill warning", status, &retval);

    status = add_log_entries(0, 16, NO_FAILURE_EXPECTED);
    print_status("Add entries 0-15", status, &retval);

    status = remove_log_entries(0, 5, NO_EOF_EXPECTED, NO_MISMATCH_EXPECTED);
    print_status("Removed entries 0-5", status, &retval);

    status = add_log_entries(16, 21, NO_FAILURE_EXPECTED);
    print_status("Add 5 more entries: 16-20", status, &retval);

    status = remove_log_entries(5, 20, NO_EOF_EXPECTED, NO_MISMATCH_EXPECTED);
    print_status("Removed entries 5-19", status, &retval);

    status = remove_log_entries(20, 21, NO_EOF_EXPECTED, NO_MISMATCH_EXPECTED);
    print_status("Removed entry 20", status, &retval);

    status = remove_log_entries(21, 22, 21, NO_MISMATCH_EXPECTED);
    print_status("Got expected EOF", status, &retval);

    return retval;
}
#endif
