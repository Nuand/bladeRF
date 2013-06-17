#include <stdio.h>
#include <libbladeRF.h>

#include "cmd.h"
#include "peekpoke.h"

int cmd_poke(struct cli_state *state, int argc, char **argv)
{
    /* Valid commands:
        poke dac <address> <value>
        poke lms <address> <value>
        poke si  <address> <value>
    */
    int rv = CMD_RET_OK;
    bool ok;
    int (*f)(struct bladerf *, uint8_t, uint8_t);
    unsigned int address, value;

    if( state->curr_device == NULL ) {
        /* TODO: Should this be checked before calling the command? */
        return CMD_RET_NODEV;
    }

    if( argc == 4 ) {

        /* Parse the value */
        if( argc == 4 ) {
            value = str2uint( argv[3], 0, MAX_VALUE, &ok );
            if( !ok ) {
                printf( "%s: %s not a valid unsigned value, defaulting to 1\n", argv[0], argv[3] );
                rv = CMD_RET_INVPARAM;
            }
        }

        /* Are we reading from the DAC? */
        if( strcasecmp( argv[1], "dac" ) == 0 ) {
            /* Parse address */
            address = str2uint( argv[2], 0, DAC_MAX_ADDRESS, &ok );
            if( !ok ) {
                invalid_address( argv[0], argv[2] );
                rv = CMD_RET_INVPARAM;
            } else {
                /* TODO: Point function pointer */
                /* f = vctcxo_dac_write */
                f = NULL;
            }
        }

        /* Are we reading from the LMS6002D */
        else if( strcasecmp( argv[1], "lms" ) == 0 ) {
            /* Parse address */
            address = str2uint( argv[2], 0, LMS_MAX_ADDRESS, &ok );
            if( !ok ) {
                invalid_address( argv[0], argv[2] );
                rv = CMD_RET_INVPARAM;
            } else {
                f = lms_spi_write;
            }
        }

        /* Are we reading from the Si5338? */
        else if( strcasecmp( argv[1], "si" ) == 0 ) {
            /* Parse address */
            address = str2uint( argv[2], 0, SI_MAX_ADDRESS, &ok );
            if( !ok ) {
                invalid_address( argv[0], argv[2] );
                rv = CMD_RET_INVPARAM;
            } else {
                f = si5338_i2c_write;
            }
        }

        /* I guess we aren't reading from anything :( */
        else {
            printf( "%s: %s is not a pokeable device\n", argv[0], argv[1] );
            rv = CMD_RET_INVPARAM;
        }

        /* Write the value to the address */
        if( rv == CMD_RET_OK && f ) {
            int ret;
            ret = f( state->curr_device, (uint8_t)address, (uint8_t)value );
            printf( "  %2.2x: %2.2x\n", address, value );
        }

    } else {
        printf( "%s: Invalid number of arguments (%d)\n", argv[0], argc );
        rv = CMD_RET_INVPARAM;
    }
    return rv;
}

