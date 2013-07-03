#include "cmd.h"
#include <stdio.h>



int cmd_rx(struct cli_state *state, int argc, char **argv)
{
    /* Valid commands:
        rx <filename> <ascii|binary> [num_samples]
    */
    int rv = CMD_RET_OK;

    if( argc == 3 || argc == 4 ) {
        unsigned int num_samples = 0;
        bool ok;
        /* Check format */
        if( strcasecmp( argv[2], "ascii" ) == 0 ) {

        } else if( strcasecmp( argv[2], "binary" ) == 0 ) {

        } else {
            cli_err(state, argv[0], "\"%s\" is an invalid format\n", argv[2] );
            rv = CMD_RET_INVPARAM;
        }

        /* Validate num_samples */
        if( argc == 4 ) {
            num_samples = str2uint( argv[3], 0, 1000000000, &ok );
            if( !ok ) {
                rv = CMD_RET_INVPARAM;
                goto done;
            }
        } else {
            /* Indefinite reception */
            num_samples = 0;
        }

        /* TODO: Validate filename */

        /* Read samples from the device until either:
            num_samples has been reached ... or
            if num_samples has not been supplied, the user hits a key */
        if( num_samples > 0 ) {
            /* TODO: Write num_samples worth of data to the file in the appropriate format */
        } else {
            /* TODO: Indefinite reception until keypress */
        }
    } else {
        printf( "%s: Invalid number of parameters (%d)\n", argv[0], argc );
        rv = CMD_RET_INVPARAM;
    }

done:
    return rv;
}

