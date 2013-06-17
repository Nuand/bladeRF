#include "cmd.h"
#include <stdio.h>

int cmd_poke(struct cli_state *state, int argc, char **argv)
{
    printf( "Inside poke!\n" ) ;
    return CMD_RET_OK ;
}
