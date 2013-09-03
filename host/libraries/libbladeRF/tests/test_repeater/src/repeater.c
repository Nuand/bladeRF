/*
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
#include <string.h>
#include <pthread.h>
#include <libbladeRF.h>
#include <assert.h>

#include "repeater.h"
#include "minmax.h"

/* Thread-safe wrapper around fprintf(stderr, ...) */
#define print_error(repeater_, ...) do { \
    pthread_mutex_lock(&repeater_->stderr_lock); \
    fprintf(stderr, __VA_ARGS__); \
    pthread_mutex_unlock(&repeater_->stderr_lock); \
} while (0)

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
};

void repeater_config_init(struct repeater_config *c)
{
    c->device_str = NULL;

    c->tx_freq = DEFAULT_FREQUENCY;
    c->rx_freq = DEFAULT_FREQUENCY;
    c->sample_rate = DEFAULT_SAMPLE_RATE;

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
                        "TX underrun encountered. Terminating TX task.\n");
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
    bool exit_early;

    /* Wait for RX task to buffer up some samples before we begin
     * consuming them */
    pthread_mutex_lock(&repeater->buf_mgmt.lock);
    while(repeater->buf_mgmt.num_filled < repeater->buf_mgmt.prefill_count &&
          repeater->buf_mgmt.tx_idx >= 0) {

        status = pthread_cond_wait(&repeater->buf_mgmt.samples_available,
                          &repeater->buf_mgmt.lock);
    }

    exit_early = repeater->buf_mgmt.tx_idx < 0;
    pthread_mutex_unlock(&repeater->buf_mgmt.lock);

    if (!exit_early) {
        /* Call stream */
        status = bladerf_stream(repeater->tx_stream, BLADERF_MODULE_TX);

        if (status < 0) {
            print_error(repeater, "TX stream failure: %s\n",
                    bladerf_strerror(status));
        }
    } else {
        printf("EARLY EXIT\n");
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
                        "RX Overrun encountered. Terminating RX task.\n");
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
        print_error(repeater, "RX stream failure: %s\n", bladerf_strerror(status));
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
    int status;
    unsigned int actual_value;

    status = bladerf_open(&repeater->device, config->device_str);
    if (!repeater->device) {
        fprintf(stderr, "Failed to open %s: %s\n", config->device_str,
                bladerf_strerror(status));
        return -1;
    }

    status = bladerf_is_fpga_configured(repeater->device);
    if (status < 0) {
        fprintf(stderr, "Failed to determine if FPGA is loaded: %s\n",
                bladerf_strerror(status));
        goto init_device_error;
    } else if (status == 0) {
        fprintf(stderr, "FPGA is not loaded. Aborting.\n");
        status = BLADERF_ERR_NODEV;
        goto init_device_error;
    }

    status = bladerf_set_bandwidth(repeater->device, BLADERF_MODULE_TX,
                                    config->bandwidth, &actual_value);

    if (status < 0) {
        fprintf(stderr, "Failed to set TX bandwidth: %s\n",
                bladerf_strerror(status));
        goto init_device_error;
    } else {
        printf("Actual TX bandwidth: %d Hz\n", actual_value);
    }

    status = bladerf_set_bandwidth(repeater->device, BLADERF_MODULE_RX,
                                    config->bandwidth, &actual_value);

    if (status < 0) {
        fprintf(stderr, "Failed to set RX bandwidth: %s\n",
                bladerf_strerror(status));
        goto init_device_error;
    } else {
        printf("Actual RX bandwidth: %d Hz\n", actual_value);
    }

    status = bladerf_set_sample_rate(repeater->device, BLADERF_MODULE_TX,
                                     config->sample_rate, &actual_value);

    if (status < 0) {
        fprintf(stderr, "Failed to set TX sample rate: %s\n",
                bladerf_strerror(status));
        goto init_device_error;
    } else {
        printf("Actual TX sample rate is %d Hz\n", actual_value);
    }

    status = bladerf_set_sample_rate(repeater->device, BLADERF_MODULE_RX,
                                     config->sample_rate, &actual_value);

    if (status < 0) {
        fprintf(stderr, "Failed to set RX sample rate: %s\n",
                bladerf_strerror(status));
        goto init_device_error;
    } else {
        printf("Actual RX sample rate is %d Hz\n", actual_value);
    }

    status = bladerf_set_frequency(repeater->device,
                                   BLADERF_MODULE_TX, config->tx_freq);
    if (status < 0) {
        fprintf(stderr, "Failed to set TX frequency: %s\n",
                bladerf_strerror(status));
        goto init_device_error;
    } else {
        printf("Set TX frequency to %d Hz\n", config->tx_freq);
    }

    status = bladerf_set_frequency(repeater->device,
                                   BLADERF_MODULE_RX, config->rx_freq);
    if (status < 0) {
        fprintf(stderr, "Failed to set RX frequency: %s\n",
                bladerf_strerror(status));
        goto init_device_error;
    } else {
        printf("Set RX frequency to %d Hz\n", config->rx_freq);
    }

    status = bladerf_enable_module(repeater->device, BLADERF_MODULE_RX, true);
    if (status < 0) {
        fprintf(stderr, "Failed to enable RX module: %s\n",
                bladerf_strerror(status));
        goto init_device_error;
    } else {
        printf("Enabled RX module\n");
    }

    status = bladerf_enable_module(repeater->device, BLADERF_MODULE_TX, true);
    if (status < 0) {
        bladerf_enable_module(repeater->device, BLADERF_MODULE_RX, false);
        fprintf(stderr, "Failed to enable TX module: %s\n",
                bladerf_strerror(status));
        goto init_device_error;
    } else {
        printf("Enabled TX module\n");
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

    /* TODO Until we can provide NULL to the init stream call to indicate that
     * we want to use user-probveded buffers, we'll just allocate and not
     * use some dummy buffers for TX */
    void **dummy;

    status = bladerf_init_stream(&repeater->rx_stream,
                                 repeater->device,
                                 rx_stream_callback,
                                 &repeater->buf_mgmt.samples,
                                 config->num_buffers,
                                 BLADERF_FORMAT_SC16_Q12,
                                 config->samples_per_buffer,
                                 config->num_transfers,
                                 repeater);
    if (status < 0) {
        fprintf(stderr, "Failed to initialize RX stream: %s\n",
                bladerf_strerror(status));
        return status;
    } else {
        repeater->buf_mgmt.rx_idx = 0;
    }


    status = bladerf_init_stream(&repeater->tx_stream,
                                 repeater->device,
                                 tx_stream_callback,
                                 &dummy,
                                 config->num_buffers,
                                 BLADERF_FORMAT_SC16_Q12,
                                 config->samples_per_buffer,
                                 config->num_transfers,
                                 repeater);
    if (status < 0) {
        fprintf(stderr, "Failed to initialize TX stream: %s\n",
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
    print_error(repeater, "Stoppping RX and tasks...\n");
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
        fprintf(stderr, "Failed to initialize device.\n");
        return -1;
    }

    /* Allocate streams */
    status = init_streams(repeater, config);
    if (status < 0) {
        fprintf(stderr, "Failed to allocate streams.\n");
        return -1;
    }

    return 0;
}

int repeater_run(struct repeater_config *config)
{
    int status;
    struct repeater repeater;

    status = init(&repeater, config);
    if (status == 0) {

        /* Start tasks and wait for a key press to shut down */
        status = start_tasks(&repeater);
        if (status < 0) {
            fprintf(stderr, "Failed to start tasks.\n");
        } else {
            printf("Repeater running. Press enter to stop.\n");
            getchar();
        }

        /* Stop tasks */
        stop_tasks(&repeater);

        deinit(&repeater);
    }

    return status;
}
