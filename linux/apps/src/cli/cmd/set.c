#include <stdio.h>
#include "cmd.h"

int cmd_set(struct cli_state *state, int argc, char **argv)
{
    int rv = CMD_RET_OK;
    printf( "Inside set!\n" );
    return rv;
}

