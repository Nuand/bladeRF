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
 */
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <host_config.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <libbladeRF.h>

#ifndef DATA_SOURCE
#   define DATA_SOURCE "/dev/urandom"
#endif

static bool shutdown_stream = false;

unsigned int count = 0;

struct test_data
{
    void                **buffers;      /* Transmit buffers */
    size_t              num_buffers;    /* Number of buffers */
    size_t              samples_per_buffer; /* Number of samples per buffer */
    unsigned int        idx;            /* The next one that needs to go out */
    bladerf_module      module;         /* Direction */
    FILE                *fout;          /* Output file (RX only) */
    ssize_t             samples_left;   /* Number of samples left */
};

int str2int(const char *str, int min, int max, bool *ok)
{
    long value;
    char *endptr;

    errno = 0;
    value = strtol(str, &endptr, 0);

    if (errno != 0 || value < (long)min || value > (long)max ||
        endptr == str || *endptr != '\0') {

        if (ok) {
            *ok = false;
        }

        return 0;
    }

    if (ok) {
        *ok = true;
    }
    return (int)value;
}

void *stream_callback(struct bladerf *dev, struct bladerf_stream *stream,
                      struct bladerf_metadata *metadata, void *samples,
                      size_t num_samples, void *user_data)
{
    struct test_data *my_data = (struct test_data *)user_data;

    count++ ;

    if( (count&0xffff) == 0 ) {
        fprintf( stderr, "Called 0x%8.8x times\n", count ) ;
    }

    /* Save off the samples to disk if we are in RX */
    if( my_data->module == BLADERF_MODULE_RX ) {
        size_t i;
        int16_t *sample = (int16_t *)samples ;
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
            shutdown_stream = true ;
        }
    }

    if (shutdown_stream) {
        return NULL;
    } else {
        void *rv = my_data->buffers[my_data->idx];
        my_data->idx = (my_data->idx + 1) % my_data->num_buffers;
        return rv ;
    }
}

int populate_test_data(struct test_data *test_data)
{
    FILE *in;
	size_t i;
    //ssize_t n_read;

    test_data->idx = 0;

    in = fopen(DATA_SOURCE, "rb");

    if (!in) {
        return -1;
    } else {
        printf( "Populating data\n" );
        for(i=0;i<test_data->num_buffers;i++) {
            size_t j;
            int16_t *buffer = (int16_t *)test_data->buffers[i];
            /*
            n_read = fread(test_data->buffers[i], sizeof(int16_t) * 2,
                            test_data->samples_per_buffer, in);

            if (n_read != (ssize_t)test_data->samples_per_buffer) {
                fprintf(stderr, "Hit partial read while gathering test data\n");
                fclose(in);
                return -1;
            }*/
            for(j = 0 ; j < test_data->samples_per_buffer ; j++) {
                *buffer = (j % 2048) ;
                buffer++;
                *buffer = -((int16_t)(j % 2048));
                buffer++;
            }
        }
        fclose(in);
        return 0;
    }
}

void handler(int signal)
{
    if (signal == SIGTERM || signal == SIGINT) {
        shutdown_stream = true;
        printf("Caught signal, canceling transfers\n");
        fflush(stdout);
    }
}

int main(int argc, char *argv[])
{
    int status;
    unsigned int actual;
    struct bladerf *dev;
    struct bladerf_stream *stream;
    struct test_data test_data;
    bool conv_ok;

    if (argc != 4 && argc != 5) {
        fprintf(stderr,
                "Usage: %s [tx|rx] <samples per buffer> <# buffers> [# samples]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (strcasecmp(argv[1], "rx") == 0 ) {
        test_data.module = BLADERF_MODULE_RX ;
    } else if (strcasecmp(argv[1], "tx") == 0 ) {
        test_data.module = BLADERF_MODULE_TX;
    } else {
        fprintf(stderr, "Invalid module: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    test_data.idx = 0;
    test_data.fout = NULL;

    test_data.samples_per_buffer = str2int(argv[2], 1, INT_MAX, &conv_ok);
    if (!conv_ok) {
        fprintf(stderr, "Invalid samples per buffer value: %s\n", argv[2]);
        return EXIT_FAILURE;
    }

    test_data.num_buffers = str2int(argv[3], 1, INT_MAX, &conv_ok);
    if (!conv_ok) {
        fprintf(stderr, "Invalid # buffers: %s\n", argv[3]);
        return EXIT_FAILURE;
    }

    if(test_data.module == BLADERF_MODULE_RX && argc == 5) {
        test_data.samples_left = str2int(argv[4], 1, INT_MAX, &conv_ok);
        if(!conv_ok) {
            fprintf(stderr, "Invalid number of samples: %s\n", argv[4]);
            return EXIT_FAILURE;
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

    status = bladerf_is_fpga_configured(dev);
    if (status < 0) {
        fprintf(stderr, "Failed to determine FPGA state: %s\n",
                bladerf_strerror(status));
        return EXIT_FAILURE;
    } else if (status == 0) {
        fprintf(stderr, "Error: FPGA is not loaded.\n");
        bladerf_close(dev);
        return EXIT_FAILURE;
    }

    status = 0;

    if (!status) {
        status = bladerf_set_frequency(dev, test_data.module, 1000000000);
        if (status < 0) {
            fprintf(stderr, "Failed to set frequency: %s\n",
                    bladerf_strerror(status));
            bladerf_close(dev);
            return EXIT_FAILURE;
        }
    }

    if (!status) {
        status = bladerf_set_sample_rate(dev, test_data.module, 4000000, &actual);
        if (status < 0) {
            fprintf(stderr, "Failed to set sample rate: %s\n",
                    bladerf_strerror(status));
            bladerf_close(dev);
            return EXIT_FAILURE;
        }
    }

    /* Initialize the stream */
    status = bladerf_init_stream(
                &stream,
                dev,
                stream_callback,
                &test_data.buffers,
                test_data.num_buffers,
                BLADERF_FORMAT_SC16_Q11,
                test_data.samples_per_buffer,
                test_data.num_buffers,
                &test_data
             );

    /* Populate buffers with test data */
    if( test_data.module == BLADERF_MODULE_TX ) {
        if (populate_test_data(&test_data) ) {
            fprintf(stderr, "Failed to populated test data\n");
            bladerf_deinit_stream(stream);
            bladerf_close(dev);
            return EXIT_FAILURE;
        }
    } else {
        /* Open up file we'll read test data to */
        test_data.fout = fopen( "samples.txt", "w" );
        if (!test_data.fout) {
            fprintf(stderr, "Failed to open samples.txt: %s\n", strerror(errno));
            bladerf_deinit_stream(stream);
            bladerf_close(dev);
            return EXIT_FAILURE;
        }
    }

    status = bladerf_enable_module(dev, test_data.module, true);
    if (status < 0) {
        fprintf(stderr, "Failed to enable module: %s\n",
                bladerf_strerror(status));
    }

    if (!status) {
        /* Start stream and stay there until we kill the stream */
        status = bladerf_stream(stream, test_data.module);

        if (status < 0) {
            fprintf(stderr, "Stream error: %s\n", bladerf_strerror(status));
        }
    }

    status = bladerf_enable_module(dev, test_data.module, false);
    if (status < 0) {
        fprintf(stderr, "Failed to enable module: %s\n",
                bladerf_strerror(status));
    }

    bladerf_deinit_stream(stream);
    bladerf_close(dev);

    if (test_data.fout) {
        fclose(test_data.fout);
    }

    return 0;
}

