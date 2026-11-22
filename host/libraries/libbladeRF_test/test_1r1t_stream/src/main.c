/*
 * test_1r1t_stream: checks that one RX channel of a bladeRF 2.0 micro streams
 * every sample at 122.88 Msps in 1R1T mode.
 *
 * With OVERSAMPLE and SC8_Q7, RX1 or RX2 streams the FPGA's 32-bit counter (bits
 * 15..0 in I and Q) through the 1R1T path of the FPGA, the USB link and
 * libbladeRF. After a short start, every sample must be the previous one plus
 * one, and the samples must arrive at 122.88 Msps (1 % tolerance). With the
 * 1R1T mode selection auto or on, the interface must be in 1R1T mode. With
 * -n N, the stream is configured, enabled, checked and disabled N times in one
 * session, and every cycle must pass. Receive only.
 */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <getopt.h>
#if !defined(_WIN32)
#include <unistd.h>
#else
#include <windows.h>
#include <clock_gettime.h>
#endif
#include <libbladeRF.h>
#include "log.h"
#include "test_common.h"

#if defined(_WIN32)
#define TIMING_CLOCK CLOCK_REALTIME
#else
#define TIMING_CLOCK CLOCK_MONOTONIC
#endif

#define SAMPLE_RATE 122880000
#define READ_SAMPLES (1 << 18)
#define WARMUP_READS 64 /* about 0.14 s */
#define MAX_REPORTS 10

void print_usage(const char *prog_name)
{
    printf("Usage: %s [-h] [-v] [-c channel] [-t seconds] [-m mode] [-n cycles]\n",
           prog_name);
    printf("  -h    Show this help message\n");
    printf("  -v    Enable verbose logging\n");
    printf("  -c    RX channel (1 or 2, default: 1)\n");
    printf("  -t    Seconds to check in each cycle (default: 10)\n");
    printf("  -m    1R1T mode selection: auto (default), off or on\n");
    printf("  -n    Stream cycles (enable, check, disable) in one session (default: 1)\n");
}

static double seconds_since(const struct timespec *start)
{
    struct timespec now;

    clock_gettime(TIMING_CLOCK, &now);
    return (now.tv_sec - start->tv_sec) + (now.tv_nsec - start->tv_nsec) * 1e-9;
}

int main(int argc, char *argv[])
{
    int status = 0;
    struct bladerf *dev = NULL;
    bladerf_channel channel = BLADERF_CHANNEL_RX(0);
    bladerf_rfic_1r1t_mode mode = BLADERF_RFIC_1R1T_AUTO;
    double seconds = 10.0;
    unsigned int num_cycles = 1;
    unsigned int cycle, failed_cycles = 0;
    int8_t *samples = NULL;
    uint64_t num_checked;
    uint64_t jumps;
    uint16_t expected;
    bool started;
    bool r1t = false;
    bool enabled = false;
    bool ok_mode, ok_rate;
    struct timespec start;
    double elapsed, rate;
    unsigned int i, n;
    int opt;
    int opt_ind = 0;

    struct option longopts[] = {
        {"help", no_argument, 0, 'h'},
        {"verbose", no_argument, 0, 'v'},
        {"channel", required_argument, 0, 'c'},
        {"time", required_argument, 0, 't'},
        {"mode", required_argument, 0, 'm'},
        {"cycles", required_argument, 0, 'n'},
        {0, 0, 0, 0}
    };

    bladerf_log_set_verbosity(BLADERF_LOG_LEVEL_WARNING);

    while ((opt = getopt_long(argc, argv, "hvc:t:m:n:", longopts, &opt_ind)) != -1) {
        switch (opt) {
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            case 'v':
                bladerf_log_set_verbosity(BLADERF_LOG_LEVEL_VERBOSE);
                break;
            case 'c':
                if (strcmp(optarg, "1") != 0 && strcmp(optarg, "2") != 0) {
                    print_usage(argv[0]);
                    return EXIT_FAILURE;
                }
                channel = BLADERF_CHANNEL_RX(atoi(optarg) - 1);
                break;
            case 't':
                seconds = atof(optarg);
                if (seconds <= 0) {
                    print_usage(argv[0]);
                    return EXIT_FAILURE;
                }
                break;
            case 'm':
                if (strcmp(optarg, "auto") == 0) {
                    mode = BLADERF_RFIC_1R1T_AUTO;
                } else if (strcmp(optarg, "off") == 0) {
                    mode = BLADERF_RFIC_1R1T_OFF;
                } else if (strcmp(optarg, "on") == 0) {
                    mode = BLADERF_RFIC_1R1T_ON;
                } else {
                    print_usage(argv[0]);
                    return EXIT_FAILURE;
                }
                break;
            case 'n':
                num_cycles = (unsigned int)atoi(optarg);
                if (num_cycles == 0) {
                    print_usage(argv[0]);
                    return EXIT_FAILURE;
                }
                break;
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    samples = malloc(READ_SAMPLES * 2);
    if (samples == NULL) {
        return EXIT_FAILURE;
    }

    CHECK_STATUS(bladerf_open(&dev, NULL));
    CHECK_STATUS(bladerf_enable_feature(dev, BLADERF_FEATURE_OVERSAMPLE, true));
    CHECK_STATUS(bladerf_set_rfic_1r1t_mode(dev, mode));
    CHECK_STATUS(bladerf_set_sample_rate(dev, channel, SAMPLE_RATE, NULL));
    CHECK_STATUS(bladerf_set_rx_mux(dev, BLADERF_RX_MUX_32BIT_COUNTER));

    for (cycle = 0; cycle < num_cycles; cycle++) {
        num_checked = 0;
        jumps       = 0;
        expected    = 0;
        started     = false;

        /* Disabling the last RX channel ends the sync stream, so every cycle
         * configures it again */
        CHECK_STATUS(bladerf_sync_config(dev, BLADERF_RX_X1, BLADERF_FORMAT_SC8_Q7,
                                         256, 32768, 32, 1000));
        CHECK_STATUS(bladerf_enable_module(dev, channel, true));
        enabled = true;
        CHECK_STATUS(bladerf_get_rfic_1r1t(dev, &r1t));

        if (num_cycles > 1) {
            printf("cycle %u: ", cycle + 1);
        }

        printf("RX%d, OVERSAMPLE, %.2f Msps, SC8_Q7, FPGA counter, %s mode, %.0f s\n",
               (channel == BLADERF_CHANNEL_RX(0)) ? 1 : 2, SAMPLE_RATE / 1e6,
               r1t ? "1R1T" : "2R2T", seconds);

        /* Skip the start, where libbladeRF can deliver buffers out of order */
        for (n = 0; n < WARMUP_READS; n++) {
            CHECK_STATUS(bladerf_sync_rx(dev, samples, READ_SAMPLES, NULL, 1000));
        }

        clock_gettime(TIMING_CLOCK, &start);

        do {
            CHECK_STATUS(bladerf_sync_rx(dev, samples, READ_SAMPLES, NULL, 1000));

            for (i = 0; i < READ_SAMPLES; i++) {
                uint16_t value = (uint8_t)samples[2 * i] |
                                 ((uint16_t)(uint8_t)samples[2 * i + 1] << 8);

                if (started && value != expected) {
                    if (jumps < MAX_REPORTS) {
                        printf("  sample %" PRIu64 ": expected 0x%04x, got 0x%04x\n",
                               num_checked + i, expected, value);
                    }
                    jumps++;
                }

                started  = true;
                expected = value + 1;
            }

            num_checked += READ_SAMPLES;
            elapsed = seconds_since(&start);
        } while (elapsed < seconds);

        rate    = num_checked / elapsed;
        ok_mode = mode == BLADERF_RFIC_1R1T_OFF || r1t;
        ok_rate = rate > 0.99 * SAMPLE_RATE && rate < 1.01 * SAMPLE_RATE;

        printf("%" PRIu64 " samples in %.2f s (%.3f Msps), %" PRIu64 " jumps\n",
               num_checked, elapsed, rate / 1e6, jumps);

        if (!ok_mode) {
            printf("the interface is not in 1R1T mode\n");
        }

        if (!ok_rate) {
            printf("the rate is not within 1 %% of %.2f Msps\n", SAMPLE_RATE / 1e6);
        }

        if (!ok_mode || !ok_rate || jumps != 0) {
            failed_cycles++;
        }

        CHECK_STATUS(bladerf_enable_module(dev, channel, false));
        enabled = false;
    }

    if (num_cycles > 1) {
        printf("%u of %u cycles failed\n", failed_cycles, num_cycles);
    }

    if (failed_cycles != 0) {
        status = 1;
    }

error:
    if (dev) {
        if (enabled) {
            bladerf_enable_module(dev, channel, false);
        }
        bladerf_set_rx_mux(dev, BLADERF_RX_MUX_BASEBAND);
        bladerf_close(dev);
    }

    free(samples);

    printf("RESULT: %s\n", status == 0 ? "PASS" : (status > 0 ? "FAIL" : "ERROR"));
    return status == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
