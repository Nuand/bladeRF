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
#include "../include/include.h"
#include <absl/strings/string_view.h>
#include <absl/strings/str_cat.h>
#include <absl/algorithm/container.h>
#include <cstdlib>
#include <getopt.h>
#include <gtest/gtest.h>
#include <libbladeRF.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define TEST_LIBBLADERF libbladeRF
#define TEST_XB200 xb200
#define BLADERF_QUARTUS_DIR "../../hdl/quartus/"
#define BLADERF_MODELSIM_DIR "../../hdl/fpga/platforms/bladerf-micro/modelsim/"
#define NIOS2SHELL "~/intelFPGA_lite/20.1/nios2eds/nios2_command_shell.sh"
int status = 0;

#define TEST_HDL_COMPILE(test_name) \
    TEST_F(hdl, compile_##test_name) { \
        status = std::system(NIOS2SHELL" vsim -c -do \"do test.do " #test_name ".do\""); \
        EXPECT_EQ(status, 0); \
    }

#define TEST_HDL_VERIFY(test_name, test_id, tb_args, run_time_us) \
    TEST_F(hdl, verify_##test_name##_##test_id) { \
        status = std::system( \
            NIOS2SHELL" vsim -c -do \"do test.do vsim nuand." #test_name " " #tb_args " " #run_time_us "\""); \
        EXPECT_EQ(status, 0); \
    }

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

TEST(TEST_LIBBLADERF, open) {
    status = std::system("./output/libbladeRF_test_open");
    ASSERT_EQ(0, status);
}

TEST(TEST_LIBBLADERF, frequency_hop) {
    status = std::system("./output/libbladeRF_test_freq_hop -i 200");
    ASSERT_EQ(0, status);
}

TEST(TEST_LIBBLADERF, peripheral_timing) {
    status = std::system("./output/libbladeRF_test_peripheral_timing");
    ASSERT_EQ(0, status);
}

TEST(TEST_LIBBLADERF, repeated_stream) {
    status = std::system("./output/libbladeRF_test_repeated_stream --tx -r 10");
    ASSERT_EQ(0, status);
    status = std::system("./output/libbladeRF_test_repeated_stream --rx -r 10");
    ASSERT_EQ(0, status);
}

TEST(TEST_LIBBLADERF, quick_retune) {
    status = std::system("./output/libbladeRF_test_quick_retune -i 100");
    ASSERT_EQ(0, status);
}

TEST(TEST_LIBBLADERF, tune_timing) {
    status = std::system("./output/libbladeRF_test_tune_timing -i 200");
    ASSERT_EQ(0, status);
}

TEST(TEST_LIBBLADERF, rx_discont) {
    status = std::system("./output/libbladeRF_test_rx_discont -b 16 -i 1000");
    ASSERT_EQ(0, status);
    status = std::system("./output/libbladeRF_test_rx_discont -b 8 -i 1000");
    ASSERT_EQ(0, status);
}

TEST(TEST_LIBBLADERF, unused_sync) {
    status = std::system("./output/libbladeRF_test_unused_sync");
    ASSERT_EQ(0, status);
}

TEST(TEST_LIBBLADERF, enable_module) {
    status = std::system("./output/libbladeRF_test_ctrl -t enable_module");
    ASSERT_EQ(0, status);
}

TEST(TEST_LIBBLADERF, loopback) {
    status = std::system("./output/libbladeRF_test_ctrl -t loopback");
    ASSERT_EQ(0, status);
}

TEST(TEST_LIBBLADERF, rx_mux) {
    status = std::system("./output/libbladeRF_test_ctrl -t rx_mux");
    ASSERT_EQ(0, status);
}

TEST(TEST_LIBBLADERF, gain) {
    status = std::system("./output/libbladeRF_test_ctrl -t gain");
    ASSERT_EQ(0, status);
}

TEST(TEST_LIBBLADERF, bandwidth) {
    status = std::system("./output/libbladeRF_test_ctrl -t bandwidth --fast");
    ASSERT_EQ(0, status);
}

TEST(TEST_LIBBLADERF, frequency) {
    status = std::system("./output/libbladeRF_test_ctrl -t frequency --fast");
    ASSERT_EQ(0, status);
}

TEST(TEST_LIBBLADERF, samplerate) {
    status = std::system("./output/libbladeRF_test_ctrl -t samplerate --fast");
    ASSERT_EQ(0, status);
}

TEST(TEST_LIBBLADERF, DISABLED_threads) {
    status = std::system("./output/libbladeRF_test_ctrl -t threads --fast");
    ASSERT_EQ(0, status);
}

TEST(TEST_XB200, DISABLED_xb200) {
    status = std::system("./output/libbladeRF_test_ctrl --xb200 -t xb200");
    ASSERT_EQ(0, status);
}

TEST(TEST_LIBBLADERF, tx_onoff_nowsched) {
    status = std::system("./output/libbladeRF_test_timestamps -t tx_onoff_nowsched");
    ASSERT_EQ(0, status);
}

TEST(TEST_LIBBLADERF, print) {
    status = std::system("./output/libbladeRF_test_timestamps -t print -f");
    ASSERT_EQ(0, status);
}

TEST(TEST_LIBBLADERF, rx_gaps) {
    status = std::system("./output/libbladeRF_test_timestamps -t rx_gaps -f");
    ASSERT_EQ(0, status);
}

TEST(TEST_LIBBLADERF, rx_scheduled) {
    status = std::system("./output/libbladeRF_test_timestamps -t rx_scheduled -f");
    ASSERT_EQ(0, status);
}

TEST(TEST_LIBBLADERF, digital_loopback) {
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

TEST(TEST_LIBBLADERF, async) {
    status = std::system("./output/libbladeRF_test_async tx 2048 16");
    ASSERT_EQ(0, status);
    status = std::system("./output/libbladeRF_test_async rx 2048 16");
    ASSERT_EQ(0, status);
}

TEST(TEST_LIBBLADERF, sync) {
    status = std::system("./output/libbladeRF_test_sync --verbosity debug -o temp.bin -c 5000");
    ASSERT_EQ(0, status);
    status = std::system("./output/libbladeRF_test_sync --verbosity debug -i temp.bin -c 5000");
    ASSERT_EQ(0, status);
    status = std::system("rm temp.bin");
    if (status != 0) {
        printf("Failed to remove temp.bin");
    }
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

TEST(TEST_LIBBLADERF, gain_calibration) {
    status = std::system("./output/libbladeRF_test_gain_calibration");
    ASSERT_EQ(0, status);
}

// ===============================
// HDL
// ===============================

class hdl : public ::testing::Test {
protected:
    char build_dir[PATH_MAX];
    void SetUp() override {
        std::string home = std::getenv("HOME");
        fs::path nios2shell_path = "";

        find_file("~/intelFPGA_lite", "nios2_command_shell.sh", &nios2shell_path);
        if (nios2shell_path.empty()) {
            std::cout << "Failed to find nios2_command_shell\n";
        } else if (!fs::exists(fs::path(home) / "intelFPGA_lite/20.1")) {
            std::cout << "[ERROR] Found the nios2_command_shell.sh in an alternate version of Quartus.\n";
            std::cout << "[ERROR] Change the NIOS2SHELL directive accordingly.\n\n";
            std::cout << "    Expected: " << NIOS2SHELL << std::endl;
            std::cout << "    Found:    " << nios2shell_path.string() << "\n\n";
        }

        if (getcwd(build_dir, PATH_MAX) == NULL) {
            perror("Failed to get current directory\n");
        }

        if (chdir(BLADERF_MODELSIM_DIR) != 0) {
            perror("Failed to move to " BLADERF_MODELSIM_DIR "\n");
        }
    }

    void TearDown() override {
        if (chdir(build_dir) != 0) {
            perror("Failed to move out of " BLADERF_MODELSIM_DIR "\n");
        }
    }
};

TEST_F(hdl, vsim_version) {
    status = std::system(NIOS2SHELL" vsim -version");
    EXPECT_EQ(status, 0);
}

class verify : public hdl,
               public testing::WithParamInterface<std::tuple<bool,bool,bool>> {
};

INSTANTIATE_TEST_SUITE_P(hdl, verify,
    testing::Combine(
        testing::Bool(),
        testing::Bool(),
        testing::Bool()),
    [](const testing::TestParamInfo<verify::ParamType>& info) {
        std::string name = absl::StrCat(
            std::get<0>(info.param) ? "8b" : "16b",
            std::get<1>(info.param),
            std::get<2>(info.param));
        absl::c_replace_if(name, [](char c) { return !std::isalnum(c); }, '_');
        return name;
    });

TEST_HDL_COMPILE(compile)
TEST_HDL_COMPILE(fx3_gpif_iq_tb)
TEST_HDL_COMPILE(fx3_gpif_iq_8bit_tb)
TEST_HDL_COMPILE(fx3_gpif_meta_8bit_tb)
TEST_HDL_COMPILE(fx3_gpif_meta_tb)
TEST_HDL_COMPILE(fx3_gpif_packet_tb)
TEST_HDL_COMPILE(fx3_gpif_tb)
TEST_HDL_COMPILE(loopback)
TEST_HDL_COMPILE(rx_counter_8bit_tb)
TEST_HDL_COMPILE(rx_timestamp_tb)

TEST_HDL_VERIFY(fx3_gpif_iq_tb, 16b00, -gENABLE_CHANNEL_0='0' -gENABLE_CHANNEL_1='0', 1000);
TEST_HDL_VERIFY(fx3_gpif_iq_tb, 16b01, -gENABLE_CHANNEL_0='0' -gENABLE_CHANNEL_1='1', 1000);
TEST_HDL_VERIFY(fx3_gpif_iq_tb, 16b10, -gENABLE_CHANNEL_0='1' -gENABLE_CHANNEL_1='0', 1000);
TEST_HDL_VERIFY(fx3_gpif_iq_tb, 16b11, -gENABLE_CHANNEL_0='1' -gENABLE_CHANNEL_1='1', 1000);

TEST_HDL_VERIFY(fx3_gpif_packet_tb, 16b00, -gENABLE_CHANNEL_0='0' -gENABLE_CHANNEL_1='0', 1000);
TEST_HDL_VERIFY(fx3_gpif_packet_tb, 16b01, -gENABLE_CHANNEL_0='0' -gENABLE_CHANNEL_1='1', 1000);
TEST_HDL_VERIFY(fx3_gpif_packet_tb, 16b10, -gENABLE_CHANNEL_0='1' -gENABLE_CHANNEL_1='0', 1000);
TEST_HDL_VERIFY(fx3_gpif_packet_tb, 16b11, -gENABLE_CHANNEL_0='1' -gENABLE_CHANNEL_1='1', 1000);

TEST_HDL_VERIFY(fx3_gpif_tb, 16b00, -gENABLE_CHANNEL_0='0' -gENABLE_CHANNEL_1='0', 1000);
TEST_HDL_VERIFY(fx3_gpif_tb, 16b01, -gENABLE_CHANNEL_0='0' -gENABLE_CHANNEL_1='1', 1000);
TEST_HDL_VERIFY(fx3_gpif_tb, 16b10, -gENABLE_CHANNEL_0='1' -gENABLE_CHANNEL_1='0', 1000);
TEST_HDL_VERIFY(fx3_gpif_tb, 16b11, -gENABLE_CHANNEL_0='1' -gENABLE_CHANNEL_1='1', 1000);

TEST_HDL_VERIFY(fx3_gpif_meta_tb, 16b00, -gENABLE_CHANNEL_0='0' -gENABLE_CHANNEL_1='0', 1000);
TEST_HDL_VERIFY(fx3_gpif_meta_tb, 16b01, -gENABLE_CHANNEL_0='0' -gENABLE_CHANNEL_1='1', 1000);
TEST_HDL_VERIFY(fx3_gpif_meta_tb, 16b10, -gENABLE_CHANNEL_0='1' -gENABLE_CHANNEL_1='0', 1000);
TEST_HDL_VERIFY(fx3_gpif_meta_tb, 16b11, -gENABLE_CHANNEL_0='1' -gENABLE_CHANNEL_1='1', 1000);

TEST_HDL_VERIFY(loopback_tb, 16b00, -gENABLE_CHANNEL_0='0' -gENABLE_CHANNEL_1='0', 1300);
TEST_HDL_VERIFY(loopback_tb, 16b01, -gENABLE_CHANNEL_0='0' -gENABLE_CHANNEL_1='1', 1300);
TEST_HDL_VERIFY(loopback_tb, 16b10, -gENABLE_CHANNEL_0='1' -gENABLE_CHANNEL_1='0', 1300);
TEST_HDL_VERIFY(loopback_tb, 16b11, -gENABLE_CHANNEL_0='1' -gENABLE_CHANNEL_1='1', 1300);

TEST_P(verify, fx3_gpif_iq_8bit_tb) {
    std::string bitmode  = std::get<0>(GetParam()) ? "1" : "0";
    std::string channel0 = std::get<1>(GetParam()) ? "1" : "0";
    std::string channel1 = std::get<2>(GetParam()) ? "1" : "0";

    std::string command = NIOS2SHELL;
    command += " vsim -c -do \"do test.do vsim nuand.fx3_gpif_iq_8bit_tb ";
    command += "-gEIGHT_BIT_MODE_EN='" + bitmode + "' ";
    command += "-gENABLE_CHANNEL_0='" + channel0 + "' ";
    command += "-gENABLE_CHANNEL_1='" + channel1 + "' ";
    command += "1000\"";

    status = std::system(command.c_str());
    EXPECT_EQ(status, 0);
}

TEST_P(verify, fx3_gpif_meta_8bit_tb) {
    std::string bitmode  = std::get<0>(GetParam()) ? "1" : "0";
    std::string channel0 = std::get<1>(GetParam()) ? "1" : "0";
    std::string channel1 = std::get<2>(GetParam()) ? "1" : "0";

    std::string command = NIOS2SHELL;
    command += " vsim -c -do \"do test.do vsim nuand.fx3_gpif_meta_8bit_tb ";
    command += "-gEIGHT_BIT_MODE_EN='" + bitmode + "' ";
    command += "-gENABLE_CHANNEL_0='" + channel0 + "' ";
    command += "-gENABLE_CHANNEL_1='" + channel1 + "' ";
    command += "1000\"";

    status = std::system(command.c_str());
    EXPECT_EQ(status, 0);
}

TEST_P(verify, rx_counter_8bit_tb) {
    std::string bitmode  = std::get<0>(GetParam()) ? "1" : "0";
    std::string channel0 = std::get<1>(GetParam()) ? "1" : "0";
    std::string channel1 = std::get<2>(GetParam()) ? "1" : "0";

    std::string command = NIOS2SHELL;
    command += " vsim -c -do \"do test.do vsim nuand.rx_counter_8bit_tb ";
    command += "-gEIGHT_BIT_MODE_EN='" + bitmode + "' ";
    command += "-gENABLE_CHANNEL_0='" + channel0 + "' ";
    command += "-gENABLE_CHANNEL_1='" + channel1 + "' ";
    command += "1000\"";

    status = std::system(command.c_str());
    EXPECT_EQ(status, 0);
}

TEST_P(verify, rx_timestamp_tb) {
    std::string bitmode  = std::get<0>(GetParam()) ? "1" : "0";
    std::string channel0 = std::get<1>(GetParam()) ? "1" : "0";
    std::string channel1 = std::get<2>(GetParam()) ? "1" : "0";

    std::string command = NIOS2SHELL;
    command += " vsim -c -do \"do test.do vsim nuand.rx_timestamp_tb ";
    command += "-gEIGHT_BIT_MODE_EN='" + bitmode + "' ";
    command += "-gENABLE_CHANNEL_0='" + channel0 + "' ";
    command += "-gENABLE_CHANNEL_1='" + channel1 + "' ";
    command += "900\"";

    status = std::system(command.c_str());
    EXPECT_EQ(status, 0);
}

// ===============================
// Synthesis
// ===============================

const std::vector<std::tuple<std::string, std::string, std::string>> fpga_images_defaults = {
    {"bladeRF",         "hosted",       "40"},
    {"bladeRF",         "hosted",       "115"},
    {"bladeRF-micro",   "hosted",       "A4"},
    {"bladeRF-micro",   "hosted",       "A9"},
    {"bladeRF-micro",   "wlan",         "A9"}
};

const std::vector<std::tuple<std::string, std::string, std::string>> fpga_images_all = {
    {"bladeRF",         "hosted",       "40"},
    {"bladeRF",         "hosted",       "115"},
    {"bladeRF",         "adsb",         "40"},
    {"bladeRF",         "adsb",         "115"},
    {"bladeRF",         "atsc_tx",      "40"},
    {"bladeRF",         "atsc_tx",      "115"},
    {"bladeRF",         "fsk_bridge",   "40"},
    {"bladeRF",         "fsk_bridge",   "115"},
    {"bladeRF",         "headless",     "40"},
    {"bladeRF",         "headless",     "115"},
    {"bladeRF",         "qpsk_tx",      "40"},
    {"bladeRF",         "qpsk_tx",      "115"},
    {"bladeRF-micro",   "hosted",       "A4"},
    {"bladeRF-micro",   "hosted",       "A9"},
    {"bladeRF-micro",   "adsb",         "A4"},
    {"bladeRF-micro",   "adsb",         "A9"},
    {"bladeRF-micro",   "foxhunt",      "A4"},
    {"bladeRF-micro",   "foxhunt",      "A9"},
    {"bladeRF-micro",   "wlan",         "A9"}
};

class synth: public ::testing::TestWithParam<std::tuple<std::string, std::string, std::string>> {
    char build_dir[PATH_MAX];
    void SetUp() override {
        std::string home = std::getenv("HOME");
        fs::path nios2shell_path = "";

        find_file("~/intelFPGA_lite", "nios2_command_shell.sh", &nios2shell_path);
        if (nios2shell_path.empty()) {
            std::cout << "Failed to find nios2_command_shell\n";
        } else if (!fs::exists(fs::path(home) / "intelFPGA_lite/20.1")) {
            std::cout << "[ERROR] Found the nios2_command_shell.sh in an alternate version of Quartus.\n";
            std::cout << "[ERROR] Change the NIOS2SHELL directive accordingly.\n\n";
            std::cout << "    Expected: " << NIOS2SHELL << std::endl;
            std::cout << "    Found:    " << nios2shell_path.string() << "\n\n";
        }

        if (getcwd(build_dir, PATH_MAX) == NULL) {
            perror("Failed to get current directory\n");
        }

        if (chdir(BLADERF_QUARTUS_DIR) != 0) {
            perror("Failed to move to " BLADERF_QUARTUS_DIR"\n");
        }
    }

    void TearDown() override {
        if (chdir(build_dir) != 0) {
            perror("Failed to move out of " BLADERF_QUARTUS_DIR"\n");
        }
    }
};

TEST_P(synth, synthesis) {
    std::string board    = std::get<0>(GetParam());
    std::string revision = std::get<1>(GetParam());
    std::string size     = std::get<2>(GetParam());

    std::string command = NIOS2SHELL;
    command += " ./build_bladerf.sh ";
    command += " -b " + board;
    command += " -r " + revision;
    command += " -s " + size;

    status = std::system(command.c_str());
    EXPECT_EQ(status, 0);
}

INSTANTIATE_TEST_SUITE_P(synthesize_defaults, synth,
    ::testing::ValuesIn(fpga_images_defaults),
    [](const testing::TestParamInfo<synth::ParamType>& info) {
        std::string name = absl::StrCat(
            std::get<0>(info.param), "_",
            std::get<1>(info.param), "_",
            std::get<2>(info.param));
        absl::c_replace_if(name, [](char c) { return !std::isalnum(c); }, '_');
        return name;
    });

INSTANTIATE_TEST_SUITE_P(DISABLED_synthesize_all, synth,
    ::testing::ValuesIn(fpga_images_all),
    [](const testing::TestParamInfo<synth::ParamType>& info) {
        std::string name = absl::StrCat(
            std::get<0>(info.param), "_",
            std::get<1>(info.param), "_",
            std::get<2>(info.param));
        absl::c_replace_if(name, [](char c) { return !std::isalnum(c); }, '_');
        return name;
    });

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
