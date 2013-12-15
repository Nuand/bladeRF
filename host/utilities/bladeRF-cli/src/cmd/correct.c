/*
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2013 Nuand LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <stdio.h>
#include <string.h>
#include <libbladeRF.h>

#include "cmd.h"
#include "conversions.h"

#define MAX_DC_OFFSET 15
#define MAX_PHASE (2048)
#define MAX_GAIN (2048)

int cmd_correct(struct cli_state *state, int argc, char **argv)
{
    int rv = CMD_RET_OK;
    int status;
    bool ok = true;
    bool isTx = false;

    if (!cli_device_is_opened(state))
    {
        return CMD_RET_NODEV;
    }


        //get the direction to print
    if (argc >= 2)
    {
        if (!strcasecmp(argv[1],"tx"))
            isTx = true;
        else if(!strcasecmp(argv[1],"rx"))
            isTx = false;
        else
            return CMD_RET_INVPARAM;
    }

    if( argc == 2 ) 
    {
        int16_t dc_i, dc_q, phase,gain;
        //argv[1]
        if(isTx)
        {
            status = bladerf_print_correction(state->dev,BLADERF_IQ_CORR_TX_DC_I, &dc_i);
            status = bladerf_print_correction(state->dev,BLADERF_IQ_CORR_TX_DC_Q, &dc_q);
            status = bladerf_print_correction(state->dev,BLADERF_IQ_CORR_TX_PHASE, &phase);
            status = bladerf_print_correction(state->dev,BLADERF_IQ_CORR_TX_GAIN, &gain);
        }
        else
        {
			status = bladerf_print_correction(state->dev,BLADERF_IQ_CORR_RX_DC_I, &dc_i);
            status = bladerf_print_correction(state->dev,BLADERF_IQ_CORR_RX_DC_Q, &dc_q);
            status = bladerf_print_correction(state->dev,BLADERF_IQ_CORR_RX_PHASE, &phase);
            status = bladerf_print_correction(state->dev,BLADERF_IQ_CORR_RX_GAIN, &gain);
        }
        printf("Current Settings: DC OFFSET I=%d Q=%d, Phase=%d, Gain=%d\n",dc_i,dc_q,phase,gain);
    }
    /* Parse the value */
    else if( argc == 4 ) 
    {
		int16_t value = 0;

        if (!strcasecmp(argv[2],"phase"))
        {
            value = str2int( argv[3], -MAX_PHASE ,MAX_PHASE, &ok );
			if(isTx)
				status = bladerf_set_correction(state->dev,BLADERF_IQ_CORR_TX_PHASE, value);
			else
				status = bladerf_set_correction(state->dev,BLADERF_IQ_CORR_RX_PHASE, value);
					
        }
        else if (!strcasecmp(argv[2],"gain"))
        {
            value = str2int( argv[3], -MAX_GAIN ,MAX_GAIN, &ok );
			if(isTx)
				status = bladerf_set_correction(state->dev,BLADERF_IQ_CORR_TX_GAIN, value);
			else
				status = bladerf_set_correction(state->dev,BLADERF_IQ_CORR_RX_GAIN, value);
        }
        else
        {
            rv = CMD_RET_INVPARAM;
        }
    }
    else if (argc == 5) //dc correction requires 2 parameters
    {

        if (!strcasecmp(argv[2],"dc"))
        {
			int16_t val_i,val_q;
			
            val_i = str2int( argv[3], -MAX_DC_OFFSET ,MAX_DC_OFFSET, &ok );
            val_q = str2int( argv[4], -MAX_DC_OFFSET ,MAX_DC_OFFSET, &ok );
			if(isTx)
			{
				status = bladerf_set_correction(state->dev,BLADERF_IQ_CORR_TX_DC_I, val_i);
				status = bladerf_set_correction(state->dev,BLADERF_IQ_CORR_TX_DC_Q, val_q);
			}
			else
			{
				status = bladerf_set_correction(state->dev,BLADERF_IQ_CORR_RX_DC_I, val_i);
				status = bladerf_set_correction(state->dev,BLADERF_IQ_CORR_RX_DC_Q, val_q);
			}

        }

    }     
    else {
        cli_err(state, argv[0], "Invalid number of arguments (%d)\n", argc);
        rv = CMD_RET_INVPARAM;
    }
    return rv;
}

