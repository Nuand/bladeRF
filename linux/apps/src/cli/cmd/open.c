#include <stdio.h>
#include "cmd.h"

/* Usage:
 *  open <device>
 */
int cmd_open(struct cli_state *state, int argc, char **argv)
{
    if (argc < 2) {
        printf("%s: No device specified\n", argv[0]);
        return CMD_RET_INVPARAM;
    } else if (argc > 2) {
        printf("%s: Extraneous arguments encountered at \"%s\"\n",
                argv[0], argv[1]);
        return CMD_RET_INVPARAM;
    }

    if (state->curr_device) {
        bladerf_close(state->curr_device);
    }

    state->curr_device = bladerf_open(argv[1]);
    if (!state->curr_device) {
        state->last_lib_error = BLADERF_ERR_IO;
        return CMD_RET_LIBBLADERF;
    } else {
        return 0;
    }
}

