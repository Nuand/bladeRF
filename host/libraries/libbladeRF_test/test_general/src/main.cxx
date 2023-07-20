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
#define TEST_XB200 xb200
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

TEST(TEST_BLADERF, quick_retune) {
    status = std::system("./output/libbladeRF_test_quick_retune -i 100");
    ASSERT_EQ(0, status);
}

TEST(TEST_BLADERF, tune_timing) {
    status = std::system("./output/libbladeRF_test_tune_timing -i 200");
    ASSERT_EQ(0, status);
}

TEST(TEST_BLADERF, rx_discont) {
    status = std::system("./output/libbladeRF_test_rx_discont -b 16 -i 1000");
    ASSERT_EQ(0, status);
    status = std::system("./output/libbladeRF_test_rx_discont -b 8 -i 1000");
    ASSERT_EQ(0, status);
}

TEST(TEST_BLADERF, unused_sync) {
    status = std::system("./output/libbladeRF_test_unused_sync");
    ASSERT_EQ(0, status);
}

TEST(TEST_BLADERF, enable_module) {
    status = std::system("./output/libbladeRF_test_ctrl -t enable_module");
    ASSERT_EQ(0, status);
}

TEST(TEST_BLADERF, loopback) {
    status = std::system("./output/libbladeRF_test_ctrl -t loopback");
    ASSERT_EQ(0, status);
}

TEST(TEST_BLADERF, rx_mux) {
    status = std::system("./output/libbladeRF_test_ctrl -t rx_mux");
    ASSERT_EQ(0, status);
}

TEST(TEST_BLADERF, gain) {
    status = std::system("./output/libbladeRF_test_ctrl -t gain");
    ASSERT_EQ(0, status);
}

TEST(TEST_BLADERF, bandwidth) {
    status = std::system("./output/libbladeRF_test_ctrl -t bandwidth --fast");
    ASSERT_EQ(0, status);
}

TEST(TEST_BLADERF, frequency) {
    status = std::system("./output/libbladeRF_test_ctrl -t frequency --fast");
    ASSERT_EQ(0, status);
}

TEST(TEST_BLADERF, samplerate) {
    status = std::system("./output/libbladeRF_test_ctrl -t samplerate --fast");
    ASSERT_EQ(0, status);
}

TEST(TEST_BLADERF, DISABLED_threads) {
    status = std::system("./output/libbladeRF_test_ctrl -t threads --fast");
    ASSERT_EQ(0, status);
}

TEST(TEST_XB200, DISABLED_xb200) {
    status = std::system("./output/libbladeRF_test_ctrl --xb200 -t xb200");
    ASSERT_EQ(0, status);
}

TEST(TEST_BLADERF, tx_onoff_nowsched) {
    status = std::system("./output/libbladeRF_test_timestamps -t tx_onoff_nowsched");
    ASSERT_EQ(0, status);
}

TEST(TEST_BLADERF, print) {
    status = std::system("./output/libbladeRF_test_timestamps -t print -f");
    ASSERT_EQ(0, status);
}

TEST(TEST_BLADERF, rx_gaps) {
    status = std::system("./output/libbladeRF_test_timestamps -t rx_gaps -f");
    ASSERT_EQ(0, status);
}

TEST(TEST_BLADERF, rx_scheduled) {
    status = std::system("./output/libbladeRF_test_timestamps -t rx_scheduled -f");
    ASSERT_EQ(0, status);
}

TEST(TEST_BLADERF, digital_loopback) {
    std::string command;
    std::string sample_formats[] = {"8", "16"};
    std::string loopback_modes[] = {"fw", "fpga", "rfic"};

    int num_formats = sizeof(sample_formats) / sizeof(sample_formats[0]);
    int num_loopback_modes = sizeof(loopback_modes) / sizeof(loopback_modes[0]);

    for (int i = 0; i < num_formats; i++) {
        for (int j = 0; j < num_loopback_modes; j++) {
            command = "./output/libbladeRF_test_digital_loopback -c 20000000";
            command += " -b " + sample_formats[i];
            command += " -l " + loopback_modes[j];
            std::cout << command << std::endl;
            status = std::system(command.c_str());
            ASSERT_EQ(0, status);
        }
    }
}

TEST(TEST_BLADERF, async) {
    status = std::system("./output/libbladeRF_test_async tx 2048 16");
    ASSERT_EQ(0, status);
    status = std::system("./output/libbladeRF_test_async rx 2048 16");
    ASSERT_EQ(0, status);
}

TEST(TEST_LIBBLADERF, rx_meta) {
    std::string command;
    std::string sample_formats[] = {"8", "16"};
    std::string mimo_enable[] = {"", "-m"};

    int num_formats = sizeof(sample_formats) / sizeof(sample_formats[0]);
    int num_mimo_states = sizeof(mimo_enable) / sizeof(mimo_enable[0]);

    for (int i = 0; i < num_formats; i++) {
        for (int j = 0; j < num_mimo_states; j++) {
            command = "./output/libbladeRF_test_rx_meta";
            command += " -b " + sample_formats[i];
            command += " " + mimo_enable[j];
            std::cout << command << std::endl;
            status = std::system(command.c_str());
            ASSERT_EQ(0, status);
        }
    }
}

TEST(TEST_LIBBLADERF, scheduled_retune) {
    std::string command;
    std::string direction[] = {"tx", "rx"};
    std::string speed[] = {"quick"};

    int num_formats = sizeof(direction) / sizeof(direction[0]);
    int num_mimo_states = sizeof(speed) / sizeof(speed[0]);

    for (int i = 0; i < num_formats; i++) {
        for (int j = 0; j < num_mimo_states; j++) {
            command = "./output/libbladeRF_test_scheduled_retune ";
            command += direction[i] + " ";
            command += "hop.csv ";
            command += speed[j];
            std::cout << command << std::endl;
            status = std::system(command.c_str());
            ASSERT_EQ(0, status);
        }
    }
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
