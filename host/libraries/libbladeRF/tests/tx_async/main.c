#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <libbladeRF.h>

#define DATA_SOURCE "/dev/urandom"

static bool shutdown = false;

unsigned int count = 0;

struct test_data
{
    void                **buffers;      /* Transmit buffers */
    size_t              num_buffers;    /* Number of transmit buffers */
    unsigned int        idx;            /* The next one that needs to go out */
    bladerf_module_t    module;         /* Direction */
    FILE                *fout;          /* Output file */
    ssize_t             samples_left;   /* Number of samples left */
};

void *stream_callback(struct bladerf *dev, struct bladerf_stream *stream,
                  struct bladerf_metadata *metadata, void *samples,
                  size_t num_samples, void *user_data)
{
    struct test_data *my_data = user_data;

    count++ ;

    if( (count&0xffff) == 0 ) {
        fprintf( stderr, "Called 0x%8.8x times\n", count ) ;
    }

    /* Save off the samples to disk if we are in RX */
    if( my_data->module == RX ) {
        size_t i;
        int16_t *sample = samples ;
        for(i = 0; i < num_samples ; i++ ) {
            *(sample) &= 0xfff ;
            if( (*sample)&0x800 ) *(sample) |= 0xf000 ;
            *(sample+1) &= 0xfff ;
            if( *(sample+1)&0x800 ) *(sample+1) |= 0xf000 ;
            fprintf( my_data->fout, "%d, %d\n", *sample, *(sample+1) );
            sample += 2 ;
        }
        my_data->samples_left -= num_samples ;
        if( my_data->samples_left <= 0 ) {
            shutdown = true ;
        }
    }

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
    int status, samples_per_buffer, buffers_per_stream, num_samples = 0;
    unsigned int actual;
    struct bladerf *dev;
    struct bladerf_stream *stream;
    bladerf_module_t module = TX;
    struct test_data test_data;

    if (argc != 4 && argc != 5) {
        fprintf(stderr,
                "Usage: %s [tx|rx] <samples per buffer> <# buffers> [# samples]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (strcasecmp(argv[1], "rx") == 0 ) {
        module = RX ;
    } else if (strcasecmp(argv[1], "tx") == 0 ) {
        module = TX;
    } else {
        fprintf(stderr, "Invalid module: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    samples_per_buffer = atoi(argv[2]);
    if (samples_per_buffer <= 0) {
        fprintf(stderr, "Invalid samples per buffer value: %s\n", argv[2]);
        return EXIT_FAILURE;
    }

    buffers_per_stream = atoi(argv[3]);
    if (buffers_per_stream <= 0 ) {
        fprintf(stderr, "Invalid # buffers per stream: %s\n", argv[3]);
        return EXIT_FAILURE;
    }

    if( module == RX && argc == 5) {
        num_samples = atoi(argv[4]);
        if( num_samples < 0 ) {
            fprintf(stderr, "Invalid number of samples: %s\n", argv[4]);
        }
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
        status = bladerf_set_frequency(dev, module, 1000000000);
        if (status < 0) {
            fprintf(stderr, "Failed to set frequency: %s\n",
                    bladerf_strerror(status));
        }
    }

    if (!status) {
        status = bladerf_set_sample_rate(dev, module, 40000000, &actual);
        if (status < 0) {
            fprintf(stderr, "Failed to set sample rate: %s\n",
                    bladerf_strerror(status));
        }
    }

    /* Initialize the stream */
    status = bladerf_init_stream(
                &stream,
                dev,
                stream_callback,
                &test_data.buffers,
                buffers_per_stream,
                FORMAT_SC16,
                samples_per_buffer,
                buffers_per_stream,
                &test_data
             ) ;

    test_data.num_buffers = buffers_per_stream ;
    test_data.idx = 0;

    /* Populate buffers here */
    if( module == TX ) {
        if (populate_test_data(&test_data, samples_per_buffer) ) {
            fprintf(stderr, "Failed to populated test data\n");
            return EXIT_FAILURE;
        }
    } else {
        test_data.samples_left = num_samples ;
        test_data.fout = fopen( "samples.txt", "w" );
    }

    /* Start stream and stay there until we kill the stream */
    status = bladerf_stream(dev, module, FORMAT_SC16, stream);
    if (status < 0) {
        fprintf(stderr, "Stream error: %s\n", bladerf_strerror(status));
    }

    bladerf_deinit_stream(stream);

    if (dev) {
        bladerf_close(dev);
    }

    fclose(test_data.fout);

    return 0;
}

