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
#include <getopt.h>
#include <libbladeRF.h>

#include "conversions.h"
#include "board/board.h"

#define CHECK_STATUS(fn) \
    do { \
        status = (fn); \
        if (status != 0) { \
            fprintf(stderr, "Failed at line %d: %s\n", \
                    __LINE__, bladerf_strerror(status)); \
            goto out; \
        } \
    } while (0)

struct flash_expectation {
    uint8_t     mid;
    uint8_t     did;
    const char *manufacturer;
    const char *device;
    uint32_t    tsize_bytes;
    uint32_t    psize_bytes;
    uint32_t    ebsize_bytes;
    uint32_t    num_pages;
    uint32_t    num_ebs;
};

static const struct flash_expectation known_flash[] = {
    { 0xC2, 0x36, "MACRONIX", "MX25U3235E",
      32 << 17, 256, 64 << 10, 16384, 64 },
    { 0xEF, 0x15, "WINBOND",  "W25Q32JV",
      32 << 17, 256, 64 << 10, 16384, 64 },
    { 0xEF, 0x16, "WINBOND",  "W25Q64JV",
      64 << 17, 256, 64 << 10, 32768, 128 },
    { 0xEF, 0x17, "WINBOND",  "W25Q128JV",
      128 << 17, 256, 64 << 10, 65536, 256 },
    { 0x1F, 0x47, "RENESAS",  "AT25FF321A",
      32 << 17, 256, 64 << 10, 16384, 64 },
};

#define NUM_KNOWN_FLASH (sizeof(known_flash) / sizeof(known_flash[0]))

static const struct flash_expectation *find_expectation(uint8_t mid,
                                                        uint8_t did)
{
    for (size_t i = 0; i < NUM_KNOWN_FLASH; i++) {
        if (known_flash[i].mid == mid && known_flash[i].did == did) {
            return &known_flash[i];
        }
    }
    return NULL;
}

static const struct option long_options[] = {
    { "device",     required_argument,  NULL,   'd' },
    { "verbosity",  required_argument,  NULL,   'v' },
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

    while (opt != -1) {
        opt = getopt_long(argc, argv, "d:v:h", long_options, &opt_ind);

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

            case 'h':
                printf("Usage: %s [options]\n", argv[0]);
                printf("  -d, --device <str>     Specify device to open.\n");
                printf("  -v, --verbosity <l>    Set libbladeRF verbosity level.\n");
                printf("  -h, --help             Show this text.\n");
                printf("\n");
                printf("Validates SPI flash identification and architecture against\n");
                printf("known device expectations. Supports Macronix, Winbond, and\n");
                printf("Renesas flash ICs.\n");
                return 0;

            default:
                break;
        }
    }

    bladerf_log_set_verbosity(log_level);

    printf("Flash ID Validation Test\n");
    printf("========================\n\n");

    CHECK_STATUS(bladerf_open(&dev, devstr));

    struct bladerf_flash_arch *fa = dev->flash_arch;

    printf("Flash identification:\n");
    printf("  Manufacturer ID: 0x%02X\n", fa->manufacturer_id);
    printf("  Device ID:       0x%02X\n", fa->device_id);
    printf("  Decode status:   %s\n",
           fa->status == STATUS_SUCCESS ? "SUCCESS" :
           fa->status == STATUS_ASSUMED ? "ASSUMED" :
           "UNINITIALIZED");
    printf("\n");

    printf("Flash architecture:\n");
    printf("  Total size:      %u bytes (%u Mbit)\n",
           fa->tsize_bytes, fa->tsize_bytes >> 17);
    printf("  Page size:       %u bytes\n", fa->psize_bytes);
    printf("  Erase block:     %u bytes (%u KiB)\n",
           fa->ebsize_bytes, fa->ebsize_bytes >> 10);
    printf("  Number of pages: %u\n", fa->num_pages);
    printf("  Number of EBs:   %u\n", fa->num_ebs);
    printf("\n");

    /* Cross-check: public API flash size should agree */
    {
        uint32_t api_size = 0;
        bool is_guess = true;
        CHECK_STATUS(bladerf_get_flash_size(dev, &api_size, &is_guess));

        printf("Public API cross-check:\n");
        printf("  bladerf_get_flash_size: %u bytes, is_guess=%s\n",
               api_size, is_guess ? "true" : "false");
        printf("\n");

        if (api_size != fa->tsize_bytes) {
            printf("  *** FAIL: API size %u != arch size %u ***\n\n",
                   api_size, fa->tsize_bytes);
            fail_count++;
        } else {
            printf("  *** PASS: API size matches arch size ***\n\n");
            pass_count++;
        }
    }

    /* Validate decode status */
    if (fa->status == STATUS_SUCCESS) {
        printf("  *** PASS: Flash ID decoded successfully ***\n\n");
        pass_count++;
    } else {
        printf("  *** FAIL: Flash ID was not decoded (status=%d) ***\n\n",
               fa->status);
        fail_count++;
    }

    /* Validate against known expectations */
    const struct flash_expectation *expect =
        find_expectation(fa->manufacturer_id, fa->device_id);

    if (expect == NULL) {
        printf("  *** FAIL: Unknown flash MID=0x%02X DID=0x%02X ***\n",
               fa->manufacturer_id, fa->device_id);
        printf("           Not in known flash table.\n\n");
        fail_count++;
    } else {
        printf("Matched: %s %s\n", expect->manufacturer, expect->device);
        printf("Validating architecture fields:\n");

#define CHECK_FIELD(field, fmt) \
    do { \
        if (fa->field != expect->field) { \
            printf("  *** FAIL: " #field " = " fmt \
                   ", expected " fmt " ***\n", \
                   fa->field, expect->field); \
            fail_count++; \
        } else { \
            printf("  PASS: " #field " = " fmt "\n", fa->field); \
            pass_count++; \
        } \
    } while (0)

        CHECK_FIELD(tsize_bytes,  "%u");
        CHECK_FIELD(psize_bytes,  "%u");
        CHECK_FIELD(ebsize_bytes, "%u");
        CHECK_FIELD(num_pages,    "%u");
        CHECK_FIELD(num_ebs,      "%u");

#undef CHECK_FIELD
        printf("\n");
    }

    /* Consistency checks */
    printf("Consistency checks:\n");

    if (fa->tsize_bytes == fa->psize_bytes * fa->num_pages) {
        printf("  PASS: tsize == psize * num_pages\n");
        pass_count++;
    } else {
        printf("  *** FAIL: tsize (%u) != psize (%u) * num_pages (%u) ***\n",
               fa->tsize_bytes, fa->psize_bytes, fa->num_pages);
        fail_count++;
    }

    if (fa->tsize_bytes == fa->ebsize_bytes * fa->num_ebs) {
        printf("  PASS: tsize == ebsize * num_ebs\n");
        pass_count++;
    } else {
        printf("  *** FAIL: tsize (%u) != ebsize (%u) * num_ebs (%u) ***\n",
               fa->tsize_bytes, fa->ebsize_bytes, fa->num_ebs);
        fail_count++;
    }

    printf("\n");
    printf("========================================\n");
    printf("  Passed: %d\n", pass_count);
    printf("  Failed: %d\n", fail_count);
    printf("  Result: %s\n",
           fail_count == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    printf("========================================\n");

out:
    if (dev != NULL) {
        bladerf_close(dev);
    }
    return (status == 0 && fail_count == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
