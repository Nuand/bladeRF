/*
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2014 Nuand LLC
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
#include "common.h"
#include <string.h>
#include <xb.h>
#include <xb200.h>

int cmd_xb200(struct cli_state *state, int argc, char **argv)
{
    int status = 0;

    if (NULL == state->dev) {
        printf("  No device is currently opened\n");
        return 0;
    }

    if (argc < 3) {
        // incorrect number of arguments
        return CLI_RET_NARGS;
    }

    // xb 200 <subcommand> <args>
    unsigned int modelnum = atoi(argv[1]);
    const char *subcommand = argv[2];

    if (modelnum != MODEL_XB200) {
        return CLI_RET_INVPARAM;
    }
 
    // xb 200 enable
    if (!strcasecmp(subcommand, "enable")) {
        printf(" Enabling XB-200 transverter board\n");
        status = bladerf_expansion_attach(state->dev, BLADERF_XB_200);
        if (status != 0) {
            state->last_lib_error = status;
            return CLI_RET_LIBBLADERF;
        }
        printf(" XB-200 Transverter board successfully enabled\n");
    }

    // xb 200 filter xx yy  - where xx is tx or rx and yy is the filter to be selected
    else if (!strcasecmp(subcommand, "filter")) {
        if (5 != argc) {
            return CLI_RET_NARGS;
        }

        const char *modulename = argv[3];
        const char *filtername = argv[4];
        status = cmd_xb200_filter(state, modulename, filtername);
        if ((status != 0) && (status != CLI_RET_INVPARAM) &&
            (status != CLI_RET_NARGS)) {
            state->last_lib_error = status;
            return CLI_RET_LIBBLADERF;
        }
    }

    // unknown subcommand
    else {
        status = CLI_RET_INVPARAM;
    }

    return status;
}

int cmd_xb200_filter(struct cli_state *state, const char *modulename, const char *filtername)
{
    int status = 0;

    if ( (NULL == state) || (NULL == modulename) || (NULL == filtername) ) {
        return CLI_RET_INVPARAM;
    }

    bladerf_xb200_filter filter;
    bladerf_module mod;

    // get which module to set filter for
    if (!strcasecmp(modulename, "rx"))
        mod = BLADERF_MODULE_RX;
    else if (!strcasecmp(modulename, "tx"))
        mod = BLADERF_MODULE_TX;
    else return CLI_RET_INVPARAM;

    // get which filter to set
    if (!strcmp(filtername, "50"))
        filter = BLADERF_XB200_50M;
    else if (!strcmp(filtername, "144"))
        filter = BLADERF_XB200_144M;
    else if (!strcmp(filtername, "222"))
        filter = BLADERF_XB200_222M;
    else if (!strcmp(filtername, "custom"))
        filter = BLADERF_XB200_CUSTOM;
    else if (!strcmp(filtername, "auto_1db"))
        filter = BLADERF_XB200_AUTO_1DB;
    else if (!strcmp(filtername, "auto_3db"))
        filter = BLADERF_XB200_AUTO_3DB;
    else return CLI_RET_INVPARAM;

    status = bladerf_xb200_set_filterbank(state->dev, mod, filter);

    if (status != 0) {
        state->last_lib_error = status;
        return CLI_RET_LIBBLADERF;
    }

    printf(" XB-200 Transverter board %s filter successfully set to %s\n", modulename, filtername);

    return status;
}

