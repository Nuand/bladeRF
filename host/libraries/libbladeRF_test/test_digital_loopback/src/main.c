/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2017-2018 Nuand LLC
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

#include <getopt.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include <pthread.h>

#include "conversions.h"
#include "log.h"
#include "test_common.h"
#include <libbladeRF.h>

/******************************************************************************
 * Type definitions
 ******************************************************************************/

enum data_mode { DATA_CONSTANT, DATA_COUNTING, DATA_RANDOM };
enum loopback_mode { LOOPBACK_FW, LOOPBACK_FPGA, LOOPBACK_RFIC };

struct tx_buffer_state {
    void **buffers;
    unsigned int idx;
    unsigned int num;
    uint64_t rand_state;
    int16_t i_count, q_count;
    enum data_mode mode;
    enum loopback_mode lb_mode;
};

struct rx_buffer_state {
    void **buffers;
    unsigned int idx;
    unsigned int num;
    uint64_t rand_state;
    int16_t i_count, q_count;
    enum data_mode mode;
    enum loopback_mode lb_mode;
    size_t max_count;
};

/******************************************************************************
 * Constants
 ******************************************************************************/

static uint16_t const CONSTANT_PATTERN_I = 0x2ee;
static uint16_t const CONSTANT_PATTERN_Q = 0x2cc;

static uint16_t const SAMPLE_MIN = -2047;
static uint16_t const SAMPLE_MAX = 2047;

static size_t const NUM_BUFFERS = 256;
static size_t const BUFFER_SIZE = 32 * 1024;
static size_t const MAX_ERRORS  = 16;

static unsigned int const NUM_TRANSFERS = 32;

static struct option const long_options[] = {
    { "device", required_argument, NULL, 'd' },
    { "loopback", required_argument, NULL, 'l' },
    { "samplerate", required_argument, 0, 's' },
    { "count", required_argument, 0, 'c' },
    { "data", required_argument, NULL, 'D' },
    { "help", no_argument, NULL, 'h' },
    { "verbosity", required_argument, 0, 1 },
    { "lib-verbosity", required_argument, 0, 2 },
    { NULL, 0, NULL, 0 },
};

static struct numeric_suffix const freq_suffixes[] = {
    { "K", 1000 },      { "kHz", 1000 },     { "M", 1000000 },
    { "MHz", 1000000 }, { "G", 1000000000 }, { "GHz", 1000000000 },
};

static size_t const num_freq_suffixes =
    sizeof(freq_suffixes) / sizeof(freq_suffixes[0]);

static struct numeric_suffix const count_suffixes[] = {
    { "K", 1000 }, { "M", 1000000 }, { "G", 1000000000 },
};

static size_t const num_count_suffixes =
    sizeof(count_suffixes) / sizeof(count_suffixes[0]);

/******************************************************************************
 * Globals
 ******************************************************************************/

static bool do_shutdown    = false;
static uint64_t good_count = 0;
static uint64_t bad_count  = 0;

/******************************************************************************
 * TX Sample Generation
 ******************************************************************************/

void *tx_callback(struct bladerf *dev,
                  struct bladerf_stream *stream,
                  struct bladerf_metadata *meta,
                  void *samples,
                  size_t num_samples,
                  void *user_data)
{
    struct tx_buffer_state *state = user_data;

    uint16_t *buf;
    uint64_t value;
    uint16_t i_val = 0;
    uint16_t q_val = 0;
    size_t i;

    if (meta->status &
        (BLADERF_META_STATUS_OVERRUN | BLADERF_META_STATUS_UNDERRUN)) {
        log_critical("TX over/under flow detected, stopping.\n");
        return BLADERF_STREAM_SHUTDOWN;
    }

    buf        = state->buffers[state->idx];
    state->idx = (state->idx + 1) % state->num;

    /* The AD9361 tends to swap channels when loopbacking */
    size_t skip_by = (LOOPBACK_RFIC == state->lb_mode) ? 4 : 2;

    for (i = 0; i < 2 * num_samples; i += skip_by) {
        switch (state->mode) {
            case DATA_CONSTANT:
                i_val = CONSTANT_PATTERN_I;
                q_val = CONSTANT_PATTERN_Q;
                break;

            case DATA_RANDOM:
                value = randval_update(&state->rand_state);
                i_val = (value >> 32) & 0x3ff;
                q_val = value & 0x3ff;
                break;

            case DATA_COUNTING:
                i_val = state->i_count++;
                q_val = state->q_count--;

                if (state->i_count > SAMPLE_MAX) {
                    state->i_count = SAMPLE_MIN;
                }

                if (state->q_count < SAMPLE_MIN) {
                    state->q_count = SAMPLE_MAX;
                }

                break;
        }

        buf[i]     = i_val;
        buf[i + 1] = q_val;

        if (skip_by == 4) {
            buf[i + 2] = i_val;
            buf[i + 3] = q_val;
        }
    }

    if (do_shutdown) {
        return BLADERF_STREAM_SHUTDOWN;
    }

    return buf;
}

void *tx_streamer(void *context)
{
    struct bladerf_stream *tx_stream = context;

    int status;

    status = bladerf_stream(tx_stream, BLADERF_TX_X1);
    if (status < 0) {
        log_error("tx bladerf_stream(): %s\n", bladerf_strerror(status));
        do_shutdown = true;
        return NULL;
    }

    log_info("TX stream exited.\n");

    return NULL;
}

/******************************************************************************
 * RX Sample Processing
 ******************************************************************************/

bool check_rx(uint16_t *buf, size_t i, uint16_t expected_i, uint16_t expected_q)
{
    static size_t error_dump    = 0;
    static bool error_throttled = false;

    // Reset error throttling
    if (0 == i) {
        error_throttled = false;
        error_dump      = 0;
    }

    log_verbose("checking: sample %6d expect %04x,%04x got "
                "%04x,%04x\n",
                i / 2, expected_i, expected_q, buf[i], buf[i + 1]);

    if (buf[i] == expected_i && buf[i + 1] == expected_q) {
        return true;
    } else {
        if (!error_throttled) {
            if (error_dump < MAX_ERRORS) {
                log_debug("MISMATCH: sample %6d expected %04x,%04x got "
                          "%04x,%04x\n",
                          i / 2, expected_i, expected_q, buf[i], buf[i + 1]);
                ++error_dump;
            } else {
                log_debug("Throttling future mismatch prints...\n");
                error_throttled = true;
            }
        }
        return false;
    }
}

void *rx_callback(struct bladerf *dev,
                  struct bladerf_stream *stream,
                  struct bladerf_metadata *meta,
                  void *samples,
                  size_t num_samples,
                  void *user_data)
{
    struct rx_buffer_state *state = user_data;
    uint16_t *buf                 = samples;
    bool match                    = true;

    uint16_t expected_i, expected_q;
    uint64_t value;
    size_t i;

    static uint64_t last_status_print = 0;
    static size_t block_count         = 0;

    /* The AD9361 tends to swap channels when loopbacking */
    size_t skip_by = (LOOPBACK_RFIC == state->lb_mode) ? 4 : 2;

    if (meta->status &
        (BLADERF_META_STATUS_OVERRUN | BLADERF_META_STATUS_UNDERRUN)) {
        log_error("RX over/under flow detected, stopping.\n");
        return BLADERF_STREAM_SHUTDOWN;
    }

    if (do_shutdown) {
        return BLADERF_STREAM_SHUTDOWN;
    }

    for (i = 0; i < 2 * num_samples; i += skip_by) {
        switch (state->mode) {
            case DATA_CONSTANT:
                expected_i = CONSTANT_PATTERN_I;
                expected_q = CONSTANT_PATTERN_Q;

                if (check_rx(buf, i, expected_i, expected_q)) {
                    ++good_count;
                } else {
                    match = false;
                    ++bad_count;
                }
                break;

            case DATA_RANDOM:
                value      = randval_update(&state->rand_state);
                expected_i = (value >> 32) & 0x3ff;
                expected_q = value & 0x3ff;

                if (check_rx(buf, i, expected_i, expected_q)) {
                    ++good_count;
                } else {
                    match = false;
                    ++bad_count;
                }
                break;

            case DATA_COUNTING:
                expected_i = state->i_count++;
                expected_q = state->q_count--;

                if (check_rx(buf, i, expected_i, expected_q)) {
                    ++good_count;
                } else {
                    match = false;
                    ++bad_count;

                    // resync...
                    state->i_count = buf[i] + 1;
                    state->q_count = buf[i + 1] - 1;
                }

                if (state->i_count > SAMPLE_MAX) {
                    state->i_count = SAMPLE_MIN;
                }
                if (state->q_count < SAMPLE_MIN) {
                    state->q_count = SAMPLE_MAX;
                }
        }
    }

    if (!match) {
        log_warning("RX Mismatch detected in block %d\n", block_count);
#if 0
        for (i = 0; i < 2*128; i++) {
            printf("%02x ", buf[i]);
        }
        printf("\n\n");
#endif
    }

    buf        = state->buffers[state->idx];
    state->idx = (state->idx + 1) % state->num;

    if ((good_count + bad_count - last_status_print) > 100000000) {
        log_info("IQ Matches: %zu / %zu (%.1f%%), %d blocks\n", good_count,
                 good_count + bad_count,
                 100.0 * ((double)good_count) /
                     ((double)(good_count + bad_count)),
                 block_count);
        last_status_print = good_count;
    }

    ++block_count;

    if ((state->max_count > 0) &&
        ((good_count + bad_count) > state->max_count)) {
        do_shutdown = true;
    }

    return buf;
}

void *rx_streamer(void *context)
{
    struct bladerf_stream *rx_stream = context;

    int status;

    status = bladerf_stream(rx_stream, BLADERF_RX_X1);
    if (status < 0) {
        log_error("rx bladerf_stream(): %s\n", bladerf_strerror(status));
        do_shutdown = true;
        return NULL;
    }

    log_info("RX stream exited.\n");

    return NULL;
}

/******************************************************************************
 * Main functions
 ******************************************************************************/

void handler(int signal)
{
    if (signal == SIGTERM || signal == SIGINT) {
        do_shutdown = true;
        log_error("Caught signal, canceling transfers\n");
        fflush(stdout);
    }
}

int configure_module(struct bladerf *dev,
                     bladerf_channel ch,
                     uint32_t sample_rate)
{
    uint64_t const FREQUENCY = 2400000000UL;
    uint32_t const BANDWIDTH = 20000000;

    int status;

    status = bladerf_set_frequency(dev, ch, FREQUENCY);
    if (status != 0) {
        log_error("Failed to set ch %d frequency = %" PRIu64 ": %s\n", ch,
                  FREQUENCY, bladerf_strerror(status));
        return status;
    }

    status = bladerf_set_bandwidth(dev, ch, BANDWIDTH, NULL);
    if (status != 0) {
        log_error("Failed to set ch %d bandwidth = %u: %s\n", ch, BANDWIDTH,
                  bladerf_strerror(status));
        return status;
    }

    status = bladerf_set_sample_rate(dev, ch, sample_rate, NULL);
    if (status != 0) {
        log_error("Failed to set ch %d sample rate = %u: %s\n", ch, sample_rate,
                  bladerf_strerror(status));
        return status;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    uint32_t sample_rate                  = 7680000;
    enum loopback_mode test_loopback_mode = LOOPBACK_FW;
    enum data_mode test_data_mode         = DATA_CONSTANT;
    char *devstr                          = NULL;
    size_t max_count                      = 0;

    struct bladerf *dev;
    struct rx_buffer_state rx_state;
    struct tx_buffer_state tx_state;
    struct bladerf_stream *rx_stream, *tx_stream;
    pthread_t rx_thread, tx_thread;
    bladerf_log_level level;
    int status;
    bool ok;

    log_set_verbosity(BLADERF_LOG_LEVEL_INFO);
    bladerf_log_set_verbosity(BLADERF_LOG_LEVEL_INFO);

    int opt     = 0;
    int opt_ind = 0;

    while (opt != -1) {
        opt = getopt_long(argc, argv, "d:l:c:D:s:h", long_options, &opt_ind);

        switch (opt) {
            case 'd':
                devstr = optarg;
                break;

            case 'l':
                if (strcmp(optarg, "fw") == 0) {
                    test_loopback_mode = LOOPBACK_FW;
                } else if (strcmp(optarg, "fpga") == 0) {
                    test_loopback_mode = LOOPBACK_FPGA;
                } else if (strcmp(optarg, "rfic") == 0) {
                    test_loopback_mode = LOOPBACK_RFIC;
                } else {
                    log_error("Unknown loopback mode: %s\n", optarg);
                    return -1;
                }
                break;

            case 's':
                sample_rate = str2uint_suffix(
                    optarg, 0, 61440000, freq_suffixes, num_freq_suffixes, &ok);
                if (!ok) {
                    log_error("Invalid sample rate: %s\n", optarg);
                    return -1;
                }
                break;

            case 'c':
                max_count =
                    str2uint_suffix(optarg, 0, UINT32_MAX, count_suffixes,
                                    num_count_suffixes, &ok);
                if (!ok) {
                    log_error("Invalid count: %s\n", optarg);
                    return -1;
                }
                break;

            case 'D':
                if (strcmp(optarg, "constant") == 0) {
                    test_data_mode = DATA_CONSTANT;
                } else if (strcmp(optarg, "counting") == 0) {
                    test_data_mode = DATA_COUNTING;
                } else if (strcmp(optarg, "random") == 0) {
                    test_data_mode = DATA_RANDOM;
                } else {
                    log_error("Unknown test mode: %s\n", optarg);
                    return -1;
                }
                break;

            case 1:
                level = str2loglevel(optarg, &ok);
                if (!ok) {
                    log_error("Invalid log level provided: %s\n", optarg);
                    return -1;
                } else {
                    log_set_verbosity(level);
                }
                break;

            case 2:
                level = str2loglevel(optarg, &ok);
                if (!ok) {
                    log_error("Invalid log level provided: %s\n", optarg);
                    return -1;
                } else {
                    bladerf_log_set_verbosity(level);
                }
                break;

            case 'h':
                printf("Usage: %s [options]\n", argv[0]);
                printf("  -d, --device <str>       Specify device to open.\n");
                printf("  -l, --loopback <mode>    Specify loopback.\n");
                printf("                             fw (default)\n");
                printf("                             fpga\n");
                printf("                             rfic\n");
                printf("  -s, --samplerate <sps>   Specify sample rate.\n");
                printf("  -c, --count <num>        Specify number of samples "
                       "to test (0 = unlimited).\n");
                printf("  -D, --data <mode>        Specify data.\n");
                printf("                             constant (default)\n");
                printf("                             counting\n");
                printf("                             random\n");
                printf("  --verbosity <level>      Set test verbosity "
                       "(Default: info)\n");
                printf("  --lib-verbosity <level>  Set libbladeRF "
                       "verbosity (Default: info)\n");
                printf("  -h, --help               Show this text.\n");
                return 0;

            default:
                break;
        }
    }

    status = bladerf_open(&dev, devstr);
    if (status < 0) {
        log_error("bladerf_open(): %s\n", bladerf_strerror(status));
        return 1;
    }

    rx_state.idx       = 0;
    rx_state.num       = NUM_TRANSFERS;
    rx_state.i_count   = 0;
    rx_state.q_count   = 0;
    rx_state.mode      = test_data_mode;
    rx_state.lb_mode   = test_loopback_mode;
    rx_state.max_count = max_count;
    randval_init(&rx_state.rand_state, 0);

    status = bladerf_init_stream(
        &rx_stream, dev, rx_callback, &rx_state.buffers, NUM_BUFFERS,
        BLADERF_FORMAT_SC16_Q11, BUFFER_SIZE, NUM_TRANSFERS, &rx_state);
    if (status < 0) {
        log_error("bladerf_init_stream(): %s\n", bladerf_strerror(status));
        bladerf_close(dev);
        return -1;
    }

    status = configure_module(dev, BLADERF_RX, sample_rate);
    if (status < 0) {
        log_error("configure_module(BLADERF_RX): %s\n",
                  bladerf_strerror(status));
        bladerf_close(dev);
        return -1;
    }

    tx_state.idx     = 0;
    tx_state.num     = NUM_TRANSFERS;
    tx_state.i_count = 0;
    tx_state.q_count = 0;
    tx_state.mode    = test_data_mode;
    tx_state.lb_mode = test_loopback_mode;
    randval_init(&tx_state.rand_state, 0);

    status = bladerf_init_stream(
        &tx_stream, dev, tx_callback, &tx_state.buffers, NUM_BUFFERS,
        BLADERF_FORMAT_SC16_Q11, BUFFER_SIZE, NUM_TRANSFERS, &tx_state);
    if (status < 0) {
        log_error("bladerf_init_stream(): %s\n", bladerf_strerror(status));
        bladerf_close(dev);
        return -1;
    }

    status = configure_module(dev, BLADERF_TX, sample_rate);
    if (status < 0) {
        log_error("configure_module(BLADERF_TX): %s\n",
                  bladerf_strerror(status));
        bladerf_close(dev);
        return -1;
    }

    // Default: disable loopback mode
    status = bladerf_set_loopback(dev, BLADERF_LB_NONE);
    if (status < 0) {
        log_error("bladerf_set_loopback(): %s\n", bladerf_strerror(status));
        bladerf_close(dev);
        return -1;
    }

    // Default: baseband mux
    status = bladerf_set_rx_mux(dev, BLADERF_RX_MUX_BASEBAND);
    if (status < 0) {
        log_error("bladerf_set_rx_mux(): %s\n", bladerf_strerror(status));
        bladerf_close(dev);
        return -1;
    }

    switch (test_loopback_mode) {
        case LOOPBACK_FW:
            status = bladerf_set_loopback(dev, BLADERF_LB_FIRMWARE);
            if (status < 0) {
                log_error("bladerf_set_loopback(): %s\n",
                          bladerf_strerror(status));
                bladerf_close(dev);
                return -1;
            }
            break;

        case LOOPBACK_FPGA:
            status = bladerf_set_rx_mux(dev, BLADERF_RX_MUX_DIGITAL_LOOPBACK);
            if (status < 0) {
                log_error("bladerf_set_rx_mux(): %s\n",
                          bladerf_strerror(status));
                bladerf_close(dev);
                return -1;
            }
            break;

        case LOOPBACK_RFIC:
            status = bladerf_set_loopback(dev, BLADERF_LB_RFIC_BIST);
            if (status < 0) {
                log_error("bladerf_set_loopback(): %s\n",
                          bladerf_strerror(status));
                bladerf_close(dev);
                return -1;
            }
            break;
    }

    if (signal(SIGINT, handler) == SIG_ERR) {
        perror("signal()");
        return -1;
    }

    if (signal(SIGTERM, handler) == SIG_ERR) {
        perror("signal()");
        return -1;
    }

    status = bladerf_enable_module(dev, BLADERF_CHANNEL_TX(0), true);
    if (status < 0) {
        log_error("bladerf_enable_module(TX): %s\n", bladerf_strerror(status));
        goto error;
    }

    status = pthread_create(&tx_thread, NULL, tx_streamer, tx_stream);
    if (status < 0) {
        log_error("pthread_create(TX): %s\n", strerror(status));
        goto error;
    }

    status = bladerf_enable_module(dev, BLADERF_CHANNEL_RX(0), true);
    if (status < 0) {
        log_error("bladerf_enable_module(RX): %s\n", bladerf_strerror(status));
        goto error;
    }

    status = pthread_create(&rx_thread, NULL, rx_streamer, rx_stream);
    if (status < 0) {
        log_error("pthread_create(RX): %s\n", strerror(status));
        goto error;
    }

    log_info("Loopback test running. Press Ctrl-C to exit...\n");

    pthread_join(tx_thread, NULL);
    pthread_join(rx_thread, NULL);

    log_info("IQ Matches: %zu / %zu (%.1f%%)\n", good_count,
             good_count + bad_count,
             100.0 * ((double)good_count) / ((double)(good_count + bad_count)));

    bladerf_deinit_stream(tx_stream);
    bladerf_deinit_stream(rx_stream);

    status = bladerf_enable_module(dev, BLADERF_CHANNEL_TX(0), false);
    if (status < 0) {
        log_error("bladerf_enable_module(): %s\n", bladerf_strerror(status));
        goto error;
    }

    status = bladerf_enable_module(dev, BLADERF_CHANNEL_RX(0), false);
    if (status < 0) {
        log_error("bladerf_enable_module(): %s\n", bladerf_strerror(status));
        goto error;
    }

    bladerf_close(dev);
    return 0;

error:
    bladerf_close(dev);
    return 1;
}
