#include "cmd.h"
#include <stdio.h>

int cmd_version(struct cli_state *state, int argc, char **argv)
{
    printf( "Inside version\n" );
    return CMD_RET_OK;
}

