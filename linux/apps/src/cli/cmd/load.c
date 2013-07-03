#include "cmd.h"
#include <stdio.h>

int cmd_load(struct cli_state *state, int argc, char **argv)
{
    /* Valid commands:
        load fpga <filename>
        load fx3 <filename>
    */
    int rv = CMD_RET_OK ;
    int librv;

    if (!state->curr_device) {
        /* FIXME: cmd_handle should probably print this, not this code */
        return CMD_RET_NODEV;
    }

    if( argc == 3 ) {
        if( strcasecmp( argv[1], "fpga" ) == 0 ) {
            printf("Loading fpga...\n");
            librv = bladerf_load_fpga(state->curr_device, argv[2]);

            if (librv < 0) {
                state->last_lib_error = librv;
                rv = CMD_RET_LIBBLADERF;
            } else {
                printf("Done.\n");
            }
        } else if( strcasecmp( argv[1], "fx3" ) == 0 ) {
            printf("Flashing firmware...\n");
            librv = bladerf_flash_firmware(state->curr_device, argv[2]);

            if (librv < 0) {
                state->last_lib_error = librv;
                rv = CMD_RET_LIBBLADERF;
            } else {
                printf("Done.\n");
            }
        } else {
            cli_err(state, argv[0],
                    "\"%s\" is not a valid programming target\n", argv[1]) ;
            rv = CMD_RET_INVPARAM;
        }
    } else {
        rv = CMD_RET_NARGS;
    }

    return rv;
}

