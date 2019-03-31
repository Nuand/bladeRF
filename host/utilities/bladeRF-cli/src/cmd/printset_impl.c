/*
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2013-2018 Nuand LLC
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
#include "cmd.h"
#include "printset.h"
#include "rxtx.h"
#include <inttypes.h>

/* agc */
static int _do_print_agc(struct cli_state *state, bladerf_channel ch, void *arg)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int status;

    bladerf_gain_mode mode = BLADERF_GAIN_DEFAULT;

    status = bladerf_get_gain_mode(state->dev, ch, &mode);
    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
        goto out;
    }

    printf("  %s AGC: %-10s\n", channel2str(ch),
           mode == BLADERF_GAIN_MANUAL ? "Disabled" : "Enabled");

out:
    return rv;
}

int print_agc(struct cli_state *state, int argc, char **argv)
{
    int rv = CLI_RET_OK;

    bladerf_channel ch = BLADERF_CHANNEL_INVALID;

    if (argc < 2 || argc > 3) {
        printf("Usage: print agc [<channel>]\n");
        rv = CLI_RET_NARGS;
        goto out;
    }

    if (3 == argc) {
        ch = str2channel(argv[2]);
        if (BLADERF_CHANNEL_INVALID == ch) {
            cli_err_nnl(state, argv[0], "Invalid channel (%s)\n", argv[2]);
            rv = CLI_RET_INVPARAM;
            goto out;
        }
    }

    rv = ps_foreach_chan(state, true, false, ch, _do_print_agc, NULL);

out:
    return rv;
}

static int _do_set_agc(struct cli_state *state, bladerf_channel ch, void *arg)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int status;

    bladerf_gain_mode *mode = arg;

    status = bladerf_set_gain_mode(state->dev, ch, *mode);
    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
        goto out;
    }

    _do_print_agc(state, ch, NULL);

out:
    return rv;
}

int set_agc(struct cli_state *state, int argc, char **argv)
{
    int rv = CLI_RET_OK;
    int status;

    bladerf_gain_mode mode = BLADERF_GAIN_DEFAULT;
    bladerf_channel ch     = BLADERF_CHANNEL_INVALID;
    bool val;

    if (argc < 3 || argc > 4) {
        printf("Usage: set agc [<channel>] <on|off>\n");
        rv = CLI_RET_NARGS;
        goto out;
    }

    if (4 == argc) {
        ch = str2channel(argv[2]);
        if (BLADERF_CHANNEL_INVALID == ch) {
            cli_err_nnl(state, argv[0], "Invalid channel (%s)\n", argv[2]);
            rv = CLI_RET_INVPARAM;
            goto out;
        }
    }

    status = str2bool(argv[argc - 1], &val);
    if (status < 0) {
        cli_err_nnl(state, argv[0], "Invalid agc value (%s)\n", argv[argc - 1]);
        rv = CLI_RET_INVPARAM;
        goto out;
    }

    mode = (val ? BLADERF_GAIN_AUTOMATIC : BLADERF_GAIN_MANUAL);
    rv   = ps_foreach_chan(state, true, false, ch, _do_set_agc, &mode);

out:
    return rv;
}

/* bandwidth */
static int _do_print_bandwidth(struct cli_state *state,
                               bladerf_channel ch,
                               void *arg)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int status;

    struct bladerf_range const *range;
    bladerf_bandwidth bw;

    status = bladerf_get_bandwidth_range(state->dev, ch, &range);
    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
        goto out;
    };

    status = bladerf_get_bandwidth(state->dev, ch, &bw);
    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
        goto out;
    }

    printf("  %s Bandwidth: %9u Hz (Range: [%.0f, %.0f])\n", channel2str(ch),
           bw, range->min * range->scale, range->max * range->scale);

out:
    return rv;
}

int print_bandwidth(struct cli_state *state, int argc, char **argv)
{
    int rv = CLI_RET_OK;

    bladerf_channel ch = BLADERF_CHANNEL_INVALID;

    if (argc < 2 || argc > 3) {
        printf("Usage: print bandwidth [<channel>]\n");
        rv = CLI_RET_NARGS;
        goto out;
    }

    if (argc == 3) {
        ch = str2channel(argv[2]);
        if (BLADERF_CHANNEL_INVALID == ch) {
            cli_err_nnl(state, argv[0], "Invalid channel (%s)\n", argv[2]);
            rv = CLI_RET_INVPARAM;
            goto out;
        }
    }

    rv = ps_foreach_chan(state, true, true, ch, _do_print_bandwidth, NULL);

out:
    return rv;
}

static int _do_set_bandwidth(struct cli_state *state,
                             bladerf_channel ch,
                             void *arg)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int status;

    unsigned int bw;
    const struct bladerf_range *range = NULL;
    bool ok;

    /* Get valid bandwidth range */
    status = bladerf_get_bandwidth_range(state->dev, ch, &range);
    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
        goto out;
    } else if (range->max >= UINT32_MAX) {
        cli_err_nnl(state, __FUNCTION__,
                    "Invalid range->max (this is a bug): %" PRIu64 "\n",
                    range->max);
        goto out;
    }

    /* Parse bandwidth */
    bw =
        str2uint_suffix(arg, (unsigned int)range->min, (unsigned int)range->max,
                        freq_suffixes, NUM_FREQ_SUFFIXES, &ok);

    if (!ok) {
        cli_err_nnl(state, __FUNCTION__, "Invalid bandwidth (%s)\n", arg);
        rv = CLI_RET_INVPARAM;
        goto out;
    }

    status = bladerf_set_bandwidth(state->dev, ch, bw, NULL);
    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
        goto out;
    }

    rv = _do_print_bandwidth(state, ch, NULL);

out:
    return rv;
}

int set_bandwidth(struct cli_state *state, int argc, char **argv)
{
    int rv = CLI_RET_OK;

    bladerf_channel ch = BLADERF_CHANNEL_INVALID;
    char *arg;

    switch (argc) {
        case 3:
            /* Will do both RX and TX */
            arg = argv[2];
            break;

        case 4:
            /* There is a channel */
            arg = argv[3];
            ch  = str2channel(argv[2]);
            if (BLADERF_CHANNEL_INVALID == ch) {
                cli_err_nnl(state, argv[0], "Invalid channel (%s)\n", argv[2]);
                rv = CLI_RET_INVPARAM;
                goto out;
            }
            break;

        default:
            /* Print usage */
            printf("Usage: set bandwidth [channel] <bandwidth>\n");
            printf("\n");
            printf("    %-15s%s\n", "channel", "Set 'rx' or 'tx' bandwidth");
            printf("    %-15s%s\n", "bandwidth", "Minimum bandwidth in Hz");
            rv = CLI_RET_INVPARAM;
            goto out;
    }

    rv = ps_foreach_chan(state, true, true, ch, _do_set_bandwidth, arg);

out:
    return rv;
}

/* filter */
char const *_rxfir_to_str(bladerf_rfic_rxfir rxfir)
{
    switch (rxfir) {
        case BLADERF_RFIC_RXFIR_BYPASS:
            return "bypass";
        case BLADERF_RFIC_RXFIR_CUSTOM:
            return "custom";
        case BLADERF_RFIC_RXFIR_DEC1:
            return "normal";
        case BLADERF_RFIC_RXFIR_DEC2:
            return "2x decimation";
        case BLADERF_RFIC_RXFIR_DEC4:
            return "4x decimation";
    }

    return "unknown";
}

bladerf_rfic_rxfir _str_to_rxfir(char const *str, bool *ok)
{
    if (ok != NULL) {
        *ok = true;
    }

    if (!strcasecmp(str, "default")) {
        return BLADERF_RFIC_RXFIR_DEFAULT;
    } else if (!strcasecmp(str, "bypass")) {
        return BLADERF_RFIC_RXFIR_BYPASS;
    } else if (!strcasecmp(str, "custom")) {
        return BLADERF_RFIC_RXFIR_CUSTOM;
    } else if (!strcasecmp(str, "normal")) {
        return BLADERF_RFIC_RXFIR_DEC1;
    } else if (!strcasecmp(str, "dec2")) {
        return BLADERF_RFIC_RXFIR_DEC2;
    } else if (!strcasecmp(str, "dec4")) {
        return BLADERF_RFIC_RXFIR_DEC4;
    } else {
        if (ok != NULL) {
            *ok = false;
        }

        return BLADERF_RFIC_RXFIR_DEFAULT;
    }
}

char const *_txfir_to_str(bladerf_rfic_txfir txfir)
{
    switch (txfir) {
        case BLADERF_RFIC_TXFIR_BYPASS:
            return "bypass";
        case BLADERF_RFIC_TXFIR_CUSTOM:
            return "custom";
        case BLADERF_RFIC_TXFIR_INT1:
            return "normal";
        case BLADERF_RFIC_TXFIR_INT2:
            return "2x interpolation";
        case BLADERF_RFIC_TXFIR_INT4:
            return "4x interpolation";
    }

    return "unknown";
}

bladerf_rfic_txfir _str_to_txfir(char const *str, bool *ok)
{
    if (ok != NULL) {
        *ok = true;
    }

    if (!strcasecmp(str, "default")) {
        return BLADERF_RFIC_TXFIR_DEFAULT;
    } else if (!strcasecmp(str, "bypass")) {
        return BLADERF_RFIC_TXFIR_BYPASS;
    } else if (!strcasecmp(str, "custom")) {
        return BLADERF_RFIC_TXFIR_CUSTOM;
    } else if (!strcasecmp(str, "normal")) {
        return BLADERF_RFIC_TXFIR_INT1;
    } else if (!strcasecmp(str, "int2")) {
        return BLADERF_RFIC_TXFIR_INT2;
    } else if (!strcasecmp(str, "int4")) {
        return BLADERF_RFIC_TXFIR_INT4;
    } else {
        if (ok != NULL) {
            *ok = false;
        }

        return BLADERF_RFIC_TXFIR_DEFAULT;
    }
}

int print_filter(struct cli_state *state, int argc, char **argv)
{
    int status;
    bladerf_rfic_rxfir rxfir;
    bladerf_rfic_txfir txfir;

    if (argc < 3 || !BLADERF_CHANNEL_IS_TX(str2channel(argv[2]))) {
        status = bladerf_get_rfic_rx_fir(state->dev, &rxfir);
        if (status >= 0) {
            printf("  RX FIR Filter: %s%s\n", _rxfir_to_str(rxfir),
                   (BLADERF_RFIC_RXFIR_DEFAULT == rxfir) ? " (default)" : "");
        }
    }

    if (argc < 3 || BLADERF_CHANNEL_IS_TX(str2channel(argv[2]))) {
        status = bladerf_get_rfic_tx_fir(state->dev, &txfir);
        if (status >= 0) {
            printf("  TX FIR Filter: %s%s\n", _txfir_to_str(txfir),
                   (BLADERF_RFIC_TXFIR_DEFAULT == txfir) ? " (default)" : "");
        }
    }

    return 0;
}

int set_filter(struct cli_state *state, int argc, char **argv)
{
    /* Usage: set filter <rx|tx> <mode> */
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int status;

    bladerf_channel ch = BLADERF_CHANNEL_INVALID;
    char *arg;
    char *argv_tmp[3];
    bool ok;

    argv_tmp[0] = "print";
    argv_tmp[1] = "filter";

    switch (argc) {
        case 4:
            /* Parse channel */
            arg = argv[3];
            ch  = str2channel(argv[2]);
            if (BLADERF_CHANNEL_INVALID == ch) {
                cli_err_nnl(state, argv[0], "Invalid channel (%s)\n", argv[2]);
                rv = CLI_RET_INVPARAM;
                goto out;
            }
            break;

        default:
            /* Print usage */
            printf("Usage: set filter <rx|tx> <mode>\n");
            printf("\n");
            printf("    %-15s%s\n", "channel", "Set 'rx' or 'tx' filter mode");
            printf("    %-15s%s\n", "mode", "Filter mode (see list below)");
            printf("\n");
            printf("  Available filter modes:\n");
            printf("    RX: default, normal, bypass, custom, dec2, dec4\n");
            printf("    TX: default, normal, bypass, custom, int2, int4\n");
            rv = CLI_RET_NARGS;
            goto out;
    }

    if (BLADERF_CHANNEL_IS_TX(ch)) {
        argv_tmp[2]              = "tx";
        bladerf_rfic_txfir txfir = _str_to_txfir(arg, &ok);

        if (!ok) {
            cli_err_nnl(state, argv[0], "Invalid filter (%s)\n", arg);
            rv = CLI_RET_INVPARAM;
            goto out;
        }

        status = bladerf_set_rfic_tx_fir(state->dev, txfir);
    } else {
        argv_tmp[2]              = "rx";
        bladerf_rfic_rxfir rxfir = _str_to_rxfir(arg, &ok);

        if (!ok) {
            cli_err_nnl(state, argv[0], "Invalid filter (%s)\n", arg);
            rv = CLI_RET_INVPARAM;
            goto out;
        }

        status = bladerf_set_rfic_rx_fir(state->dev, rxfir);
    }

    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
        goto out;
    }

    rv = print_filter(state, 3, argv_tmp);

out:
    return rv;
}

/* frequency */
static int _do_print_frequency(struct cli_state *state,
                               bladerf_channel ch,
                               void *arg)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int status;

    struct bladerf_range const *range;
    bladerf_frequency freq;

    status = bladerf_get_frequency_range(state->dev, ch, &range);
    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
        goto out;
    };

    status = bladerf_get_frequency(state->dev, ch, &freq);
    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
        goto out;
    }

    printf("  %s Frequency: %10" BLADERF_PRIuFREQ " Hz (Range: [%.0f, %.0f])\n",
           channel2str(ch), freq, range->min * range->scale,
           range->max * range->scale);

out:
    return rv;
}

int print_frequency(struct cli_state *state, int argc, char **argv)
{
    int rv = CLI_RET_OK;

    bladerf_channel ch = BLADERF_CHANNEL_INVALID;

    if (argc < 2 || argc > 3) {
        printf("Usage: print frequency [channel]\n");
        rv = CLI_RET_NARGS;
        goto out;
    }

    if (3 == argc) {
        ch = str2channel(argv[2]);
        if (BLADERF_CHANNEL_INVALID == ch) {
            cli_err_nnl(state, argv[0], "Invalid channel (%s)\n", argv[2]);
            rv = CLI_RET_INVPARAM;
            goto out;
        }
    }

    rv = ps_foreach_chan(state, true, true, ch, _do_print_frequency, NULL);

out:
    return rv;
}

static int _do_set_frequency(struct cli_state *state,
                             bladerf_channel ch,
                             void *arg)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int status;

    uint64_t freq;
    const struct bladerf_range *range = NULL;
    bool ok;

    /* Get valid frequency range */
    status = bladerf_get_frequency_range(state->dev, ch, &range);
    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
        goto out;
    }

    /* Parse frequency */
    freq = str2uint64_suffix(arg, range->min, range->max, freq_suffixes,
                             NUM_FREQ_SUFFIXES, &ok);
    if (!ok) {
        cli_err_nnl(state, __FUNCTION__, "Invalid frequency (%s)\n", arg);
        rv = CLI_RET_INVPARAM;
        goto out;
    }

    status = bladerf_set_frequency(state->dev, ch, freq);
    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
        goto out;
    }

    rv = _do_print_frequency(state, ch, NULL);

out:
    return rv;
}

int set_frequency(struct cli_state *state, int argc, char **argv)
{
    /* Usage: set frequency [<rx|tx>] <frequency in Hz> */
    int rv = CLI_RET_OK;

    bladerf_channel ch = BLADERF_CHANNEL_INVALID;
    char *arg;

    const char *freqwarning =
        "For best results, it is not recommended to set both RX and TX to the\n"
        "same frequency. Instead, consider offsetting them by at least 1 MHz\n"
        "and mixing digitally.\n"
        "\n"
        "For the above reason, 'set frequency <value>` is deprecated and\n"
        "scheduled for removal in future bladeRF-cli versions.\n"
        "\n"
        "Please use 'set frequency rx' and 'set frequency tx' to configure\n"
        "channels individually.\n"
        "\n";

    switch (argc) {
        case 3:
            /* Will do both RX and TX... for now.
             * This is marked deprecated and will be removed in
             * future bladeRF-cli revisions */
            printf("%s", freqwarning);
            arg = argv[2];
            break;

        case 4:
            /* Parse channel */
            arg = argv[3];
            ch  = str2channel(argv[2]);
            if (BLADERF_CHANNEL_INVALID == ch) {
                cli_err_nnl(state, argv[0], "Invalid channel (%s)\n", argv[2]);
                rv = CLI_RET_INVPARAM;
                goto out;
            }
            break;

        default:
            /* Print usage */
            printf("Usage: set frequency <channel> <frequency>\n");
            printf("\n");
            printf("    %-15s%s\n", "channel", "Set 'rx' or 'tx' frequency");
            printf("    %-15s%s\n", "frequency", "Frequency in Hz");
            rv = CLI_RET_NARGS;
            goto out;
    }

    rv = ps_foreach_chan(state, true, true, ch, _do_set_frequency, arg);

out:
    return rv;
}

/* gain */
static int _do_print_gain(struct cli_state *state,
                          bladerf_channel ch,
                          void *arg)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int status;

    char *stage = (char *)arg;

    size_t const max_count = 16; /* Max number of stages to display */
    char **stages;

    struct bladerf_range const *range = NULL;

    bool printed = false;
    int gain;
    int count;
    int i;

    stages = calloc(max_count, sizeof(char *));
    if (NULL == stages) {
        rv = CLI_RET_MEM;
        goto out;
    }

    /* Print the overall gain, if no gain stage is specified */
    if (NULL == stage) {
        status = bladerf_get_gain_range(state->dev, ch, &range);
        if (status < 0) {
            *err = status;
            rv   = CLI_RET_LIBBLADERF;
            goto out;
        };

        status = bladerf_get_gain(state->dev, ch, &gain);
        if (status < 0) {
            *err = status;
            rv   = CLI_RET_LIBBLADERF;
            goto out;
        };

        printed = true;
        printf("  Gain %-4soverall: %4d dB (Range: [%g, %g])\n",
               channel2str(ch), gain, range->min * range->scale,
               range->max * range->scale);
    }

    /* How many gain stages are available? */
    count = bladerf_get_gain_stages(state->dev, ch, (char const **)stages,
                                    max_count);

    /* Loop over the gain stages */
    for (i = 0; i < count && rv == CLI_RET_OK; ++i) {
        if (NULL != stage && 0 != strcmp((stages[i]), stage)) {
            /* This is not the stage we're looking for */
            continue;
        }

        /* Get the allowed range of gain values */
        status =
            bladerf_get_gain_stage_range(state->dev, ch, stages[i], &range);
        if (status < 0) {
            *err = status;
            rv   = CLI_RET_LIBBLADERF;
            goto out;
        };

        /* Get the current value of the gain stage */
        status = bladerf_get_gain_stage(state->dev, ch, stages[i], &gain);
        if (status < 0) {
            *err = status;
            rv   = CLI_RET_LIBBLADERF;
            goto out;
        };

        /* Send it out */
        printed = true;
        printf("          %8s: %4d dB (Range: [%g, %g])\n", stages[i], gain,
               range->min * range->scale, range->max * range->scale);
    }

    if (!printed) {
        rv = CLI_RET_INVPARAM;
        goto out;
    }

out:
    free(stages);

    return rv;
}

int print_gain(struct cli_state *state, int argc, char **argv)
{
    int rv = CLI_RET_OK;

    char *stage        = NULL;
    bladerf_channel ch = BLADERF_CHANNEL_INVALID;

    switch (argc) {
        case 2:
            /* All channels, all stages */
            break;

        case 3:
            /* One channel, all stages */
            ch = str2channel(argv[2]);
            if (BLADERF_CHANNEL_INVALID == ch) {
                cli_err_nnl(state, argv[0], "Invalid channel (%s)\n", argv[2]);
                rv = CLI_RET_INVPARAM;
                goto out;
            }
            break;

        case 4:
            /* One channel, one stage */
            stage = argv[3];
            ch    = str2channel(argv[2]);
            if (BLADERF_CHANNEL_INVALID == ch) {
                cli_err_nnl(state, argv[0], "Invalid channel (%s)\n", argv[2]);
                rv = CLI_RET_INVPARAM;
                goto out;
            }
            break;

        default:
            /* Print usage */
            printf("Usage: print gain [<channel> [<stage>]]\n");
            printf("\n");
            printf("    %-15s%s\n", "channel",
                   "Channel identifier (e.g. 'rx', 'tx2')");
            printf("    %-15s%s\n", "stage", "Name of gain stage to query");
            rv = CLI_RET_NARGS;
            goto out;
    }

    /* Loop over channels */
    rv = ps_foreach_chan(state, true, true, ch, _do_print_gain, stage);

    if (CLI_RET_INVPARAM == rv) {
        /* We did not find any applicable values :( */
        printf("  Channel and/or stage not found: %s %s\n", channel2str(ch),
               stage);
    }

out:
    return rv;
}

static int _do_set_gain(struct cli_state *state,
                        bladerf_channel ch,
                        char *stage,
                        int gain)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int status;

    bladerf_gain_mode mode;

    if (!BLADERF_CHANNEL_IS_TX(ch)) {
        status = bladerf_get_gain_mode(state->dev, ch, &mode);
        if (status < 0) {
            *err = status;
            rv   = CLI_RET_LIBBLADERF;
            goto out;
        }

        if (mode != BLADERF_GAIN_MGC) {
            printf("  Gain on %s cannot be changed while AGC is enabled.\n",
                   channel2str(ch));
            rv = CLI_RET_STATE;
            goto out;
        }

        if (!rxtx_task_running(state->rx)) {
            printf("Note: This change will not be visible until the channel is "
                   "enabled.\n");
        }
    }

    if (stage != NULL) {
        printf("Setting %s %s gain to %d dB\n", channel2str(ch), stage, gain);
        status = bladerf_set_gain_stage(state->dev, ch, stage, gain);
    } else {
        printf("Setting %s overall gain to %d dB\n", channel2str(ch), gain);
        status = bladerf_set_gain(state->dev, ch, gain);
    }

    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
        goto out;
    }

    rv = _do_print_gain(state, ch, stage);

out:
    return rv;
}

int set_gain(struct cli_state *state, int argc, char **argv)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int status;

    char *stage        = NULL;
    bladerf_channel ch = BLADERF_CHANNEL_INVALID;
    char *value_str    = NULL;

    struct bladerf_range const *range = NULL;

    int gain;
    bool ok;

    switch (argc) {
        case 4:
            /* One channel, default stage */
            value_str = argv[3];
            break;

        case 5:
            /* One channel, one stage */
            stage     = argv[3];
            value_str = argv[4];
            break;

        default:
            /* Print usage */
            printf("Usage: set gain <channel> [<stage>] <value>\n");
            printf("\n");
            printf("    %-15s%s\n", "channel",
                   "Channel identifier (e.g. 'rx', 'tx2')");
            printf("    %-15s%s\n", "stage", "Name of gain stage to set");
            printf("    %-15s%s\n", "value", "Gain value in dB");
            goto out;
    }

    ch = str2channel(argv[2]);
    if (BLADERF_CHANNEL_INVALID == ch) {
        cli_err_nnl(state, argv[0], "Invalid channel (%s)\n", argv[2]);
        rv = CLI_RET_INVPARAM;
        goto out;
    }

    if (stage != NULL) {
        /* Query the allowed range of a specific gain stage */
        status = bladerf_get_gain_stage_range(state->dev, ch, stage, &range);
    } else {
        /* Query the allowed range of overall gain */
        status = bladerf_get_gain_range(state->dev, ch, &range);
    }

    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
        goto out;
    } else if (range->max >= UINT32_MAX) {
        cli_err_nnl(state, argv[0],
                    "Invalid range->max (this is a bug): %" PRIu64 "\n",
                    range->max);
    }

    gain = str2int(value_str, (unsigned int)range->min,
                   (unsigned int)range->max, &ok);
    if (!ok) {
        cli_err_nnl(state, argv[0], "Invalid gain setting for %s (%s)\n",
                    (stage == NULL) ? channel2str(ch) : stage, value_str);
        rv = CLI_RET_INVPARAM;
        goto out;
    }

    rv = _do_set_gain(state, ch, stage, gain);

out:
    return rv;
}

/* lnagain */
int print_lnagain(struct cli_state *state, int argc, char **argv)
{
    return _do_print_gain(state, BLADERF_CHANNEL_RX(0), "lna");
}

int set_lnagain(struct cli_state *state, int argc, char **argv)
{
    int rv = CLI_RET_OK;

    if (argc != 3) {
        rv = CLI_RET_NARGS;
        goto out;
    }

    char *args[] = { "set", "gain", "rx1", "lna", argv[2] };

    rv = set_gain(state, 5, args);

out:
    return rv;
}

/* loopback */
int print_loopback(struct cli_state *state, int argc, char **argv)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int status;

    bladerf_loopback loopback;

    status = bladerf_get_loopback(state->dev, &loopback);
    if (status != 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
        goto out;
    }

    printf("  Loopback mode: %s\n", loopback2str(loopback));

out:
    return rv;
}

int set_loopback(struct cli_state *state, int argc, char **argv)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int status;

    bladerf_loopback loopback;

    if (argc == 2) {
        printf("Usage: %s %s <mode>, where <mode> is one of the following:\n",
               argv[0], argv[1]);
        printf("\n");

        if (ps_is_board(state->dev, BOARD_BLADERF1)) {
            printf("  %-18s%s\n", "bb_txlpf_rxvga2",
                   "Baseband loopback: TXLPF output --> RXVGA2 input\n");
            printf("  %-18s%s\n", "bb_txlpf_rxlpf",
                   "Baseband loopback: TXLPF output --> RXLPF input\n");
            printf("  %-18s%s\n", "bb_txvga1_rxvga2",
                   "Baseband loopback: TXVGA1 output --> RXVGA2 input\n");
            printf("  %-18s%s\n", "bb_txvga1_rxlpf",
                   "Baseband loopback: TXVGA1 output --> RXLPF input\n");
            printf("  %-18s%s\n", "rf_lna1",
                   "RF loopback: TXMIX --> RXMIX via LNA1 path\n");
            printf("  %-18s%s\n", "rf_lna2",
                   "RF loopback: TXMIX --> RXMIX via LNA2 path\n");
            printf("  %-18s%s\n", "rf_lna3",
                   "RF loopback: TXMIX --> RXMIX via LNA3 path\n");
        }

        if (ps_is_board(state->dev, BOARD_BLADERF2)) {
            printf("  %-18s%s\n", "rfic_bist", "RFIC BIST loopback\n");
        }

        printf("  %-18s%s\n", "firmware", "Firmware-based sample loopback\n");
        printf("  %-18s%s\n", "none", "Loopback disabled - Normal operation\n");

        rv = CLI_RET_INVPARAM;
        goto out;

    } else if (argc != 3) {
        rv = CLI_RET_NARGS;
        goto out;
    }

    status = str2loopback(argv[2], &loopback);
    if (status != 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
        goto out;
    }

    status = bladerf_set_loopback(state->dev, loopback);
    if (status != 0) {
        if (status == BLADERF_ERR_UNSUPPORTED) {
           printf("  If tuning mode is FPGA, consider setting tuning mode"
                     " to  host by running `set tuning_mode host`.");
        }
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
        goto out;
    }

    rv = print_loopback(state, 2, argv);

out:
    return rv;
}

/* rssi */
static int _do_print_rssi(struct cli_state *state,
                          bladerf_channel ch,
                          void *arg)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int status;

    int pre_rssi, sym_rssi;

    status = bladerf_get_rfic_rssi(state->dev, ch, &pre_rssi, &sym_rssi);
    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
        goto out;
    }

    printf("  %s RSSI: preamble = %d dB, symbol = %d dB\n", channel2str(ch),
           pre_rssi, sym_rssi);

out:
    return rv;
}

int print_rssi(struct cli_state *state, int argc, char **argv)
{
    int rv = CLI_RET_OK;

    bladerf_channel ch = BLADERF_CHANNEL_INVALID;

    if (argc < 2 || argc > 3) {
        printf("Usage: print rssi [channel]\n");
        rv = CLI_RET_NARGS;
        goto out;
    }

    if (3 == argc) {
        ch = str2channel(argv[2]);
        if (BLADERF_CHANNEL_INVALID == ch) {
            cli_err_nnl(state, argv[0], "Invalid channel (%s)\n", argv[2]);
            rv = CLI_RET_INVPARAM;
            goto out;
        }
    }

    rv = ps_foreach_chan(state, true, false, ch, _do_print_rssi, NULL);

out:
    return rv;
}

/* rx_mux */
int print_rx_mux(struct cli_state *state, int argc, char **argv)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int status;

    const char *mux_str;
    bladerf_rx_mux mux_setting;

    status = bladerf_get_rx_mux(state->dev, &mux_setting);
    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
        goto out;
    }

    switch (mux_setting) {
        case BLADERF_RX_MUX_BASEBAND:
            mux_str = "BASEBAND - Baseband samples";
            break;
        case BLADERF_RX_MUX_12BIT_COUNTER:
            mux_str = "12BIT_COUNTER - 12-bit Up-Count I, Down-Count Q";
            break;
        case BLADERF_RX_MUX_32BIT_COUNTER:
            mux_str = "32BIT_COUNTER - 32-bit Up-Counter";
            break;
        case BLADERF_RX_MUX_DIGITAL_LOOPBACK:
            mux_str =
                "DIGITAL_LOOPBACK: Digital Loopback of TX->RX in the FPGA";
            break;
        default:
            mux_str = "Unknown";
    }

    printf("  RX mux: %s\n", mux_str);

out:
    return rv;
}

int set_rx_mux(struct cli_state *state, int argc, char **argv)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int status;

    bladerf_rx_mux mux_val;

    if (argc != 3) {
        printf("Usage: set rx_mux <mode>\n");
        printf("  Available modes: BASEBAND, 12BIT_COUNTER, 32BIT_COUNTER, "
               "DIGITAL_LOOPBACK\n");
        rv = CLI_RET_NARGS;
        goto out;
    }

    if (!strcasecmp(argv[2], "BASEBAND") ||
        !strcasecmp(argv[2], "BASEBAND_LMS")) {
        mux_val = BLADERF_RX_MUX_BASEBAND;
    } else if (!strcasecmp(argv[2], "12BIT_COUNTER")) {
        mux_val = BLADERF_RX_MUX_12BIT_COUNTER;
    } else if (!strcasecmp(argv[2], "32BIT_COUNTER")) {
        mux_val = BLADERF_RX_MUX_32BIT_COUNTER;
    } else if (!strcasecmp(argv[2], "DIGITAL_LOOPBACK")) {
        mux_val = BLADERF_RX_MUX_DIGITAL_LOOPBACK;
    } else {
        cli_err_nnl(state, argv[0], "Invalid RX mux mode (%s)\n", argv[2]);
        rv = CLI_RET_INVPARAM;
        goto out;
    }

    status = bladerf_set_rx_mux(state->dev, mux_val);
    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
        goto out;
    }

    rv = print_rx_mux(state, 2, argv);

out:
    return rv;
}

/* rx_mux */
int print_tuning_mode(struct cli_state *state, int argc, char **argv)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int status;

    bladerf_tuning_mode mode;

    status = bladerf_get_tuning_mode(state->dev, &mode);
    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
        goto out;
    }

    printf("  Tuning Mode: %s\n", mode == BLADERF_TUNING_MODE_HOST ? "Host" : "FPGA");

out:
    return rv;
}

int set_tuning_mode(struct cli_state *state, int argc, char **argv)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int status;

    bladerf_tuning_mode mode;

    if (argc != 3 && argc != 4) {
        if (argc == 2) {
            printf("Usage: %s %s <mode>\n", argv[0], argv[1]);
            printf("\n  <mode> values:\n");
            printf("      host, FPGA\n");
            return 0;
        } else {
            return CLI_RET_NARGS;
        }
    }

    mode = str_to_tuning_mode(argv[2]);
    if (mode == BLADERF_TUNING_MODE_INVALID) {
        cli_err(state, argv[1], "Invalid tuning mode.\n");
        return CLI_RET_INVPARAM;
    }

    status = bladerf_set_tuning_mode(state->dev, mode);
    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
        goto out;
    }
out:
    return rv;
}

/* rxvga1 */
int print_rxvga1(struct cli_state *state, int argc, char **argv)
{
    return _do_print_gain(state, BLADERF_CHANNEL_RX(0), "rxvga1");
}

int set_rxvga1(struct cli_state *state, int argc, char **argv)
{
    if (argc != 3) {
        return CLI_RET_NARGS;
    }

    char *args[] = { "set", "gain", "rx1", "rxvga1", argv[2] };

    return set_gain(state, 5, args);
}

/* rxvga2 */
int print_rxvga2(struct cli_state *state, int argc, char **argv)
{
    return _do_print_gain(state, BLADERF_CHANNEL_RX(0), "rxvga2");
}

int set_rxvga2(struct cli_state *state, int argc, char **argv)
{
    if (argc != 3) {
        return CLI_RET_NARGS;
    }

    char *args[] = { "set", "gain", "rx1", "rxvga2", argv[2] };

    return set_gain(state, 5, args);
}

/* samplerate */
static int _do_print_samplerate(struct cli_state *state,
                                bladerf_channel ch,
                                void *arg)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int status;

    struct bladerf_range const *range;
    struct bladerf_rational_rate rate;

    status = bladerf_get_sample_rate_range(state->dev, ch, &range);
    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
        goto out;
    };

    status = bladerf_get_rational_sample_rate(state->dev, ch, &rate);
    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
        goto out;
    }

    printf("  %s sample rate: %" PRIu64 " %" PRIu64 "/%" PRIu64
           " (Range: [%.0f, %.0f])\n",
           channel2str(ch), rate.integer, rate.num, rate.den,
           range->min * range->scale, range->max * range->scale);

out:
    return rv;
}

int print_samplerate(struct cli_state *state, int argc, char **argv)
{
    int rv = CLI_RET_OK;

    bladerf_channel ch = BLADERF_CHANNEL_INVALID;

    if (argc < 2 || argc > 3) {
        rv = CLI_RET_NARGS;
        goto out;
    }

    if (argc == 3) {
        ch = str2channel(argv[2]);
        if (BLADERF_CHANNEL_INVALID == ch) {
            cli_err_nnl(state, argv[0], "Invalid channel (%s)\n", argv[2]);
            rv = CLI_RET_INVPARAM;
            goto out;
        }
    }

    rv = ps_foreach_chan(state, true, true, ch, _do_print_samplerate, NULL);

out:
    return rv;
}

static int _do_set_samplerate(struct cli_state *state,
                              bladerf_channel ch,
                              void *arg)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int status;

    struct bladerf_rational_rate *rate = (struct bladerf_rational_rate *)arg;
    struct bladerf_rational_rate actual;

    const char *settext =
        "  Setting %s sample rate - req: %9" PRIu64 " %" PRIu64 "/%" PRIu64
        "Hz, actual: %9" PRIu64 " %" PRIu64 "/%" PRIu64 "Hz\n";

    const char *samplewarning =
        "\n"
        "  Warning: The provided sample rate may result in timeouts with the\n"
        "           current %s connection. A SuperSpeed connection or a lower\n"
        "           sample rate are recommended.\n";

    status = bladerf_set_rational_sample_rate(state->dev, ch, rate, &actual);

    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
        goto out;
    }

    printf(settext, channel2str(ch), rate->integer, rate->num, rate->den,
           actual.integer, actual.num, actual.den);

    /* Discontinuities have been reported for 2.0 on Intel
     * controllers above 6MHz. */
    bladerf_dev_speed usb_speed = bladerf_device_speed(state->dev);

    if ((actual.integer > 6000000 ||
         ((actual.integer == 6000000) && (actual.num != 0))) &&
        usb_speed != BLADERF_DEVICE_SPEED_SUPER) {
        printf(samplewarning, devspeed2str(usb_speed));
    }

out:
    return rv;
}

int set_samplerate(struct cli_state *state, int argc, char **argv)
{
    /* Usage: set samplerate [rx|tx] [integer [numerator denominator]]*/
    int rv = CLI_RET_OK;

    bladerf_channel ch = BLADERF_CHANNEL_INVALID;
    struct bladerf_rational_rate rate;
    bool ok;
    size_t idx;

    const char *helptext =
        "Usage:\n\n"
        "  set samplerate [rx|tx] [integer [numerator denominator]]\n"
        "\n"
        "Setting the sample rate first addresses an optional RX or TX\n"
        "channel. Omitting the channel will set both of the channels.\n"
        "\n"
        "In the case of a whole integer sample rate, only the integer portion\n"
        "is required in the command. In the case of a fractional rate, the\n"
        "numerator follows the integer portion followed by the denominator.\n"
        "\n"
        "Examples:\n"
        "\n"
        "  Set the sample rate for both RX and TX to 4MHz:\n"
        "\n"
        "    set samplerate 4M\n"
        "\n"
        "  Set the sample rate for both RX and TX to GSM 270833 1/3Hz:\n"
        "\n"
        "    set samplerate 270833 1 3\n"
        "\n"
        "  Set the sample rate for RX to 40MHz:\n"
        "\n"
        "    set samplerate rx 40M\n"
        "\n";


    /* Check argument count */
    switch (argc) {
        /* Valid quantities: 3 through 6 */
        case 4:
        case 6:
            /* Parse channel */
            ch = str2channel(argv[2]);
            if (BLADERF_CHANNEL_INVALID == ch) {
                cli_err_nnl(state, argv[0], "Invalid channel (%s)\n", argv[2]);
                rv = CLI_RET_INVPARAM;
                goto out;
            }
            break;

        case 3:
        case 5:
            break;

        /* Invalid quantities */
        case 2:
            printf("%s", helptext);
            rv = CLI_RET_OK;
            goto out;

        default:
            printf("%s", helptext);
            rv = CLI_RET_NARGS;
            goto out;
    }

    /* Initialize values */
    rate.integer = 0;
    rate.num     = 0;
    rate.den     = 1;
    idx          = (argc == 3 || argc == 5) ? 2 : 3;

    /* Allow a value of zero to be specified for the integer portion
     * so that the entire value can be specified in num/den. libbladeRF
     * will return an error if the sample rate is invalid */
    rate.integer = str2uint64_suffix(argv[idx], 0, UINT64_MAX, freq_suffixes,
                                     NUM_FREQ_SUFFIXES, &ok);

    /* Integer portion didn't make it */
    if (!ok) {
        cli_err_nnl(state, argv[0],
                    "Invalid %s value in specified sample rate (%s)\n",
                    "integer", argv[idx]);
        rv = CLI_RET_INVPARAM;
        goto out;
    }

    /* Take in num/den if they are present */
    if (argc == 5 || argc == 6) {
        idx++;
        rate.num = str2uint64_suffix(argv[idx], 0, UINT64_MAX, freq_suffixes,
                                     NUM_FREQ_SUFFIXES, &ok);
        if (!ok) {
            cli_err_nnl(state, argv[0],
                        "Invalid %s value in specified sample rate (%s)\n",
                        "numerator", argv[idx]);
            rv = CLI_RET_INVPARAM;
            goto out;
        }
    }

    if (argc == 5 || argc == 6) {
        idx++;
        rate.den = str2uint64_suffix(argv[idx], 1, UINT64_MAX, freq_suffixes,
                                     NUM_FREQ_SUFFIXES, &ok);
        if (!ok) {
            cli_err_nnl(state, argv[0],
                        "Invalid %s value in specified sample rate (%s)\n",
                        "denominator", argv[idx]);
            rv = CLI_RET_INVPARAM;
            goto out;
        }
    }

    rv = ps_foreach_chan(state, true, true, ch, _do_set_samplerate, &rate);

out:
    return rv;
}

/* sampling */
int print_sampling(struct cli_state *state, int argc, char **argv)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int status;

    bladerf_sampling mode;

    if (argc != 2) {
        rv = CLI_RET_NARGS;
        goto out;
    }

    /* Read the ADC input mux */
    status = bladerf_get_sampling(state->dev, &mode);
    if (status) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
        goto out;
    }

    switch (mode) {
        case BLADERF_SAMPLING_EXTERNAL:
            printf("  Sampling: External\n");
            break;

        case BLADERF_SAMPLING_INTERNAL:
            printf("  Sampling: Internal\n");
            break;

        default:
            printf("  Sampling: Unknown\n");
            break;
    }

out:
    return rv;
}

int set_sampling(struct cli_state *state, int argc, char **argv)
{
    /* Usage: set sampling [internal|external] */
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int status;

    bladerf_sampling mode;

    if (argc != 3) {
        rv = CLI_RET_NARGS;
        goto out;
    }

    if (strcasecmp("internal", argv[2]) == 0) {
        mode = BLADERF_SAMPLING_INTERNAL;
    } else if (strcasecmp("external", argv[2]) == 0) {
        mode = BLADERF_SAMPLING_EXTERNAL;
    } else {
        cli_err_nnl(state, argv[0], "Invalid sampling mode (%s)\n", argv[2]);
        rv = CLI_RET_INVPARAM;
        goto out;
    }

    status = bladerf_set_sampling(state->dev, mode);
    if (status) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
        goto out;
    }

    print_sampling(state, 2, argv);

out:
    return rv;
}

/* smb_mode */
int print_smb_mode(struct cli_state *state, int argc, char **argv)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int status;

    bladerf_smb_mode mode;
    const char *mode_str;

    status = bladerf_get_smb_mode(state->dev, &mode);
    if (status != 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
        goto out;
    }

    mode_str = smb_mode_to_str(mode);

    printf("  SMB Mode:   %s\n", mode_str);

    if (mode == BLADERF_SMB_MODE_OUTPUT) {
        struct bladerf_rational_rate rate;

        status = bladerf_get_rational_smb_frequency(state->dev, &rate);
        if (status != 0) {
            *err = status;
            rv   = CLI_RET_LIBBLADERF;
            goto out;
        }

        /* If the user has not set a custom output frequency, the rate will be
         * zero. Just note the reference clock frequency in this case. */
        if (rate.integer == 0 && rate.num == 0 && rate.den == 0) {
            printf("  SMB Output: 38.4 MHz (reference clock)\n");
        } else {
            /* Otherwise, show the custom frequency in use */
            printf("  SMB Output: %" PRIu64 " + %" PRIu64 "/%" PRIu64 " Hz\n",
                   rate.integer, rate.num, rate.den);
        }
    }

out:
    return rv;
}

int set_smb_mode(struct cli_state *state, int argc, char **argv)
{
    int *err = &state->last_lib_error;
    int status;

    bladerf_smb_mode mode, curr_mode;
    struct bladerf_rational_rate rate;
    bool ok;

    /* Usage:
     *  3: set smb_mode <mode>
     *  4: set smb_mode output <freq integer>
     *  6: set smb_mode output <freq integer> <freq num> <freq denom>
     */
    if (argc != 3 && argc != 4 && argc != 6) {
        if (argc == 2) {
            printf("Usage: %s %s <mode>\n", argv[0], argv[1]);
            printf("       %s %s output [frequency]\n", argv[0], argv[1]);
            printf("\n  <mode> values:\n");
            printf("      disabled, input, output\n");
            return 0;
        } else {
            return CLI_RET_NARGS;
        }
    }

    mode = str_to_smb_mode(argv[2]);
    if (mode == BLADERF_SMB_MODE_INVALID) {
        cli_err(state, argv[1], "Invalid SMB clock port mode.\n");
        return CLI_RET_INVPARAM;
    }

    status = bladerf_get_smb_mode(state->dev, &curr_mode);
    if (status != 0) {
        goto out;
    }

    /* Help folks avoid shooting themselves in the foot when an XB-200 is
     * connected and enabled. */
    if (curr_mode == BLADERF_SMB_MODE_UNAVAILBLE) {
        cli_err_nnl(state, argv[0],
                    "The SMB clock port mode is not available to be changed.\n"
                    "            "
                    "(This is the case when an XB-200 is in use.)\n");
        return CLI_RET_INVPARAM;
    }

    if (argc == 3) {
        status = bladerf_set_smb_mode(state->dev, mode);
    } else {
        if (mode != BLADERF_SMB_MODE_OUTPUT) {
            cli_err(state, argv[2],
                    "Additional arguments may not be used with this mode.\n");
            return CLI_RET_INVPARAM;
        }


        rate.integer = str2uint64_suffix(argv[3], 0, UINT64_MAX, freq_suffixes,
                                         NUM_FREQ_SUFFIXES, &ok);
        if (!ok) {
            cli_err(state, argv[3], "Invalid integer frequency value.\n");
            return CLI_RET_INVPARAM;
        }

        if (argc == 6) {
            rate.num = str2uint64_suffix(argv[4], 0, UINT64_MAX, freq_suffixes,
                                         NUM_FREQ_SUFFIXES, &ok);
            if (!ok) {
                cli_err(state, argv[4], "Invalid frequency numerator value.\n");
                return CLI_RET_INVPARAM;
            }

            rate.den = str2uint64_suffix(argv[5], 1, UINT64_MAX, freq_suffixes,
                                         NUM_FREQ_SUFFIXES, &ok);
            if (!ok) {
                cli_err(state, argv[5],
                        "Invalid frequency denominator value.\n");
                return CLI_RET_INVPARAM;
            }
        } else {
            printf("Ignoring numden\n");
            rate.num = 0;
            rate.den = 1;
        }

        status = bladerf_set_rational_smb_frequency(state->dev, &rate, NULL);
    }

out:
    if (status != 0) {
        *err = status;
        return CLI_RET_LIBBLADERF;
    } else {
        return print_smb_mode(state, 2, argv);
    }
}

/* trimdac */
int print_trimdac(struct cli_state *state, int argc, char **argv)
{
    int *err = &state->last_lib_error;
    int status;

    uint16_t curr, cal;

    status = bladerf_get_vctcxo_trim(state->dev, &cal);
    if (status != 0) {
        *err = status;
        return CLI_RET_LIBBLADERF;
    }

    status = bladerf_dac_read(state->dev, &curr);
    if (status != 0) {
        *err = status;
        return CLI_RET_LIBBLADERF;
    }

    printf("  Current VCTCXO trim: 0x%04x\n", curr);
    printf("  Stored VCTCXO trim:  0x%04x\n", cal);

    return 0;
}

int set_trimdac(struct cli_state *state, int argc, char **argv)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int status;

    unsigned int val;
    bool ok;

    if (argc != 3) {
        rv = CLI_RET_NARGS;
    } else {
        val = str2uint(argv[2], 0, 65535, &ok);
        if (!ok) {
            cli_err_nnl(state, argv[0], "Invalid VCTCXO DAC value (%s)\n",
                        argv[2]);
            rv = CLI_RET_INVPARAM;
        } else {
            status = bladerf_dac_write(state->dev, val);
            if (status < 0) {
                *err = status;
                rv   = CLI_RET_LIBBLADERF;
            } else {
                rv = print_trimdac(state, 2, argv);
            }
        }
    }
    return rv;
}

/* txvga1 */
int print_txvga1(struct cli_state *state, int argc, char **argv)
{
    return _do_print_gain(state, BLADERF_CHANNEL_TX(0), "txvga1");
}

int set_txvga1(struct cli_state *state, int argc, char **argv)
{
    if (argc != 3) {
        return CLI_RET_NARGS;
    }

    char *args[] = { "set", "gain", "tx1", "txvga1", argv[2] };

    return set_gain(state, 5, args);
}

/* txvga2 */
int print_txvga2(struct cli_state *state, int argc, char **argv)
{
    return _do_print_gain(state, BLADERF_CHANNEL_TX(0), "txvga2");
}

int set_txvga2(struct cli_state *state, int argc, char **argv)
{
    if (argc != 3) {
        return CLI_RET_NARGS;
    }

    char *args[] = { "set", "gain", "tx1", "txvga2", argv[2] };

    return set_gain(state, 5, args);
}

/* vctcxo_tamer */
int print_vctcxo_tamer(struct cli_state *state, int argc, char *argv[])
{
    int *err = &state->last_lib_error;
    int status;

    const char *mode_str;

    bladerf_vctcxo_tamer_mode mode = BLADERF_VCTCXO_TAMER_INVALID;

    if (argc != 2) {
        return CLI_RET_NARGS;
    }

    status = bladerf_get_vctcxo_tamer_mode(state->dev, &mode);
    if (status != 0) {
        *err = status;
        return CLI_RET_LIBBLADERF;
    }

    switch (mode) {
        case BLADERF_VCTCXO_TAMER_DISABLED:
            mode_str = "Disabled";
            break;

        case BLADERF_VCTCXO_TAMER_1_PPS:
            mode_str = "1 PPS";
            break;

        case BLADERF_VCTCXO_TAMER_10_MHZ:
            mode_str = "10 MHz";
            break;

        default:
            mode_str = "Unknown";
            break;
    }

    printf("  VCTCXO tamer mode: %s\n", mode_str);
    return CLI_RET_OK;
}

int set_vctcxo_tamer(struct cli_state *state, int argc, char **argv)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int status;

    bladerf_vctcxo_tamer_mode mode = BLADERF_VCTCXO_TAMER_INVALID;

    if (argc != 3) {
        return CLI_RET_NARGS;
    }

    if (!strcasecmp(argv[2], "disabled") || !strcasecmp(argv[2], "off")) {
        mode = BLADERF_VCTCXO_TAMER_DISABLED;
    } else if (!strcasecmp(argv[2], "1PPS") || !strcasecmp(argv[2], "1 PPS")) {
        mode = BLADERF_VCTCXO_TAMER_1_PPS;
    } else if (!strcasecmp(argv[2], "10MHZ") ||
               !strcasecmp(argv[2], "10 MHZ")) {
        mode = BLADERF_VCTCXO_TAMER_10_MHZ;
    } else if (!strcasecmp(argv[2], "10M")) {
        mode = BLADERF_VCTCXO_TAMER_10_MHZ;
    } else {
        cli_err_nnl(state, argv[0], "Invalid VCTCXO tamer option (%s)\n",
                    argv[2]);
        return CLI_RET_INVPARAM;
    }

    status = bladerf_set_vctcxo_tamer_mode(state->dev, mode);
    if (status != 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
    } else {
        rv = print_vctcxo_tamer(state, 2, argv);
    }

    return rv;
}
