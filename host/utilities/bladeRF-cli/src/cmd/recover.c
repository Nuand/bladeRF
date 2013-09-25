#include <stdio.h>
#include <limits.h>
#include <stdbool.h>
#include <conversions.h>
#include "cmd.h"
#include "interactive.h"

int cmd_recover(struct cli_state *state, int argc, char **argv)
{
    int status;
    char *dev_id, *path, *expanded_path;

    if (argc != 3) {
        return CMD_RET_NARGS;
    }

    dev_id = argv[1];
    path = argv[2];

    if ((expanded_path = interactive_expand_path(path)) == NULL) {
        cli_err(state, "Unable to expand FPGA file path: \"%s\"", path);
        return CMD_RET_INVPARAM;
    }

    status = bladerf_recover(dev_id, expanded_path);

    if (status == 0) {
        printf("Loaded! An open is now required\n");
        return CMD_RET_OK;
    } else {
        state->last_lib_error = status;
        return CMD_RET_LIBBLADERF;
    }
}


