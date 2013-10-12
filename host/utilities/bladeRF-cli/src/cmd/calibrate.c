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

#include "common.h"
#include "cmd.h"

int cmd_calibrate(struct cli_state *state, int argc, char **argv)
{
    /* Valid commands:
        calibrate [module]
    */
    int rv = CMD_RET_OK;
    if (!cli_device_is_opened(state)) {
        return CMD_RET_NODEV;
    }

    if( argc == 1 ) {
        /* Ensure both TX and RX are enabled */
        bladerf_enable_module(state->dev, BLADERF_MODULE_TX, true);
        bladerf_enable_module(state->dev, BLADERF_MODULE_RX, true);
        /* Calibrate LPF Tuning Module */
        bladerf_calibrate_dc(state->dev, BLADERF_DC_CAL_LPF_TUNING);

        /* Calibrate TX LPF Filter */
        bladerf_calibrate_dc(state->dev, BLADERF_DC_CAL_TX_LPF);

        /* Calibrate RX LPF Filter */
        bladerf_calibrate_dc(state->dev, BLADERF_DC_CAL_RX_LPF);

        /* Calibrate RX VGA2 */
        bladerf_calibrate_dc(state->dev, BLADERF_DC_CAL_RXVGA2);

    } else if( argc == 2 ) {
        bladerf_cal_module module;
        /* Figure out which module we are calibrating */
        if (strcasecmp(argv[1], "tuning") == 0) {
            module = BLADERF_DC_CAL_LPF_TUNING;
        } else if (strcasecmp(argv[1], "txlpf") == 0) {
            module = BLADERF_DC_CAL_TX_LPF;
            bladerf_enable_module(state->dev, BLADERF_MODULE_TX, true);
        } else if (strcasecmp(argv[1], "rxlpf") == 0) {
            module = BLADERF_DC_CAL_RX_LPF;
            bladerf_enable_module(state->dev, BLADERF_MODULE_RX, true);
        } else if (strcasecmp(argv[1], "rxvga2") == 0) {
            module = BLADERF_DC_CAL_RXVGA2;
            bladerf_enable_module(state->dev, BLADERF_MODULE_RX, true);
        } else {
            cli_err(state, argv[0], "Invalid module provided (%s)", argv[1]);
            return CMD_RET_INVPARAM;
        }
        /* Calibrate it */
        bladerf_calibrate_dc(state->dev, module);
    } else {
        return CMD_RET_INVPARAM;
    }

    return rv ;
}
