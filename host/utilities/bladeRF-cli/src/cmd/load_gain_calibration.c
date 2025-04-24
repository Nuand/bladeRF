#include <stdio.h>
#include <string.h>
#include "cmd.h"
#include "input.h"

int cmd_load_gain_calibration(struct cli_state *state, int argc, char **argv)
{
    /* Valid commands:
        load_gain <rx|tx> <filename>
    */
    int rv = CLI_RET_OK;

    if ( argc == 3 ) {
        int lib_status = 0;
        struct bladerf *dev = state->dev;
        char *expanded_path = input_expand_path(argv[2]);

        if (expanded_path == NULL) {
            cli_err(state, NULL,
                    "Unable to expand file path: \"%s\"\n", argv[2]);
            return CLI_RET_INVPARAM;
        }

        if (!strcasecmp(argv[1], "rx")) {

            printf("\n  Loading RX Gain table from %s...\n\n", expanded_path);
            lib_status = bladerf_load_gain_calibration(dev, BLADERF_CHANNEL_RX(0), expanded_path);
            if (lib_status == 0) {
                printf("  RX load success!\n\n");
            }

        } else if (!strcasecmp(argv[1], "tx")) {

            printf("\n  Loading TX Gain table from %s...\n\n", expanded_path);
            lib_status = bladerf_load_gain_calibration(dev, BLADERF_CHANNEL_TX(0), expanded_path);
            if (lib_status == 0) {
                printf("  TX load success!\n\n");
            }

        } else {
            cli_err(state, argv[0], "  Invalid type: %s\n", argv[1]);
            rv = CLI_RET_INVPARAM;
        }

        free(expanded_path);
        if (lib_status < 0) {
            state->last_lib_error = lib_status;
            rv = CLI_RET_LIBBLADERF;
        }

    } else {
        rv = CLI_RET_NARGS;
    }

    return rv;
}

