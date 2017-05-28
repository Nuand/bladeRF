#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>

#include <pthread.h>

#include <libbladeRF.h>

#include "test_common.h"

enum data_mode { DATA_CONSTANT, DATA_RANDOM };
enum loopback_mode { LOOPBACK_FW, LOOPBACK_FPGA, LOOPBACK_RFIC };

#define CONSTANT_PATTERN_I      0x2ee
#define CONSTANT_PATTERN_Q      0x2cc

static bool shutdown = false;
static uint64_t good_count = 0;
static uint64_t bad_count = 0;

struct tx_buffer_state {
    void **buffers;
    unsigned int idx;
    unsigned int num;
    uint64_t rand_state;
    enum data_mode mode;
};

void *tx_callback(struct bladerf *dev, struct bladerf_stream *stream,
                  struct bladerf_metadata *meta, void *samples, size_t num_samples,
                  void *user_data)
{
    struct tx_buffer_state *state = user_data;
    unsigned int i;
    uint16_t *buf;

    if (meta->status & (BLADERF_META_STATUS_OVERRUN|BLADERF_META_STATUS_UNDERRUN)) {
        printf("TX over/under flow detected, stopping.\n");
        return BLADERF_STREAM_SHUTDOWN;
    }

    buf = state->buffers[state->idx];
    state->idx = (state->idx + 1) % state->num;

    if (state->mode == DATA_CONSTANT) {
        for (i = 0; i < 2*num_samples; i += 2) {
            buf[i] = CONSTANT_PATTERN_I;
            buf[i+1] = CONSTANT_PATTERN_Q;
        }
    } else if (state->mode == DATA_RANDOM) {
        for (i = 0; i < 2*num_samples; i += 2) {
            uint64_t value = randval_update(&state->rand_state);
            buf[i] = (value >> 32) & 0x3ff;
            buf[i+1] = value & 0x3ff;
        }
    }

    if (shutdown) {
        return BLADERF_STREAM_SHUTDOWN;
    }

    return buf;
}

struct rx_buffer_state {
    void **buffers;
    unsigned int idx;
    unsigned int num;
    uint64_t rand_state;
    enum data_mode mode;
};

void *rx_callback(struct bladerf *dev, struct bladerf_stream *stream,
                  struct bladerf_metadata *meta, void *samples, size_t num_samples,
                  void *user_data)
{
    struct rx_buffer_state *state = user_data;
    unsigned int i;
    uint16_t *buf = samples;
    bool match = true;

    if (meta->status & (BLADERF_META_STATUS_OVERRUN|BLADERF_META_STATUS_UNDERRUN)) {
        printf("RX over/under flow detected, stopping.\n");
        return BLADERF_STREAM_SHUTDOWN;
    }

    if (state->mode == DATA_CONSTANT) {
        for (i = 0; i < 2*num_samples; i += 2) {
            if (buf[i] == CONSTANT_PATTERN_I && buf[i+1] == CONSTANT_PATTERN_Q) {
                good_count += 1;
            } else {
                match = false;
                bad_count += 1;
            }
        }
    } else if (state->mode == DATA_RANDOM) {
        for (i = 0; i < 2*num_samples; i += 2) {
            uint64_t value = randval_update(&state->rand_state);
            uint16_t expected_i = (value >> 32) & 0x3ff;
            uint16_t expected_q = value & 0x3ff;

            if (buf[i] == expected_i && buf[i+1] == expected_q) {
                good_count += 1;
            } else {
                match = false;
                bad_count += 1;
            }
        }
    }

    if (!match) {
        printf("RX Mismatch\n");
        #if 0
        for (i = 0; i < 2*128; i++) {
            printf("%02x ", buf[i]);
        }
        printf("\n\n");
        #endif
    }

    if (shutdown) {
        return BLADERF_STREAM_SHUTDOWN;
    }

    buf = state->buffers[state->idx];
    state->idx = (state->idx + 1) % state->num;

    return buf;
}

void *tx_streamer(void *context) {
    struct bladerf_stream *tx_stream = context;
    int status;

    status = bladerf_stream(tx_stream, BLADERF_TX_X1);
    if (status < 0) {
        fprintf(stderr, "tx bladerf_stream(): %s\n", bladerf_strerror(status));
        shutdown = true;
        return NULL;
    }

    printf("TX stream exited.\n");

    return NULL;
}

void *rx_streamer(void *context) {
    struct bladerf_stream *rx_stream = context;
    int status;

    status = bladerf_stream(rx_stream, BLADERF_RX_X1);
    if (status < 0) {
        fprintf(stderr, "rx bladerf_stream(): %s\n", bladerf_strerror(status));
        shutdown = true;
        return NULL;
    }

    printf("RX stream exited.\n");

    return NULL;
}

void handler(int signal)
{
    if (signal == SIGTERM || signal == SIGINT) {
        shutdown = true;
        printf("Caught signal, canceling transfers\n");
        fflush(stdout);
    }
}

static const struct option long_options[] = {
    { "device",     required_argument,  NULL,   'd' },
    { "loopback",   required_argument,  NULL,   'l' },
    { "test",       required_argument,  NULL,   't' },
    { "help",       no_argument,        NULL,   'h' },
    { NULL,         0,                  NULL,   0   },
};

int main(int argc, char *argv[])
{
    int opt = 0;
    int opt_ind = 0;
    enum loopback_mode test_loopback_mode = LOOPBACK_FW;
    enum data_mode test_data_mode = DATA_CONSTANT;
    char *devstr = NULL;

    struct bladerf *dev;
    struct rx_buffer_state rx_state;
    struct tx_buffer_state tx_state;
    struct bladerf_stream *rx_stream, *tx_stream;
    pthread_t rx_thread, tx_thread;
    int status;

    opt = 0;

    while (opt != -1) {
        opt = getopt_long(argc, argv, "d:l:D:h", long_options, &opt_ind);

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
                    fprintf(stderr, "Unknown loopback mode: %s\n", optarg);
                    return -1;
                }
                break;

            case 'D':
                if (strcmp(optarg, "constant") == 0) {
                    test_data_mode = DATA_CONSTANT;
                } else if (strcmp(optarg, "random") == 0) {
                    test_data_mode = DATA_RANDOM;
                } else {
                    fprintf(stderr, "Unknown test mode: %s\n", optarg);
                    return -1;
                }
                break;

            case 'h':
                printf("Usage: %s [options]\n", argv[0]);
                printf("  -d, --device <str>     Specify device to open.\n");
                printf("  -l, --loopback <mode>  Specify loopback.\n");
                printf("                           fw (default)\n");
                printf("                           fpga\n");
                printf("                           rfic\n");
                printf("  -D, --data <mode>      Specify data.\n");
                printf("                           constant (default)\n");
                printf("                           random\n");
                printf("  -h, --help             Show this text.\n");
                return 0;

            default:
                break;
        }
    }

    status = bladerf_open(&dev, devstr);
    if (status < 0) {
        fprintf(stderr, "bladerf_open(): %s\n", bladerf_strerror(status));
        return 1;
    }

    rx_state.idx = 0;
    rx_state.num = 32;
    rx_state.mode = test_data_mode;
    randval_init(&rx_state.rand_state, 0);

    status = bladerf_init_stream(&rx_stream, dev, rx_callback,
                              &rx_state.buffers, 32, BLADERF_FORMAT_SC16_Q11,
                              32*1024, 16, &rx_state);
    if (status < 0) {
        fprintf(stderr, "bladerf_init_stream(): %s\n", bladerf_strerror(status));
        bladerf_close(dev);
        return -1;
    }

    tx_state.idx = 0;
    tx_state.num = 32;
    tx_state.mode = test_data_mode;
    randval_init(&tx_state.rand_state, 0);

    status = bladerf_init_stream(&tx_stream, dev, tx_callback,
                              &tx_state.buffers, 32, BLADERF_FORMAT_SC16_Q11,
                              32*1024, 16, &tx_state);
    if (status < 0) {
        fprintf(stderr, "bladerf_init_stream(): %s\n", bladerf_strerror(status));
        bladerf_close(dev);
        return -1;
    }

    if (test_loopback_mode == LOOPBACK_FW) {
        status = bladerf_set_loopback(dev, BLADERF_LB_FIRMWARE);
        if (status < 0) {
            fprintf(stderr, "bladerf_set_loopback(): %s\n", bladerf_strerror(status));
            bladerf_close(dev);
            return -1;
        }
    } else if (test_loopback_mode == LOOPBACK_FPGA) {
        status = bladerf_set_rx_mux(dev, BLADERF_RX_MUX_DIGITAL_LOOPBACK);
        if (status < 0) {
            fprintf(stderr, "bladerf_set_rx_mux(): %s\n", bladerf_strerror(status));
            bladerf_close(dev);
            return -1;
        }
    } else if (test_loopback_mode == LOOPBACK_RFIC) {
        status = bladerf_set_loopback(dev, BLADERF_LB_AD9361_BIST);
        if (status < 0) {
            fprintf(stderr, "bladerf_set_loopback(): %s\n", bladerf_strerror(status));
            bladerf_close(dev);
            return -1;
        }
    }

    if (signal(SIGINT, handler) == SIG_ERR) {
        perror("signal()");
        return -1;
    }

    if (signal(SIGTERM, handler) == SIG_ERR) {
        perror("signal()");
        return -1;
    }

    status = bladerf_enable_module(dev, BLADERF_RX, true);
    if (status < 0) {
        fprintf(stderr, "bladerf_enable_module(): %s\n", bladerf_strerror(status));
        goto error;
    }

    status = bladerf_enable_module(dev, BLADERF_TX, true);
    if (status < 0) {
        fprintf(stderr, "bladerf_enable_module(): %s\n", bladerf_strerror(status));
        goto error;
    }

    status = pthread_create(&rx_thread, NULL, rx_streamer, rx_stream);
    if (status < 0) {
        fprintf(stderr, "pthread_create:() %s\n", strerror(status));
        goto error;
    }

    status = pthread_create(&tx_thread, NULL, tx_streamer, tx_stream);
    if (status < 0) {
        fprintf(stderr, "pthread_create:() %s\n", strerror(status));
        goto error;
    }

    printf("Loopback test running. Press Ctrl-C to exit...\n");

    pthread_join(tx_thread, NULL);
    pthread_join(rx_thread, NULL);

    printf("IQ Matches: %zu / %zu (%.1f%%)\n", good_count, good_count + bad_count, 100.0*((double)good_count)/((double)(good_count + bad_count)));

    bladerf_deinit_stream(tx_stream);
    bladerf_deinit_stream(rx_stream);

    status = bladerf_enable_module(dev, BLADERF_TX, false);
    if (status < 0) {
        fprintf(stderr, "bladerf_enable_module(): %s\n", bladerf_strerror(status));
        goto error;
    }

    status = bladerf_enable_module(dev, BLADERF_RX, false);
    if (status < 0) {
        fprintf(stderr, "bladerf_enable_module(): %s\n", bladerf_strerror(status));
        goto error;
    }

    bladerf_close(dev);
    return 0;

error:
    bladerf_close(dev);
    return 1;
}
