#include <stdio.h>
#include <limits.h>
#include <stdbool.h>
#include <conversions.h>
#include "cmd.h"

int cmd_recover(struct cli_state *state, int argc, char **argv)
{
    int status;

    if (argc != 3) {
        return CMD_RET_NARGS;
    }

    status = bladerf_recover(argv[1], argv[2]);

    if (status == 0) {
        printf("Loaded! An open is now required\n");
        return CMD_RET_OK;
    } else {
        state->last_lib_error = status;
        return CMD_RET_LIBBLADERF;
    }
}


