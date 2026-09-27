/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2026 Nuand LLC
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
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <getopt.h>
#if defined(_WIN32)
#include "clock_gettime.h"
#endif
#include <libbladeRF.h>

#include "conversions.h"
#include "board/board.h"

#if defined(_WIN32)
#define TIMED_ERASE_CLOCK CLOCK_REALTIME
#elif defined(CLOCK_MONOTONIC_PRECISE)
#define TIMED_ERASE_CLOCK CLOCK_MONOTONIC_PRECISE
#elif defined(CLOCK_MONOTONIC_RAW)
#define TIMED_ERASE_CLOCK CLOCK_MONOTONIC_RAW
#elif defined(CLOCK_MONOTONIC)
#define TIMED_ERASE_CLOCK CLOCK_MONOTONIC
#else
#define TIMED_ERASE_CLOCK CLOCK_REALTIME
#endif

#define CHECK_STATUS(fn) \
    do { \
        status = (fn); \
        if (status != 0) { \
            fprintf(stderr, "Failed at line %d: %s\n", \
                    __LINE__, bladerf_strerror(status)); \
            goto out; \
        } \
    } while (0)

#define RECORD_RESULT(name, passed) \
    do { \
        if (passed) { \
            printf("  %-40s PASS\n", name); \
            pass_count++; \
        } else { \
            printf("  %-40s FAIL\n", name); \
            fail_count++; \
        } \
    } while (0)

static uint32_t xorshift32(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static void fill_walking_ones(uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        buf[i] = 1 << (i % 8);
    }
}

static void fill_sequential(uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        buf[i] = (uint8_t)(i & 0xFF);
    }
}

static void fill_prng(uint8_t *buf, uint32_t len, uint32_t seed)
{
    uint32_t state = seed;
    for (uint32_t i = 0; i < len; i += 4) {
        uint32_t r = xorshift32(&state);
        uint32_t remaining = len - i;
        uint32_t chunk = remaining < 4 ? remaining : 4;
        memcpy(buf + i, &r, chunk);
    }
}

static bool verify_all_ff(const uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        if (buf[i] != 0xFF) {
            return false;
        }
    }
    return true;
}

static void find_first_mismatch(const uint8_t *expected,
                                const uint8_t *actual,
                                uint32_t len, uint32_t psize)
{
    uint32_t i;
    uint32_t ctx_start, ctx_end, j;

    for (i = 0; i < len; i++) {
        if (expected[i] != actual[i]) {
            printf("    Mismatch at byte %u (page %u, offset %u)\n",
                   i, i / psize, i % psize);
            printf("    Expected: 0x%02X  Read: 0x%02X\n",
                   expected[i], actual[i]);

            ctx_start = (i >= 8) ? (i - 8) : 0;
            ctx_end = (i + 8 < len) ? (i + 8) : len;

            printf("    Expected context [%u..%u]:",
                   ctx_start, ctx_end - 1);
            for (j = ctx_start; j < ctx_end; j++) {
                printf(" %02X", expected[j]);
            }
            printf("\n");

            printf("    Actual   context [%u..%u]:",
                   ctx_start, ctx_end - 1);
            for (j = ctx_start; j < ctx_end; j++) {
                printf(" %02X", actual[j]);
            }
            printf("\n");

            return;
        }
    }
}

static const uint8_t known_mids[] = { 0xC2, 0xEF, 0x1F };
#define NUM_KNOWN_MIDS (sizeof(known_mids) / sizeof(known_mids[0]))

static bool is_known_mid(uint8_t mid)
{
    for (size_t i = 0; i < NUM_KNOWN_MIDS; i++) {
        if (known_mids[i] == mid) {
            return true;
        }
    }
    return false;
}

static const struct option long_options[] = {
    { "device",     required_argument,  NULL,   'd' },
    { "verbosity",  required_argument,  NULL,   'v' },
    { "cycles",     required_argument,  NULL,   'n' },
    { "blocks",     required_argument,  NULL,   'b' },
    { "erase",      no_argument,        NULL,   'e' },
    { "help",       no_argument,        NULL,   'h' },
    { NULL,         0,                  NULL,   0   },
};

int main(int argc, char *argv[])
{
    int opt = 0;
    int opt_ind = 0;
    int status = 0;
    char *devstr = NULL;
    struct bladerf *dev = NULL;
    bladerf_log_level log_level = BLADERF_LOG_LEVEL_INFO;
    int pass_count = 0;
    int fail_count = 0;
    int endurance_cycles = 10;
    int num_blocks = 2;
    bool erase_on_exit = false;

    uint8_t *backup_buf = NULL;
    uint8_t *work_buf = NULL;
    uint8_t *read_buf = NULL;

    while (opt != -1) {
        opt = getopt_long(argc, argv, "d:v:n:b:eh", long_options, &opt_ind);

        switch (opt) {
            case 'd':
                devstr = optarg;
                break;

            case 'v':
            {
                bool ok;
                log_level = str2loglevel(optarg, &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid log level: %s\n", optarg);
                    return EXIT_FAILURE;
                }
                break;
            }

            case 'n':
            {
                bool ok;
                endurance_cycles = str2uint(optarg, 1, 10000, &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid cycle count: %s\n", optarg);
                    return EXIT_FAILURE;
                }
                break;
            }

            case 'b':
            {
                bool ok;
                num_blocks = str2uint(optarg, 1, 64, &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid block count: %s\n", optarg);
                    return EXIT_FAILURE;
                }
                break;
            }

            case 'e':
                erase_on_exit = true;
                break;

            case 'h':
                printf("Usage: %s [options]\n", argv[0]);
                printf("  -d, --device <str>     Specify device to open.\n");
                printf("  -v, --verbosity <l>    Set libbladeRF verbosity level.\n");
                printf("  -n, --cycles <N>       Endurance test cycles (default: 10).\n");
                printf("  -b, --blocks <N>       Erase blocks to test (default: 2, max 64).\n");
                printf("  -e, --erase            Erase test region on exit instead of restoring.\n");
                printf("  -h, --help             Show this text.\n");
                printf("\n");
                printf("Comprehensive SPI flash data-integrity validation.\n");
                printf("Tests erase, write, read, patterns, boundaries, and alignment.\n");
                printf("Uses the last N erase blocks of flash.\n");
                return 0;

            default:
                break;
        }
    }

    bladerf_log_set_verbosity(log_level);

    printf("Flash Validation Test\n");
    printf("=====================\n\n");

    CHECK_STATUS(bladerf_open(&dev, devstr));

    struct bladerf_flash_arch *fa = dev->flash_arch;
    uint32_t psize = fa->psize_bytes;
    uint32_t ebsize = fa->ebsize_bytes;
    uint32_t tsize = fa->tsize_bytes;
    uint32_t pages_per_eb = ebsize / psize;
    uint32_t region_size = ebsize * (uint32_t)num_blocks;
    uint32_t total_pages = pages_per_eb * (uint32_t)num_blocks;
    uint32_t test_addr = tsize - region_size;

    {
        const char *board_name = bladerf_get_board_name(dev);
        char serial[BLADERF_SERIAL_LENGTH];
        bladerf_get_serial(dev, serial);

        printf("Device:      %s (serial %s)\n", board_name, serial);
        printf("Flash:       MID=0x%02X DID=0x%02X (%u bytes)\n",
               fa->manufacturer_id, fa->device_id, tsize);
        printf("Page size:   %u bytes\n", psize);
        printf("EB size:     %u bytes (%u pages/EB)\n", ebsize, pages_per_eb);
        printf("Test region: 0x%08X (%d block%s, %u bytes)\n\n",
               test_addr, num_blocks, num_blocks > 1 ? "s" : "",
               region_size);
    }

    backup_buf = malloc(region_size);
    work_buf = malloc(region_size);
    read_buf = malloc(region_size);
    if (backup_buf == NULL || work_buf == NULL || read_buf == NULL) {
        fprintf(stderr, "Failed to allocate buffers (%u bytes)\n",
                region_size);
        status = BLADERF_ERR_MEM;
        goto out;
    }

    printf("Saving test region contents...\n");
    CHECK_STATUS(bladerf_read_flash_bytes(dev, backup_buf,
                                          test_addr, region_size));
    printf("  Backup complete.\n\n");

    printf("Results:\n");

    /* Test 0: Flash ID sanity check */
    {
        RECORD_RESULT("Flash decode status",
                       fa->status == STATUS_SUCCESS);
        RECORD_RESULT("Flash MID known",
                       is_known_mid(fa->manufacturer_id));

        bool geom_ok = (fa->tsize_bytes > 0) &&
                       (fa->psize_bytes > 0) &&
                       (fa->ebsize_bytes > 0) &&
                       (fa->tsize_bytes ==
                        fa->psize_bytes * fa->num_pages) &&
                       (fa->tsize_bytes ==
                        fa->ebsize_bytes * fa->num_ebs);
        RECORD_RESULT("Flash geometry consistent", geom_ok);
    }

    /* Test 1: Erase verify */
    {
        CHECK_STATUS(bladerf_erase_flash_bytes(dev, test_addr, region_size));
        CHECK_STATUS(bladerf_read_flash_bytes(dev, read_buf,
                                              test_addr, region_size));

        if (!verify_all_ff(read_buf, region_size)) {
            memset(work_buf, 0xFF, region_size);
            find_first_mismatch(work_buf, read_buf, region_size, psize);
            RECORD_RESULT("Erase verify (all 0xFF)", false);
        } else {
            RECORD_RESULT("Erase verify (all 0xFF)", true);
        }
    }

    /* Test 1b: Erase discipline - prove erase actually clears data */
    {
        CHECK_STATUS(bladerf_erase_flash_bytes(dev, test_addr, ebsize));

        memset(work_buf, 0xAA, psize);
        CHECK_STATUS(bladerf_write_flash_bytes(dev, work_buf,
                                                test_addr, psize));

        memset(work_buf, 0x55, psize);
        CHECK_STATUS(bladerf_write_flash_bytes(dev, work_buf,
                                                test_addr, psize));

        CHECK_STATUS(bladerf_read_flash_bytes(dev, read_buf,
                                               test_addr, psize));

        memset(work_buf, 0x00, psize);
        RECORD_RESULT("Erase discipline (0xAA & 0x55 = 0x00)",
                       memcmp(work_buf, read_buf, psize) == 0);

        CHECK_STATUS(bladerf_erase_flash_bytes(dev, test_addr, ebsize));
        memset(work_buf, 0xAA, psize);
        CHECK_STATUS(bladerf_write_flash_bytes(dev, work_buf,
                                                test_addr, psize));
        CHECK_STATUS(bladerf_read_flash_bytes(dev, read_buf,
                                               test_addr, psize));
        RECORD_RESULT("Erase discipline (erase+write 0xAA)",
                       memcmp(work_buf, read_buf, psize) == 0);
    }

    /* Test 2: Page patterns */
    {
        struct {
            const char *name;
            uint8_t val;
            enum { PAT_FILL, PAT_WALK, PAT_SEQ } type;
        } patterns[] = {
            { "Page pattern 0x00",         0x00, PAT_FILL },
            { "Page pattern 0xFF",         0xFF, PAT_FILL },
            { "Page pattern 0xAA",         0xAA, PAT_FILL },
            { "Page pattern 0x55",         0x55, PAT_FILL },
            { "Page pattern 0xA5",         0xA5, PAT_FILL },
            { "Page pattern 0x5A",         0x5A, PAT_FILL },
            { "Page pattern walking-1",    0,    PAT_WALK },
            { "Page pattern sequential",   0,    PAT_SEQ  },
        };
        int npatterns = sizeof(patterns) / sizeof(patterns[0]);

        for (int p = 0; p < npatterns; p++) {
            CHECK_STATUS(bladerf_erase_flash_bytes(dev, test_addr, ebsize));

            switch (patterns[p].type) {
                case PAT_FILL:
                    memset(work_buf, patterns[p].val, psize);
                    break;
                case PAT_WALK:
                    fill_walking_ones(work_buf, psize);
                    break;
                case PAT_SEQ:
                    fill_sequential(work_buf, psize);
                    break;
            }

            CHECK_STATUS(bladerf_write_flash_bytes(dev, work_buf, test_addr, psize));
            CHECK_STATUS(bladerf_read_flash_bytes(dev, read_buf, test_addr, psize));

            if (memcmp(work_buf, read_buf, psize) != 0) {
                find_first_mismatch(work_buf, read_buf, psize, psize);
                RECORD_RESULT(patterns[p].name, false);
            } else {
                RECORD_RESULT(patterns[p].name, true);
            }
        }
    }

    /* Test 3: Multi-page sequential - write full region */
    {
        char label[64];
        uint32_t i, pg, offset;

        CHECK_STATUS(bladerf_erase_flash_bytes(dev, test_addr, region_size));

        for (i = 0; i < region_size; i++) {
            work_buf[i] = (uint8_t)(i & 0xFF);
        }

        for (pg = 0; pg < total_pages; pg++) {
            offset = pg * psize;
            CHECK_STATUS(bladerf_write_flash_bytes(
                dev, work_buf + offset, test_addr + offset, psize));
        }

        CHECK_STATUS(bladerf_read_flash_bytes(dev, read_buf,
                                              test_addr, region_size));

        snprintf(label, sizeof(label),
                 "Multi-page sequential (%u pages)", total_pages);
        if (memcmp(work_buf, read_buf, region_size) != 0) {
            find_first_mismatch(work_buf, read_buf, region_size, psize);
            RECORD_RESULT(label, false);
        } else {
            RECORD_RESULT(label, true);
        }
    }

    /* Test 4: Random pattern with reproducible seed */
    {
        uint32_t seed = 0xDEADBEEF;
        printf("  (PRNG seed: 0x%08X)\n", seed);

        CHECK_STATUS(bladerf_erase_flash_bytes(dev, test_addr, ebsize));
        fill_prng(work_buf, psize, seed);
        CHECK_STATUS(bladerf_write_flash_bytes(dev, work_buf, test_addr, psize));
        CHECK_STATUS(bladerf_read_flash_bytes(dev, read_buf, test_addr, psize));

        if (memcmp(work_buf, read_buf, psize) != 0) {
            find_first_mismatch(work_buf, read_buf, psize, psize);
            RECORD_RESULT("Random pattern (PRNG)", false);
        } else {
            RECORD_RESULT("Random pattern (PRNG)", true);
        }
    }

    /* Test 4b: Read consistency - read same data multiple times */
    {
        bool consistent = true;
        int nreads = 5;

        CHECK_STATUS(bladerf_erase_flash_bytes(dev, test_addr, ebsize));
        fill_prng(work_buf, psize, 0xCAFEBABE);
        CHECK_STATUS(bladerf_write_flash_bytes(dev, work_buf,
                                                test_addr, psize));

        for (int r = 0; r < nreads; r++) {
            CHECK_STATUS(bladerf_read_flash_bytes(dev, read_buf,
                                                   test_addr, psize));
            if (memcmp(work_buf, read_buf, psize) != 0) {
                printf("    Read %d/%d mismatch\n", r + 1, nreads);
                find_first_mismatch(work_buf, read_buf, psize, psize);
                consistent = false;
                break;
            }
        }

        char label[64];
        snprintf(label, sizeof(label),
                 "Read consistency (%d reads)", nreads);
        RECORD_RESULT(label, consistent);
    }

    /* Test 5: Cross-page corruption check */
    {
        CHECK_STATUS(bladerf_erase_flash_bytes(dev, test_addr, ebsize));

        memset(work_buf, 0xAA, psize);
        memset(work_buf + psize, 0x55, psize);

        CHECK_STATUS(bladerf_write_flash_bytes(dev, work_buf,
                                              test_addr, psize));
        CHECK_STATUS(bladerf_write_flash_bytes(dev, work_buf + psize,
                                               test_addr + psize, psize));
        CHECK_STATUS(bladerf_read_flash_bytes(dev, read_buf,
                                              test_addr, 2 * psize));

        if (memcmp(work_buf, read_buf, 2 * psize) != 0) {
            find_first_mismatch(work_buf, read_buf, 2 * psize, psize);
            RECORD_RESULT("Cross-page corruption check", false);
        } else {
            RECORD_RESULT("Cross-page corruption check", true);
        }
    }

    /* Test 6: Multi-block PRNG fill (only when --blocks > 1) */
    if (num_blocks > 1) {
        char label[64];
        uint32_t seed = (uint32_t)time(NULL);
        uint32_t pg, offset;

        printf("  (Multi-block PRNG seed: 0x%08X)\n", seed);

        CHECK_STATUS(bladerf_erase_flash_bytes(dev, test_addr, region_size));
        fill_prng(work_buf, region_size, seed);

        for (pg = 0; pg < total_pages; pg++) {
            offset = pg * psize;
            CHECK_STATUS(bladerf_write_flash_bytes(
                dev, work_buf + offset, test_addr + offset, psize));
        }

        CHECK_STATUS(bladerf_read_flash_bytes(dev, read_buf,
                                              test_addr, region_size));

        snprintf(label, sizeof(label),
                 "Multi-block PRNG (%d blocks)", num_blocks);
        if (memcmp(work_buf, read_buf, region_size) != 0) {
            find_first_mismatch(work_buf, read_buf, region_size, psize);
            RECORD_RESULT(label, false);
        } else {
            RECORD_RESULT(label, true);
        }
    }

    /* Test 7: Boundary write+verify - last page of flash */
    {
        uint32_t last_page_addr = tsize - psize;
        uint32_t last_eb_addr = tsize - ebsize;

        CHECK_STATUS(bladerf_erase_flash_bytes(dev, last_eb_addr, ebsize));
        fill_prng(work_buf, psize, 0xB00DA4EE);
        CHECK_STATUS(bladerf_write_flash_bytes(dev, work_buf,
                                                last_page_addr, psize));
        CHECK_STATUS(bladerf_read_flash_bytes(dev, read_buf,
                                               last_page_addr, psize));

        if (memcmp(work_buf, read_buf, psize) != 0) {
            find_first_mismatch(work_buf, read_buf, psize, psize);
            RECORD_RESULT("Boundary write+verify (last page)", false);
        } else {
            RECORD_RESULT("Boundary write+verify (last page)", true);
        }
    }

    /* Test 8: Endurance - cycle through different pages each iteration */
    {
        bool endurance_ok = true;
        int cycle_failures = 0;

        for (int c = 0; c < endurance_cycles; c++) {
            int s;
            uint32_t pg = (uint32_t)c % pages_per_eb;
            uint32_t pg_addr = test_addr + pg * psize;

            s = bladerf_erase_flash_bytes(dev, test_addr, ebsize);
            if (s != 0) { endurance_ok = false; cycle_failures++; continue; }

            fill_prng(work_buf, psize, (uint32_t)c);
            s = bladerf_write_flash_bytes(dev, work_buf, pg_addr, psize);
            if (s != 0) { endurance_ok = false; cycle_failures++; continue; }

            s = bladerf_read_flash_bytes(dev, read_buf, pg_addr, psize);
            if (s != 0) { endurance_ok = false; cycle_failures++; continue; }

            if (memcmp(work_buf, read_buf, psize) != 0) {
                printf("    Cycle %d page %u mismatch\n", c, pg);
                endurance_ok = false;
                cycle_failures++;
            }
        }

        char label[64];
        snprintf(label, sizeof(label), "Endurance (%d cycles)", endurance_cycles);
        if (cycle_failures > 0) {
            printf("  (%d/%d cycles failed)\n", cycle_failures, endurance_cycles);
        }
        RECORD_RESULT(label, endurance_ok);
    }

    /* Test 9: Alignment enforcement */
    {
        int s;

        s = bladerf_read_flash_bytes(dev, read_buf, test_addr + 1, psize);
        RECORD_RESULT("Alignment (misaligned read addr)",
                       s == BLADERF_ERR_INVAL);

        s = bladerf_write_flash_bytes(dev, work_buf, test_addr + 1, psize);
        RECORD_RESULT("Alignment (misaligned write addr)",
                       s == BLADERF_ERR_INVAL);

        s = bladerf_read_flash_bytes(dev, read_buf, test_addr, psize + 1);
        RECORD_RESULT("Alignment (misaligned read len)",
                       s == BLADERF_ERR_INVAL);

        s = bladerf_write_flash_bytes(dev, work_buf, test_addr, psize + 1);
        RECORD_RESULT("Alignment (misaligned write len)",
                       s == BLADERF_ERR_INVAL);

        s = bladerf_erase_flash_bytes(dev, test_addr + psize, ebsize);
        RECORD_RESULT("Alignment (misaligned erase addr)",
                       s == BLADERF_ERR_INVAL);

        s = bladerf_erase_flash_bytes(dev, test_addr,
                                       ebsize + psize);
        RECORD_RESULT("Alignment (misaligned erase len)",
                       s == BLADERF_ERR_INVAL);
    }

    /* Test 10: Out-of-bounds rejection */
    {
        int s;

        s = bladerf_read_flash_bytes(dev, read_buf, tsize, psize);
        RECORD_RESULT("Bounds (read at tsize)",
                       s == BLADERF_ERR_INVAL);

        s = bladerf_write_flash_bytes(dev, work_buf, tsize, psize);
        RECORD_RESULT("Bounds (write at tsize)",
                       s == BLADERF_ERR_INVAL);

        s = bladerf_read_flash_bytes(dev, read_buf, tsize - psize, 2 * psize);
        RECORD_RESULT("Bounds (read past end)",
                       s == BLADERF_ERR_INVAL);

        s = bladerf_write_flash_bytes(dev, work_buf, tsize - psize, 2 * psize);
        RECORD_RESULT("Bounds (write past end)",
                       s == BLADERF_ERR_INVAL);

        s = bladerf_erase_flash_bytes(dev, tsize, ebsize);
        RECORD_RESULT("Bounds (erase at tsize)",
                       s == BLADERF_ERR_INVAL);

        s = bladerf_erase_flash_bytes(dev, tsize - ebsize, 2 * ebsize);
        RECORD_RESULT("Bounds (erase past end)",
                       s == BLADERF_ERR_INVAL);
    }

    /* Test 11: Timed block erase
     * AT25FF321A datasheet: 64KB erase typ 700ms, max 2250ms.
     * Other flash parts are faster; this validates the firmware timeout
     * is long enough for worst-case Renesas timing. */
    {
        struct timespec t0, t1;
        double elapsed_ms;
        uint32_t spec_max_ms = 2250;
        int ts;

        CHECK_STATUS(bladerf_erase_flash_bytes(dev, test_addr, ebsize));

        memset(work_buf, 0x55, psize);
        CHECK_STATUS(bladerf_write_flash_bytes(dev, work_buf, test_addr, psize));

        ts = clock_gettime(TIMED_ERASE_CLOCK, &t0);
        if (ts != 0) {
            fprintf(stderr, "  Failed to get start time\n");
            goto skip_timed;
        }

        CHECK_STATUS(bladerf_erase_flash_bytes(dev, test_addr, ebsize));

        ts = clock_gettime(TIMED_ERASE_CLOCK, &t1);
        if (ts != 0) {
            fprintf(stderr, "  Failed to get end time\n");
            goto skip_timed;
        }

        elapsed_ms = (t1.tv_sec - t0.tv_sec) * 1000.0
                   + (t1.tv_nsec - t0.tv_nsec) / 1e6;

        printf("  (64KB erase: %.1f ms, spec max: %u ms)\n",
               elapsed_ms, spec_max_ms);
        RECORD_RESULT("Timed erase (under spec max)",
                       elapsed_ms < spec_max_ms);

skip_timed:
        CHECK_STATUS(bladerf_read_flash_bytes(dev, read_buf, test_addr, psize));
        RECORD_RESULT("Timed erase verify (all 0xFF)",
                       verify_all_ff(read_buf, psize));
    }

    printf("\n");

    /* Restore or erase */
    if (erase_on_exit) {
        printf("Erasing test region...\n");
        CHECK_STATUS(bladerf_erase_flash_bytes(dev, test_addr, region_size));
        printf("  Test region erased.\n");
    } else {
        uint32_t pg, offset;

        printf("Restoring test region...\n");
        CHECK_STATUS(bladerf_erase_flash_bytes(dev, test_addr, region_size));

        for (pg = 0; pg < total_pages; pg++) {
            offset = pg * psize;
            CHECK_STATUS(bladerf_write_flash_bytes(
                dev, backup_buf + offset, test_addr + offset, psize));
        }

        CHECK_STATUS(bladerf_read_flash_bytes(dev, read_buf,
                                              test_addr, region_size));
        if (memcmp(backup_buf, read_buf, region_size) != 0) {
            fprintf(stderr, "  WARNING: Restore verification failed!\n");
        } else {
            printf("  Restore verified.\n");
        }
    }

    printf("\n========================================\n");
    printf("  Passed: %d\n", pass_count);
    printf("  Failed: %d\n", fail_count);
    printf("  Result: %s\n",
           fail_count == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    printf("========================================\n");

out:
    free(backup_buf);
    free(work_buf);
    free(read_buf);

    if (dev != NULL) {
        bladerf_close(dev);
    }
    return (status == 0 && fail_count == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
