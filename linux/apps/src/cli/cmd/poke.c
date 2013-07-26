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
    int status;
    bool ok;
    int (*f)(struct bladerf *, uint8_t, uint8_t);
    unsigned int address, value;

    if (!cli_device_is_opened(state)) {
        return CMD_RET_NODEV;
    }

    if( argc == 4 ) {

        /* Parse the value */
        if( argc == 4 ) {
            value = str2uint( argv[3], 0, MAX_VALUE, &ok );
            if( !ok ) {
                cli_err(state, argv[0],
                        "Invalid number of addresses provided (%s)", argv[3]);
                return CMD_RET_INVPARAM;
            }
        }

        /* Are we reading from the DAC? */
        if( strcasecmp( argv[1], "dac" ) == 0 ) {
            /* Parse address */
            address = str2uint( argv[2], 0, DAC_MAX_ADDRESS, &ok );
            if( !ok ) {
                invalid_address(state, argv[0], argv[2]);
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
                invalid_address(state, argv[0], argv[2]);
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
                invalid_address(state, argv[0], argv[2]);
                rv = CMD_RET_INVPARAM;
            } else {
                f = si5338_i2c_write;
            }
        }

        /* I guess we aren't reading from anything :( */
        else {
            cli_err(state, argv[0], "%s is not a pokeable device\n", argv[1] );
            rv = CMD_RET_INVPARAM;
        }

        /* Write the value to the address */
        if( rv == CMD_RET_OK && f ) {
            status = f( state->dev, (uint8_t)address, (uint8_t)value );
            if (status < 0) {
                state->last_lib_error = status;
                rv = CMD_RET_LIBBLADERF;
            } else {
                printf( "  0x%2.2x: 0x%2.2x\n", address, value );
            }
        }

    } else {
        cli_err(state, argv[0], "Invalid number of arguments (%d)\n", argc);
        rv = CMD_RET_INVPARAM;
    }
    return rv;
}

