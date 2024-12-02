#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <libbladeRF.h>
#include <math.h>

#define CHECK_STATUS(fn) do { \
    status = fn; \
    if (status != 0) { \
        fprintf(stderr, "%s:%d: %s failed: %s\n", __FILE__, __LINE__, #fn, \
                bladerf_strerror(status)); \
        goto error; \
    } \
} while (0)

#define CHECK_NULL(...) do { \
    const void* _args[] = { __VA_ARGS__, NULL }; \
    for (size_t _i = 0; _args[_i] != NULL; ++_i) { \
        if (_args[_i] == NULL) { \
            fprintf(stderr, "%s:%d: Argument %zu is NULL\n", __FILE__, __LINE__, _i + 1); \
            return BLADERF_ERR_INVAL; \
        } \
    } \
} while (0)

static const struct option long_options[] = {
    { "device",     required_argument,  NULL,   'd' },
    { "profiles",   required_argument,  NULL,   'p' },
    { "samplerate", required_argument,  NULL,   's' },
    { "verbosity",  required_argument,  NULL,   'v' },
    { "help",       no_argument,        NULL,   'h' },
    { NULL,         0,                  NULL,   0   },
};

int main(int argc, char *argv[])
{
    int opt = 0;
    int opt_ind = 0;
    int status = 0;
    char *devstr = NULL;
    struct bladerf *dev;
    const size_t num_samples = 508;
    int16_t *samples = NULL;
    const size_t num_buffers = 2;
    const size_t buffer_size = 2048;
    int verbosity = BLADERF_LOG_LEVEL_INFO;

    size_t freq_min = 915e6;
    size_t freq_max = 5.05e9;
    size_t num_profiles = 8;
    size_t freq_step;
    bladerf_frequency *freqs = NULL;
    struct bladerf_quick_tune *quick_tune = NULL;
    struct bladerf_metadata metadata;
    bladerf_timestamp *ts = NULL;
    unsigned int samplerate = 10e6;
    double *powers = NULL;

    while (opt != -1) {
        opt = getopt_long(argc, argv, "d:p:s:v:h", long_options, &opt_ind);

        switch (opt) {
            case 'd':
                devstr = optarg;
                break;

            case 'p':
                num_profiles = atoi(optarg);
                break;

            case 's':
                samplerate = atof(optarg) * 1e6;
                break;

            case 'v':
                verbosity = atoi(optarg);
                break;

            case 'h':
                printf("Usage: %s [options]\n", argv[0]);
                printf("  -d, --device <str>     Specify device to open\n");
                printf("  -p, --profiles <n>     Number of frequency profiles (default: 8)\n");
                printf("  -s, --samplerate <n>   Sample rate in MHz (default: 10)\n");
                printf("  -v, --verbosity <n>    Set libbladeRF verbosity level (default: 3)\n");
                printf("                         0=verbose, 1=debug, 2=info, 3=warning, 4=error, 5=critical, 6=silent\n");
                printf("  -h, --help             Show this text\n");
                return 0;

            default:
                break;
        }
    }

    bladerf_log_set_verbosity(verbosity);

    freq_step = (freq_max - freq_min) / (num_profiles - 1);

    samples = (int16_t *)malloc(num_samples * 2 * sizeof(int16_t));
    freqs = (bladerf_frequency *)malloc(num_profiles * sizeof(bladerf_frequency));
    quick_tune = (struct bladerf_quick_tune *)malloc(num_profiles * sizeof(struct bladerf_quick_tune));
    ts = (bladerf_timestamp *)malloc((num_profiles + 1) * sizeof(bladerf_timestamp));
    powers = (double *)malloc(num_profiles * sizeof(double));
    CHECK_NULL(samples, freqs, quick_tune, ts, powers);

    CHECK_STATUS(bladerf_open(&dev, devstr));

    CHECK_STATUS(bladerf_sync_config(dev, BLADERF_RX_X1,
                                   BLADERF_FORMAT_SC16_Q11_META,
                                   num_buffers, buffer_size, 1, 1e3));
    CHECK_STATUS(bladerf_set_gain_mode(dev, BLADERF_CHANNEL_RX(0), BLADERF_GAIN_MANUAL));
    CHECK_STATUS(bladerf_set_gain(dev, BLADERF_CHANNEL_RX(0), 0));
    CHECK_STATUS(bladerf_set_sample_rate(dev, BLADERF_CHANNEL_RX(0), samplerate, NULL));
    CHECK_STATUS(bladerf_set_bandwidth(dev, BLADERF_CHANNEL_RX(0), samplerate, NULL));
    CHECK_STATUS(bladerf_enable_module(dev, BLADERF_CHANNEL_RX(0), true));

    printf("Configuring %zu frequency profiles:\n", num_profiles);
    for (size_t i = 0; i < num_profiles; i++) {
        freqs[i] = freq_min + i*freq_step;
        printf("Profile %zu: %.3f MHz\n", i, freqs[i] / 1e6);
        CHECK_STATUS(bladerf_set_frequency(dev, BLADERF_CHANNEL_RX(0), freqs[i]));
        CHECK_STATUS(bladerf_get_quick_tune(dev, BLADERF_CHANNEL_RX(0), &quick_tune[i]));
    }

    CHECK_STATUS(bladerf_get_timestamp(dev, BLADERF_RX, &ts[0]));
    ts[0] += samplerate;
    for (size_t i = 0; i < num_profiles; i++) {
        CHECK_STATUS(bladerf_schedule_retune(dev, BLADERF_CHANNEL_RX(0),
                                           ts[i], freqs[i], &(quick_tune[i])));
        ts[i+1] = ts[i] + 10e3;
    }

    for (size_t i = 0; i < num_profiles; i++) {
        metadata.flags = 0;
        metadata.timestamp = ts[i+1];
        CHECK_STATUS(bladerf_sync_rx(dev, samples, num_samples, &metadata, 1000));

        if (metadata.status & BLADERF_META_STATUS_OVERRUN) {
            fprintf(stderr, "Overflow detected\n");
            status = EXIT_FAILURE;
            goto error;
        }

        double power_sum = 0;
        for (size_t j = 0; j < metadata.actual_count; j++) {
            double i_val = samples[2*j] / 2047.0;
            double q_val = samples[2*j+1] / 2047.0;
            power_sum += i_val * i_val + q_val * q_val;
        }
        powers[i] = power_sum / metadata.actual_count;
    }

    // Close now to dismiss overrun warnings
    bladerf_enable_module(dev, BLADERF_CHANNEL_RX(0), false);
    bladerf_close(dev);
    dev = NULL;

    printf("\nMeasured powers:\n");
    for (size_t i = 0; i < num_profiles; i++) {
        printf("%.3f MHz: %.2f dBFS\n", freqs[i] / 1e6, 10 * log10(powers[i]));
    }

error:
    if (dev != NULL) {
        bladerf_enable_module(dev, BLADERF_CHANNEL_RX(0), false);
        bladerf_close(dev);
    }

    free(samples);
    free(freqs);
    free(quick_tune);
    free(ts);
    free(powers);

    return status;
}
