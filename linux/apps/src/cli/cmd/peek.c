#include "cmd.h"
#include <stdio.h>

void invalid_address( char *fn, char *str ) {
    printf( "%s: %s is not a valid address to read\n", fn, str ) ;
    return ;
}

int cmd_peek(struct cli_state *state, int argc, char **argv)
{
    /* Valid commands:
        peek dac <address> [num addresses]
        peek lms <address> [num addresses]
        peek si  <address> [num addresses]
    */
    int rv = CMD_RET_OK ;
    unsigned int i ;
    bool ok ;
    if( argc == 3 || argc == 4 ) {
        unsigned int count = 1 ;
        unsigned int address ;

        /* Parse the number of addresses */
        if( argc == 4 ) {
            count = str2uint( argv[3], 0, 128, &ok ) ;
            if( !ok ) {
                printf( "%s: %s not a valid unsigned value, defaulting to 1\n", argv[0], argv[3] );
                count = 1;
            }
        }

        /* Are we reading from the DAC? */
        if( strcasecmp( argv[1], "dac" ) == 0 ) {
            /* Parse address */
            /* TODO: Make sure min/max is correct */
            address = str2uint( argv[2], 0, 128, &ok ) ;
            if( !ok ) {
                invalid_address( argv[0], argv[2] );
                rv = CMD_RET_INVPARAM;
            } else {
                for( i = 0 ; i < count ; i++ ) {
                    /* TODO: Read and print */
                }
            }
        }

        /* Are we reading from the LMS6002D */
        else if( strcasecmp( argv[1], "lms" ) == 0 ) {
            /* Parse address */
            /* TODO: Make sure min/max is correct */
            address = str2uint( argv[2], 0, 128, &ok ) ;
            if( !ok ) {
                invalid_address( argv[0], argv[2] );
                rv = CMD_RET_INVPARAM ;
            } else {
                for( i = 0 ; i < count ; i++ ) {
                    /* TODO: Read and print */
                }
            }
        }

        /* Are we reading from the Si5338? */
        else if( strcasecmp( argv[1], "si" ) == 0 ) {
            /* Parse address */
            /* TODO: Make sure min/max is correct */
            address = str2uint( argv[2], 0, 128, &ok ) ;
            if( !ok ) {
                invalid_address( argv[0], argv[2] );
                rv = CMD_RET_INVPARAM ;
            } else {
                for( i = 0 ; i < count ; i++ ) {
                    /* TODO: Read and print */
                }
            }
        }

        /* I guess we aren't reading from anything :( */
        else {
            printf( "%s: %s is not a peekable device\n", argv[0], argv[1] );
            rv = CMD_RET_INVPARAM ;
        }

        /* TODO Just suppressing warning until this is put to use... */
        (void)address;

    } else {
        printf( "%s: Invalid number of arguments (%d)\n", argv[0], argc );
        rv = CMD_RET_INVPARAM;
    }
    return rv;
}

