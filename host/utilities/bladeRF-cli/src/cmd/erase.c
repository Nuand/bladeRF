#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <conversions.h>
#include "cmd.h"

int cmd_erase(struct cli_state *state, int argc, char **argv)
{
    int status;
    int page_offset, n_bytes;
    bool ok;

    if (!cli_device_is_opened(state)) {
        return CMD_RET_NODEV;
    }

    if (argc != 3) {
        return CMD_RET_NARGS;
    }

    page_offset = str2uint(argv[1], 0, INT_MAX, &ok);
    if(!ok) {
        cli_err(state, argv[0], "Invalid value for \"page_offset\" (%s)", argv[1]);
        return CMD_RET_INVPARAM;
    }

    n_bytes = str2uint(argv[2], 0, INT_MAX, &ok);
    if(!ok) {
        cli_err(state, argv[0], "Invalid value for \"n_bytes\" (%s)", argv[2]);
        return CMD_RET_INVPARAM;
    }

    status = bladerf_erase_flash(state->dev, page_offset, n_bytes);

    if (status >= 0) {
        printf("Erased %d pages at %d\n", status, page_offset);
        return CMD_RET_OK;
    } else {
        state->last_lib_error = status;
        return CMD_RET_LIBBLADERF;
    }
}

