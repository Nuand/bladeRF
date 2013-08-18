#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <libbladeRF.h>

#define DATA_SOURCE "/dev/urandom"

static bool shutdown = false;

struct test_data
{
    void            **buffers;          /* Transmit buffers */
    size_t          num_buffers;        /* Number of transmit buffers */
    unsigned int    idx;                /* The next one that needs to go out */
};

void *tx_callback(struct bladerf *dev, struct bladerf_stream *stream,
                  struct bladerf_metadata *metadata, void *samples,
                  size_t num_samples, void *user_data)
{
    struct test_data *my_data = user_data;

    if (shutdown) {
        return NULL;
    } else {
        void *rv = my_data->buffers[my_data->idx];
        my_data->idx = (my_data->idx + 1) % my_data->num_buffers;
        return rv ;
    }
}

int populate_test_data(struct test_data *test_data, size_t buffer_size)
{
    FILE *in;
    ssize_t n_read;

    test_data->idx = 0;

    in = fopen(DATA_SOURCE, "rb");

    if (!in) {
        return -1;
    } else {
        size_t i;
        for(i=0;i<test_data->num_buffers;i++) {
            n_read = fread(test_data->buffers[i], sizeof(int16_t)*2, buffer_size, in);
            if (n_read != (ssize_t)buffer_size) {
                fclose(in);
                return -1;
            }
        }
        fclose(in);
        return 0;
    }
}

void handler(int signal)
{
    if (signal == SIGTERM || signal == SIGINT) {
        shutdown = true;
        printf("Caught signal, canceling transfers\n");
        fflush(stdout);
    }
}

int main(int argc, char *argv[])
{
    int status, samples_per_buffer, buffers_per_stream;
    unsigned int actual;
    struct bladerf *dev;
    struct bladerf_stream *stream;
    struct test_data test_data;
    void **buffers;

    if (argc != 3) {
        fprintf(stderr,
                "Usage: %s <samples per buffer> <# buffers>\n", argv[0]);
        return EXIT_FAILURE;
    }

    samples_per_buffer = atoi(argv[1]);
    if (samples_per_buffer <= 0) {
        fprintf(stderr, "Invalid samples per buffer value: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    buffers_per_stream = atoi(argv[2]);
    if (buffers_per_stream <= 0 ) {
        fprintf(stderr, "Invalid # buffers per stream: %s\n", argv[2]);
        return EXIT_FAILURE;
    }


    if (signal(SIGINT, handler) == SIG_ERR ||
        signal(SIGTERM, handler) == SIG_ERR) {
        fprintf(stderr, "Failed to set up signal handler\n");
        return EXIT_FAILURE;
    }

    status = bladerf_open(&dev, NULL);
    if (status < 0) {
        fprintf(stderr, "Failed to open device: %s\n", bladerf_strerror(status));
        return EXIT_FAILURE;
    }

    if (!status) {
        status = bladerf_set_frequency(dev, TX, 1000000000);
        if (status < 0) {
            fprintf(stderr, "Failed to set TX frequency: %s\n",
                    bladerf_strerror(status));
        }
    }

    if (!status) {
        status = bladerf_set_sample_rate(dev, TX, 40000000, &actual);
        if (status < 0) {
            fprintf(stderr, "Failed to set TX sample rate: %s\n",
                    bladerf_strerror(status));
        }
    }

    /* Initialize the stream */
    status = bladerf_init_stream(
                stream,
                dev,
                tx_callback,
                &buffers,
                buffers_per_stream,
                FORMAT_SC16,
                samples_per_buffer,
                buffers_per_stream,
                &test_data
             ) ;

    /* Populate buffers here */
    if (populate_test_data(&test_data, samples_per_buffer) ) {
        fprintf(stderr, "Failed to populated test data\n");
        return EXIT_FAILURE;
    }

    /* Start stream */
    status = bladerf_tx_stream(dev, FORMAT_SC16, stream);
    if (status < 0) {
        fprintf(stderr, "TX Stream error: %s\n", bladerf_strerror(status));
    }

    while( shutdown == false ) {
        sleep(1) ;
    }

    if (dev) {
        bladerf_close(dev);
    }

    return 0;
}

