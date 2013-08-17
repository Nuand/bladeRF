#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <libbladeRF.h>

#define DATA_SOURCE "/dev/urandom"

static bool shutdown = false;

struct test_data
{
    uint8_t *data;
    size_t   size;
    unsigned int idx;
};


void tx_callback(struct bladerf *dev, struct bladerf_stream *stream,
                 struct bladerf_metadata *metadata, void *samples,
                 size_t num_samples)
{
    struct test_data *my_data = stream->data;
    size_t n_bytes = 2 * sizeof(int16_t) * num_samples;

    if (shutdown) {
        stream->state = BLADERF_STREAM_CANCELLING;
    } else {
        my_data->idx += n_bytes;

        if (my_data->idx >= my_data->size) {
            my_data->idx = 0;
        }
        memcpy(samples, &my_data->data[my_data->idx], n_bytes);
    }
}

int populate_test_data(struct test_data *test_data)
{
    FILE *in;
    ssize_t n_read;

    test_data->idx = 0;
    test_data->size = 1 * 1024 * 1024;
    test_data->data = malloc(test_data->size);

    if (!test_data->data) {
        test_data->size = 0;
        return -1;
    }

    in = fopen(DATA_SOURCE, "rb");

    if (!in) {
        free(test_data->data);
        test_data->data = NULL;
        test_data->size = 0;
        return -1;
    } else {
        n_read = fread(test_data->data, 1, test_data->size, in);
        if (n_read != (ssize_t)test_data->size) {
            free(test_data->data);
            test_data->data = NULL;
            test_data->size = 0;
            return -1;
        }
        size_t i;
        for(i=0;i<test_data->size;){
            test_data->data[i++] = 0x7f ;
            test_data->data[i++] = 0x7f ;
        }
        return 0;
    }
}

void handler(int signal)
{
    if (signal == SIGUSR1) {
        shutdown = true;
        printf("Caught SIGUSR1, canceling transfers\n");
        fflush(stdout);
    } else {
        printf("Wrong signal bro\n");
        fflush(stdout);
    }
}

int main(int argc, char *argv[])
{
    int status;
    unsigned int actual;
    struct bladerf *dev;
    struct bladerf_stream stream;
    struct test_data test_data;

    if (argc != 3) {
        fprintf(stderr,
                "Usage: %s <samples per buffer> <# buffers>\n", argv[0]);
        return EXIT_FAILURE;
    }

    stream.samples_per_buffer = atoi(argv[1]);
    if (stream.samples_per_buffer <= 0) {
        fprintf(stderr, "Invalid samples per buffer value: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    stream.buffers_per_stream = atoi(argv[2]);
    if (stream.buffers_per_stream <= 0 ) {
        fprintf(stderr, "Invalid # buffers per stream: %s\n", argv[2]);
        return EXIT_FAILURE;
    }

    if (populate_test_data(&test_data)) {
        fprintf(stderr, "Failed to populated test data\n");
        return EXIT_FAILURE;
    }

    if (signal(SIGUSR1, handler) == SIG_ERR) {
        fprintf(stderr, "Failed to set up signal handler\n");
        return EXIT_FAILURE;
    }

    stream.data = &test_data;

    stream.cb = tx_callback;

    status = bladerf_open(&dev, NULL);
    if (status < 0) {
        fprintf(stderr, "Failed to open device: %s\n", bladerf_strerror(status));
        return EXIT_FAILURE;
    }

    bladerf_set_sample_rate( dev, TX, 40000000, &actual );

    status = bladerf_tx_stream(dev, FORMAT_SC16, &stream);
    if (status) {
        fprintf(stderr, "TX Stream error: %s\n", bladerf_strerror(status));
    }

    if (dev) {
        bladerf_close(dev);
    }

    free(test_data.data);

    return 0;
}
