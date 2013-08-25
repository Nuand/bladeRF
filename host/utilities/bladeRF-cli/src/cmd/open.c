#include <stdio.h>
#include <string.h>
#include "common.h"
#include "cmd.h"

/* Usage:
 *  open [device identifier items]
 *
 * It's a bit silly, but we just merge all the argv items into the
 * device identifier string here.
 */
int cmd_open(struct cli_state *state, int argc, char **argv)
{
    char *dev_ident = NULL;
    size_t dev_ident_len;
    int i;
    int status;

    /* Disallow opening of a diffrent device if the current one is doing work */
    if (cli_device_in_use(state)) {
        return CMD_RET_BUSY;
    }

    if (state->dev) {
        bladerf_close(state->dev);
    }

    /* We have some device indentifer items */
    if (argc > 1) {
        dev_ident_len = argc - 2; /* # spaces to place between args */

        for (i = 1; i < argc; i++) {
            dev_ident_len += strlen(argv[i]);
        }

        /* Room for '\0' terminator */
        dev_ident_len++;

        dev_ident = calloc(dev_ident_len, 1);
        if (!dev_ident) {
            return CMD_RET_MEM;
        }

        for (i = 1; i < argc; i++) {
            strncat(dev_ident, argv[i], (dev_ident_len - 1 - strlen(dev_ident)));

            if (i != (argc - 1)) {
                dev_ident[strlen(dev_ident)] = ' ';
            }
        }

        printf("Using device string: %s\n", dev_ident);
    }

    status = bladerf_open(&state->dev, dev_ident);
    if (status) {
        state->last_lib_error = status;
        return CMD_RET_LIBBLADERF;
    } else {
        return 0;
    }
}

