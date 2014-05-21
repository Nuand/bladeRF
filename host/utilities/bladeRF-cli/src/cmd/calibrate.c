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

#include "calibrate.h"
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
        return CLI_RET_NODEV;
    }

    /* The FPGA needs to be loaded */
    fpga_status = bladerf_is_fpga_configured(state->dev);
    if (fpga_status < 0) {
        state->last_lib_error = fpga_status;
        return CLI_RET_LIBBLADERF;
    } else if (fpga_status != 1) {
        return CLI_RET_NOFPGA;
    }


    if (argc == 1) {

        printf("TO DO - autocalibration at the current gain & freq\n");
        return 0;

    } else if (argc == 2) {
        unsigned int ops = 0;

        if (strcasecmp(argv[1], "dc_lms_tuning") == 0) {
            ops = CAL_DC_LMS_TUNING;
        } else if (strcasecmp(argv[1], "dc_lms_txlpf") == 0) {
            ops = CAL_DC_LMS_TXLPF;
        } else if (strcasecmp(argv[1], "dc_lms_rxlpf") == 0) {
            ops = CAL_DC_LMS_RXLPF;
        } else if (strcasecmp(argv[1], "dc_lms_rxvga2") == 0) {
            ops = CAL_DC_LMS_RXVGA2;
        } else if (strcasecmp(argv[1], "dc_lms") == 0) {
            ops = CAL_DC_LMS_ALL;
        } else if (strcasecmp(argv[1], "dc_rx") == 0) {
            ops = CAL_DC_AUTO_RX;
        } else if (strcasecmp(argv[1], "dc_tx") == 0) {
            ops = CAL_DC_AUTO_TX;
        } else if (strcasecmp(argv[1], "dc") == 0) {
            ops = CAL_DC_AUTO;
        } else {
            cli_err(state, argv[0], "Invalid calibration option: %s", argv[1]);
            return CLI_RET_INVPARAM;
        }

        if (IS_DC_CAL(ops)) {
            status = calibrate_dc(state->dev, ops);
            if (status != 0) {
                goto error;
            }
        }

    } else {
        return CLI_RET_NARGS;
    }

error:
    if (status != 0) {
        state->last_lib_error = status;
        status = CLI_RET_LIBBLADERF;
    }

    return status;
}
