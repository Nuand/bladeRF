/*
 * test_1r1t_state: checks how libbladeRF chooses the RFIC digital interface
 * mode (bladerf_set_rfic_1r1t_mode()) on a bladeRF 2.0 micro.
 *
 * Bits 7:6 of the AD9361 registers 0x003 (RX) and 0x002 (TX) show the mode:
 * 01 is 1R1T mode on channel 1, 10 is 1R1T mode on channel 2, 11 is 2R2T
 * mode, and bladerf_get_rfic_1r1t() must agree. RX1 and RX2 stream from the
 * FPGA's counter for a moment. TX1 and TX2 are enabled for a moment without
 * samples: run it only with both TX ports terminated.
 *
 * With an FPGA image older than v0.17.0, the test checks that AUTO stays in
 * 2R2T mode and that ON is refused; nothing is transmitted then.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <libbladeRF.h>
#include "log.h"
#include "test_common.h"

#define RATE_OVERSAMPLE 122880000
#define RATE_DEFAULT 61440000

static unsigned int failures = 0;

void print_usage(const char *prog_name)
{
    printf("Usage: %s [-h] [-v]\n", prog_name);
    printf("  -h    Show this help message\n");
    printf("  -v    Enable verbose logging\n");
}

/* AD9361 channel enable bits (7:6 of 0x003 and 0x002) */
static uint8_t channel_bits(bool r1t, int ch)
{
    if (!r1t) {
        return 0xc0;
    }

    return (ch == 1) ? 0x40 : 0x80;
}

/* Check that the interface is in 1R1T mode (r1t) on RX channel rx_ch and TX
 * channel tx_ch (1 or 2), or in 2R2T mode */
static int expect_mode(struct bladerf *dev, const char *step, bool r1t,
                       int rx_ch, int tx_ch)
{
    int status;
    uint8_t rx   = 0;
    uint8_t tx   = 0;
    bool enabled = !r1t;
    bool ok;

    CHECK_STATUS(bladerf_get_rfic_register(dev, 0x003, &rx));
    CHECK_STATUS(bladerf_get_rfic_register(dev, 0x002, &tx));
    CHECK_STATUS(bladerf_get_rfic_1r1t(dev, &enabled));

    ok = enabled == r1t && (rx & 0xc0) == channel_bits(r1t, rx_ch) &&
         (tx & 0xc0) == channel_bits(r1t, tx_ch);
    printf("%-50s %s: 0x003 = 0x%02x, 0x002 = 0x%02x: %s\n", step,
           enabled ? "1R1T" : "2R2T", rx, tx, ok ? "pass" : "FAIL");
    failures += ok ? 0 : 1;

error:
    return status;
}

static void expect_status(const char *step, int actual, int expected)
{
    bool ok = actual == expected;

    printf("%-50s returns %d (%s): %s\n", step, actual,
           bladerf_strerror(actual), ok ? "pass" : "FAIL");
    failures += ok ? 0 : 1;
}

static void expect_mode_selection(struct bladerf *dev, const char *step,
                                  bladerf_rfic_1r1t_mode wanted)
{
    bladerf_rfic_1r1t_mode mode = BLADERF_RFIC_1R1T_ON;
    int status = bladerf_get_rfic_1r1t_mode(dev, &mode);
    bool ok    = status == 0 && mode == wanted;

    printf("%-50s mode %d: %s\n", step, (int)mode, ok ? "pass" : "FAIL");
    failures += ok ? 0 : 1;
}

/* Stream ch from the FPGA's 32-bit counter for a moment (SC8_Q7: bits 15..0
 * in I and Q) and check that every sample after the start counts up */
static int check_counter(struct bladerf *dev, bladerf_channel ch,
                         const char *step)
{
    int status;
    const unsigned int num_samples = 1 << 18;
    const unsigned int num_reads   = 128; /* 0.27 s at 122.88 Msps */
    int8_t *samples = malloc(2 * num_samples);
    unsigned int jumps = 0;
    bool started = false;
    uint16_t expected = 0;
    unsigned int i, n;

    if (samples == NULL) {
        return BLADERF_ERR_MEM;
    }

    CHECK_STATUS(bladerf_set_rx_mux(dev, BLADERF_RX_MUX_32BIT_COUNTER));
    CHECK_STATUS(bladerf_sync_config(dev, BLADERF_RX_X1, BLADERF_FORMAT_SC8_Q7,
                                     256, 32768, 32, 1000));
    CHECK_STATUS(bladerf_enable_module(dev, ch, true));

    /* Skip the start, where libbladeRF can deliver buffers out of order */
    for (n = 0; n < 32; n++) {
        CHECK_STATUS(bladerf_sync_rx(dev, samples, num_samples, NULL, 1000));
    }

    for (n = 0; n < num_reads; n++) {
        CHECK_STATUS(bladerf_sync_rx(dev, samples, num_samples, NULL, 1000));

        for (i = 0; i < num_samples; i++) {
            uint16_t value = (uint8_t)samples[2 * i] |
                             ((uint16_t)(uint8_t)samples[2 * i + 1] << 8);

            if (started && value != expected) {
                jumps++;
            }

            started  = true;
            expected = value + 1;
        }
    }

    printf("%-50s %u samples, %u jumps: %s\n", step, num_samples * num_reads,
           jumps, jumps == 0 ? "pass" : "FAIL");
    failures += jumps == 0 ? 0 : 1;

error:
    bladerf_enable_module(dev, ch, false);
    bladerf_set_rx_mux(dev, BLADERF_RX_MUX_BASEBAND);
    free(samples);
    return status;
}

static int test_unsupported(struct bladerf *dev)
{
    int status;

    CHECK_STATUS(bladerf_enable_feature(dev, BLADERF_FEATURE_OVERSAMPLE, true));
    CHECK_STATUS(bladerf_set_sample_rate(dev, BLADERF_CHANNEL_RX(0),
                                         RATE_OVERSAMPLE, NULL));
    CHECK_STATUS(expect_mode(dev, "AUTO, OVERSAMPLE, 122.88 Msps", false, 1, 1));

    expect_status("ON", bladerf_set_rfic_1r1t_mode(dev, BLADERF_RFIC_1R1T_ON),
                  BLADERF_ERR_UNSUPPORTED);
    CHECK_STATUS(expect_mode(dev, "after ON was refused", false, 1, 1));

    expect_status("OFF", bladerf_set_rfic_1r1t_mode(dev, BLADERF_RFIC_1R1T_OFF),
                  0);
    expect_status("AUTO", bladerf_set_rfic_1r1t_mode(dev, BLADERF_RFIC_1R1T_AUTO),
                  0);

error:
    return status;
}

/* Enable ch for a moment and check the mode while it is on */
static int expect_enabled(struct bladerf *dev, bladerf_channel ch,
                          const char *step, bool r1t, int rx_ch, int tx_ch)
{
    int status;

    CHECK_STATUS(bladerf_enable_module(dev, ch, true));
    status = expect_mode(dev, step, r1t, rx_ch, tx_ch);
    bladerf_enable_module(dev, ch, false);

error:
    return status;
}

static int test_supported(struct bladerf **devp)
{
    int status;
    struct bladerf *dev = *devp;
    const bladerf_channel rx1 = BLADERF_CHANNEL_RX(0);
    const bladerf_channel rx2 = BLADERF_CHANNEL_RX(1);
    const bladerf_channel tx1 = BLADERF_CHANNEL_TX(0);
    const bladerf_channel tx2 = BLADERF_CHANNEL_TX(1);

    /* AUTO at the OVERSAMPLE rates: channel 1 while nothing is enabled */
    CHECK_STATUS(bladerf_enable_feature(dev, BLADERF_FEATURE_OVERSAMPLE, true));
    CHECK_STATUS(bladerf_set_sample_rate(dev, rx1, RATE_OVERSAMPLE, NULL));
    CHECK_STATUS(expect_mode(dev, "AUTO, OVERSAMPLE, 122.88 Msps", true, 1, 1));

    CHECK_STATUS(bladerf_set_bandwidth(dev, rx1, 56000000, NULL));
    CHECK_STATUS(expect_mode(dev, "after an RX bandwidth of 56 MHz", true, 1, 1));

    CHECK_STATUS(bladerf_set_frequency(dev, rx1, 2400000000));
    CHECK_STATUS(expect_mode(dev, "after an RX frequency of 2.4 GHz", true, 1, 1));

    CHECK_STATUS(bladerf_set_gain_mode(dev, rx1, BLADERF_GAIN_MGC));
    CHECK_STATUS(bladerf_set_gain(dev, rx1, 20));
    CHECK_STATUS(expect_mode(dev, "after an RX gain of 20 dB", true, 1, 1));

    /* AUTO, one channel of each direction, on either channel */
    CHECK_STATUS(check_counter(dev, rx2, "AUTO, RX2 streams the counter"));
    CHECK_STATUS(expect_mode(dev, "after RX2 was enabled", true, 2, 1));

    CHECK_STATUS(check_counter(dev, rx1, "AUTO, RX1 streams the counter"));
    CHECK_STATUS(expect_mode(dev, "after RX1 was enabled", true, 1, 1));

    CHECK_STATUS(expect_enabled(dev, tx2, "AUTO, TX2 on", true, 1, 2));
    CHECK_STATUS(expect_enabled(dev, tx1, "AUTO, TX1 on", true, 1, 1));

    CHECK_STATUS(bladerf_enable_module(dev, rx2, true));
    CHECK_STATUS(expect_enabled(dev, tx1, "AUTO, RX2 and TX1 on", true, 2, 1));
    CHECK_STATUS(expect_enabled(dev, tx2, "AUTO, RX2 and TX2 on", true, 2, 2));
    CHECK_STATUS(bladerf_enable_module(dev, rx2, false));

    /* AUTO, two channels of one direction: 2R2T mode */
    CHECK_STATUS(bladerf_enable_module(dev, rx1, true));
    CHECK_STATUS(expect_enabled(dev, rx2, "AUTO, RX1 and RX2 on", false, 1, 1));
    CHECK_STATUS(bladerf_enable_module(dev, rx1, false));

    CHECK_STATUS(check_counter(dev, rx1, "AUTO, RX1 streams the counter again"));
    CHECK_STATUS(expect_mode(dev, "after RX1 was enabled again", true, 1, 1));

    /* ON: either channel, one per direction */
    CHECK_STATUS(bladerf_set_rfic_1r1t_mode(dev, BLADERF_RFIC_1R1T_ON));
    CHECK_STATUS(expect_mode(dev, "ON", true, 1, 1));

    CHECK_STATUS(bladerf_enable_module(dev, rx2, true));
    CHECK_STATUS(expect_mode(dev, "ON, RX2 on", true, 2, 1));
    expect_status("ON, enable RX1 while RX2 is on",
                  bladerf_enable_module(dev, rx1, true),
                  BLADERF_ERR_UNSUPPORTED);
    CHECK_STATUS(bladerf_enable_module(dev, rx2, false));

    CHECK_STATUS(bladerf_enable_module(dev, tx2, true));
    CHECK_STATUS(expect_mode(dev, "ON, TX2 on", true, 1, 2));
    expect_status("ON, enable TX1 while TX2 is on",
                  bladerf_enable_module(dev, tx1, true),
                  BLADERF_ERR_UNSUPPORTED);
    CHECK_STATUS(bladerf_enable_module(dev, tx2, false));

    CHECK_STATUS(bladerf_sync_config(dev, BLADERF_RX_X2, BLADERF_FORMAT_SC8_Q7,
                                     256, 32768, 32, 1000));
    expect_status("ON, enable RX1 with an RX X2 layout",
                  bladerf_enable_module(dev, rx1, true),
                  BLADERF_ERR_UNSUPPORTED);
    CHECK_STATUS(bladerf_sync_config(dev, BLADERF_RX_X1, BLADERF_FORMAT_SC8_Q7,
                                     256, 32768, 32, 1000));

    /* OFF */
    CHECK_STATUS(bladerf_set_rfic_1r1t_mode(dev, BLADERF_RFIC_1R1T_OFF));
    CHECK_STATUS(expect_mode(dev, "OFF, OVERSAMPLE, 122.88 Msps", false, 1, 1));

    CHECK_STATUS(check_counter(dev, rx1, "OFF, RX1 streams the counter"));
    CHECK_STATUS(expect_mode(dev, "after RX1 was enabled in OFF", false, 1, 1));

    /* AUTO without OVERSAMPLE */
    CHECK_STATUS(bladerf_set_rfic_1r1t_mode(dev, BLADERF_RFIC_1R1T_AUTO));
    CHECK_STATUS(expect_mode(dev, "AUTO again", true, 1, 1));

    CHECK_STATUS(bladerf_enable_feature(dev, BLADERF_FEATURE_OVERSAMPLE, false));
    CHECK_STATUS(bladerf_set_sample_rate(dev, rx1, RATE_DEFAULT, NULL));
    CHECK_STATUS(expect_mode(dev, "AUTO, no OVERSAMPLE, 61.44 Msps", false, 1, 1));

    /* A new session starts in AUTO and 2R2T mode */
    CHECK_STATUS(bladerf_enable_feature(dev, BLADERF_FEATURE_OVERSAMPLE, true));
    CHECK_STATUS(bladerf_set_sample_rate(dev, rx1, RATE_OVERSAMPLE, NULL));
    CHECK_STATUS(bladerf_enable_module(dev, rx2, true));
    CHECK_STATUS(expect_mode(dev, "AUTO, RX2 on, before a close", true, 2, 1));

    bladerf_close(dev);
    *devp = NULL;
    CHECK_STATUS(bladerf_open(devp, NULL));
    dev = *devp;

    expect_mode_selection(dev, "open again", BLADERF_RFIC_1R1T_AUTO);
    CHECK_STATUS(expect_mode(dev, "open again", false, 1, 1));

error:
    return status;
}

/* The test checks only digital data and needs no TX power: before TX is
 * enabled, the TX gain goes to the lowest whole dB of its range */
static int tx_gain_min(struct bladerf *dev, bladerf_channel ch)
{
    int status;
    const struct bladerf_range *range = NULL;
    bladerf_gain gain;
    bladerf_gain readback = 0;

    CHECK_STATUS(bladerf_get_gain_range(dev, ch, &range));
    gain = (bladerf_gain)(range->min * range->scale);
    if (gain < range->min * range->scale) {
        gain++;
    }

    CHECK_STATUS(bladerf_set_gain(dev, ch, gain));
    CHECK_STATUS(bladerf_get_gain(dev, ch, &readback));
    printf("TX%d gain: %d dB, the lowest of %.2f..%.2f dB\n",
           (int)(ch >> 1) + 1, readback, range->min * range->scale,
           range->max * range->scale);

error:
    return status;
}

int main(int argc, char *argv[])
{
    int status = 0;
    struct bladerf *dev = NULL;
    struct bladerf_version version;
    bool supported;
    int opt;
    int opt_ind = 0;

    struct option longopts[] = {
        {"help", no_argument, 0, 'h'},
        {"verbose", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };

    bladerf_log_set_verbosity(BLADERF_LOG_LEVEL_WARNING);

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

    CHECK_STATUS(bladerf_open(&dev, NULL));
    CHECK_STATUS(bladerf_fpga_version(dev, &version));

    supported = version.major > 0 || version.minor >= 17;
    printf("FPGA v%u.%u.%u: %s\n", version.major, version.minor, version.patch,
           supported ? "1R1T supported" : "no 1R1T support");

    expect_mode_selection(dev, "fresh open", BLADERF_RFIC_1R1T_AUTO);
    CHECK_STATUS(expect_mode(dev, "fresh open", false, 1, 1));

    CHECK_STATUS(tx_gain_min(dev, BLADERF_CHANNEL_TX(0)));
    CHECK_STATUS(tx_gain_min(dev, BLADERF_CHANNEL_TX(1)));

    if (supported) {
        CHECK_STATUS(test_supported(&dev));
    } else {
        CHECK_STATUS(test_unsupported(dev));
    }

error:
    if (dev) {
        bladerf_close(dev);
    }

    if (status != 0) {
        printf("RESULT: ERROR\n");
        return EXIT_FAILURE;
    }

    printf("RESULT: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
