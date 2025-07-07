#include <stdlib.h>
#include <getopt.h>
#include "test_common.h"
#include "libbladeRF.h"
#include "log.h"
#include "conversions.h"

#define dword_t int32_t
#define DYNAMIC_PACKET_LEN 0

typedef struct test {
    bladerf_frequency frequency;
    bladerf_sample_rate samplerate;
    bladerf_bandwidth bandwidth;

    // Sync interface
    bladerf_channel channel;
    uint32_t num_buffers;
    uint32_t buffer_len;

    // Packets
    size_t num_dwords;
    size_t num_packets;
} test_t;

void test_config_init(test_t *t) {
    t->frequency    = 915e6;
    t->samplerate   = 10e6;
    t->bandwidth    = 10e6;

    t->channel      = BLADERF_CHANNEL_TX(0);
    t->num_buffers  = 512;
    t->buffer_len   = 4096;

    t->num_dwords   = DYNAMIC_PACKET_LEN;
    t->num_packets  = 10;
}

int bladerf_config(struct bladerf *dev, const test_t *test) {
    int status = 0;
    CHECK_NULL(dev, test);

    CHECK_STATUS(bladerf_set_frequency(dev, test->channel, test->frequency));
    CHECK_STATUS(bladerf_set_sample_rate(dev, test->channel, test->samplerate, NULL));
    CHECK_STATUS(bladerf_set_bandwidth(dev, test->channel, test->bandwidth, NULL));
    CHECK_STATUS(bladerf_sync_config(dev, test->channel, BLADERF_FORMAT_PACKET_META,
                                     test->num_buffers, test->buffer_len, 16, 3e6));

error:
    return status;
}

int send_packets_variable_len(struct bladerf *dev, const test_t *test) {
    CHECK_NULL(dev, test);
    int status = 0;
    struct bladerf_metadata meta;

    size_t dynamic_packet_len = 0;
    size_t packet_sizes[4] = {40, 17, 43, dynamic_packet_len};

    memset(&meta, 0, sizeof(meta));
    meta.flags = BLADERF_META_FLAG_TX_BURST_START | BLADERF_META_FLAG_TX_NOW | BLADERF_META_FLAG_TX_BURST_END;

    CHECK_STATUS(bladerf_enable_module(dev, test->channel, true));

    size_t packet_idx = 0;
    for (size_t i = 0; i < test->num_packets; i++) {
        dynamic_packet_len = 20 + (i % 300);
        packet_sizes[3] = dynamic_packet_len;

        size_t current_packet_len = packet_sizes[packet_idx % 4]; // Select packet size in round-robin fashion
        size_t packet_size = current_packet_len * sizeof(dword_t);
        dword_t *packet = malloc(packet_size);
        CHECK_NULL(packet);

        for (size_t j = 0; j < current_packet_len; j++) {
            packet[j] = j;
        }

        CHECK_STATUS(bladerf_sync_tx(dev, packet, bytes_to_dwords(packet_size), &meta, 3e3));

        free(packet);
        packet_idx++; // Move to the next packet size for the next iteration
    }

    CHECK_STATUS(bladerf_enable_module(dev, test->channel, false));

error:
    return status;
}

int send_packets_static_len(struct bladerf *dev, const test_t *test) {
    int status = 0;
    struct bladerf_metadata meta;
    CHECK_NULL(dev);

    size_t packets_size = test->num_dwords * sizeof(dword_t);
    dword_t *packet = malloc(packets_size);
    CHECK_NULL(packet);

    for (size_t i = 0; i < test->num_dwords; i++) {
        packet[i] = i;
    }

    memset(&meta, 0, sizeof(meta));
    meta.flags = BLADERF_META_FLAG_TX_BURST_START | BLADERF_META_FLAG_TX_NOW |
                 BLADERF_META_FLAG_TX_BURST_END;

    CHECK_STATUS(bladerf_enable_module(dev, test->channel, true));

    for (size_t i = 0; i < test->num_packets; i++) {
        CHECK_STATUS(bladerf_sync_tx(dev, packet, bytes_to_dwords(packets_size), &meta, 3e3));
    }

    CHECK_STATUS(bladerf_enable_module(dev, test->channel, false));

error:
    free(packet);
    return status;
}

static const struct option long_options[] = {
    { "packets",    required_argument,  NULL,   'n' },
    { "size",       required_argument,  NULL,   's' },
    { "device",     required_argument,  NULL,   'd' },
    { "verbosity",  required_argument,  NULL,   'v' },
    { "help",       no_argument,        NULL,   'h' },
    { NULL,         0,                  NULL,    0  },
};

int main(int argc, char *argv[])
{
    int status = 0;
    int opt = 0;
    int opt_ind = 0;
    bool ok;
    struct bladerf *dev;
    char *devstr = NULL;
    test_t test;
    bladerf_log_level log_level;

    test_config_init(&test);

    while ((opt = getopt_long(argc, argv, "n:s:d:v:h", long_options, &opt_ind)) != -1) {
        switch (opt) {
            case 'n':
                test.num_packets = strtoul(optarg, NULL, 0);
                break;

            case 's':
                test.num_dwords = strtoul(optarg, NULL, 0);
                break;

            case 'd':
                devstr = optarg;
                break;

            case 'v':
                log_level = str2loglevel(optarg, &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid log level: %s\n", optarg);
                    return -1;
                }
                bladerf_log_set_verbosity(log_level);
                break;

            case 'h':
                printf("Usage: %s [options]\n", argv[0]);
                printf("Options:\n");
                printf("  -n, --packets <n>         Number of packets to send.\n");
                printf("  -s, --size <n>            Number of dwords per packet.\n");
                printf("  -d, --device <str>        Specify the device to open. Allows selection of a specific\n");
                printf("                             bladeRF device if multiple are available.\n");
                printf("  -v, --verbosity <level>   Set libbladeRF verbosity level.\n");
                printf("  -h, --help                Show this help message and exit.\n");
                printf("\n");
                printf("The program transmits packets using the bladeRF library. If '--size 0' is used, packets\n");
                printf("will be sent with variable lengths in a round-robin fashion: 40, 17, 43 dwords, and\n");
                printf("a dynamic packet length of '20 + (packet_count %% 300)'. Otherwise, all packets\n");
                printf("will have a static length as specified by '--size'.\n");
                return 0;

            default:
                return EXIT_FAILURE;
        }
    }

    printf("Opening device: %s\n", (devstr == NULL) ? "any" : devstr);
    CHECK_STATUS(bladerf_open(&dev, devstr));
    CHECK_STATUS(bladerf_config(dev, &test));

    if (test.num_dwords == DYNAMIC_PACKET_LEN) {
        printf("Sending %zu packets with the following packet length and order:\n", test.num_packets);
        printf("Packet Lengths: [%i, %i, %i, 20 + (packets_sent %% 300)] dwords\n", 40, 17, 43);

        CHECK_STATUS(send_packets_variable_len(dev, &test));
    } else {
        printf("Sending %zu packets: %zu dwords/packet (%zu bytes/packet)\n",
               test.num_packets, test.num_dwords, test.num_dwords * sizeof(dword_t));

        CHECK_STATUS(send_packets_static_len(dev, &test));
    }

error:
    bladerf_close(dev);
    return (status == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
