/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2013 Nuand LLC
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 *
 *
 * This file implements the guts of the simple full-duplex repeater.
 *
 * We run two threads to block on bladerf_stream() calls and handle
 * callbacks, while the "main" thread waits for user input to shut down.
 *
 * When starting a TX stream, an initial set of callbacks are immediately fired
 * off to fill the initial set of transfers. Before starting the TX stream, we
 * first wait to start the RX stream has filled enough buffers to account for
 * the initial set of TX transfers. This is referred to as the "prefill count"
 * in this code.
 *
 * We set the prefill count halfway between the total number of buffers and
 * transfers:
 *
 *                           +---------+  <-- # buffers (32)
 *                           |         |
 *                           +---------+  <-- prefill count (24)
 *                           |         |
 *                           +---------+  <-- # transfers (16)
 *                           |         |
 *                           |         |
 *                           +---------+  0
 *
 *
 *
 * Although we designate each thread "RX" or "TX", note that these threads
 * may receive callbacks for the other module. Therefore, the callbacks
 * cannot block [1] and must be implemented in a thread-safe manner;
 * accesses to buffer management data are locked via a mutex.
 *
 * [1] If a TX callback were to block to wait for RX buffers to
 *     fill more, deadlock could occur because two TX callbacks could be fired
 *     off before an RX callback is exectuted to release the TX callbacks.
 *
 *
 * Overruns and underruns are not handled intelligently in this simple test.
 * If either occur (they shouldn't), the associated tasks shut down and wait
 * for the user to terminate the program.
 */

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <pthread.h>
#include <libbladeRF.h>
#include "rel_assert.h"

#include "repeater.h"
#include "minmax.h"

#if BLADERF_OS_WINDOWS
#include <conio.h>

static int term_init()
{
    return 0;
}

static int term_deinit()
{
    return 0;
}

static char get_key()
{
    return (char)_getch();
}
#else
#include <errno.h>
#include <unistd.h>
#include <termios.h>

static struct termios *termios_backup = NULL;

static int term_init()
{
    struct termios termios_new;
    int status;

    termios_backup = malloc(sizeof(*termios_backup));
    if (termios_backup == NULL) {
        perror("malloc");
        return -1;
    }

    status = tcgetattr(STDIN_FILENO, termios_backup);
    if (status != 0) {
        perror("tcgetattr");
        return status;
    }

    memcpy(&termios_new, &termios_backup, sizeof(termios_new));

    termios_new.c_cc[VMIN] = 1;
    termios_new.c_cc[VTIME] = 0;

    termios_new.c_lflag &= ~ICANON;
    termios_new.c_lflag &= ~ECHO;

    status = tcsetattr(0, TCSANOW, &termios_new);
    if (status != 0) {
        perror("tcsetattr");
        return status;
    }

    return 0;
}

static int term_deinit()
{
    int status;

    if (termios_backup == NULL) {
        status = -1;
    } else {
        status = tcsetattr(0, TCSANOW, termios_backup);
        free(termios_backup);
    }

    return status;
}

static char get_key()
{
    return (char)getc(stdin);
}

#endif

/* Thread-safe wrapper around fprintf(stderr, ...) */
#define print_error(repeater_, ...) do { \
    pthread_mutex_lock(&repeater_->stderr_lock); \
    fprintf(stderr, __VA_ARGS__); \
    pthread_mutex_unlock(&repeater_->stderr_lock); \
} while (0)

#define GAIN_TXVGA1_MIN     -35
#define GAIN_TXVGA1_MAX     4
#define GAIN_TXVGA2_MIN     0
#define GAIN_TXVGA2_MAX     25
#define GAIN_RXVGA1_MIN     5
#define GAIN_RXVGA1_MAX     30
#define GAIN_RXVGA2_MIN     0
#define GAIN_RXVGA2_MAX     30

#define KEY_DEC_TX          '1'
#define KEY_INC_TX          '2'
#define KEY_DEC_RX          '3'
#define KEY_INC_RX          '4'
#define KEY_QUIT            'q'
#define KEY_HELP            'h'


struct buf_mgmt
{
    int rx_idx;         /* Next sample buffer to RX into. < 0 indicates stop */
    int tx_idx;         /* Next sample buffer to TX from. < 0 indicates stop */
    size_t num_filled;  /* Number of buffers filled with RX data awaiting TX */

    size_t prefill_count;   /* Since the initial set of TX callbacks will look
                               to fill all available transfers, we must
                               ensure that the RX task has prefilled a
                               sufficient number of buffers */

    void **samples;         /* Sample buffers */
    size_t num_buffers;     /* # of sample buffers */

    /* Used to signal the TX thread when a few samplse have been buffered up */
    pthread_cond_t  samples_available;

    pthread_mutex_t lock;
};

struct repeater
{
    struct bladerf *device;

    pthread_t rx_task;
    struct bladerf_stream *rx_stream;

    pthread_t tx_task;
    struct bladerf_stream *tx_stream;

    pthread_mutex_t stderr_lock;

    struct buf_mgmt buf_mgmt;

    int gain_tx;                /**< TX gain */
    int gain_rx;                /**< RX gain */

    struct bladerf_range gain_range_rx;
    struct bladerf_range gain_range_tx;
};

void repeater_config_init(struct repeater_config *c)
{
    c->device_str = NULL;

    c->tx_freq = DEFAULT_FREQUENCY;
    c->rx_freq = DEFAULT_FREQUENCY;
    c->sample_rate = DEFAULT_SAMPLE_RATE;
    c->bandwidth = DEFAULT_BANDWIDTH;

    c->num_buffers = DEFAULT_NUM_BUFFERS;
    c->num_transfers = DEFAULT_NUM_TRANSFERS;
    c->samples_per_buffer = DEFAULT_SAMPLES_PER_BUFFER;

    c->verbosity = BLADERF_LOG_LEVEL_INFO;
}

void repeater_config_deinit(struct repeater_config *c)
{
    free(c->device_str);
    c->device_str = NULL;
}

static void *tx_stream_callback(struct bladerf *dev,
                                struct bladerf_stream *stream,
                                struct bladerf_metadata *meta,
                                void *samples,
                                size_t num_samples,
                                void *user_data)
{
    void *ret;
    struct repeater *repeater = (struct repeater *)user_data;

    pthread_mutex_lock(&repeater->buf_mgmt.lock);

    if (repeater->buf_mgmt.tx_idx < 0) {
        ret = NULL;
    } else {

        /* We shouldn't encounter underruns, but for the sake of simplicity
         * in this test/example, we'll just error out if we hit one */
        if (repeater->buf_mgmt.num_filled == 0) {
            ret = NULL;
            print_error(repeater,
                        "TX underrun encountered. Terminating TX task.\r\n");
        } else {

            ret = repeater->buf_mgmt.samples[repeater->buf_mgmt.tx_idx++];

            if (repeater->buf_mgmt.tx_idx >= (int)repeater->buf_mgmt.num_buffers) {
                repeater->buf_mgmt.tx_idx = 0;
            }

            repeater->buf_mgmt.num_filled--;
        }
    }

    pthread_mutex_unlock(&repeater->buf_mgmt.lock);

    return ret;
}

void * tx_task_run(void *repeater_)
{
    int status;
    struct repeater *repeater = (struct repeater *)repeater_;
    bool exit_early = false;

    /* Wait for RX task to buffer up some samples before we begin
     * consuming them */
    pthread_mutex_lock(&repeater->buf_mgmt.lock);
    while(repeater->buf_mgmt.num_filled < repeater->buf_mgmt.prefill_count &&
          repeater->buf_mgmt.tx_idx >= 0) {

        status = pthread_cond_wait(&repeater->buf_mgmt.samples_available,
                          &repeater->buf_mgmt.lock);

        if (status != 0) {
            print_error(repeater, "TX startup wait failed (%d)\n", status);
            exit_early = true;
        }
    }

    exit_early |= repeater->buf_mgmt.tx_idx < 0;
    pthread_mutex_unlock(&repeater->buf_mgmt.lock);

    if (!exit_early) {
        /* Call stream */
        status = bladerf_stream(repeater->tx_stream, BLADERF_MODULE_TX);

        if (status < 0) {
            print_error(repeater, "TX stream failure: %s\r\n",
                    bladerf_strerror(status));
        }
    } else {
        printf("EARLY EXIT\r\n");
    }

    return NULL;
}

static void *rx_stream_callback(struct bladerf *dev,
                                struct bladerf_stream *stream,
                                struct bladerf_metadata *meta,
                                void *samples,
                                size_t num_samples,
                                void *user_data)
{
    struct repeater *repeater = (struct repeater *)user_data;
    void *ret;

    pthread_mutex_lock(&repeater->buf_mgmt.lock);

    if (repeater->buf_mgmt.rx_idx < 0) {
        ret = NULL;
    } else {
        ret = repeater->buf_mgmt.samples[repeater->buf_mgmt.rx_idx++];

        /* Wrap back around */
        if (repeater->buf_mgmt.rx_idx >= (int)repeater->buf_mgmt.num_buffers) {
            repeater->buf_mgmt.rx_idx = 0;
        }

        if (repeater->buf_mgmt.num_filled >= 2 * repeater->buf_mgmt.num_buffers) {
            /* We shouldn't encounter overruns, but if we do, we'll just
             * error out of the program, for the sake of simplicity */
            print_error(repeater,
                        "RX Overrun encountered. Terminating RX task.\r\n");
            ret = NULL;
        } else {
            repeater->buf_mgmt.num_filled++;
        }
        pthread_cond_signal(&repeater->buf_mgmt.samples_available);
    }

    pthread_mutex_unlock(&repeater->buf_mgmt.lock);

    return ret;
}

void * rx_task_run(void *repeater_)
{
    int status;
    struct repeater *repeater = (struct repeater *)repeater_;

    status = bladerf_stream(repeater->rx_stream, BLADERF_MODULE_RX);
    if (status < 0) {
        print_error(repeater, "RX stream failure: %s\r\n", bladerf_strerror(status));
    }

    return NULL;
}

static inline void repeater_init(struct repeater *repeater,
                                 struct repeater_config *config)
{
    memset(repeater, 0, sizeof(*repeater));

    pthread_mutex_init(&repeater->stderr_lock, NULL);
    pthread_mutex_init(&repeater->buf_mgmt.lock, NULL);
    pthread_cond_init(&repeater->buf_mgmt.samples_available, NULL);

    repeater->buf_mgmt.num_filled = 0;
    repeater->buf_mgmt.num_buffers = config->num_buffers;

    /* We must prefill, at a minimum, 1 + the number of TX transfers,
     * since the intial set of TX callbacks will look to populate each
     * transfer. Below, we try to set the prefill count halfway between
     * the number of buffers and the number of transfers. */
    repeater->buf_mgmt.prefill_count = config->num_transfers;
    repeater->buf_mgmt.prefill_count +=
        (repeater->buf_mgmt.num_buffers - config->num_transfers) / 2;

    assert(repeater->buf_mgmt.prefill_count != 0);

    bladerf_log_set_verbosity(config->verbosity);
}

static int init_device(struct repeater *repeater, struct repeater_config *config)
{
    int status = 0;
    unsigned int actual_value;
    const struct bladerf_range *gain_ptr;

    status = bladerf_open(&repeater->device, config->device_str);
    if (!repeater->device) {
        fprintf(stderr, "Failed to open %s: %s\r\n", config->device_str,
                bladerf_strerror(status));
        return -1;
    }

    status = bladerf_is_fpga_configured(repeater->device);
    if (status < 0) {
        fprintf(stderr, "Failed to determine if FPGA is loaded: %s\r\n",
                bladerf_strerror(status));
        goto init_device_error;
    } else if (status == 0) {
        fprintf(stderr, "FPGA is not loaded. Aborting.\r\n");
        status = BLADERF_ERR_NODEV;
        goto init_device_error;
    }

    status = bladerf_set_bandwidth(repeater->device, BLADERF_MODULE_TX,
                                    config->bandwidth, &actual_value);

    if (status < 0) {
        fprintf(stderr, "Failed to set TX bandwidth: %s\r\n",
                bladerf_strerror(status));
        goto init_device_error;
    } else {
        printf("Actual TX bandwidth: %d Hz\r\n", actual_value);
    }

    status = bladerf_set_bandwidth(repeater->device, BLADERF_MODULE_RX,
                                    config->bandwidth, &actual_value);

    if (status < 0) {
        fprintf(stderr, "Failed to set RX bandwidth: %s\r\n",
                bladerf_strerror(status));
        goto init_device_error;
    } else {
        printf("Actual RX bandwidth: %d Hz\r\n", actual_value);
    }

    status = bladerf_set_sample_rate(repeater->device, BLADERF_MODULE_TX,
                                     config->sample_rate, &actual_value);

    if (status < 0) {
        fprintf(stderr, "Failed to set TX sample rate: %s\r\n",
                bladerf_strerror(status));
        goto init_device_error;
    } else {
        printf("Actual TX sample rate is %d Hz\r\n", actual_value);
    }

    status = bladerf_set_sample_rate(repeater->device, BLADERF_MODULE_RX,
                                     config->sample_rate, &actual_value);

    if (status < 0) {
        fprintf(stderr, "Failed to set RX sample rate: %s\r\n",
                bladerf_strerror(status));
        goto init_device_error;
    } else {
        printf("Actual RX sample rate is %d Hz\r\n", actual_value);
    }

    status = bladerf_set_frequency(repeater->device,
                                   BLADERF_MODULE_TX, config->tx_freq);
    if (status < 0) {
        fprintf(stderr, "Failed to set TX frequency: %s\r\n",
                bladerf_strerror(status));
        goto init_device_error;
    } else {
        printf("Set TX frequency to %d Hz\r\n", config->tx_freq);
    }

    status = bladerf_set_frequency(repeater->device,
                                   BLADERF_MODULE_RX, config->rx_freq);
    if (status < 0) {
        fprintf(stderr, "Failed to set RX frequency: %s\r\n",
                bladerf_strerror(status));
        goto init_device_error;
    } else {
        printf("Set RX frequency to %d Hz\r\n", config->rx_freq);
    }

    status = bladerf_enable_module(repeater->device, BLADERF_MODULE_RX, true);
    if (status < 0) {
        fprintf(stderr, "Failed to enable RX module: %s\r\n",
                bladerf_strerror(status));
        goto init_device_error;
    } else {
        printf("Enabled RX module\r\n");
    }

    status = bladerf_enable_module(repeater->device, BLADERF_MODULE_TX, true);
    if (status < 0) {
        bladerf_enable_module(repeater->device, BLADERF_MODULE_RX, false);
        fprintf(stderr, "Failed to enable TX module: %s\r\n",
                bladerf_strerror(status));
        goto init_device_error;
    } else {
        printf("Enabled TX module\r\n");
    }

    status = bladerf_get_gain(repeater->device,
                    BLADERF_CHANNEL_TX(0), &repeater->gain_tx);
    if (status < 0) {
        fprintf(stderr, "Failed to get TX gain: %s\r\n",
                bladerf_strerror(status));
        goto init_device_error;
    } else {
        printf("TX gain is: %d\r\n", repeater->gain_tx);
    }

    status = bladerf_get_gain_range(repeater->device,
                    BLADERF_CHANNEL_TX(0), &gain_ptr);
    if (status < 0) {
        fprintf(stderr, "Failed to get TX gain range: %s\r\n",
                bladerf_strerror(status));
        goto init_device_error;
    } else {
        memcpy(&repeater->gain_range_tx, gain_ptr,
                                        sizeof(struct bladerf_range));

        printf("TX gain range is: [%"PRIi64", %"PRIi64"]\r\n",
                                           repeater->gain_range_tx.min,
                                           repeater->gain_range_tx.max);
    }

    status = bladerf_get_gain(repeater->device,
                    BLADERF_CHANNEL_RX(0), &repeater->gain_rx);
    if (status < 0) {
        fprintf(stderr, "Failed to get RX gain: %s\r\n",
                bladerf_strerror(status));
        goto init_device_error;
    } else {
        printf("RX gain is: %d\r\n", repeater->gain_rx);
    }

    status = bladerf_get_gain_range(repeater->device,
                    BLADERF_CHANNEL_RX(0), &gain_ptr);
    if (status < 0) {
        fprintf(stderr, "Failed to get RX gain range: %s\r\n",
                bladerf_strerror(status));
        goto init_device_error;
    } else {
        memcpy(&repeater->gain_range_rx, gain_ptr,
                                        sizeof(struct bladerf_range));

        printf("RX gain range is: [%"PRIi64", %"PRIi64"]\r\n",
                                           repeater->gain_range_rx.min,
                                           repeater->gain_range_rx.max);
    }

    return status;

init_device_error:
    bladerf_close(repeater->device);
    repeater->device = NULL;

    return status;
}

static int init_streams(struct repeater *repeater,
                        struct repeater_config *config)
{
    int status;

    status = bladerf_init_stream(&repeater->rx_stream,
                                 repeater->device,
                                 rx_stream_callback,
                                 &repeater->buf_mgmt.samples,
                                 config->num_buffers,
                                 BLADERF_FORMAT_SC16_Q11,
                                 config->samples_per_buffer,
                                 config->num_transfers,
                                 repeater);
    if (status < 0) {
        fprintf(stderr, "Failed to initialize RX stream: %s\r\n",
                bladerf_strerror(status));
        return status;
    } else {
        repeater->buf_mgmt.rx_idx = 0;
    }


    status = bladerf_init_stream(&repeater->tx_stream,
                                 repeater->device,
                                 tx_stream_callback,
                                 NULL,
                                 config->num_buffers,
                                 BLADERF_FORMAT_SC16_Q11,
                                 config->samples_per_buffer,
                                 config->num_transfers,
                                 repeater);
    if (status < 0) {
        fprintf(stderr, "Failed to initialize TX stream: %s\r\n",
                bladerf_strerror(status));
        return status;
    } else {
        repeater->buf_mgmt.tx_idx = 0;
    }

    repeater->buf_mgmt.num_buffers = config->num_buffers;

    return 0;
}

static int start_tasks(struct repeater *repeater)
{
    int status;

    status = pthread_create(&repeater->rx_task, NULL, rx_task_run, repeater);
    if (status < 0) {
        return -1;
    }

    status = pthread_create(&repeater->tx_task, NULL, tx_task_run, repeater);
    if (status < 0) {
        pthread_cancel(repeater->rx_task);
        pthread_join(repeater->rx_task, NULL);
        return -1;
    }

    return 0;
}

static void stop_tasks(struct repeater *repeater)
{
    print_error(repeater, "Stoppping RX and tasks...\r\n");
    pthread_mutex_lock(&repeater->buf_mgmt.lock);
    repeater->buf_mgmt.tx_idx = -1;
    repeater->buf_mgmt.rx_idx = -1;
    pthread_mutex_unlock(&repeater->buf_mgmt.lock);
    pthread_join(repeater->rx_task, NULL);

    /* Fire off the "samples available" signal to the TX thread, in case
     * it is still awaiting for the prefill completion */
    pthread_mutex_lock(&repeater->buf_mgmt.lock);
    pthread_cond_signal(&repeater->buf_mgmt.samples_available);
    pthread_mutex_unlock(&repeater->buf_mgmt.lock);
    pthread_join(repeater->tx_task, NULL);

}

static void deinit(struct repeater *repeater)
{

    if (repeater->device) {
        if (repeater->rx_stream) {
            bladerf_deinit_stream(repeater->rx_stream);
        }

        if (repeater->tx_stream) {
            bladerf_deinit_stream(repeater->tx_stream);
        }

        bladerf_enable_module(repeater->device, BLADERF_MODULE_RX, false);
        bladerf_enable_module(repeater->device, BLADERF_MODULE_TX, false);

        bladerf_close(repeater->device);
        repeater->device = NULL;
    }
}

static int init(struct repeater *repeater, struct repeater_config *config)
{
    int status;

    repeater_init(repeater, config);

    /* Configure the bladeRF */
    status = init_device(repeater, config);
    if (status < 0) {
        fprintf(stderr, "Failed to initialize device.\r\n");
        return -1;
    }

    /* Allocate streams */
    status = init_streams(repeater, config);
    if (status < 0) {
        fprintf(stderr, "Failed to allocate streams.\r\n");
        return -1;
    }

    return 0;
}

static void repeater_help()
{
    printf("\r\nHotkeys\r\n");
    printf("------------------------------------------------\r\n");
    printf("TX Gain: Decrement = %c, Increment = %c\r\n",
           KEY_DEC_TX, KEY_INC_TX);

    printf("RX Gain: Decrement = %c, Increment = %c\r\n",
           KEY_DEC_RX, KEY_INC_RX);

    printf("Quit: q\r\n");
    printf("Hotkey list: h\r\n\r\n");
}

static int repeater_handle_key(struct repeater *repeater, char key)
{
    int status = 0;

    switch (key) {
        case KEY_DEC_TX:
            if (repeater->gain_tx >= repeater->gain_range_tx.min) {
                status = bladerf_set_gain(repeater->device,
                                            BLADERF_CHANNEL_TX(0),
                                            --repeater->gain_tx);

                if (status < 0) {
                    repeater->gain_tx++;
                    fprintf(stderr, "Failed to increase TX gain: %s\r\n",
                            bladerf_strerror(status));
                } else {
                    printf("TX gain decreased to: %d\r\n",
                            repeater->gain_tx);
                }
            }
            break;

        case KEY_INC_TX:
            if (repeater->gain_tx <= repeater->gain_range_tx.max) {
                status = bladerf_set_gain(repeater->device,
                                            BLADERF_CHANNEL_TX(0),
                                            ++repeater->gain_tx);

                if (status < 0) {
                    repeater->gain_tx--;
                    fprintf(stderr, "Failed to increase TX gain: %s\r\n",
                            bladerf_strerror(status));
                } else {
                    printf("TX gain increased to: %d\r\n",
                            repeater->gain_tx);
                }
            }
            break;

        case KEY_DEC_RX:
            if (repeater->gain_rx >= repeater->gain_range_rx.min) {
                status = bladerf_set_gain(repeater->device,
                        BLADERF_CHANNEL_RX(0),
                        --repeater->gain_rx);

                if (status < 0) {
                    repeater->gain_rx++;
                    fprintf(stderr, "Failed to decrease RX gain: %s\r\n",
                            bladerf_strerror(status));
                } else {
                    printf("RX gain decreased to %d\r\n",
                            repeater->gain_rx);
                }
            }
            break;

        case KEY_INC_RX:
            if (repeater->gain_rx <= repeater->gain_range_tx.max) {
                status = bladerf_set_gain(repeater->device,
                        BLADERF_CHANNEL_RX(0),
                        ++repeater->gain_rx);

                if (status < 0) {
                    repeater->gain_rx--;
                    fprintf(stderr, "Failed to increase RX gain: %s\r\n",
                            bladerf_strerror(status));
                } else {
                    printf("RX gain increased to %d\r\n",
                            repeater->gain_rx);
                }
            }
            break;

        case KEY_HELP:
            repeater_help();
            break;

        default:
            break;
    }

    return status;
}

int repeater_run(struct repeater_config *config)
{
    int status;
    char key;
    struct repeater repeater;

    status = term_init();
    if (status != 0) {
        return 1;
    }

    status = init(&repeater, config);
    if (status == 0) {

        /* Start tasks and wait for a key press to shut down */
        status = start_tasks(&repeater);
        if (status < 0) {
            fprintf(stderr, "Failed to start tasks.\r\n");
        } else {
            repeater_help();
            do {
                key = get_key();
                repeater_handle_key(&repeater, key);
            } while (key != KEY_QUIT);
        }

        /* Stop tasks */
        stop_tasks(&repeater);

        deinit(&repeater);
    }

    return term_deinit();
}
