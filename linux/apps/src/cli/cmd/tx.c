#include "cmd.h"
#include <stdio.h>

int cmd_tx(struct cli_state *state, int argc, char **argv)
{
    printf( "Inside tx\n" );
    return CMD_RET_OK;
}

