#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <limits.h>
#include <libbladeRF.h>

#include "conversions.h"

static const struct option long_options[] = {
    { "device",     required_argument,  NULL,   'd' },
    { "count",      required_argument,  NULL,   'c' },
    { "wait",       required_argument,  NULL,   'w' },
    { "reset",      no_argument,        NULL,   'r' },
    { "no-reset",   no_argument,        NULL,   'n' },
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
    bool reset_on_open = true;
    unsigned int count = 1;
    unsigned int wait = 250;
    unsigned int i;
    bool ok;
    bladerf_log_level log_level = BLADERF_LOG_LEVEL_INFO;

    opt = 0;

    while (opt != -1) {
        opt = getopt_long(argc, argv, "d:c:w:rnv:h", long_options, &opt_ind);

        switch (opt) {
            case 'd':
                devstr = optarg;
                break;

            case 'c':
                count = str2uint(optarg, 1, UINT_MAX, &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid count: %s\n", optarg);
                    return -1;
                }
                break;

            case 'w':
                wait = str2uint(optarg, 1, 60 * 60 * 1000, &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid wait period: %s\n", optarg);
                    return -1;
                } else {
                    wait *= 1000;
                }
                break;

            case 'r':
                reset_on_open = true;
                break;

            case 'n':
                reset_on_open = false;
                break;

            case 'v':
                log_level = str2loglevel(optarg, &ok);
                if (!ok) {
                    fprintf(stderr, "Invalid log level: %s\n", optarg);
                    return -1;
                }
                break;

            case 'h':
                printf("Usage: %s [options]\n", argv[0]);
                printf("  -d, --device <str>    Specify device to open.\n");
                printf("  -c, --count <n>       Number of times to open/close.\n");
                printf("  -w, --wait <n>        Wait <n> ms before closing.\n");
                printf("  -r, --reset           Enable USB reset on open.\n");
                printf("  -n, --no-reset        Disable USB reset on open.\n");
                printf("  -v, --verbosity <l>   Set libbladeRF verbosity level.\n");
                printf("  -h, --help            Show this text.\n");
                printf("\n");
                return 0;

            default:
                break;
        }
    }

    bladerf_log_set_verbosity(log_level);

    bladerf_set_usb_reset_on_open(reset_on_open);

    for (i = 0; i < count; i++) {
        status = bladerf_open(&dev, devstr);
        if (status != 0) {
            fprintf(stderr, "Failed to open device: %s\n",
                    bladerf_strerror(status));
        } else {
            usleep(wait);
            bladerf_close(dev);
            printf("Iteration %u complete.\n", i + 1);
        }
    }


    return 0;
}
