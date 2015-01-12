#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <libbladeRF.h>

static const struct option long_options[] = {
    { "device",     required_argument,  NULL,   'd' },
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

    opt = 0;
    bladerf_log_set_verbosity(BLADERF_LOG_LEVEL_VERBOSE);

    while (opt != -1) {
        opt = getopt_long(argc, argv, "d:rnh", long_options, &opt_ind);

        switch (opt) {
            case 'd':
                devstr = optarg;
                break;

            case 'r':
                /* This is the default option, so it's actually redundant to
                 * do this. However, this is here to allow us to exercise the
                 * API call. */
                bladerf_set_usb_reset_on_open(true);
                break;

            case 'n':
                bladerf_set_usb_reset_on_open(false);
                break;

            case 'h':
                printf("Usage: %s [options]\n", argv[0]);
                printf("  -d, --device <str>    Specify device to open.\n");
                printf("  -r, --reset           Enable USB reset on open.\n");
                printf("  -n, --no-reset        Disable USB reset on open.\n");
                printf("  -h, --help            Show this text.\n");
                printf("\n");
                return 0;

            default:
                break;
        }
    }

    status = bladerf_open(&dev, devstr);
    if (status != 0) {
        fprintf(stderr, "Failed to open device: %s\n",
                bladerf_strerror(status));
        return 1;
    }

    puts("\n\nPress any key to close the device and exit.\n");
    (void) getchar();

    bladerf_close(dev);

    return 0;
}
