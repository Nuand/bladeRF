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

#define MAX_RX_DC_OFFSET 15
#define MAX_TX_DC_OFFSET 127
#define MAX_PHASE (2048)
#define MAX_GAIN (2048)

static int print_correction(struct cli_state *state, bladerf_module module)
{
    int status;
    int16_t dc_i, dc_q, phase,gain;
    struct bladerf *dev = state->dev;

    status = bladerf_get_correction(dev, module, BLADERF_CORR_LMS_DCOFF_I, &dc_i);
    if (status != 0) {
        goto print_correction_out;
    }

    status = bladerf_get_correction(dev, module, BLADERF_CORR_LMS_DCOFF_Q, &dc_q);
    if (status != 0) {
        goto print_correction_out;
    }

    status = bladerf_get_correction(dev, module, BLADERF_CORR_FPGA_PHASE, &phase);
    if (status != 0) {
        goto print_correction_out;
    }

    status = bladerf_get_correction(dev, module, BLADERF_CORR_FPGA_GAIN, &gain);

print_correction_out:

    if (status != 0) {
        state->last_lib_error = status;
        status = CMD_RET_LIBBLADERF;
    } else {
        printf("\nCurrent Settings: DC Offset I=%d Q=%d, Phase=%d, Gain=%d\n\n",
               dc_i, dc_q, phase, gain);
    }

    return status;
}

static inline int set_phase_correction(struct cli_state *state,
                                       bladerf_module module, int16_t value)
{
    int status;
    struct bladerf *dev = state->dev;

    status = bladerf_set_correction(dev, module, BLADERF_CORR_FPGA_PHASE, value);

    if (status != 0) {
        state->last_lib_error = status;
        status = CMD_RET_LIBBLADERF;
    }

    return status;
}

static inline int set_gain_correction(struct cli_state *state,
                                      bladerf_module module, int16_t value)
{
    int status;
    struct bladerf *dev = state->dev;

    status = bladerf_set_correction(dev, module, BLADERF_CORR_FPGA_GAIN, value);

    if (status != 0) {
        state->last_lib_error = status;
        status = CMD_RET_LIBBLADERF;
    }

    return status;
}

static inline int set_dc_correction(struct cli_state *state,
                                    bladerf_module module,
                                    int16_t val_i, int16_t val_q)
{
    int status;
    struct bladerf *dev = state->dev;

    status = bladerf_set_correction(dev, module, BLADERF_CORR_LMS_DCOFF_I, val_i);
    if (status == 0) {
        status = bladerf_set_correction(dev, module, BLADERF_CORR_LMS_DCOFF_Q, val_q);
    }

    if (status != 0) {
        state->last_lib_error = status;
        status = CMD_RET_LIBBLADERF;
    }

    return status;
}

/* Usage:
 *
 *  Print the current correction state
 *  corect <rx|tx>
 *
 *  Set phase or gain:
 *  correct <rx|tx> <phase|gain> <value>
 *
 *  Set DC offset:
 *  correct <rx|tx> <dc> <i_off> <q_off>
 */
int cmd_correct(struct cli_state *state, int argc, char **argv)
{
    int rv = CMD_RET_OK;
    int fpga_status;
    bool ok;
    bladerf_module module;

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

    /* Get the direction to print */
    if (argc >= 2) {
        if (!strcasecmp(argv[1], "tx")) {
            module = BLADERF_MODULE_TX;
        } else if(!strcasecmp(argv[1], "rx")) {
            module = BLADERF_MODULE_RX;
        } else {
            cli_err(state, argv[0],
                    "Invalid module provided. Valid options are: 'rx' or 'tx'");
            return CMD_RET_INVPARAM;
        }
    } else {
        return CMD_RET_NARGS;
    }

    if (argc == 2) {
        rv = print_correction(state, module);
    } else if (argc == 4) {
		int16_t value = 0;

        if (!strcasecmp(argv[2], "phase")) {

            value = str2int(argv[3], -MAX_PHASE, MAX_PHASE, &ok);
            if (!ok) {
                cli_err(state, argv[0], "Phase value must be in [%d, %d]",
                        -MAX_PHASE, MAX_PHASE);
                rv = CMD_RET_INVPARAM;
            } else {
                rv = set_phase_correction(state, module, value);
            }

        } else if (!strcasecmp(argv[2], "gain")){

            value = str2int( argv[3], -MAX_GAIN, MAX_GAIN, &ok);
            if (!ok) {
                cli_err(state, argv[0], "Gain value must be in [%d, %d]",
                        -MAX_GAIN, MAX_GAIN);
                rv = CMD_RET_INVPARAM;
            } else {
                rv = set_gain_correction(state, module, value);
            }

        } else {
            cli_err(state, argv[0],
                    "Invalid correction module for %d arguments: %s",
                    argc - 2, argv[2]);
            rv = CMD_RET_INVPARAM;
        }
    } else if (argc == 5) {

        if (!strcasecmp(argv[2], "dc")) {
			int16_t val_i, val_q;
            const int16_t max = module == BLADERF_MODULE_TX ?
                                    MAX_TX_DC_OFFSET : MAX_RX_DC_OFFSET;

            const int16_t min = module == BLADERF_MODULE_TX ?
                                    -(MAX_TX_DC_OFFSET + 1) : -MAX_RX_DC_OFFSET;

            val_i = str2int(argv[3], min, max, &ok);
            if (!ok) {
                cli_err(state, argv[0], "I offset must be in [%d, %d]", min, max);
                rv = CMD_RET_INVPARAM;
                return rv;
            }

            val_q = str2int(argv[4], min, max, &ok);
            if (!ok) {
                cli_err(state, argv[0], "Q offset must be in [%d, %d]", min, max);
                rv = CMD_RET_INVPARAM;
                return rv;
            }

            rv = set_dc_correction(state, module, val_i, val_q);

        } else {
            cli_err(state, argv[0],
                    "Invalid correction module for %d arguments: %s",
                    argc - 2, argv[2]);
            rv = CMD_RET_INVPARAM;
        }
    } else {
        rv = CMD_RET_NARGS;
    }

    return rv;
}

