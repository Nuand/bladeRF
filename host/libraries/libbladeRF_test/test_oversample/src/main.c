#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <getopt.h>
#if !defined(_WIN32)
#include <unistd.h>  // for getopt
#else
#include <windows.h>
#include <clock_gettime.h>
#endif
#include <libbladeRF.h>
#include <math.h>
#include "log.h"
#include "test_common.h"
#include "libbladeRF.h"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_RESET   "\x1b[0m"

/* Use CLOCK_MONOTONIC as a portable alternative that works on FreeBSD and Linux */
#if defined(_WIN32)
/* On Windows, only CLOCK_REALTIME is supported by our implementation */
#define TIMING_CLOCK CLOCK_REALTIME
#else
#ifndef CLOCK_MONOTONIC_PRECISE
#ifdef __FreeBSD__
/* FreeBSD defines CLOCK_MONOTONIC_PRECISE for higher precision */
#define TIMING_CLOCK CLOCK_MONOTONIC_PRECISE
#elif defined(CLOCK_MONOTONIC_RAW) && defined(__linux__)
/* Linux has CLOCK_MONOTONIC_RAW which is not affected by NTP adjustments */
#define TIMING_CLOCK CLOCK_MONOTONIC_RAW
#else
/* Fallback to CLOCK_MONOTONIC which is POSIX standard */
#define TIMING_CLOCK CLOCK_MONOTONIC
#endif
#else
#define TIMING_CLOCK CLOCK_MONOTONIC_PRECISE
#endif
#endif

void print_usage(const char *prog_name)
{
    printf("Usage: %s [-h] [-v]\n", prog_name);
    printf("  -h    Show this help message\n");
    printf("  -v    Enable verbose logging\n");
}

int main(int argc, char *argv[])
{
    int status = 0;
    struct bladerf *dev = NULL;
    int opt;
    int opt_ind = 0;
    bladerf_log_set_verbosity(BLADERF_LOG_LEVEL_INFO);

    bladerf_frequency freq = 915e6;
    bladerf_sample_rate samp_rate = 122.88e6;

    const unsigned int num_buffers = 64;
    const unsigned int buffer_size = 16384;
    const unsigned int num_transfers = 16;
    const unsigned int timeout_ms = 1000;

    struct timespec start, end;

    struct option longopts[] = {
        {"help", no_argument, 0, 'h'},
        {"verbose", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "hv", longopts, &opt_ind)) != -1) {
        switch (opt) {
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            case 'v':
                bladerf_log_set_verbosity(BLADERF_LOG_LEVEL_VERBOSE);
                break;
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    const unsigned int num_samples = 1*samp_rate; // 1s of samples
    int8_t *samples = (int8_t *)malloc(num_samples * sizeof(int16_t));
    double power = 0;

    CHECK_STATUS(bladerf_open(&dev, NULL));
    CHECK_STATUS(bladerf_set_frequency(dev, BLADERF_CHANNEL_RX(0), freq));
    printf("Frequency: %.2f MHz\n", freq / 1e6);
    CHECK_STATUS(bladerf_enable_feature(dev, BLADERF_FEATURE_OVERSAMPLE, true));
    CHECK_STATUS(bladerf_sync_config(dev, BLADERF_RX_X1, BLADERF_FORMAT_SC8_Q7,
        num_buffers, buffer_size, num_transfers, timeout_ms));

    // Not only sets the sample rate, but also applies the oversample register configuration
    CHECK_STATUS(bladerf_set_sample_rate(dev, BLADERF_CHANNEL_RX(0), samp_rate, NULL));
    printf("Sample Rate: %.2f MHz\n", samp_rate / 1e6);

    // Collect samples
    CHECK_STATUS(bladerf_enable_module(dev, BLADERF_MODULE_RX, true));
    CHECK_STATUS(clock_gettime(TIMING_CLOCK, &start));
    CHECK_STATUS(bladerf_sync_rx(dev, samples, num_samples, NULL, 0));
    CHECK_STATUS(clock_gettime(TIMING_CLOCK, &end));
    CHECK_STATUS(bladerf_enable_module(dev, BLADERF_MODULE_RX, false));

    const double expected_sync_time = (double)num_samples / samp_rate;
    double time_taken = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
    printf("Time taken: %f seconds\n", time_taken);
    printf("Expected sync time: %f seconds\n", expected_sync_time);
    printf("%% error: %f\n", (time_taken - expected_sync_time) / expected_sync_time * 100);

    // Throw error if sync time is outside 5% error margin
    double error_percent = (time_taken - expected_sync_time) / expected_sync_time * 100;
    double tolerance = 1.0; // 1% tolerance
    if (error_percent > tolerance || error_percent < -tolerance) {
        printf(ANSI_COLOR_RED "Sync time error: %f%%" ANSI_COLOR_RESET "\n", error_percent);
        status = 1;
    }

    // Calculate power
    for (size_t i = 0; i < num_samples/2; i++) {
        double i_sample = (double)samples[2*i];
        double q_sample = (double)samples[2*i+1];
        power += i_sample * i_sample + q_sample * q_sample;
    }
    double avg_power = power / num_samples;
    const double MAX_POWER = (INT8_MAX * INT8_MAX) + (INT8_MAX * INT8_MAX);
    double dBFS = 20 * log10(avg_power/MAX_POWER);
    printf(ANSI_COLOR_GREEN "Avg Power: %fdBFS" ANSI_COLOR_RESET "\n" , dBFS);

error:
    if (dev) bladerf_close(dev);
    return (status == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
