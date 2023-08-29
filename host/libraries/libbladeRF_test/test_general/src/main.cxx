/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2023 Nuand LLC
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

#include "conversions.h"
#include "log.h"
#include <cstdlib>
#include <getopt.h>
#include <gtest/gtest.h>
#include <libbladeRF.h>
#include <stdio.h>
#include <stdlib.h>

#define TEST_LIBBLADERF libbladeRF
#define TEST_BLADERF device
int status = 0;

TEST(TEST_LIBBLADERF, version) {
    status = std::system("./output/libbladeRF_test_version");
    ASSERT_EQ(0, status);
}

TEST(TEST_LIBBLADERF, c) {
    status = std::system("./output/libbladeRF_test_c");
    ASSERT_EQ(0, status);
}

TEST(TEST_LIBBLADERF, cpp) {
    status = std::system("./output/libbladeRF_test_cpp");
    ASSERT_EQ(0, status);
}

TEST(TEST_LIBBLADERF, parse) {
    status = std::system("./output/libbladeRF_test_parse");
    ASSERT_EQ(0, status);
}

TEST(TEST_LIBBLADERF, interleaver) {
    status = std::system("./output/libbladeRF_test_interleaver");
    ASSERT_EQ(0, status);
}

TEST(TEST_BLADERF, open) {
    status = std::system("./output/libbladeRF_test_open");
    ASSERT_EQ(0, status);
}

TEST(TEST_BLADERF, frequency_hop) {
    status = std::system("./output/libbladeRF_test_freq_hop -i 200");
    ASSERT_EQ(0, status);
}

TEST(TEST_BLADERF, peripheral_timing) {
    status = std::system("./output/libbladeRF_test_peripheral_timing");
    ASSERT_EQ(0, status);
}

TEST(TEST_BLADERF, repeated_stream) {
    status = std::system("./output/libbladeRF_test_repeated_stream --tx -r 10");
    ASSERT_EQ(0, status);
    status = std::system("./output/libbladeRF_test_repeated_stream --rx -r 10");
    ASSERT_EQ(0, status);
}

#define OPTARG "v:h"
static struct option long_options[] = {
    { "verbosity",  required_argument,  NULL,   'v' },
    { "help",       no_argument,        NULL,   'h' },
    { NULL,         0,                  NULL,   0   },
};

int main(int argc, char** argv) {
    bool ok = true;
    int opt = 0;
    int opt_ind = 0;
    bladerf_log_level log_level;

    while (opt != -1) {
        opt = getopt_long(argc, argv, OPTARG, long_options, &opt_ind);

        switch (opt) {
            case 'v':
                log_level = str2loglevel(optarg, &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid log level: %s\n", optarg);
                    return EXIT_FAILURE;
                }
                bladerf_log_set_verbosity(log_level);
                break;

            case 'h':
                printf("Usage: %s [options]\n", argv[0]);
                printf("  -v, --verbosity <l>          Set libbladeRF verbosity level.\n");
                printf("  -h, --help                   Show this text.\n");
                printf("\n");
                printf("  --gtest_list_tests                List all available tests without running them.\n");
                printf("  --gtest_filter=<filter>           Run only the tests matching the filter.\n");
                printf("                                        Example filters: \"libbladeRF*\", \"hdl*\", \"synthesis*\"\n");
                printf("  --gtest_also_run_disabled_tests   Run only the tests matching the filter.\n");
                printf("  --gtest_output=\"json\"           Generates JSON report of test run.\n");
                printf("\n");
                return EXIT_SUCCESS;

            default:
                break;
        }
    }

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
