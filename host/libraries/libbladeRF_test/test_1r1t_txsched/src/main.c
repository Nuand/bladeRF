/*
 * test_1r1t_txsched: checks scheduled TX bursts in 1R1T mode on a bladeRF 2.0
 * micro, through the AD9361's internal loopback (BLADERF_LB_RFIC_BIST).
 *
 * TX1 feeds RX1, or TX2 feeds RX2: the loopback joins matching channels only.
 * The test sends K bursts of L samples, each P samples after the previous one,
 * at TX timestamps a lead ahead of the TX time: SC8_Q7_META at 122.88 Msps
 * with OVERSAMPLE, or SC16_Q11_META at 61.44 Msps. Each burst sample carries
 * its number, counted over all bursts. RX streams continuously with metadata.
 * The test passes when every burst arrives at the same delay (RX timestamp
 * minus TX timestamp) as the first, P samples after the previous one, with
 * every sample unchanged. The bladeRF also transmits at its TX port.
 */
#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <getopt.h>
#include <unistd.h>
#include <libbladeRF.h>
#include "log.h"
#include "test_common.h"

#define BUF_SAMPLES 32768
#define RX_BUFFERS 256
#define TX_BUFFERS 32
#define NUM_TRANSFERS 16
#define TIMEOUT_MS 5000
#define LEAD_S 0.1    /* the first burst's TX timestamp after the TX time */
#define WARMUP_S 0.3  /* RX streams this long before the bursts are sent */
#define MAX_REPORTS 10

struct test {
    struct bladerf *dev;
    bool sc16;
    double rate;
    unsigned int num_bursts;
    unsigned int burst_len;
    uint64_t period;
    volatile bool rx_warm;
    volatile bool tx_done;
    volatile bool stop;
    int tx_status;
    uint64_t tx_ts0;
    struct timespec tx_done_time;
};

void print_usage(const char *prog_name)
{
    printf("Usage: %s [-h] [-v] [-c channel] [-m mode] [-f format] [-k bursts]"
           " [-l length] [-p period]\n", prog_name);
    printf("  -h    Show this help message\n");
    printf("  -v    Enable verbose logging\n");
    printf("  -c    Channel: 1 (TX1 to RX1, default) or 2 (TX2 to RX2)\n");
    printf("  -m    1R1T mode selection: auto (default), off or on\n");
    printf("  -f    sc8 (default; 122.88 Msps with OVERSAMPLE) or sc16 (61.44 Msps)\n");
    printf("  -k    Number of bursts (default: 100)\n");
    printf("  -l    Samples per burst, 2 to 32767 (default: 4096)\n");
    printf("  -p    Samples from one burst's start to the next (default: 10 ms)\n");
}

static double seconds_since(const struct timespec *start)
{
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - start->tv_sec) + (now.tv_nsec - start->tv_nsec) * 1e-9;
}

/* Burst sample n carries w = n mod 2^15. Its Q is never zero, and the TX
 * sends zeros between the bursts. */
static void put_sample(bool sc16, void *buf, unsigned int i, unsigned int w)
{
    if (sc16) {
        int16_t *s = buf;

        s[2 * i]     = (int16_t)(w & 0x7ff);
        s[2 * i + 1] = (int16_t)(-2048 + (int)(w >> 11));
    } else {
        int8_t *s = buf;

        s[2 * i]     = (int8_t)(uint8_t)(w & 0xff);
        s[2 * i + 1] = (int8_t)(uint8_t)(0x80 | (w >> 8));
    }
}

static bool get_sample(bool sc16, const void *buf, size_t i, unsigned int *w)
{
    if (sc16) {
        const int16_t *s = buf;

        if (s[2 * i + 1] == 0) {
            return false;
        }

        *w = (((unsigned int)(s[2 * i + 1] + 2048) & 0xf) << 11) |
             ((unsigned int)s[2 * i] & 0x7ff);
    } else {
        const int8_t *s = buf;

        if (s[2 * i + 1] == 0) {
            return false;
        }

        *w = (((unsigned int)(uint8_t)s[2 * i + 1] & 0x7f) << 8) |
             (uint8_t)s[2 * i];
    }

    return true;
}

static void *tx_task(void *arg)
{
    struct test *t = arg;
    size_t sample_bytes = t->sc16 ? 4 : 2;
    void *burst = calloc(t->burst_len, sample_bytes);
    void *zeros = calloc(2, sample_bytes);
    struct bladerf_metadata meta;
    uint64_t now = 0;
    unsigned int k, j;
    int status = 0;

    if (burst == NULL || zeros == NULL) {
        status = BLADERF_ERR_MEM;
        goto out;
    }

    while (!t->rx_warm && !t->stop) {
        usleep(1000);
    }

    if (t->stop) {
        goto out;
    }

    status = bladerf_get_timestamp(t->dev, BLADERF_TX, &now);
    if (status != 0) {
        fprintf(stderr, "bladerf_get_timestamp: %s\n", bladerf_strerror(status));
        goto out;
    }

    t->tx_ts0 = now + (uint64_t)(LEAD_S * t->rate);

    for (k = 0; k < t->num_bursts && !t->stop; k++) {
        for (j = 0; j < t->burst_len; j++) {
            put_sample(t->sc16, burst, j,
                       (unsigned int)(((uint64_t)k * t->burst_len + j) & 0x7fff));
        }

        memset(&meta, 0, sizeof(meta));
        meta.flags     = BLADERF_META_FLAG_TX_BURST_START;
        meta.timestamp = t->tx_ts0 + k * t->period;

        status = bladerf_sync_tx(t->dev, burst, t->burst_len, &meta, TIMEOUT_MS);
        if (status != 0) {
            fprintf(stderr, "burst %u: bladerf_sync_tx: %s\n", k,
                    bladerf_strerror(status));
            break;
        }

        /* End the burst with zeros, so the TX holds zero until the next one */
        meta.flags = BLADERF_META_FLAG_TX_BURST_END;

        status = bladerf_sync_tx(t->dev, zeros, 2, &meta, TIMEOUT_MS);
        if (status != 0) {
            fprintf(stderr, "burst %u end: bladerf_sync_tx: %s\n", k,
                    bladerf_strerror(status));
            break;
        }
    }

out:
    t->tx_status = status;
    clock_gettime(CLOCK_MONOTONIC, &t->tx_done_time);
    t->tx_done = true;
    free(burst);
    free(zeros);
    return NULL;
}

int main(int argc, char *argv[])
{
    int status = 0;
    struct bladerf *dev = NULL;
    struct test t;
    bladerf_rfic_1r1t_mode mode = BLADERF_RFIC_1R1T_AUTO;
    unsigned int ch = 0;
    bladerf_format fmt;
    uint64_t period = 0;
    void *samples = NULL;
    uint64_t *start_ts = NULL;
    pthread_t tx_thread;
    bool tx_started = false;
    bool r1t = false;
    bool in_burst = false;
    unsigned int burst = 0, pos = 0, reports = 0;
    uint64_t bad = 0, extra = 0, gaps = 0, overruns = 0, prev_end = 0;
    bool have_prev = false;
    struct timespec rx_start;
    uint64_t rx_count = 0;
    int64_t delay0 = 0, delay_min = 0, delay_max = 0;
    uint64_t spacing_min = 0, spacing_max = 0;
    unsigned int k, delay_errors = 0, spacing_errors = 0;
    size_t i;
    int opt;
    int opt_ind = 0;

    struct option longopts[] = {
        {"help", no_argument, 0, 'h'},
        {"verbose", no_argument, 0, 'v'},
        {"channel", required_argument, 0, 'c'},
        {"mode", required_argument, 0, 'm'},
        {"format", required_argument, 0, 'f'},
        {"bursts", required_argument, 0, 'k'},
        {"length", required_argument, 0, 'l'},
        {"period", required_argument, 0, 'p'},
        {0, 0, 0, 0}
    };

    memset(&t, 0, sizeof(t));
    t.num_bursts = 100;
    t.burst_len  = 4096;

    bladerf_log_set_verbosity(BLADERF_LOG_LEVEL_WARNING);

    while ((opt = getopt_long(argc, argv, "hvc:m:f:k:l:p:", longopts, &opt_ind)) != -1) {
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
                ch = (unsigned int)atoi(optarg) - 1;
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
                    t.sc16 = false;
                } else if (strcmp(optarg, "sc16") == 0) {
                    t.sc16 = true;
                } else {
                    print_usage(argv[0]);
                    return EXIT_FAILURE;
                }
                break;
            case 'k':
                t.num_bursts = (unsigned int)atoi(optarg);
                break;
            case 'l':
                t.burst_len = (unsigned int)atoi(optarg);
                break;
            case 'p':
                period = strtoull(optarg, NULL, 0);
                break;
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    t.rate   = t.sc16 ? 61440000.0 : 122880000.0;
    t.period = period ? period : (uint64_t)(t.rate / 100);
    fmt      = t.sc16 ? BLADERF_FORMAT_SC16_Q11_META : BLADERF_FORMAT_SC8_Q7_META;

    if (t.num_bursts == 0 || t.burst_len < 2 || t.burst_len > 32767 ||
        t.period < 2 * (uint64_t)t.burst_len) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    samples  = malloc((size_t)BUF_SAMPLES * (t.sc16 ? 4 : 2));
    start_ts = calloc(t.num_bursts, sizeof(start_ts[0]));
    if (samples == NULL || start_ts == NULL) {
        status = BLADERF_ERR_MEM;
        goto error;
    }

    CHECK_STATUS(bladerf_open(&dev, NULL));
    t.dev = dev;

    /* The RFIC BIST loopback needs host tuning mode */
    CHECK_STATUS(bladerf_set_tuning_mode(dev, BLADERF_TUNING_MODE_HOST));

    if (!t.sc16) {
        CHECK_STATUS(bladerf_enable_feature(dev, BLADERF_FEATURE_OVERSAMPLE, true));
    }

    CHECK_STATUS(bladerf_set_rfic_1r1t_mode(dev, mode));
    CHECK_STATUS(bladerf_set_sample_rate(dev, BLADERF_CHANNEL_RX(ch),
                                         (bladerf_sample_rate)t.rate, NULL));
    CHECK_STATUS(bladerf_set_sample_rate(dev, BLADERF_CHANNEL_TX(ch),
                                         (bladerf_sample_rate)t.rate, NULL));
    CHECK_STATUS(bladerf_set_rx_mux(dev, BLADERF_RX_MUX_BASEBAND));
    CHECK_STATUS(bladerf_set_loopback(dev, BLADERF_LB_RFIC_BIST));
    CHECK_STATUS(bladerf_sync_config(dev, BLADERF_RX_X1, fmt, RX_BUFFERS,
                                     BUF_SAMPLES, NUM_TRANSFERS, TIMEOUT_MS));
    CHECK_STATUS(bladerf_sync_config(dev, BLADERF_TX_X1, fmt, TX_BUFFERS,
                                     BUF_SAMPLES, NUM_TRANSFERS, TIMEOUT_MS));
    CHECK_STATUS(bladerf_enable_module(dev, BLADERF_CHANNEL_TX(ch), true));
    CHECK_STATUS(bladerf_enable_module(dev, BLADERF_CHANNEL_RX(ch), true));
    CHECK_STATUS(bladerf_get_rfic_1r1t(dev, &r1t));

    printf("TX%u to RX%u, %s, %.2f Msps, %s mode: %u bursts of %u samples every "
           "%" PRIu64 " samples\n", ch + 1, ch + 1,
           t.sc16 ? "SC16_Q11_META" : "SC8_Q7_META, OVERSAMPLE", t.rate / 1e6,
           r1t ? "1R1T" : "2R2T", t.num_bursts, t.burst_len, t.period);

    if (pthread_create(&tx_thread, NULL, tx_task, &t) != 0) {
        status = BLADERF_ERR_UNEXPECTED;
        goto error;
    }

    tx_started = true;
    clock_gettime(CLOCK_MONOTONIC, &rx_start);

    while (burst < t.num_bursts) {
        struct bladerf_metadata meta;

        memset(&meta, 0, sizeof(meta));
        meta.flags = BLADERF_META_FLAG_RX_NOW;

        CHECK_STATUS(bladerf_sync_rx(dev, samples, BUF_SAMPLES, &meta, TIMEOUT_MS));

        if (meta.status & BLADERF_META_STATUS_OVERRUN) {
            overruns++;
        }

        if (have_prev && meta.timestamp != prev_end) {
            gaps++;
        }

        have_prev = true;
        prev_end  = meta.timestamp + meta.actual_count;
        rx_count += meta.actual_count;

        if (!t.rx_warm && rx_count >= (uint64_t)(WARMUP_S * t.rate)) {
            t.rx_warm = true;
        }

        for (i = 0; i < meta.actual_count; i++) {
            unsigned int w;
            bool payload = get_sample(t.sc16, samples, i, &w);

            if (!in_burst) {
                if (!payload) {
                    continue;
                }

                if (!t.rx_warm || burst >= t.num_bursts) {
                    extra++;
                    continue;
                }

                in_burst        = true;
                pos             = 0;
                start_ts[burst] = meta.timestamp + i;
            }

            if (payload) {
                unsigned int expected =
                    (unsigned int)(((uint64_t)burst * t.burst_len + pos) & 0x7fff);

                if (w != expected) {
                    if (reports++ < MAX_REPORTS) {
                        printf("  burst %u sample %u: 0x%04x, expected 0x%04x\n",
                               burst, pos, w, expected);
                    }

                    bad++;
                }

                if (++pos == t.burst_len) {
                    in_burst = false;
                    burst++;
                }
            } else {
                if (reports++ < MAX_REPORTS) {
                    printf("  burst %u ended after %u of %u samples\n", burst,
                           pos, t.burst_len);
                }

                bad += t.burst_len - pos;
                in_burst = false;
                burst++;
            }
        }

        if (t.tx_done && (t.tx_status != 0 ||
                          seconds_since(&t.tx_done_time) >
                              LEAD_S + 1.0 + t.num_bursts * t.period / t.rate)) {
            break;
        }

        if (!t.tx_done && seconds_since(&rx_start) > 60.0) {
            printf("the TX did not finish within 60 s\n");
            break;
        }
    }

    t.stop = true;
    pthread_join(tx_thread, NULL);
    tx_started = false;

    for (k = 0; k < burst && k < t.num_bursts; k++) {
        int64_t delay = (int64_t)(start_ts[k] - (t.tx_ts0 + k * t.period));

        if (k == 0) {
            delay0 = delay_min = delay_max = delay;
        } else {
            uint64_t spacing = start_ts[k] - start_ts[k - 1];

            if (k == 1 || spacing < spacing_min) {
                spacing_min = spacing;
            }

            if (k == 1 || spacing > spacing_max) {
                spacing_max = spacing;
            }

            if (spacing != t.period) {
                spacing_errors++;
            }
        }

        if (delay != delay0) {
            if (delay_errors++ < MAX_REPORTS) {
                printf("  burst %u: delay %" PRId64 " samples, burst 0's %" PRId64 "\n",
                       k, delay, delay0);
            }
        }

        if (delay < delay_min) {
            delay_min = delay;
        }

        if (delay > delay_max) {
            delay_max = delay;
        }
    }

    printf("TX->RX delay: %" PRId64 " samples (RX timestamp minus TX timestamp;"
           " min %" PRId64 ", max %" PRId64 ")\n", delay0, delay_min, delay_max);
    printf("bursts received: %u of %u; RX spacing %" PRIu64 "..%" PRIu64
           " samples (expected %" PRIu64 ")\n", burst, t.num_bursts, spacing_min,
           spacing_max, t.period);
    printf("wrong or missing burst samples: %" PRIu64 "; samples outside the bursts:"
           " %" PRIu64 "; RX gaps %" PRIu64 ", overruns %" PRIu64 "; TX status %d\n",
           bad, extra, gaps, overruns, t.tx_status);

    if (mode != BLADERF_RFIC_1R1T_OFF && !r1t) {
        printf("the interface is not in 1R1T mode\n");
    }

    if (burst != t.num_bursts || bad != 0 || extra != 0 || delay_errors != 0 ||
        spacing_errors != 0 || t.tx_status != 0 ||
        (mode != BLADERF_RFIC_1R1T_OFF && !r1t)) {
        status = 1;
    }

error:
    if (tx_started) {
        t.stop = true;
        pthread_join(tx_thread, NULL);
    }

    if (dev) {
        bladerf_enable_module(dev, BLADERF_CHANNEL_RX(ch), false);
        bladerf_enable_module(dev, BLADERF_CHANNEL_TX(ch), false);
        bladerf_set_loopback(dev, BLADERF_LB_NONE);
        bladerf_close(dev);
    }

    free(samples);
    free(start_ts);

    printf("RESULT: %s\n", status == 0 ? "PASS" : (status > 0 ? "FAIL" : "ERROR"));
    return status == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
