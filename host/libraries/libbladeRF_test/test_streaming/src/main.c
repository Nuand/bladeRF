#include <getopt.h>
#include <libbladeRF.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "conversions.h"

#define INT12_MAX 2047
#define BUFFER_SIZE_ALIGNMENT 1024

#define CHECK_STATUS(fn) \
    do { \
        status = (fn); \
        if (status != 0) { \
            fprintf(stderr, "Error at %s:%d (%s): %s\n", \
                __FILE__, __LINE__, __func__, bladerf_strerror(status)); \
            goto error; \
        } \
    } while (0)

typedef struct {
    bool tx;
    bool rx;
    bool mimo;
    size_t num_samples;
    uint64_t sample_rate;
    int16_t *tx_samples;
    int16_t *rx_samples;
    bladerf_format format;
    struct bladerf_metadata tx_meta;
    struct bladerf_metadata rx_meta;
    size_t num_buffers;
    size_t buffer_size;
    size_t num_transfers;
    size_t timeout_ms;
} test_config;

void print_config(const test_config *config, const char *device_string, const char *fpga_file) {
    printf("\nConfiguration Summary:\n");
    printf("---------------------\n");
    printf("Device:         %s\n", device_string ? device_string : "any");
    printf("FPGA file:      %s\n", fpga_file ? fpga_file : "Not specified");
    printf("TX enabled:     %s\n", config->tx ? "Yes" : "No");
    printf("RX enabled:     %s\n", config->rx ? "Yes" : "No");
    printf("MIMO enabled:   %s\n", config->mimo ? "Yes" : "No");
    printf("Format:         %s\n", config->format == BLADERF_FORMAT_SC16_Q11_META ? "SC16 Q11 META" : "SC16 Q11");
    printf("Sample rate:    %.3f MHz\n", config->sample_rate / 1e6);
    printf("# of samples:   %zu\n", config->num_samples);
    printf("Buffer size:    %zu\n", config->buffer_size);
    printf("# of buffers:   %zu\n", config->num_buffers);
    printf("# of transfers: %zu\n", config->num_transfers);
    printf("---------------------\n\n");
}


// Define suffix table for engineering notation
static const struct numeric_suffix freq_suffixes[] = {
    { "K", 1000 },
    { "k", 1000 },
    { "M", 1000000 },
    { "m", 1000000 },
    { "G", 1000000000 },
    { "g", 1000000000 },
};

void print_bladerf_flag_statuses(uint32_t flags) {
    printf("BladeRF Flag Statuses:\n");
    printf("%-40s %d\n", "BLADERF_META_STATUS_OVERRUN:", !!(flags & BLADERF_META_STATUS_OVERRUN));
    printf("%-40s %d\n", "BLADERF_META_STATUS_UNDERRUN:", !!(flags & BLADERF_META_STATUS_UNDERRUN));
    printf("%-40s %d\n", "BLADERF_META_FLAG_TX_BURST_START:", !!(flags & BLADERF_META_FLAG_TX_BURST_START));
    printf("%-40s %d\n", "BLADERF_META_FLAG_TX_BURST_END:", !!(flags & BLADERF_META_FLAG_TX_BURST_END));
    printf("%-40s %d\n", "BLADERF_META_FLAG_TX_NOW:", !!(flags & BLADERF_META_FLAG_TX_NOW));
    printf("%-40s %d\n", "BLADERF_META_FLAG_TX_UPDATE_TIMESTAMP:", !!(flags & BLADERF_META_FLAG_TX_UPDATE_TIMESTAMP));
    printf("%-40s %d\n", "BLADERF_META_FLAG_RX_NOW:", !!(flags & BLADERF_META_FLAG_RX_NOW));
    printf("%-40s %d\n", "BLADERF_META_FLAG_RX_HW_UNDERFLOW:", !!(flags & BLADERF_META_FLAG_RX_HW_UNDERFLOW));
    printf("%-40s %d\n", "BLADERF_META_FLAG_RX_HW_MINIEXP1:", !!(flags & BLADERF_META_FLAG_RX_HW_MINIEXP1));
    printf("%-40s %d\n", "BLADERF_META_FLAG_RX_HW_MINIEXP2:", !!(flags & BLADERF_META_FLAG_RX_HW_MINIEXP2));
}

void print_help() {
    printf("Usage: test_streaming [options]\n");
    printf("Options:\n");
    printf("  -d, --device      <device>    Specify the device string\n");
    printf("  -f, --fpga        <file>      Specify the FPGA file\n");
    printf("  -v, --verbosity               Enable verbose logging\n");
    printf("  -t, --tx                      Enable TX test\n");
    printf("  -r, --rx                      Enable RX test\n");
    printf("  -m, --mimo                    Enable MIMO test\n");
    printf("  -e, --meta                    Enable META test\n");
    printf("  -s, --samplerate  <rate>      Specify the sample rate\n");
    printf("  -n, --num-samples <count>     Specify the number of samples\n");
    printf("  -b, --buffer-size <size>      Specify the buffer size (multiple of 1024)\n");
    printf("  -h, --help                    Display this help message\n");
    printf("\n");

    printf("Description:\n");
    printf("The streaming test allows for cross testing between MIMO, SISO, TX, RX, meta,\n");
    printf("and non-meta configurations.\n");
    printf("\n");

    printf("Example: TX a CW at 50MSPS and at a frequency of (915MHz+50MHz/16) with meta enabled\n");
    printf("./output/libbladeRF_test_streaming -te -s 50M\n");
    printf("\n");
    printf("Example: TX and RX at 25MSPS and at a frequency of (915MHz+25MHz/16) without meta enabled\n");
    printf("./output/libbladeRF_test_streaming -tr -s 25M\n");
}

int main(int argc, char *argv[])
{
    int status = 0;
    struct bladerf *dev = NULL;
    const char *fpga_file = NULL;
    const char *device_string = NULL;
    int c;
    bool ok;

    test_config config = {
        .tx            = false,
        .rx            = false,
        .mimo          = false,
        .sample_rate   = 61.44e6,
        .num_samples   = 5 * 61.44e6,
        .tx_samples    = NULL,
        .format        = BLADERF_FORMAT_SC16_Q11,
        .num_buffers   = 512,
        .buffer_size   = 4096,
        .num_transfers = 64,
        .timeout_ms    = 1000,
        .tx_meta.flags = BLADERF_META_FLAG_TX_BURST_START |
                         BLADERF_META_FLAG_TX_NOW |
                         BLADERF_META_FLAG_TX_BURST_END,
        .rx_meta.flags = BLADERF_META_FLAG_RX_NOW,
    };

    bladerf_log_set_verbosity(BLADERF_LOG_LEVEL_INFO);

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"device",      required_argument, 0, 'd'},
            {"fpga",        required_argument, 0, 'f'},
            {"verbosity",   no_argument,       0, 'v'},
            {"tx",          no_argument,       0, 't'},
            {"rx",          no_argument,       0, 'r'},
            {"mimo",        no_argument,       0, 'm'},
            {"meta",        no_argument,       0, 'e'},
            {"samplerate",  required_argument, 0, 's'},
            {"num-samples", required_argument, 0, 'n'},
            {"buffer-size", required_argument, 0, 'b'},
            {"help",        no_argument,       0, 'h'},
            {0, 0, 0, 0}
        };

        c = getopt_long(argc, argv, "d:f:vtrmes:n:b:h", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
            case 'd':
                device_string = optarg;
                break;
            case 'f':
                fpga_file = optarg;
                break;
            case 'v':
                bladerf_log_set_verbosity(BLADERF_LOG_LEVEL_VERBOSE);
                break;
            case 't':
                config.tx = true;
                break;
            case 'r':
                config.rx = true;
                break;
            case 'm':
                config.mimo = true;
                break;
            case 'n':
                config.num_samples = str2uint64_suffix(
                    optarg, 1, UINT64_MAX, freq_suffixes,
                    sizeof(freq_suffixes) / sizeof(freq_suffixes[0]), &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid number of samples: %s\n", optarg);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'b': {
                size_t requested_size = strtoull(optarg, NULL, 0);
                config.buffer_size = ((requested_size + BUFFER_SIZE_ALIGNMENT - 1) / BUFFER_SIZE_ALIGNMENT) * BUFFER_SIZE_ALIGNMENT;
                if (requested_size % BUFFER_SIZE_ALIGNMENT != 0) {
                    fprintf(stderr, "Requested buffer size rounded up to %zu\n", config.buffer_size);
                }
                break;
            }
            case 'e':
                config.format = BLADERF_FORMAT_SC16_Q11_META;
                break;
            case 's':
                config.sample_rate = str2uint64_suffix(
                    optarg, 0, UINT64_MAX, freq_suffixes,
                    sizeof(freq_suffixes) / sizeof(freq_suffixes[0]), &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid sample rate: %s\n", optarg);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'h':
            case '?':
                print_help();
                exit(EXIT_FAILURE);
            default:
                printf("?? getopt returned character code 0%o ??\n", c);
        }
    }

    if (!config.tx && !config.rx) {
        fprintf(stderr, "[Error] TX or RX must be specified.\n");
        print_help();
        exit(EXIT_FAILURE);
    }

    print_config(&config, device_string, fpga_file);

    // Allocate memory for samples (2* for IQ components and 2* for MIMO)
    config.tx_samples = malloc(4 * config.num_samples * sizeof(int16_t));
    config.rx_samples = malloc(4 * config.num_samples * sizeof(int16_t));
    if (config.tx_samples == NULL || config.rx_samples == NULL) {
        fprintf(stderr, "Failed to allocate memory for samples\n");
        exit(EXIT_FAILURE);
    }

    printf("Opening device: %s\n", (device_string == NULL) ? "any" : device_string);
    CHECK_STATUS(bladerf_open(&dev, device_string));

    if (fpga_file != NULL) {
        printf("Loading the FPGA image...\n");
        CHECK_STATUS(bladerf_load_fpga(dev, fpga_file));
    }

    printf("Setting sample rate to %0.3f MHz...\n", config.sample_rate / 1e6);
    CHECK_STATUS(bladerf_set_sample_rate(dev, BLADERF_CHANNEL_RX(0), config.sample_rate, NULL));
    CHECK_STATUS(bladerf_set_sample_rate(dev, BLADERF_CHANNEL_TX(0), config.sample_rate, NULL));

    printf("Setting frequency to 915 MHz...\n");
    CHECK_STATUS(bladerf_set_frequency(dev, BLADERF_CHANNEL_RX(0), 915e6));
    CHECK_STATUS(bladerf_set_frequency(dev, BLADERF_CHANNEL_TX(0), 915e6));

    CHECK_STATUS(bladerf_set_gain(dev, BLADERF_CHANNEL_TX(0), 30));
    CHECK_STATUS(bladerf_set_gain(dev, BLADERF_CHANNEL_RX(0), 0));

    for (size_t i = 0; i < config.num_samples; i++) {
        double angle = (2.0 * M_PI * (i % 16)) / 16.0;
        config.tx_samples[2 * i] = (int16_t)round(INT12_MAX * cos(angle));     // I component
        config.tx_samples[2 * i + 1] = (int16_t)round(INT12_MAX * sin(angle)); // Q component
    }

    if (config.mimo) {
        for (size_t i = 0; i < config.num_samples; i++) {
            double angle = (2.0 * M_PI * (i % 16)) / 16.0;
            config.tx_samples[4 * i + 0] = (int16_t)round(INT12_MAX * cos(angle)); // Ch0 I
            config.tx_samples[4 * i + 1] = (int16_t)round(INT12_MAX * sin(angle)); // Ch0 Q
            config.tx_samples[4 * i + 2] = (int16_t)round(INT12_MAX * cos(angle)); // Ch1 I
            config.tx_samples[4 * i + 3] = (int16_t)round(INT12_MAX * sin(angle)); // Ch1 Q
        }
    }

    if (config.tx) {
        printf("Streaming TX...\n");
        CHECK_STATUS(bladerf_sync_config(
            dev, (config.mimo == true) ? BLADERF_TX_X2 : BLADERF_TX_X1,
            config.format, config.num_buffers, config.buffer_size,
            config.num_transfers, config.timeout_ms));
        CHECK_STATUS(bladerf_enable_module(dev, BLADERF_CHANNEL_TX(0), true));
        if (config.mimo)
            CHECK_STATUS(bladerf_enable_module(dev, BLADERF_CHANNEL_TX(1), true));
        CHECK_STATUS(bladerf_sync_tx(dev, config.tx_samples, config.num_samples,
            &config.tx_meta, 5000));
        CHECK_STATUS(bladerf_enable_module(dev, BLADERF_CHANNEL_TX(0), false));
    }

    if (config.rx) {
        printf("Streaming RX...\n");
        CHECK_STATUS(bladerf_sync_config(
            dev, (config.mimo == true) ? BLADERF_RX_X2 : BLADERF_RX_X1,
            config.format, config.num_buffers, config.buffer_size,
            config.num_transfers, config.timeout_ms));

        CHECK_STATUS(bladerf_enable_module(dev, BLADERF_CHANNEL_RX(0), true));
        if (config.mimo)
            CHECK_STATUS(bladerf_enable_module(dev, BLADERF_CHANNEL_RX(1), true));

        /* Retrieve the current timestamp */
        config.rx_meta.flags = 0;
        status = bladerf_get_timestamp(dev, BLADERF_RX, &config.rx_meta.timestamp);
        if (status != 0) {
            fprintf(stderr, "Failed to get current RX timestamp: %s\n",
                    bladerf_strerror(status));
        } else {
            printf("Current RX timestamp: 0x%016" PRIx64 "\n", config.rx_meta.timestamp);
        }

        /* Schedule first RX to be 0.1s in the future */
        config.rx_meta.timestamp += config.sample_rate / 10;
        printf("Scheduling RX for 0x%016" PRIx64 "\n", config.rx_meta.timestamp);

        status = bladerf_sync_rx(dev, config.rx_samples, config.num_samples,
            &config.rx_meta, config.timeout_ms);
        if (status != 0) {
            fprintf(stderr, "RX failed: %s\n", bladerf_strerror(status));
        } else if (config.rx_meta.status & BLADERF_META_STATUS_OVERRUN) {
            fprintf(stderr, "Overrun detected. %u valid samples were read.\n",
            config.rx_meta.actual_count);
            print_bladerf_flag_statuses(config.rx_meta.status);
        } else {
            printf("RX'd %u samples at t=0x%016" PRIx64 "\n", config.rx_meta.actual_count,
            config.rx_meta.timestamp);
        }
        CHECK_STATUS(bladerf_enable_module(dev, BLADERF_CHANNEL_RX(0), false));
    }

error:
    if (config.tx_samples)
        free(config.tx_samples);
    if (config.rx_samples)
        free(config.rx_samples);
    if (dev)
        bladerf_close(dev);

    return status;
}
