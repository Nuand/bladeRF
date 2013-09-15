#include <stdio.h>
#include <limits.h>
#include <stdbool.h>
#include <conversions.h>
#include "cmd.h"

int cmd_jump_to_bootloader(struct cli_state *state, int argc, char **argv)
{
    int status;

    if (!cli_device_is_opened(state)) {
        return CMD_RET_NODEV;
    }

    if (argc != 1) {
        return CMD_RET_NARGS;
    }

    status = bladerf_jump_to_bootloader(state->dev);

    if (status == 0) {
        return CMD_RET_OK;
    } else {
        state->last_lib_error = status;
        return CMD_RET_LIBBLADERF;
    }
}

