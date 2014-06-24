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

static int get_dc_tbl_args(struct cli_state *s, int argc, char **argv,
                           unsigned int *f_low, unsigned int *f_inc,
                           unsigned int *f_high)
{
    bool ok;
    const unsigned int min = *f_low;

    if (argc == 3) {
        /* Just use the defaults */
        return 0;
    } else if (argc != 6) {
        return CLI_RET_NARGS;
    }

    *f_low = str2uint_suffix(argv[3], min, BLADERF_FREQUENCY_MAX,
                             freq_suffixes, NUM_FREQ_SUFFIXES, &ok);

    if (!ok) {
        cli_err(s, argv[0], "Invalid min frequency (%s)\n", argv[3]);
        return CLI_RET_INVPARAM;
    }

    *f_inc = str2uint_suffix(argv[4], 1, BLADERF_FREQUENCY_MAX,
                             freq_suffixes, NUM_FREQ_SUFFIXES, &ok);

    if (!ok) {
        cli_err(s, argv[0], "Invalid frequency increment (%s)\n", argv[4]);
        return CLI_RET_INVPARAM;
    }

    *f_high = str2uint_suffix(argv[5], min, BLADERF_FREQUENCY_MAX,
                              freq_suffixes, NUM_FREQ_SUFFIXES, &ok);

    if (!ok) {
        cli_err(s, argv[0], "Invalid max frequency (%s)\n", argv[5]);
        return CLI_RET_INVPARAM;
    }

    if (*f_low >= *f_high) {
        cli_err(s, argv[0], "Low frequency cannot be >= high frequency\n");
        return CLI_RET_INVPARAM;
    }

    if ((*f_high - *f_low) / *f_inc == 0) {
        cli_err(s, argv[0], "The specified frequency increment would yield "
                            "an empty table.\n");
        return CLI_RET_INVPARAM;
    }

    return 0;
}

int cmd_calibrate(struct cli_state *state, int argc, char **argv)
{
    int status = 0;
    unsigned int ops = 0;
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


    if (argc == 2) {
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
            cli_err(state, argv[0],
                    "Invalid calibration option or missing arguments: %s",
                    argv[1]);
            return CLI_RET_INVPARAM;
        }

        if (IS_DC_CAL(ops)) {
            status = calibrate_dc(state->dev, ops);
            if (status != 0) {
                goto error;
            }
        }
    } else if (argc >=  3) {
        /* TODO set the min based upon whether or not an XB200 is attached */
        unsigned int f_low = BLADERF_FREQUENCY_MIN;
        unsigned int f_inc = 2500000;
        unsigned int f_high = BLADERF_FREQUENCY_MAX;
        bladerf_module module;

        status = get_dc_tbl_args(state, argc, argv,
                                 &f_low, &f_inc, &f_high);
        if (status != 0) {
            return status;
        }

        if (strcasecmp(argv[1], "dc_rx_tbl") == 0) {
            module = BLADERF_MODULE_RX;
        } else if (strcasecmp(argv[1], "dc_tx_tbl") == 0) {
            module = BLADERF_MODULE_TX;
        } else {
            cli_err(state, argv[0], "Invalid calibration option: %s", argv[1]);
            return CLI_RET_INVPARAM;
        }

        status = calibrate_dc_gen_tbl(state->dev, module, argv[2],
                                      f_low, f_inc, f_high);


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
