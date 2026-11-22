/*
 * test_1r1t_meta: checks the RX metadata timestamps in 1R1T and 2R2T mode on
 * a bladeRF 2.0 micro.
 *
 * RX1 or RX2 streams the FPGA's 32-bit counter with metadata, and the test reads one
 * message's samples at a time "now": SC8_Q7_META at 122.88 Msps with
 * OVERSAMPLE (bits 15..0 of the counter in I and Q, 4088 samples per message)
 * or SC16_Q11_META at 61.44 Msps (bits 15..0 in I, 2044 samples per message).
 * The counter and the timestamp both count samples, so in each message the
 * counter must count up, and the timestamp minus the counter (modulo 2^16)
 * must be the same in every message. Overruns at the start of a stream drop
 * whole messages and do not change that difference. Receive only.
 */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <libbladeRF.h>
#include "log.h"
#include "test_common.h"

void print_usage(const char *prog_name)
{
    printf("Usage: %s [-h] [-v] [-c channel] [-m mode] [-f format] [-n reads]\n", prog_name);
    printf("  -h    Show this help message\n");
    printf("  -v    Enable verbose logging\n");
    printf("  -c    RX channel (1 or 2, default: 1)\n");
    printf("  -m    1R1T mode selection: auto (default), off or on\n");
    printf("  -f    sc8 (default; 122.88 Msps with OVERSAMPLE) or sc16 (61.44 Msps)\n");
    printf("  -n    Number of messages to read (default: 32)\n");
}

int main(int argc, char *argv[])
{
    int status = 0;
    struct bladerf *dev = NULL;
    bladerf_channel channel = BLADERF_CHANNEL_RX(0);
    bladerf_rfic_1r1t_mode mode = BLADERF_RFIC_1R1T_AUTO;
    bool sc16 = false;
    unsigned int num_reads = 32;
    unsigned int failures = 0;
    bool r1t = false;
    int8_t *samples = NULL;
    uint16_t offset = 0;
    unsigned int num_samples, k;
    int opt;
    int opt_ind = 0;

    struct option longopts[] = {
        {"help", no_argument, 0, 'h'},
        {"verbose", no_argument, 0, 'v'},
        {"channel", required_argument, 0, 'c'},
        {"mode", required_argument, 0, 'm'},
        {"format", required_argument, 0, 'f'},
        {"reads", required_argument, 0, 'n'},
        {0, 0, 0, 0}
    };

    bladerf_log_set_verbosity(BLADERF_LOG_LEVEL_WARNING);

    while ((opt = getopt_long(argc, argv, "hvc:m:f:n:", longopts, &opt_ind)) != -1) {
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
            case 'f':
                if (strcmp(optarg, "sc8") == 0) {
                    sc16 = false;
                } else if (strcmp(optarg, "sc16") == 0) {
                    sc16 = true;
                } else {
                    print_usage(argv[0]);
                    return EXIT_FAILURE;
                }
                break;
            case 'n':
                num_reads = (unsigned int)atoi(optarg);
                if (num_reads < 2) {
                    print_usage(argv[0]);
                    return EXIT_FAILURE;
                }
                break;
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    num_samples = sc16 ? 2044 : 4088;
    samples     = malloc(num_samples * 2 * sizeof(int16_t));
    if (samples == NULL) {
        return EXIT_FAILURE;
    }

    CHECK_STATUS(bladerf_open(&dev, NULL));

    if (!sc16) {
        CHECK_STATUS(bladerf_enable_feature(dev, BLADERF_FEATURE_OVERSAMPLE, true));
    }

    CHECK_STATUS(bladerf_set_rfic_1r1t_mode(dev, mode));
    CHECK_STATUS(bladerf_set_sample_rate(dev, channel,
                                         sc16 ? 61440000 : 122880000, NULL));
    CHECK_STATUS(bladerf_set_rx_mux(dev, BLADERF_RX_MUX_32BIT_COUNTER));
    CHECK_STATUS(bladerf_sync_config(dev, BLADERF_RX_X1,
                                     sc16 ? BLADERF_FORMAT_SC16_Q11_META
                                          : BLADERF_FORMAT_SC8_Q7_META,
                                     256, 32768, 32, 3500));
    CHECK_STATUS(bladerf_enable_module(dev, channel, true));
    CHECK_STATUS(bladerf_get_rfic_1r1t(dev, &r1t));

    printf("RX%d, %s, %s, FPGA counter, reads of %u samples \"now\"\n",
           (channel == BLADERF_CHANNEL_RX(0)) ? 1 : 2, r1t ? "1R1T" : "2R2T",
           sc16 ? "SC16_Q11_META, 61.44 Msps"
                : "SC8_Q7_META, 122.88 Msps (OVERSAMPLE)",
           num_samples);

    for (k = 0; k < num_reads; k++) {
        struct bladerf_metadata meta;
        uint16_t first = 0;
        uint16_t last  = 0;
        uint16_t diff;
        unsigned int jumps = 0;
        unsigned int i;
        bool ok;

        memset(&meta, 0, sizeof(meta));
        meta.flags = BLADERF_META_FLAG_RX_NOW;
        CHECK_STATUS(bladerf_sync_rx(dev, samples, num_samples, &meta, 3500));

        for (i = 0; i < meta.actual_count; i++) {
            uint16_t value;

            if (sc16) {
                value = (uint16_t)((int16_t *)samples)[2 * i];
            } else {
                value = (uint8_t)samples[2 * i] |
                        ((uint16_t)(uint8_t)samples[2 * i + 1] << 8);
            }

            if (i == 0) {
                first = value;
            } else if (value != (uint16_t)(last + 1)) {
                jumps++;
            }

            last = value;
        }

        diff = (uint16_t)(meta.timestamp - first);
        if (k == 0) {
            offset = diff;
        }

        ok = meta.actual_count > 0 && jumps == 0 && diff == offset;
        failures += ok ? 0 : 1;

        printf("read %2u: timestamp %10" PRIu64 ", %u samples%s, counter "
               "0x%04x..0x%04x, %u jumps, timestamp - counter 0x%04x: %s\n",
               k, meta.timestamp, meta.actual_count,
               (meta.status & BLADERF_META_STATUS_OVERRUN) ? " (overrun)" : "",
               first, last, jumps, diff, ok ? "pass" : "FAIL");
    }

error:
    if (dev) {
        bladerf_enable_module(dev, channel, false);
        bladerf_set_rx_mux(dev, BLADERF_RX_MUX_BASEBAND);
        bladerf_close(dev);
    }

    free(samples);

    if (status != 0) {
        printf("RESULT: ERROR\n");
        return EXIT_FAILURE;
    }

    printf("RESULT: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
