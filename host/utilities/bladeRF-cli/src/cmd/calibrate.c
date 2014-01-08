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
    int status = 0;
    int fpga_status;

    if (!cli_device_is_opened(state)) {
        return CMD_RET_NODEV;
    }

    /* The FPGA needs to be loaded */
    fpga_status = bladerf_is_fpga_configured(state->dev);
    if (fpga_status < 0) {
        state->last_lib_error = fpga_status;
        return CMD_RET_LIBBLADERF;
    } else if (fpga_status != 1) {
        return CMD_RET_NOFPGA;
    }


    if  (argc == 1) {

        /* Ensure both TX and RX are enabled */
        status = bladerf_enable_module(state->dev, BLADERF_MODULE_TX, true);
        if (status != 0) {
            goto cmd_calibrate_err;
        }

        status = bladerf_enable_module(state->dev, BLADERF_MODULE_RX, true);
        if (status != 0) {
            goto cmd_calibrate_err;
        }

        /* Calibrate LPF Tuning Module */
        status = bladerf_calibrate_dc(state->dev, BLADERF_DC_CAL_LPF_TUNING);
        if (status != 0) {
            goto cmd_calibrate_err;
        }

        /* Calibrate TX LPF Filter */
        status = bladerf_calibrate_dc(state->dev, BLADERF_DC_CAL_TX_LPF);
        if (status != 0) {
            goto cmd_calibrate_err;
        }

        /* Calibrate RX LPF Filter */
        status = bladerf_calibrate_dc(state->dev, BLADERF_DC_CAL_RX_LPF);
        if (status != 0) {
            goto cmd_calibrate_err;
        }

        /* Calibrate RX VGA2 */
        status = bladerf_calibrate_dc(state->dev, BLADERF_DC_CAL_RXVGA2);
        if (status != 0) {
            goto cmd_calibrate_err;
        }

    } else if (argc == 2) {
        bladerf_cal_module module;

        /* Figure out which module we are calibrating */
        if (strcasecmp(argv[1], "tuning") == 0) {
            module = BLADERF_DC_CAL_LPF_TUNING;
        } else if (strcasecmp(argv[1], "txlpf") == 0) {
            module = BLADERF_DC_CAL_TX_LPF;
            status = bladerf_enable_module(state->dev, BLADERF_MODULE_TX, true);
        } else if (strcasecmp(argv[1], "rxlpf") == 0) {
            module = BLADERF_DC_CAL_RX_LPF;
            status = bladerf_enable_module(state->dev, BLADERF_MODULE_RX, true);
        } else if (strcasecmp(argv[1], "rxvga2") == 0) {
            module = BLADERF_DC_CAL_RXVGA2;
            status = bladerf_enable_module(state->dev, BLADERF_MODULE_RX, true);
        } else {
            cli_err(state, argv[0], "Invalid module provided (%s)", argv[1]);
            return CMD_RET_INVPARAM;
        }

        if (status != 0) {
            /* Calibrate it */
            status = bladerf_calibrate_dc(state->dev, module);
        }
    } else {
        return CMD_RET_INVPARAM;
    }

cmd_calibrate_err:
    if (status != 0) {
        state->last_lib_error = status;
        status = CMD_RET_LIBBLADERF;
    }

    return status;
}
