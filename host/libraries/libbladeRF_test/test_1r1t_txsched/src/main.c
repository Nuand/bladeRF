/*
 * test_1r1t_txsched: checks scheduled TX bursts in 1R1T mode on a bladeRF 2.0
 * micro, through the AD9361's internal loopback (BLADERF_LB_RFIC_BIST).
 *
 * TX1 feeds RX1, or TX2 feeds RX2: the loopback joins matching channels only.
 * The test sends K bursts of L samples, each P samples after the previous one,
 * at TX timestamps a lead ahead of the TX time: SC8_Q7_META at 122.88 Msps
 * with OVERSAMPLE, or SC16_Q11_META at 61.44 Msps. Each burst sample carries
 * its number, counted over all bursts. RX streams continuously with metadata,
 * and a burst is found where two samples carry the first two numbers of a
 * burst: between bursts the TX holds its last sample, which need not be zero.
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
           " [-l length] [-p period] [-r rate] [-o file [-n samples]]\n", prog_name);
    printf("  -h    Show this help message\n");
    printf("  -v    Enable verbose logging\n");
    printf("  -c    Channel: 1 (TX1 to RX1, default) or 2 (TX2 to RX2)\n");
    printf("  -m    1R1T mode selection: auto (default), off or on\n");
    printf("  -f    sc8 (default; 122.88 Msps with OVERSAMPLE) or sc16 (61.44 Msps)\n");
    printf("  -k    Number of bursts (default: 100)\n");
    printf("  -l    Samples per burst, 2 to 32767 (default: 4096)\n");
    printf("  -p    Samples from one burst's start to the next (default: 10 ms)\n");
    printf("  -r    Sample rate in Hz (default: 122880000 for sc8, 61440000 for sc16)\n");
    printf("  -o    Write the first -n RX samples after the warm-up to this file\n");
    printf("  -n    Samples for -o (default: 2000000)\n");
}

static double seconds_since(const struct timespec *start)
{
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - start->tv_sec) + (now.tv_nsec - start->tv_nsec) * 1e-9;
}

/* Burst sample n carries w = n mod 2^15; its Q is never zero */
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

/* The number a sample would carry; false for a zero sample */
static bool get_sample(bool sc16, const void *buf, size_t i, unsigned int *w)
{
    if (sc16) {
        const int16_t *s = buf;

        *w = (((unsigned int)(s[2 * i + 1] + 2048) & 0xf) << 11) |
             ((unsigned int)s[2 * i] & 0x7ff);
        return s[2 * i] != 0 || s[2 * i + 1] != 0;
    } else {
        const int8_t *s = buf;

        *w = (((unsigned int)(uint8_t)s[2 * i + 1] & 0x7f) << 8) |
             (uint8_t)s[2 * i];
        return s[2 * i] != 0 || s[2 * i + 1] != 0;
    }
}

/* The number carried by sample pos of burst k */
static unsigned int burst_value(const struct test *t, unsigned int k, unsigned int pos)
{
    return (unsigned int)(((uint64_t)k * t->burst_len + pos) & 0x7fff);
}

/* Look for bursts from n on: start_of[] covers bursts n to n + 7 */
static void advance(const struct test *t, int16_t *start_of, unsigned int *next,
                    unsigned int n)
{
    unsigned int k;

    for (k = *next; k < n && k < t->num_bursts; k++) {
        start_of[burst_value(t, k, 0)] = -1;
    }

    for (k = (n > *next + 8) ? n : *next + 8; k < n + 8 && k < t->num_bursts; k++) {
        start_of[burst_value(t, k, 0)] = (int16_t)k;
    }

    *next = n;
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
    double rate = 0;
    const char *dump_name = NULL;
    FILE *dump = NULL;
    uint64_t dump_left = 2000000;
    void *samples = NULL;
    int16_t *start_of = NULL;
    uint64_t *start_ts = NULL;
    bool *seen = NULL;
    pthread_t tx_thread;
    bool tx_started = false;
    bool r1t = false;
    enum { IDLE, CANDIDATE, IN_BURST } state = IDLE;
    unsigned int next = 0, burst = 0, cand = 0, pos = 0, reports = 0;
    unsigned int received = 0, prev = 0, expected = 0;
    uint64_t cand_ts = 0;
    uint64_t bad = 0, gaps = 0, overruns = 0, prev_end = 0;
    bool have_prev = false;
    struct timespec rx_start;
    uint64_t rx_count = 0;
    int64_t delay0 = 0, delay_min = 0, delay_max = 0;
    uint64_t spacing_min = 0, spacing_max = 0;
    unsigned int k, n_delays = 0, delay_errors = 0, spacing_errors = 0;
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
        {"rate", required_argument, 0, 'r'},
        {"output", required_argument, 0, 'o'},
        {"dump-samples", required_argument, 0, 'n'},
        {0, 0, 0, 0}
    };

    memset(&t, 0, sizeof(t));
    t.num_bursts = 100;
    t.burst_len  = 4096;

    bladerf_log_set_verbosity(BLADERF_LOG_LEVEL_WARNING);

    while ((opt = getopt_long(argc, argv, "hvc:m:f:k:l:p:r:o:n:", longopts, &opt_ind)) != -1) {
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
            case 'r':
                rate = atof(optarg);
                break;
            case 'o':
                dump_name = optarg;
                break;
            case 'n':
                dump_left = strtoull(optarg, NULL, 0);
                break;
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    t.rate   = rate > 0 ? rate : (t.sc16 ? 61440000.0 : 122880000.0);
    t.period = period ? period : (uint64_t)(t.rate / 100);
    fmt      = t.sc16 ? BLADERF_FORMAT_SC16_Q11_META : BLADERF_FORMAT_SC8_Q7_META;

    if (t.num_bursts == 0 || t.burst_len < 2 || t.burst_len > 32767 ||
        t.period < 2 * (uint64_t)t.burst_len) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    samples  = malloc((size_t)BUF_SAMPLES * (t.sc16 ? 4 : 2));
    start_ts = calloc(t.num_bursts, sizeof(start_ts[0]));
    seen     = calloc(t.num_bursts, sizeof(seen[0]));
    start_of = malloc(0x8000 * sizeof(start_of[0]));
    if (samples == NULL || start_ts == NULL || seen == NULL || start_of == NULL) {
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

    /* start_of[w]: the burst whose first sample carries w, among the next 8
     * (one table look-up per sample keeps up with the RX stream) */
    for (i = 0; i < 0x8000; i++) {
        start_of[i] = -1;
    }

    for (k = 0; k < t.num_bursts && k < 8; k++) {
        start_of[burst_value(&t, k, 0)] = (int16_t)k;
    }

    while (next < t.num_bursts) {
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

            if (dump_name != NULL) {
                dump = fopen(dump_name, "wb");
                if (dump != NULL) {
                    printf("dump: RX samples from timestamp %" PRIu64 " (TX timestamps"
                           " are printed below) into %s\n", meta.timestamp + meta.actual_count,
                           dump_name);
                }
            }
        } else if (dump != NULL && dump_left > 0) {
            size_t n = meta.actual_count < dump_left ? meta.actual_count : dump_left;

            fwrite(samples, t.sc16 ? 4 : 2, n, dump);
            dump_left -= n;
        }

        for (i = 0; i < meta.actual_count && next < t.num_bursts; i++) {
            unsigned int w;
            bool nonzero = get_sample(t.sc16, samples, i, &w);

            if (state == IN_BURST) {
                if (w != expected) {
                    if (reports++ < MAX_REPORTS) {
                        printf("  burst %u sample %u: 0x%04x, expected 0x%04x\n",
                               burst, pos, w, expected);
                    }

                    bad++;
                }

                expected = (expected + 1) & 0x7fff;

                if (++pos == t.burst_len) {
                    state = IDLE;
                    advance(&t, start_of, &next, burst + 1);
                }

                continue;
            }

            if (state == CANDIDATE) {
                if (nonzero && w == burst_value(&t, cand, 1)) {
                    burst           = cand;
                    start_ts[burst] = cand_ts;
                    seen[burst]     = true;
                    received++;
                    pos      = 2;
                    expected = burst_value(&t, burst, 2);
                    state    = (pos == t.burst_len) ? IDLE : IN_BURST;
                    advance(&t, start_of, &next, (state == IDLE) ? burst + 1 : burst);
                    continue;
                }

                state = IDLE;
            }

            /* A burst may start here: this sample carries the first number of
             * the next burst or of one of the 7 after it */
            if (nonzero && t.rx_warm && start_of[w] >= 0) {
                cand    = (unsigned int)start_of[w];
                cand_ts = meta.timestamp + i;
                state   = CANDIDATE;
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

    for (k = 0; k < t.num_bursts; k++) {
        int64_t delay;

        if (!seen[k]) {
            if (reports++ < MAX_REPORTS) {
                printf("  burst %u not found\n", k);
            }

            continue;
        }

        delay = (int64_t)(start_ts[k] - (t.tx_ts0 + k * t.period));

        if (n_delays++ == 0) {
            delay0 = delay_min = delay_max = delay;
        } else {
            /* Spacing per period, from the previous burst found */
            uint64_t spacing = (start_ts[k] - start_ts[prev]) / (k - prev);

            if (n_delays == 2 || spacing < spacing_min) {
                spacing_min = spacing;
            }

            if (n_delays == 2 || spacing > spacing_max) {
                spacing_max = spacing;
            }

            if (start_ts[k] - start_ts[prev] != (k - prev) * t.period) {
                spacing_errors++;
            }
        }

        prev = k;

        if (delay != delay0) {
            if (delay_errors++ < MAX_REPORTS) {
                printf("  burst %u: delay %" PRId64 " samples, the first burst's %" PRId64 "\n",
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

    printf("first burst's TX timestamp %" PRIu64 ", then every %" PRIu64 " samples\n",
           t.tx_ts0, t.period);
    printf("TX->RX delay: %" PRId64 " samples (RX timestamp minus TX timestamp;"
           " min %" PRId64 ", max %" PRId64 ")\n", delay0, delay_min, delay_max);
    printf("bursts received: %u of %u; RX spacing %" PRIu64 "..%" PRIu64
           " samples (expected %" PRIu64 ")\n", received, t.num_bursts, spacing_min,
           spacing_max, t.period);
    printf("wrong burst samples: %" PRIu64 "; RX gaps %" PRIu64 ", overruns %" PRIu64
           "; TX status %d\n", bad, gaps, overruns, t.tx_status);

    if (mode != BLADERF_RFIC_1R1T_OFF && !r1t) {
        printf("the interface is not in 1R1T mode\n");
    }

    if (received != t.num_bursts || bad != 0 || delay_errors != 0 ||
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

    if (dump != NULL) {
        fclose(dump);
    }

    free(samples);
    free(start_ts);
    free(seen);
    free(start_of);

    printf("RESULT: %s\n", status == 0 ? "PASS" : (status > 0 ? "FAIL" : "ERROR"));
    return status == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
