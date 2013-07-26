#include <stdio.h>
#include <libbladeRF.h>

#include "common.h"
#include "cmd.h"
#include "peekpoke.h"

int cmd_peek(struct cli_state *state, int argc, char **argv)
{
    /* Valid commands:
        peek dac <address> [num addresses]
        peek lms <address> [num addresses]
        peek si  <address> [num addresses]
    */
    int rv = CMD_RET_OK;
    bool ok;
    int (*f)(struct bladerf *, uint8_t, uint8_t *);
    unsigned int count, address, max_address;

    if (!cli_device_is_opened(state)) {
        return CMD_RET_NODEV;
    }

    if( argc == 3 || argc == 4 ) {
        count = 1;

        /* Parse the number of addresses */
        if( argc == 4 ) {
            count = str2uint( argv[3], 0, MAX_NUM_ADDRESSES, &ok );
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
                /* f = vctcxo_dac_read */
                f = NULL;
                max_address = DAC_MAX_ADDRESS;
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
                f = lms_spi_read;
                max_address = LMS_MAX_ADDRESS;
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
                /* TODO: Populate this function when it is available */
                /* f = si5338_i2c_read; */
                f = NULL;
                max_address = SI_MAX_ADDRESS;
            }
        }

        /* I guess we aren't reading from anything :( */
        else {
            cli_err(state, argv[0], "%s is not a peekable device\n", argv[1]);
            rv = CMD_RET_INVPARAM;
        }

        /* Loop over the addresses and output the values */
        if( rv == CMD_RET_OK && f ) {
            int status;
            uint8_t val;
            for(; count > 0 && address < max_address; count-- ) {
                status = f( state->dev, (uint8_t)address, &val );
                if (status < 0) {
                    state->last_lib_error = status;
                    rv = CMD_RET_LIBBLADERF;
                } else {
                    printf( "  0x%2.2x: 0x%2.2x\n", address++, val );
                }
            }
        }

    } else {
        cli_err(state, argv[0], "Invalid number of arguments (%d)\n",  argc);
        rv = CMD_RET_INVPARAM;
    }
    return rv;
}

