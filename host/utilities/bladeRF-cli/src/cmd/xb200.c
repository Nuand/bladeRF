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
#include <assert.h>
#include <string.h>
#include "common.h"
#include "xb.h"
#include "xb200.h"

int cmd_xb200(struct cli_state *state, int argc, char **argv)
{
    int status = 0;
    int modelnum = MODEL_INVALID;
    const char *subcommand = NULL;
    const char *modulename = NULL;
    const char *filtername = NULL;

    if (argc < 3) {
        // incorrect number of arguments
        return CLI_RET_NARGS;
    }

    // xb 200 <subcommand> <args>
    modelnum = atoi(argv[1]);
    subcommand = argv[2];

    if (modelnum != MODEL_XB200) {
        assert(0);  // developer error
        return CLI_RET_INVPARAM;
    }

    putchar('\n');

    // xb 200 enable
    if (!strcasecmp(subcommand, "enable")) {
        printf("  Enabling XB-200 transverter board\n");
        status = bladerf_expansion_attach(state->dev, BLADERF_XB_200);
        if (status != 0) {
            state->last_lib_error = status;
            return CLI_RET_LIBBLADERF;
        }
        printf("  XB-200 Transverter board successfully enabled\n\n");

    } else if (!strcasecmp(subcommand, "filter")) {

        // xb 200 filter <tx | rx> <filter>
        if (5 != argc) {
            return CLI_RET_NARGS;
        }

        modulename = argv[3];
        filtername = argv[4];
        status = cmd_xb200_filter(state, modulename, filtername);
        if ((status != 0) && (status != CLI_RET_INVPARAM) &&
            (status != CLI_RET_NARGS)) {
            state->last_lib_error = status;
            return CLI_RET_LIBBLADERF;
        }

    } else {
        // unknown subcommand
        cli_err(state, subcommand, "Invalid subcommand for xb %d\n", modelnum);
        status = CLI_RET_INVPARAM;
    }

    return status;
}

int cmd_xb200_filter(struct cli_state *state, const char *modulename,
                     const char *filtername)
{
    int status = 0;

    bladerf_xb200_filter filter = BLADERF_XB200_50M;
    bladerf_module mod = BLADERF_MODULE_RX;

    if ( (NULL == state) || (NULL == modulename) || (NULL == filtername) ) {
        assert(0);  // developer error
        return CLI_RET_INVPARAM;
    }

    // get which module to set filter for
    if (!strcasecmp(modulename, "rx")) {
        mod = BLADERF_MODULE_RX;
    } else if (!strcasecmp(modulename, "tx")) {
        mod = BLADERF_MODULE_TX;
    } else {
        cli_err(state, modulename, "Invalid module for xb 200 filter\n");
        return CLI_RET_INVPARAM;
    }

    // get which filter to set
    if (!strcmp(filtername, "50")) {
        filter = BLADERF_XB200_50M;
    } else if (!strcmp(filtername, "144")) {
        filter = BLADERF_XB200_144M;
    } else if (!strcmp(filtername, "222")) {
        filter = BLADERF_XB200_222M;
    } else if (!strcmp(filtername, "custom")) {
        filter = BLADERF_XB200_CUSTOM;
    } else if (!strcmp(filtername, "auto_1db")) {
        filter = BLADERF_XB200_AUTO_1DB;
    } else if (!strcmp(filtername, "auto_3db")) {
        filter = BLADERF_XB200_AUTO_3DB;
    } else {
        cli_err(state, filtername, "Invalid filter for xb 200 filter\n",
                filtername);
        return CLI_RET_INVPARAM;
    }

    status = bladerf_xb200_set_filterbank(state->dev, mod, filter);

    if (status != 0) {
        state->last_lib_error = status;
        return CLI_RET_LIBBLADERF;
    }

    printf("  XB-200 Transverter board %s filter successfully set to %s\n\n",
           modulename, filtername);

    return status;
}
