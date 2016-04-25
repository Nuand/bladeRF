/*
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2016 Nuand LLC
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
#include "rel_assert.h"
#include "common.h"
#include "xb.h"
#include "xb300.h"
#include "conversions.h"

static int enable_xb300(struct cli_state *state)
{
    int status;

    printf("  Enabling XB-300 Amplifier board\n");
    status = bladerf_expansion_attach(state->dev, BLADERF_XB_300);
    if (status != 0) {
        state->last_lib_error = status;
        return CLI_RET_LIBBLADERF;
    }

    printf("  XB-300 Amplifier board successfully enabled\n\n");

    return status;
}

static int print_trx_mode(struct cli_state *state)
{
    int status;
    bladerf_xb300_trx trx;

    status = bladerf_xb300_get_trx(state->dev, &trx);
    if (status != 0) {
        return status;
    }

    printf("  TRX mode: ");
    switch (trx) {
        case BLADERF_XB300_TRX_TX:
            printf("TX\n");
            break;

        case BLADERF_XB300_TRX_RX:
            printf("RX\n");
            break;

        case BLADERF_XB300_TRX_UNSET:
            printf("Unset\n");
            break;

        default:
            printf("Invalid state\n");
            break;
    }

    printf("\n");

    return status;
}

static int set_trx_mode(struct cli_state *state, const char *trxmode)
{
    int status;

    if (!strcasecmp(trxmode, "tx")) {
        status = bladerf_xb300_set_trx(state->dev, BLADERF_XB300_TRX_TX);
    } else if (!strcasecmp(trxmode, "rx")) {
        status = bladerf_xb300_set_trx(state->dev, BLADERF_XB300_TRX_RX);
    } else {
        cli_err(state, trxmode, "Invalid TRX option.");
        status = CLI_RET_INVPARAM;
    }

    return status;
}

static int print_enable(struct cli_state *state,
                        const char *name, bladerf_xb300_amplifier amp)
{
    int status;
    bool enable;

    status = bladerf_xb300_get_amplifier_enable(state->dev, amp, &enable);
    if (status == 0) {
        printf("  %s: %s\n\n", name, enable ? "Enabled" : "Disabled");
    } else {
        state->last_lib_error = status;
        status = CLI_RET_LIBBLADERF;
    }

    return status;
}

static int set_enable(struct cli_state *state,
                      bladerf_xb300_amplifier amp, const char *enable_str)
{
    int status;
    bool enable;

    status = str2bool(enable_str, &enable);
    if (status != 0) {
        cli_err(state, enable_str, "Invalid enable/disable string.\n\n");
        return CLI_RET_INVPARAM;
    }

    status = bladerf_xb300_set_amplifier_enable(state->dev, amp, enable);
    if (status != 0) {
        state->last_lib_error = status;
        status = CLI_RET_LIBBLADERF;
    }

    return status;
}

int cmd_xb300(struct cli_state *state, int argc, char **argv)
{
    int status = 0;
    int modelnum = MODEL_INVALID;
    const char *subcommand = NULL;
    float pdet_pwr;

    if (argc < 3) {
        /* Incorrect number of arguments */
        return CLI_RET_NARGS;
    }

    /* xb 300 <subcommand> <args> */
    modelnum = atoi(argv[1]);
    subcommand = argv[2];

    if (modelnum != MODEL_XB300) {
        assert(0);  /* Developer error */
        return CLI_RET_INVPARAM;
    }

    putchar('\n');

    /* xb 300 enable */
    if (!strcasecmp(subcommand, "enable")) {
        status = enable_xb300(state);
    } else if (!strcasecmp(subcommand, "trx")) {
        /* xb 300 trx [tx|rx] */
        if (3 == argc) {
            status = print_trx_mode(state);
        } else {
            status = set_trx_mode(state, argv[3]);
            if (status == 0) {
                status = print_trx_mode(state);
            }
        }
    } else if (!strcasecmp(subcommand, "lna")) {
        /* xb 300 lna [on|off] */
        if (3 == argc) {
            status = print_enable(state, "LNA", BLADERF_XB300_AMP_LNA);
        } else {
            status = set_enable(state, BLADERF_XB300_AMP_LNA, argv[3]);
            if (status == 0) {
                status = print_enable(state, "LNA", BLADERF_XB300_AMP_LNA);
            }
        }
    } else if (!strcasecmp(subcommand, "pa")) {
        /* xb 300 pa [on|off] */
        if (3 == argc) {
            status = print_enable(state, "PA", BLADERF_XB300_AMP_PA);
        } else {
            status = set_enable(state, BLADERF_XB300_AMP_PA, argv[3]);
            if (status == 0) {
                status = print_enable(state, "PA", BLADERF_XB300_AMP_PA);
            }
        }
    } else if (!strcasecmp(subcommand, "aux")) {
        /* xb 300 lna [on|off] */
        if (3 == argc) {
            status = print_enable(state, "Aux. PA", BLADERF_XB300_AMP_PA_AUX);
        } else {
            status = set_enable(state, BLADERF_XB300_AMP_PA_AUX, argv[3]);
            if (status == 0) {
                status = print_enable(state, "Aux. PA", BLADERF_XB300_AMP_PA_AUX);
            }
        }
    } else if (!strcasecmp(subcommand, "pwr")) {
        status = bladerf_xb300_get_output_power(state->dev, &pdet_pwr);
        if (status) {
            state->last_lib_error = status;
            status = CLI_RET_LIBBLADERF;
        } else {
            printf("  PA output power: %s%f dBm\n\n",
                   pdet_pwr < 0 ? "" : "+", pdet_pwr);
        }
    } else {
        /* Unknown subcommand */
        cli_err(state, subcommand, "Invalid subcommand for xb %d\n", modelnum);
        status = CLI_RET_INVPARAM;
    }

    return status;
}
