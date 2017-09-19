/*
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2013-2015 Nuand LLC
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
#include "conversions.h"
#include "host_config.h"
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

#define PRINTSET_DECL(x)                             \
    int print_##x(struct cli_state *, int, char **); \
    int set_##x(struct cli_state *, int, char **);

// clang-format off
#define PRINTSET_ENTRY(x, printall_option, _board) \
    {                                              \
        FIELD_INIT(.print, print_##x),             \
        FIELD_INIT(.set, set_##x),                 \
        FIELD_INIT(.name, #x),                     \
        FIELD_INIT(.pa_option, printall_option),   \
        FIELD_INIT(.board, _board),                \
    }
// clang-format on

/**
 * Any special actions we should take when printing all values when invoking
 * the 'print' command with no arguments.
 */
enum printall_option {
    PRINTALL_OPTION_NONE,           /**< No special actions */
    PRINTALL_OPTION_SKIP,           /**< Skip this entry */
    PRINTALL_OPTION_APPEND_NEWLINE, /**< Print an extra newline after entry */
};

enum board_option {
    BOARD_ANY,      /**< Supported on all hardware */
    BOARD_BLADERF1, /**< Supported on bladeRF 1 */
    BOARD_BLADERF2, /**< Supported on bladeRF 2 */
};

/* An entry in the printset table */
struct printset_entry {
    /** Function pointer to thing that prints parameter */
    int (*print)(struct cli_state *s, int argc, char **argv);

    /** Function pointer to setter */
    int (*set)(struct cli_state *s, int argc, char **argv);

    /** String associated with name */
    const char *name;

    /**
     * Additional options when printing this entry as part of the
     * 'print command' with no options (i.e., print everything) */
    enum printall_option pa_option;

    /** Board associated with this parameter, or BOARD_ANY for generic */
    enum board_option const board;
};

/* Declarations */
PRINTSET_DECL(adf_enable);
PRINTSET_DECL(bandwidth);
PRINTSET_DECL(frequency);
PRINTSET_DECL(agc);
PRINTSET_DECL(gpio);
PRINTSET_DECL(loopback);
PRINTSET_DECL(lnagain);
PRINTSET_DECL(rx_mux);
PRINTSET_DECL(rxvga1);
PRINTSET_DECL(rxvga2);
PRINTSET_DECL(txvga1);
PRINTSET_DECL(txvga2);
PRINTSET_DECL(sampling);
PRINTSET_DECL(samplerate);
PRINTSET_DECL(smb_mode);
PRINTSET_DECL(trimdac);
PRINTSET_DECL(vctcxo_tamer);
PRINTSET_DECL(xb_spi);
PRINTSET_DECL(xb_gpio);
PRINTSET_DECL(xb_gpio_dir);

// clang-format off
/* print/set parameter table */
struct printset_entry printset_table[] = {
    PRINTSET_ENTRY(bandwidth,    PRINTALL_OPTION_APPEND_NEWLINE, BOARD_ANY),
    PRINTSET_ENTRY(frequency,    PRINTALL_OPTION_APPEND_NEWLINE, BOARD_ANY),
    PRINTSET_ENTRY(agc,          PRINTALL_OPTION_APPEND_NEWLINE, BOARD_ANY),
    PRINTSET_ENTRY(gpio,         PRINTALL_OPTION_APPEND_NEWLINE, BOARD_BLADERF1),
    PRINTSET_ENTRY(loopback,     PRINTALL_OPTION_APPEND_NEWLINE, BOARD_ANY),
    PRINTSET_ENTRY(rx_mux,       PRINTALL_OPTION_APPEND_NEWLINE, BOARD_ANY),
    PRINTSET_ENTRY(lnagain,      PRINTALL_OPTION_NONE,           BOARD_BLADERF1),
    PRINTSET_ENTRY(rxvga1,       PRINTALL_OPTION_NONE,           BOARD_BLADERF1),
    PRINTSET_ENTRY(rxvga2,       PRINTALL_OPTION_NONE,           BOARD_BLADERF1),
    PRINTSET_ENTRY(txvga1,       PRINTALL_OPTION_NONE,           BOARD_BLADERF1),
    PRINTSET_ENTRY(txvga2,       PRINTALL_OPTION_APPEND_NEWLINE, BOARD_BLADERF1),
    PRINTSET_ENTRY(sampling,     PRINTALL_OPTION_APPEND_NEWLINE, BOARD_BLADERF1),
    PRINTSET_ENTRY(samplerate,   PRINTALL_OPTION_APPEND_NEWLINE, BOARD_ANY),
    PRINTSET_ENTRY(smb_mode,     PRINTALL_OPTION_APPEND_NEWLINE, BOARD_BLADERF1),
    PRINTSET_ENTRY(adf_enable,   PRINTALL_OPTION_APPEND_NEWLINE, BOARD_BLADERF2),
    PRINTSET_ENTRY(trimdac,      PRINTALL_OPTION_APPEND_NEWLINE, BOARD_ANY),
    PRINTSET_ENTRY(vctcxo_tamer, PRINTALL_OPTION_APPEND_NEWLINE, BOARD_BLADERF1),
    PRINTSET_ENTRY(xb_spi,       PRINTALL_OPTION_SKIP,           BOARD_BLADERF1),
    PRINTSET_ENTRY(xb_gpio,      PRINTALL_OPTION_NONE,           BOARD_BLADERF1),
    PRINTSET_ENTRY(xb_gpio_dir,  PRINTALL_OPTION_NONE,           BOARD_BLADERF1),

    /* End of table marked by entry with NULL/empty fields */
    { FIELD_INIT(.print, NULL), FIELD_INIT(.set, NULL), FIELD_INIT(.name, ""),
      FIELD_INIT(.pa_option, PRINTALL_OPTION_NONE),
      FIELD_INIT(.board, BOARD_ANY) }
};
// clang-format on

static bool _is_rx(bladerf_channel ch)
{
    return (ch == BLADERF_CHANNEL_RX(0) || ch == BLADERF_CHANNEL_RX(1));
}

static bool _is_tx(bladerf_channel ch)
{
    return (ch == BLADERF_CHANNEL_TX(0) || ch == BLADERF_CHANNEL_TX(1));
}

static bool _is_board(struct bladerf *dev, enum board_option board)
{
    char const *board_name = bladerf_get_board_name(dev);

    if (BOARD_ANY == board) {
        return true;
    }

    if (BOARD_BLADERF1 == board && strcmp(board_name, "bladerf1") == 0) {
        return true;
    }

    if (BOARD_BLADERF2 == board && strcmp(board_name, "bladerf2") == 0) {
        return true;
    }

    return false;
}

static int _gpio_modify(struct bladerf *dev, uint32_t mask, uint32_t val)
{
    int status;
    uint32_t gpio;

    // read from gpio
    status = bladerf_config_gpio_read(dev, &gpio);
    if (status < 0) {
        return status;
    }

    // clear bits
    gpio &= ~(mask);

    // set bits
    gpio |= (val & mask);

    // write back to gpio
    status = bladerf_config_gpio_write(dev, gpio);
    if (status < 0) {
        return status;
    }

    return status;
}

bladerf_channel get_channel(char *ch, bool *ok)
{
    bladerf_channel rv;
    *ok = true;

    if (strcasecmp(ch, "rx") == 0 || strcasecmp(ch, "rx0") == 0) {
        rv = BLADERF_CHANNEL_RX(0);
    } else if (strcasecmp(ch, "rx1") == 0) {
        rv = BLADERF_CHANNEL_RX(1);
    } else if (strcasecmp(ch, "tx") == 0 || strcasecmp(ch, "tx0") == 0) {
        rv = BLADERF_CHANNEL_TX(0);
    } else if (strcasecmp(ch, "tx1") == 0) {
        rv = BLADERF_CHANNEL_TX(1);
    } else {
        rv  = BLADERF_CHANNEL_INVALID;
        *ok = false;
    }

    return rv;
}

static char *get_channel_str(bladerf_channel ch)
{
    switch (ch) {
        case BLADERF_CHANNEL_RX(0):
            return "RX0";
        case BLADERF_CHANNEL_RX(1):
            return "RX1";
        case BLADERF_CHANNEL_TX(0):
            return "TX0";
        case BLADERF_CHANNEL_TX(1):
            return "TX1";
        default:
            return "??";
    }
}

static inline void invalid_channel(struct cli_state *s,
                                   const char *cmd,
                                   const char *channel)
{
    cli_err_nnl(s, cmd, "Invalid channel (%s)\n", channel);
}

static inline void invalid_gain(struct cli_state *s,
                                const char *cmd,
                                const char *param,
                                const char *gain)
{
    cli_err_nnl(s, cmd, "Invalid gain setting for %s (%s)\n", param, gain);
}

int print_adf_enable(struct cli_state *state, int argc, char **argv)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int status;
    uint32_t val;

    status = bladerf_config_gpio_read(state->dev, &val);
    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
    }

    if (rv == CLI_RET_OK) {
        int adf_chip_enable = (val >> 11) & 0x01;  // 11

        printf("  ADF Chip Enable: %s\n",
               adf_chip_enable ? "enabled" : "disabled");
    }

    return rv;
}

int set_adf_enable(struct cli_state *state, int argc, char **argv)
{
    /* set adf_enable {0,1} */
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int status;
    uint32_t val;

    if (argc == 3) {
        bool ok;
        val = str2uint(argv[2], 0, 1, &ok);

        if (!ok) {
            cli_err_nnl(state, argv[0], "Invalid adf_enable value (%s)\n",
                        argv[2]);
            rv = CLI_RET_INVPARAM;
        } else {
            status = _gpio_modify(state->dev, 0x800, (val << 11));
            if (status < 0) {
                *err = status;
                rv   = CLI_RET_LIBBLADERF;
            } else {
                rv = print_adf_enable(state, argc, argv);
            }
        }
    } else {
        rv = CLI_RET_NARGS;
    }
    return rv;
}

int print_bandwidth(struct cli_state *state, int argc, char **argv)
{
    /* Usage: print bandwidth [rx|tx]*/
    int rv                  = CLI_RET_OK;
    int *err                = &state->last_lib_error;
    bladerf_channel channel = BLADERF_CHANNEL_INVALID;
    unsigned int bw;
    bool ok;
    int status;

    switch (argc) {
        case 2:
            /* Do both RX and TX */
            break;
        case 3:
            /* Parse channel */
            channel = get_channel(argv[2], &ok);
            if (!ok) {
                invalid_channel(state, argv[0], argv[2]);
                rv = CLI_RET_INVPARAM;
            }
            break;
        default:
            rv = CLI_RET_NARGS;
            break;
    }

    if (rv == CLI_RET_OK && (argc == 2 || _is_rx(channel))) {
        status = bladerf_get_bandwidth(state->dev, BLADERF_CHANNEL_RX(0), &bw);
        if (status < 0) {
            *err = status;
            rv   = CLI_RET_LIBBLADERF;
        } else {
            printf("  RX Bandwidth: %9u Hz\n", bw);
        }
    }

    if (rv == CLI_RET_OK && (argc == 2 || _is_tx(channel))) {
        status = bladerf_get_bandwidth(state->dev, BLADERF_CHANNEL_TX(0), &bw);
        if (status < 0) {
            *err = status;
            rv   = CLI_RET_LIBBLADERF;
        } else {
            printf("  TX Bandwidth: %9u Hz\n", bw);
        }
    }

    return rv;
}

static int _set_print_bandwidth(struct cli_state *state,
                                bladerf_channel ch,
                                unsigned int bw)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    unsigned int actual;
    int status;

    if (rv == CLI_RET_OK) {
        status = bladerf_set_bandwidth(state->dev, ch, bw, NULL);
        if (status < 0) {
            *err = status;
            rv   = CLI_RET_LIBBLADERF;
        }
    }

    if (rv == CLI_RET_OK) {
        status = bladerf_get_bandwidth(state->dev, ch, &actual);
        if (status < 0) {
            *err = status;
            rv   = CLI_RET_LIBBLADERF;
        } else {
            printf("  Set %s bandwidth - req:%9u Hz actual:%9u Hz\n",
                   get_channel_str(ch), bw, actual);
        }
    }

    return rv;
}

int set_bandwidth(struct cli_state *state, int argc, char **argv)
{
    /* Usage: set bandwidth [<rx|tx>] <bandwidth in Hz> */
    int rv                  = CLI_RET_OK;
    int *err                = &state->last_lib_error;
    bladerf_channel channel = BLADERF_CHANNEL_INVALID;
    unsigned int bw;
    struct bladerf_range range;
    bool ok;
    int status;

    switch (argc) {
        case 3:
            /* Will do both RX and TX */
            break;
        case 4:
            /* Parse channel */
            channel = get_channel(argv[2], &ok);
            if (!ok) {
                invalid_channel(state, argv[0], argv[2]);
                rv = CLI_RET_INVPARAM;
            }
            break;
        default:
            /* Print usage */
            printf("Usage: set bandwidth [channel] <bandwidth>\n");
            printf("\n");
            printf("    %-15s%s\n", "channel", "Set 'rx' or 'tx' bandwidth");
            printf("    %-15s%s\n", "bandwidth", "Minimum bandwidth in Hz");
            return rv;
    }

    if (rv == CLI_RET_OK) {
        /* Get valid bandwidth range */
        status = bladerf_get_bandwidth_range(state->dev, channel, &range);
        if (status < 0) {
            *err = status;
            rv   = CLI_RET_LIBBLADERF;
        }
    }

    if (rv == CLI_RET_OK) {
        /* Parse bandwidth */
        bw = str2uint_suffix(argv[argc - 1], range.min, range.max,
                             freq_suffixes, NUM_FREQ_SUFFIXES, &ok);

        if (!ok) {
            cli_err_nnl(state, argv[0], "Invalid bandwidth (%s)\n",
                        argv[argc - 1]);
            rv = CLI_RET_INVPARAM;
        }
    }

    if (rv == CLI_RET_OK && (argc == 3 || _is_rx(channel))) {
        /* Change RX bandwidth */
        rv = _set_print_bandwidth(state, BLADERF_CHANNEL_RX(0), bw);
    }

    if (rv == CLI_RET_OK && (argc == 3 || _is_tx(channel))) {
        /* Change TX bandwidth */
        rv = _set_print_bandwidth(state, BLADERF_CHANNEL_TX(0), bw);
    }

    return rv;
}

int print_frequency(struct cli_state *state, int argc, char **argv)
{
    /* Usage: print frequency [<rx|tx>] */
    int rv                  = CLI_RET_OK;
    int *err                = &state->last_lib_error;
    bladerf_channel channel = BLADERF_CHANNEL_INVALID;
    uint64_t freq;
    bool ok;
    int status;

    switch (argc) {
        case 2:
            /* Do both RX and TX */
            break;
        case 3:
            /* Parse channel */
            channel = get_channel(argv[2], &ok);
            if (!ok) {
                invalid_channel(state, argv[0], argv[2]);
                rv = CLI_RET_INVPARAM;
            }
            break;
        default:
            rv = CLI_RET_NARGS;
            break;
    }

    if (rv == CLI_RET_OK && (argc == 2 || _is_rx(channel))) {
        status =
            bladerf_get_frequency(state->dev, BLADERF_CHANNEL_RX(0), &freq);
        if (status < 0) {
            *err = status;
            rv   = CLI_RET_LIBBLADERF;
        } else {
            printf("  RX Frequency: %10" PRIu64 " Hz\n", freq);
        }
    }

    if (rv == CLI_RET_OK && (argc == 2 || _is_tx(channel))) {
        status =
            bladerf_get_frequency(state->dev, BLADERF_CHANNEL_TX(0), &freq);
        if (status < 0) {
            *err = status;
            rv   = CLI_RET_LIBBLADERF;
        } else {
            printf("  TX Frequency: %10" PRIu64 " Hz\n", freq);
        }
    }

    return rv;
}

static int _set_print_frequency(struct cli_state *state,
                                bladerf_channel ch,
                                uint64_t freq)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    uint64_t actual;
    int status;

    if (rv == CLI_RET_OK) {
        status = bladerf_set_frequency(state->dev, ch, freq);
        if (status < 0) {
            *err = status;
            rv   = CLI_RET_LIBBLADERF;
        }
    }

    if (rv == CLI_RET_OK) {
        status = bladerf_get_frequency(state->dev, ch, &actual);
        if (status < 0) {
            *err = status;
            rv   = CLI_RET_LIBBLADERF;
        } else {
            printf("  Set %s frequency - req:%10" PRIu64 " Hz actual:%10" PRIu64
                   " Hz\n",
                   get_channel_str(ch), freq, actual);
        }
    }

    return rv;
}

int set_frequency(struct cli_state *state, int argc, char **argv)
{
    /* Usage: set frequency [<rx|tx>] <frequency in Hz> */
    int rv                  = CLI_RET_OK;
    int *err                = &state->last_lib_error;
    bladerf_channel channel = BLADERF_CHANNEL_INVALID;
    uint64_t freq;
    struct bladerf_range range;
    bool ok;
    int status;

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
            break;
        case 4:
            /* Parse channel */
            channel = get_channel(argv[2], &ok);
            if (!ok) {
                invalid_channel(state, argv[0], argv[2]);
                rv = CLI_RET_INVPARAM;
            }
            break;
        default:
            /* Print usage */
            printf("Usage: set frequency <channel> <frequency>\n");
            printf("\n");
            printf("    %-15s%s\n", "channel", "Set 'rx' or 'tx' frequency");
            printf("    %-15s%s\n", "frequency", "Frequency in Hz");
            return rv;
    }

    if (rv == CLI_RET_OK) {
        /* Get valid frequency range */
        status = bladerf_get_frequency_range(state->dev, channel, &range);
        if (status < 0) {
            *err = status;
            rv   = CLI_RET_LIBBLADERF;
        }
    }

    if (rv == CLI_RET_OK) {
        /* Parse frequency */
        freq = str2uint64_suffix(argv[argc - 1], range.min, range.max,
                                 freq_suffixes, NUM_FREQ_SUFFIXES, &ok);

        if (!ok) {
            cli_err_nnl(state, argv[0], "Invalid frequency (%s)\n",
                        argv[argc - 1]);
            rv = CLI_RET_INVPARAM;
        }
    }

    if (rv == CLI_RET_OK && argc == 3) {
        /* Change RX frequency */
        rv = _set_print_frequency(state, BLADERF_CHANNEL_RX(0), freq);
        /* Change TX frequency */
        rv = _set_print_frequency(state, BLADERF_CHANNEL_TX(0), freq);

        return rv;
    }

    if (rv == CLI_RET_OK) {
        rv = _set_print_frequency(state, channel, freq);
    }

    return rv;
}

static void _print_gpio_bladerf1(uint32_t val)
{
    printf("    %-20s%-10s\n",
           "LMS Enable:", val & 0x01 ? "Enabled" : "Reset");  // Active low
    printf("    %-20s%-10s\n",
           "LMS RX Enable:", val & 0x02 ? "Enabled" : "Disabled");
    printf("    %-20s%-10s\n",
           "LMS TX Enable:", val & 0x04 ? "Enabled" : "Disabled");
    printf("    %-20s", "TX Band:");
    if (((val >> 3) & 3) == 2) {
        printf("Low Band (300M - 1.5GHz)\n");
    } else if (((val >> 3) & 3) == 1) {
        printf("High Band (1.5GHz - 3.8GHz)\n");
    } else {
        printf("Invalid Band Selection!\n");
    }
    printf("    %-20s", "RX Band:");
    if (((val >> 5) & 3) == 2) {
        printf("Low Band (300M - 1.5GHz)\n");
    } else if (((val >> 5) & 3) == 1) {
        printf("High Band (1.5GHz - 3.8GHz)\n");
    } else {
        printf("Invalid Band Selection!\n");
    }
    printf("    %-20s", "RX Source:");
    switch ((val & 0x300) >> 8) {
        case 0:
            printf("Baseband\n");
            break;
        case 1:
            printf("Internal 12-bit counter\n");
            break;
        case 2:
            printf("Internal 32-bit counter\n");
            break;
        case 3:
            printf("Whitened Entropy\n");
            break;
        default:
            printf("????\n");
            break;
    }
}

static void _print_gpio_bladerf2(uint32_t val)
{
    int usb_speed       = (val >> 7) & 0x01;   // 7
    int rx_mux_sel      = (val >> 8) & 0x07;   // 10 downto 8
    int adf_chip_enable = (val >> 11) & 0x01;  // 11
    int leds            = (val >> 12) & 0x07;  // 14 downto 12
    int led_mode        = (val >> 15) & 0x01;  // 15
    int meta_sync       = (val >> 16) & 0x01;  // 16
    int xb_mode         = (val >> 30) & 0x03;  // 31 downto 30

    printf("    %-20s%-10s\n", "USB Speed:", usb_speed ? "1" : "0");
    printf("    %-20s0x%08x ", "RX Mux:", rx_mux_sel);
    switch (rx_mux_sel) {
        case 0:
            printf("(Baseband)\n");
            break;
        case 1:
            printf("(Internal 12-bit counter)\n");
            break;
        case 2:
            printf("(Internal 32-bit counter)\n");
            break;
        case 3:
            printf("(Whitened Entropy)\n");
            break;
        default:
            printf("( ???? )\n");
            break;
    }

    printf("    %-20s%-10s\n", "ADF Chip Enable:", adf_chip_enable ? "1" : "0");
    printf("    %-20s%-10s\n", "LED Mode:", led_mode ? "Manual" : "Default");

    if (led_mode) {
        printf("        %-16s%-10s\n", "LED 1:", leds & 0x01 ? "On" : "Off");
        printf("        %-16s%-10s\n", "LED 2:", leds & 0x02 ? "On" : "Off");
        printf("        %-16s%-10s\n", "LED 3:", leds & 0x04 ? "On" : "Off");
    }

    printf("    %-20s%-10s\n", "Meta Sync:", meta_sync ? "1" : "0");
    printf("    %-20s0x%08x\n", "XB Mode:", xb_mode);
}

int print_agc(struct cli_state *state, int argc, char **argv)
{
    int rv = CLI_RET_OK, status;
    bladerf_gain_mode mode;

    status = bladerf_get_gain_mode( state->dev, BLADERF_MODULE_RX, &mode );
    if (status < 0) {
        state->last_lib_error = status;
        rv = CLI_RET_LIBBLADERF;
    } else {
        printf( "   AGC: %-10s\n",
                mode == BLADERF_GAIN_MANUAL ? "Disabled" : "Enabled" );
    }
    return rv;
}

int set_agc(struct cli_state *state, int argc, char **argv)
{
    int rv = CLI_RET_OK, status;
    bladerf_gain_mode mode;

    if (argc < 3) {
        printf( "Usage: set agc <on|off>\n\n" );
        rv = CLI_RET_NARGS;
    }

    if (!strcmp(argv[2], "on")) {
       mode = BLADERF_GAIN_AUTOMATIC;
    } else if (!strcmp(argv[2], "off")) {
       mode = BLADERF_GAIN_MANUAL;
    } else {
        cli_err_nnl(state, argv[0], "Invalid AGC value (%s)\n", argv[2]);
        rv = CLI_RET_INVPARAM;
    }

    if (rv == CLI_RET_OK) {
        status = bladerf_set_gain_mode( state->dev, BLADERF_MODULE_RX, mode );
        if (status < 0) {
            state->last_lib_error = status;
            rv = CLI_RET_LIBBLADERF;
        } else {
            printf( "   AGC: %-10s\n",
                    mode == BLADERF_GAIN_MANUAL ? "Disabled" : "Enabled" );
        }
    }

    return rv;
}

int print_gpio(struct cli_state *state, int argc, char **argv)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int status;
    uint32_t val;

    status = bladerf_config_gpio_read(state->dev, &val);
    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
    }

    if (rv == CLI_RET_OK) {
        printf("  GPIO: 0x%8.8x\n", val);

        if (_is_board(state->dev, BOARD_BLADERF1)) {
            _print_gpio_bladerf1(val);
        }

        if (_is_board(state->dev, BOARD_BLADERF2)) {
            _print_gpio_bladerf2(val);
        }
    }

    return rv;
}

int set_gpio(struct cli_state *state, int argc, char **argv)
{
    /* set gpio <value> */
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    uint32_t val;
    bool ok;

    if (argc == 3) {
        val = str2uint(argv[2], 0, UINT_MAX, &ok);
        if (!ok) {
            cli_err_nnl(state, argv[0], "Invalid gpio value (%s)\n", argv[2]);
            rv = CLI_RET_INVPARAM;
        } else {
            rv = bladerf_config_gpio_write(state->dev, val);
            if (rv != 0) {
                *err = rv;
                rv   = CLI_RET_LIBBLADERF;
            } else {
                rv = print_gpio(state, argc, argv);
            }
        }
    } else {
        rv = CLI_RET_NARGS;
    }
    return rv;
}

struct xb_gpio_lut {
    const char *name;
    uint32_t bitmask;
};

// clang-format off
static const struct xb_gpio_lut xb_pins[] = {
    { "GPIO_1",          BLADERF_XB_GPIO_01 },
    { "GPIO_2",          BLADERF_XB_GPIO_02 },
    { "GPIO_3",          BLADERF_XB_GPIO_03 },
    { "GPIO_4",          BLADERF_XB_GPIO_04 },
    { "GPIO_5",          BLADERF_XB_GPIO_05 },
    { "GPIO_6",          BLADERF_XB_GPIO_06 },
    { "GPIO_7",          BLADERF_XB_GPIO_07 },
    { "GPIO_8",          BLADERF_XB_GPIO_08 },
    { "GPIO_9",          BLADERF_XB_GPIO_09 },
    { "GPIO_10",         BLADERF_XB_GPIO_10 },
    { "GPIO_11",         BLADERF_XB_GPIO_11 },
    { "GPIO_12",         BLADERF_XB_GPIO_12 },
    { "GPIO_13",         BLADERF_XB_GPIO_13 },
    { "GPIO_14",         BLADERF_XB_GPIO_14 },
    { "GPIO_15",         BLADERF_XB_GPIO_15 },
    { "GPIO_16",         BLADERF_XB_GPIO_16 },
    { "GPIO_17",         BLADERF_XB_GPIO_17 },
    { "GPIO_18",         BLADERF_XB_GPIO_18 },
    { "GPIO_19",         BLADERF_XB_GPIO_19 },
    { "GPIO_20",         BLADERF_XB_GPIO_20 },
    { "GPIO_21",         BLADERF_XB_GPIO_21 },
    { "GPIO_22",         BLADERF_XB_GPIO_22 },
    { "GPIO_23",         BLADERF_XB_GPIO_23 },
    { "GPIO_24",         BLADERF_XB_GPIO_24 },
    { "GPIO_25",         BLADERF_XB_GPIO_25 },
    { "GPIO_26",         BLADERF_XB_GPIO_26 },
    { "GPIO_27",         BLADERF_XB_GPIO_27 },
    { "GPIO_28",         BLADERF_XB_GPIO_28 },
    { "GPIO_29",         BLADERF_XB_GPIO_29 },
    { "GPIO_30",         BLADERF_XB_GPIO_30 },
    { "GPIO_31",         BLADERF_XB_GPIO_31 },
    { "GPIO_32",         BLADERF_XB_GPIO_32 },
};

static const struct xb_gpio_lut xb200_pins[] = {
    { "J7_1",         BLADERF_XB200_PIN_J7_1 },
    { "J7_2",         BLADERF_XB200_PIN_J7_2 },
    { "J7_6",         BLADERF_XB200_PIN_J7_6 },
    { "J13_1",        BLADERF_XB200_PIN_J13_1 },
    { "J13_2",        BLADERF_XB200_PIN_J13_2 },
    { "J16_1",        BLADERF_XB200_PIN_J16_1 },
    { "J16_2",        BLADERF_XB200_PIN_J16_2 },
    { "J16_3",        BLADERF_XB200_PIN_J16_3 },
    { "J16_4",        BLADERF_XB200_PIN_J16_4 },
    { "J16_5",        BLADERF_XB200_PIN_J16_5 },
    { "J16_6",        BLADERF_XB200_PIN_J16_6 },
};

static const struct xb_gpio_lut xb100_pins[] = {
    { "J2_3",         BLADERF_XB100_PIN_J2_3 },
    { "J2_4",         BLADERF_XB100_PIN_J2_4 },
    { "J3_3",         BLADERF_XB100_PIN_J3_3 },
    { "J3_4",         BLADERF_XB100_PIN_J3_4 },
    { "J4_3",         BLADERF_XB100_PIN_J4_3 },
    { "J4_4",         BLADERF_XB100_PIN_J4_4 },
    { "J5_3",         BLADERF_XB100_PIN_J5_3 },
    { "J5_4",         BLADERF_XB100_PIN_J5_4 },
    { "J11_2",        BLADERF_XB100_PIN_J11_2 },
    { "J11_3",        BLADERF_XB100_PIN_J11_3 },
    { "J11_4",        BLADERF_XB100_PIN_J11_4 },
    { "J11_5",        BLADERF_XB100_PIN_J11_5 },
    { "J12_5",        BLADERF_XB100_PIN_J12_5 },
    { "LED_D1",       BLADERF_XB100_LED_D1 },
    { "LED_D2",       BLADERF_XB100_LED_D2 },
    { "LED_D3",       BLADERF_XB100_LED_D3 },
    { "LED_D4",       BLADERF_XB100_LED_D4 },
    { "LED_D5",       BLADERF_XB100_LED_D5 },
    { "LED_D6",       BLADERF_XB100_LED_D6 },
    { "LED_D7",       BLADERF_XB100_LED_D7 },
    { "LED_D8",       BLADERF_XB100_LED_D8 },
    { "TLED_RED",     BLADERF_XB100_TLED_RED },
    { "TLED_GREEN",   BLADERF_XB100_TLED_GREEN },
    { "TLED_BLUE",    BLADERF_XB100_TLED_BLUE },
    { "DIP_SW1",      BLADERF_XB100_DIP_SW1 },
    { "DIP_SW2",      BLADERF_XB100_DIP_SW2 },
    { "DIP_SW3",      BLADERF_XB100_DIP_SW3 },
    { "DIP_SW4",      BLADERF_XB100_DIP_SW4 },
    { "BTN_J6",       BLADERF_XB100_BTN_J6 },
    { "BTN_J7",       BLADERF_XB100_BTN_J7 },
    { "BTN_J8",       BLADERF_XB100_BTN_J8 },
};
// clang-format on

static int get_xb_lut(struct cli_state *state,
                      const struct xb_gpio_lut **lut,
                      size_t *len)
{
    bladerf_xb xb_type;
    int status;

    status = bladerf_expansion_get_attached(state->dev, &xb_type);
    if (status < 0) {
        return status;
    }

    switch (xb_type) {
        case BLADERF_XB_100:
            *lut = xb100_pins;
            *len = ARRAY_SIZE(xb100_pins);
            break;

        case BLADERF_XB_200:
            *lut = xb200_pins;
            *len = ARRAY_SIZE(xb200_pins);
            break;

        default:
            *lut = xb_pins;
            *len = ARRAY_SIZE(xb_pins);
    }

    return 0;
}

static int str2xbgpio(struct cli_state *state,
                      const char *str,
                      const struct xb_gpio_lut **io,
                      bool nnl_on_error)
{
    const struct xb_gpio_lut *lut = NULL;
    size_t lut_len;
    int status;

    *io = NULL;

    status = get_xb_lut(state, &lut, &lut_len);
    if (status != 0) {
        state->last_lib_error = status;
        return CLI_RET_LIBBLADERF;
    }

    /* Should not occur - would be indicative of a bug */
    if (lut == NULL || lut_len == 0) {
        return CLI_RET_UNKNOWN;
    }

    for (size_t i = 0; i < lut_len; i++) {
        if (!strcasecmp(str, lut[i].name)) {
            *io = &lut[i];
            return 0;
        }
    }

    return CLI_RET_INVPARAM;
}

static int print_xbio_base(struct cli_state *state,
                           int argc,
                           char **argv,
                           bool is_dir)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;

    const struct xb_gpio_lut *lut = NULL;
    const struct xb_gpio_lut *pin = NULL;

    size_t lut_len;
    uint32_t val;
    int status;

    enum {
        PRINT_LIST,
        PRINT_ALL,
        PRINT_REGISTER,
        PRINT_PIN,
    } action;


    if (argc <= 2) {
        printf("Usage: print xb_gpio%s <name>\n\n", is_dir ? "_dir" : "");

        printf("<name> describes what to print. "
               "It may be one of the following:\n\n");

        printf("  \"all\" - Print the register value and the state of\n"
               "  individual pins. Note that the pin names shown\n"
               "  is dependent upon which expansion board has been\n"
               "  enabled.\n\n");

        printf(
            "  \"reg\" or \"register\" - Print the GPIO%s register value.\n\n",
            is_dir ? " direction" : "");

        printf("  \"list\" - Display available pins to print.\n\n");

        printf("  One of the pin names from the aforementioned list.\n");

        return 0;

    } else if (argc > 3) {
        return CLI_RET_NARGS;
    }

    if (is_dir) {
        status = bladerf_expansion_gpio_dir_read(state->dev, &val);
    } else {
        status = bladerf_expansion_gpio_read(state->dev, &val);
    }

    if (status < 0) {
        *err = status;
        return CLI_RET_LIBBLADERF;
    }

    status = get_xb_lut(state, &lut, &lut_len);
    if (status < 0) {
        *err = status;
        return CLI_RET_LIBBLADERF;
    }

    /* Should not occur - indicative of a bug */
    if (lut == NULL || lut_len == 0) {
        return CLI_RET_UNKNOWN;
    }

    if (!strcasecmp(argv[2], "all")) {
        action = PRINT_ALL;
    } else if (!strcasecmp(argv[2], "list")) {
        action = PRINT_LIST;
    } else if (!strcasecmp(argv[2], "reg") ||
               !strcasecmp(argv[2], "register")) {
        action = PRINT_REGISTER;
    } else {
        status = str2xbgpio(state, argv[2], &pin, true);
        if (status != 0) {
            cli_err_nnl(state, "Invalid pin name or option", "%s\n", argv[2]);
            return CLI_RET_INVPARAM;
        }

        action = PRINT_PIN;
    }

    if (action == PRINT_ALL || action == PRINT_REGISTER) {
        printf("  Expansion GPIO%s register: 0x%8.8x\n",
               is_dir ? " direction" : "", val);
    }

    if (action == PRINT_ALL) {
        printf("\n");

        for (size_t i = 0; i < lut_len; i++) {
            uint32_t bit = (val & lut[i].bitmask) ? 1 : 0;

            if (is_dir) {
                printf("%12s: %s\n", lut[i].name, bit ? "output" : "input");
            } else {
                printf("%12s: %u\n", lut[i].name, bit);
            }
        }
    } else if (action == PRINT_LIST) {
        for (size_t i = 0; i < lut_len; i++) {
            printf("%12s\n", lut[i].name);
        }
    } else if (action == PRINT_PIN) {
        uint32_t bit = (val & pin->bitmask) ? 1 : 0;

        if (is_dir) {
            printf("%12s: %s\n", pin->name, bit ? "output" : "input");
        } else {
            printf("%12s: %u\n", pin->name, bit);
        }
    }

    return rv;
}

int set_xbio_base(struct cli_state *state, int argc, char **argv, bool is_dir)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;

    const struct xb_gpio_lut *pin;
    const char *suffix = is_dir ? "_dir" : "";

    uint32_t val;
    bool ok;

    switch (argc) {
        case 0:
        case 1:
        case 2:
            printf("Usage: set xb_gpio%s <name> <value>\n\n", suffix);

            printf("<name> describes what to set. It may be one of the "
                   "following:\n\n");

            printf("  \"reg\" or \"register\" - Set the entire GPIO%s register "
                   "value.\n\n",
                   is_dir ? " direction" : "");

            printf("  One of the pin names listed by: \"print xb_gpio%s "
                   "list\"\n\n",
                   suffix);

            if (is_dir) {
                printf("Specify <value> as \"input\" (0) or \"output\" (1).\n");
            }

            break;

        case 4:
            /* Specifying a pin by name */
            rv = str2xbgpio(state, argv[2], &pin, false);
            if (rv == 0) {
                val = UINT32_MAX;

                if (is_dir) {
                    if (!strcasecmp(argv[3], "in") ||
                        !strcasecmp(argv[3], "input")) {
                        val = 0;
                    } else if (!strcasecmp(argv[3], "out") ||
                               !strcasecmp(argv[3], "output")) {
                        val = 1;
                    }
                }

                if (!is_dir || val == UINT32_MAX) {
                    val = str2uint(argv[3], 0, 1, &ok);
                    if (!ok) {
                        cli_err_nnl(state, "Invalid pin value", "%s\n",
                                    argv[3]);
                        return CLI_RET_INVPARAM;
                    }
                }

                if (val == 1) {
                    val = pin->bitmask;
                }

                if (is_dir) {
                    rv = bladerf_expansion_gpio_dir_masked_write(
                        state->dev, pin->bitmask, val);
                } else {
                    rv = bladerf_expansion_gpio_masked_write(state->dev,
                                                             pin->bitmask, val);
                }

                if (rv < 0) {
                    *err = rv;
                    rv   = CLI_RET_LIBBLADERF;
                } else {
                    rv = print_xbio_base(state, 3, argv, is_dir);
                }
            } else if (!strcasecmp("reg", argv[2]) ||
                       !strcasecmp("register", argv[2])) {
                /* Specifying a write of the entire XB GPIO (Direction) reg */
                val = str2uint(argv[3], 0, UINT32_MAX, &ok);
                if (!ok) {
                    cli_err_nnl(state, "Invalid register value", "%s\n",
                                argv[3]);
                    return CLI_RET_INVPARAM;
                }

                if (is_dir) {
                    rv = bladerf_expansion_gpio_dir_write(state->dev, val);
                } else {
                    rv = bladerf_expansion_gpio_write(state->dev, val);
                }

                if (rv < 0) {
                    *err = rv;
                    rv   = CLI_RET_LIBBLADERF;
                } else {
                    rv = print_xbio_base(state, 3, argv, is_dir);
                }

            } else {
                cli_err_nnl(state, "Invalid pin name or option", "%s\n",
                            argv[2]);
                rv = CLI_RET_INVPARAM;
            }
            break;


        default:
            rv = CLI_RET_NARGS;
    }

    return rv;
}

int print_xb_gpio(struct cli_state *state, int argc, char **argv)
{
    return print_xbio_base(state, argc, argv, false);
}

int set_xb_gpio(struct cli_state *state, int argc, char **argv)
{
    return set_xbio_base(state, argc, argv, false);
}

int print_xb_gpio_dir(struct cli_state *state, int argc, char **argv)
{
    return print_xbio_base(state, argc, argv, true);
}

int set_xb_gpio_dir(struct cli_state *state, int argc, char **argv)
{
    return set_xbio_base(state, argc, argv, true);
}

int print_lnagain(struct cli_state *state, int argc, char **argv)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    bladerf_lna_gain gain;
    int status;

    status = bladerf_get_lna_gain(state->dev, &gain);
    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
    } else {
        switch (gain) {
            case BLADERF_LNA_GAIN_MAX:
                printf("  RXLNA Gain:    6 dB\n");
                break;

            case BLADERF_LNA_GAIN_MID:
                printf("  RXLNA Gain:    3 dB\n");
                break;

            case BLADERF_LNA_GAIN_BYPASS:
                printf("  RXLNA Gain:    0 dB\n");
                break;

            default:
                printf("  RXLNA Gain:    Unknown (%d)\n", gain);
                break;
        }
    }

    return rv;
}

int set_lnagain(struct cli_state *state, int argc, char **argv)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int status;

    if (argc != 3) {
        rv = CLI_RET_NARGS;
    } else {
        bladerf_lna_gain gain = BLADERF_LNA_GAIN_UNKNOWN;
        if (strcasecmp(argv[2], "6") == 0) {
            gain = BLADERF_LNA_GAIN_MAX;
        } else if (strcasecmp(argv[2], "3") == 0) {
            gain = BLADERF_LNA_GAIN_MID;
        } else if (strcasecmp(argv[2], "0") == 0) {
            gain = BLADERF_LNA_GAIN_BYPASS;
        } else {
            invalid_gain(state, argv[0], argv[1], argv[2]);
        }

        if (gain != BLADERF_LNA_GAIN_UNKNOWN) {
            status = bladerf_set_lna_gain(state->dev, gain);
            if (status < 0) {
                *err = status;
                rv   = CLI_RET_LIBBLADERF;
            } else {
                rv = print_lnagain(state, 2, argv);
            }
        } else {
            rv = CLI_RET_INVPARAM;
        }
    }

    return rv;
}

int print_loopback(struct cli_state *state, int argc, char **argv)
{
    int status;
    bladerf_loopback loopback;
    int *err = &state->last_lib_error;

    status = bladerf_get_loopback(state->dev, &loopback);
    if (status != 0) {
        *err = status;
        return CLI_RET_LIBBLADERF;
    }

    switch (loopback) {
        case BLADERF_LB_BB_TXLPF_RXVGA2:
            printf("  Loopback mode: bb_txlpf_rxvga2\n");
            break;

        case BLADERF_LB_BB_TXLPF_RXLPF:
            printf("  Loopback mode: bb_txlpf_rxlpf\n");
            break;

        case BLADERF_LB_BB_TXVGA1_RXVGA2:
            printf("  Loopback mode: bb_txvga1_rxvga2\n");
            break;

        case BLADERF_LB_BB_TXVGA1_RXLPF:
            printf("  Loopback mode: bb_txvga1_rxlpf\n");
            break;

        case BLADERF_LB_RF_LNA1:
            printf("  Loopback mode: rf_lna1\n");
            break;

        case BLADERF_LB_RF_LNA2:
            printf("  Loopback mode: rf_lna2\n");
            break;

        case BLADERF_LB_RF_LNA3:
            printf("  Loopback mode: rf_lna3\n");
            break;

        case BLADERF_LB_FIRMWARE:
            printf("  Loopback mode: firmware\n");
            break;

        case BLADERF_LB_NONE:
            printf("  Loopback mode: none\n");
            break;

        case BLADERF_LB_AD9361_BIST:
            printf("  Loopback mode: ad9361_bist\n");
            break;

        default:
            cli_err_nnl(state, argv[0],
                        "Read back unexpected loopback mode: %d\n",
                        (int)loopback);
            return CLI_RET_INVPARAM;
    }

    return 0;
}

int set_loopback(struct cli_state *state, int argc, char **argv)
{
    int status;
    bladerf_loopback loopback;
    int *err = &state->last_lib_error;

    if (argc == 2) {
        printf("Usage: %s %s <mode>, where <mode> is one of the following:\n",
               argv[0], argv[1]);
        printf("\n");
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
        printf("  %-18s%s\n", "firmware", "Firmware-based sample loopback\n");
        printf("  %-18s%s\n", "ad9361_bist", "AD9361 BIST loopback\n");
        printf("  %-18s%s\n", "none", "Loopback disabled - Normal operation\n");

        return CLI_RET_INVPARAM;

    } else if (argc != 3) {
        return CLI_RET_NARGS;
    }

    if (!strcasecmp(argv[2], "bb_txlpf_rxvga2")) {
        loopback = BLADERF_LB_BB_TXLPF_RXVGA2;
    } else if (!strcasecmp(argv[2], "bb_txlpf_rxlpf")) {
        loopback = BLADERF_LB_BB_TXLPF_RXLPF;
    } else if (!strcasecmp(argv[2], "bb_txvga1_rxvga2")) {
        loopback = BLADERF_LB_BB_TXVGA1_RXVGA2;
    } else if (!strcasecmp(argv[2], "bb_txvga1_rxlpf")) {
        loopback = BLADERF_LB_BB_TXVGA1_RXLPF;
    } else if (!strcasecmp(argv[2], "rf_lna1")) {
        loopback = BLADERF_LB_RF_LNA1;
    } else if (!strcasecmp(argv[2], "rf_lna2")) {
        loopback = BLADERF_LB_RF_LNA2;
    } else if (!strcasecmp(argv[2], "rf_lna3")) {
        loopback = BLADERF_LB_RF_LNA3;
    } else if (!strcasecmp(argv[2], "firmware")) {
        loopback = BLADERF_LB_FIRMWARE;
    } else if (!strcasecmp(argv[2], "ad9361_bist")) {
        loopback = BLADERF_LB_AD9361_BIST;
    } else if (!strcasecmp(argv[2], "none")) {
        loopback = BLADERF_LB_NONE;
    } else {
        cli_err_nnl(state, argv[0], "Invalid loopback mode provided: %s\n",
                    argv[2]);
        return CLI_RET_INVPARAM;
    }

    status = bladerf_set_loopback(state->dev, loopback);
    if (status != 0) {
        *err = status;
        return CLI_RET_LIBBLADERF;
    } else {
        return print_loopback(state, 2, argv);
    }
}

int print_rx_mux(struct cli_state *state, int argc, char **argv)
{
    int status;
    const char *mux_str;
    bladerf_rx_mux mux_setting;
    int *err = &state->last_lib_error;

    status = bladerf_get_rx_mux(state->dev, &mux_setting);
    if (status < 0) {
        *err = status;
        return CLI_RET_LIBBLADERF;
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
    return 0;
}

int set_rx_mux(struct cli_state *state, int argc, char **argv)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    bladerf_rx_mux mux_val;
    int status;

    if (argc != 3) {
        return CLI_RET_NARGS;
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
        cli_err_nnl(state, "Invalid RX mux mode", "%s\n", argv[2]);
        return CLI_RET_INVPARAM;
    }

    status = bladerf_set_rx_mux(state->dev, mux_val);
    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
    } else {
        rv = print_rx_mux(state, 2, argv);
    }

    return rv;
}

int print_rxvga1(struct cli_state *state, int argc, char **argv)
{
    int rv   = CLI_RET_OK, gain, status;
    int *err = &state->last_lib_error;

    status = bladerf_get_rxvga1(state->dev, &gain);
    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
    } else {
        printf("  RXVGA1 Gain: %3d dB\n", gain);
    }

    return rv;
}

int set_rxvga1(struct cli_state *state, int argc, char **argv)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int gain;
    int status;
    bool ok;

    if (argc != 3) {
        rv = CLI_RET_NARGS;
    } else {
        gain = str2int(argv[2], BLADERF_RXVGA1_GAIN_MIN,
                       BLADERF_RXVGA1_GAIN_MAX, &ok);
        if (!ok) {
            invalid_gain(state, argv[0], argv[1], argv[2]);
            rv = CLI_RET_INVPARAM;
        } else {
            status = bladerf_set_rxvga1(state->dev, gain);
            if (status < 0) {
                *err = status;
                rv   = CLI_RET_LIBBLADERF;
            } else {
                rv = print_rxvga1(state, 2, argv);
            }
        }
    }
    return rv;
}

int print_rxvga2(struct cli_state *state, int argc, char **argv)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int gain;
    int status;

    status = bladerf_get_rxvga2(state->dev, &gain);
    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
    } else {
        printf("  RXVGA2 Gain: %3d dB\n", gain);
    }

    return rv;
}

int set_rxvga2(struct cli_state *state, int argc, char **argv)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int gain;
    int status;
    bool ok;

    if (argc != 3) {
        rv = CLI_RET_NARGS;
    } else {
        gain = str2int(argv[2], BLADERF_RXVGA2_GAIN_MIN,
                       BLADERF_RXVGA2_GAIN_MAX, &ok);

        if (!ok) {
            invalid_gain(state, argv[0], argv[1], argv[2]);
            rv = CLI_RET_INVPARAM;
        } else {
            status = bladerf_set_rxvga2(state->dev, gain);
            if (status < 0) {
                *err = status;
                rv   = CLI_RET_LIBBLADERF;
            } else {
                rv = print_rxvga2(state, 2, argv);
            }
        }
    }
    return rv;
}

int print_samplerate(struct cli_state *state, int argc, char **argv)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    struct bladerf_rational_rate rate;
    bladerf_channel channel;
    int status;
    bool ok;

    if (argc < 2 || argc > 3) {
        rv = CLI_RET_NARGS;
    }

    if (rv == CLI_RET_OK && argc == 3) {
        channel = get_channel(argv[2], &ok);
        if (!ok) {
            invalid_channel(state, argv[0], argv[2]);
            rv = CLI_RET_INVPARAM;
        }
    }

    if (rv == CLI_RET_OK && (argc == 2 || _is_rx(channel))) {
        status = bladerf_get_rational_sample_rate(state->dev,
                                                  BLADERF_CHANNEL_RX(0), &rate);
        if (status < 0) {
            *err = status;
            rv   = CLI_RET_LIBBLADERF;
        } else {
            printf("  RX sample rate: %" PRIu64 " %" PRIu64 "/%" PRIu64 "\n",
                   rate.integer, rate.num, rate.den);
        }
    }

    if (rv == CLI_RET_OK && (argc == 2 || _is_tx(channel))) {
        status = bladerf_get_rational_sample_rate(state->dev,
                                                  BLADERF_CHANNEL_TX(0), &rate);
        if (status < 0) {
            *err = status;
            rv   = CLI_RET_LIBBLADERF;
        } else {
            printf("  TX sample rate: %" PRIu64 " %" PRIu64 "/%" PRIu64 "\n",
                   rate.integer, rate.num, rate.den);
        }
    }

    return rv;
}

int set_samplerate(struct cli_state *state, int argc, char **argv)
{
    /* Usage: set samplerate [rx|tx] [integer [numerator denominator]]*/
    int rv                  = CLI_RET_OK;
    int *err                = &state->last_lib_error;
    bladerf_channel channel = BLADERF_CHANNEL_INVALID;
    struct bladerf_rational_rate rate, actual;
    bladerf_dev_speed usb_speed;
    bool ok;
    size_t idx;
    int status;

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

    const char *samplewarning =
        "\n"
        "  Warning: The provided sample rate may result in timeouts with the\n"
        "           current %s connection. A SuperSpeed connection or a lower\n"
        "           sample rate are recommended.\n";

    const char *settext =
        "  Setting %s sample rate - req: %9" PRIu64 " %" PRIu64 "/%" PRIu64
        "Hz, actual: %9" PRIu64 " %" PRIu64 "/%" PRIu64 "Hz\n";

    /* Check argument count */
    switch (argc) {
        /* Valid quantities: 3 through 6 */
        case 4:
        case 6:
            /* Parse channel */
            channel = get_channel(argv[2], &ok);
            if (!ok) {
                invalid_channel(state, argv[0], argv[2]);
                rv = CLI_RET_INVPARAM;
            }
            break;
        case 3:
        case 5:
            break;

        /* Invalid quantities */
        case 2:
            printf("%s", helptext);
            return 0;
        default:
            printf("%s", helptext);
            rv = CLI_RET_NARGS;
            break;
    }

    if (rv == CLI_RET_OK) {
        /* Initialize values */
        idx          = 0;
        usb_speed    = bladerf_device_speed(state->dev);
        rate.integer = 0;
        rate.num     = 0;
        rate.den     = 1;
        idx          = (argc == 3 || argc == 5) ? 2 : 3;

        /* Allow a value of zero to be specified for the integer portion
         * so that the entire value can be specified in num/den. libbladeRF
         * will return an error if the sample rate is invalid */
        rate.integer = str2uint64_suffix(argv[idx], 0, UINT64_MAX,
                                         freq_suffixes, NUM_FREQ_SUFFIXES, &ok);

        /* Integer portion didn't make it */
        if (!ok) {
            cli_err_nnl(state, argv[0],
                        "Invalid %s value in specified sample rate (%s)\n",
                        "integer", argv[idx]);
            rv = CLI_RET_INVPARAM;
        }
    }

    /* Take in num/den if they are present */
    if (rv == CLI_RET_OK && (argc == 5 || argc == 6)) {
        idx++;
        rate.num = str2uint64_suffix(argv[idx], 0, UINT64_MAX, freq_suffixes,
                                     NUM_FREQ_SUFFIXES, &ok);
        if (!ok) {
            cli_err_nnl(state, argv[0],
                        "Invalid %s value in specified sample rate (%s)\n",
                        "numerator", argv[idx]);
            rv = CLI_RET_INVPARAM;
        }
    }

    if (rv == CLI_RET_OK && (argc == 5 || argc == 6)) {
        idx++;
        rate.den = str2uint64_suffix(argv[idx], 1, UINT64_MAX, freq_suffixes,
                                     NUM_FREQ_SUFFIXES, &ok);
        if (!ok) {
            cli_err_nnl(state, argv[0],
                        "Invalid %s value in specified sample rate (%s)\n",
                        "denominator", argv[idx]);
            rv = CLI_RET_INVPARAM;
        }
    }

    if (rv == CLI_RET_OK && (argc == 3 || argc == 5 || _is_rx(channel))) {
        status = bladerf_set_rational_sample_rate(
            state->dev, BLADERF_CHANNEL_RX(0), &rate, &actual);

        if (status < 0) {
            *err = status;
            rv   = CLI_RET_LIBBLADERF;
        } else {
            printf(settext, "RX", rate.integer, rate.num, rate.den,
                   actual.integer, actual.num, actual.den);
        }
    }

    if (rv == CLI_RET_OK && (argc == 3 || argc == 5 || _is_tx(channel))) {
        status = bladerf_set_rational_sample_rate(
            state->dev, BLADERF_CHANNEL_TX(0), &rate, &actual);

        if (status < 0) {
            *err = status;
            rv   = CLI_RET_LIBBLADERF;
        } else {
            printf(settext, "TX", rate.integer, rate.num, rate.den,
                   actual.integer, actual.num, actual.den);
        }
    }

    /* Discontinuities have been reported for 2.0 on Intel
     * controllers above 6MHz. */
    if (rv == CLI_RET_OK && usb_speed != BLADERF_DEVICE_SPEED_SUPER &&
        (actual.integer > 6000000 ||
         ((actual.integer == 6000000) && (actual.num != 0)))) {
        printf(samplewarning, devspeed2str(usb_speed));
    }

    return rv;
}

int set_sampling(struct cli_state *state, int argc, char **argv)
{
    /* Usage: set sampling [internal|external] */
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    bladerf_sampling mode;
    int status;

    if (argc != 3) {
        rv = CLI_RET_NARGS;
    } else {
        if (strcasecmp("internal", argv[2]) == 0) {
            mode = BLADERF_SAMPLING_INTERNAL;
        } else if (strcasecmp("external", argv[2]) == 0) {
            mode = BLADERF_SAMPLING_EXTERNAL;
        } else {
            cli_err_nnl(state, argv[0], "Invalid sampling mode (%s)\n",
                        argv[2]);
            return CLI_RET_INVPARAM;
        }

        status = bladerf_set_sampling(state->dev, mode);
        if (status) {
            *err = status;
            rv   = CLI_RET_LIBBLADERF;
        } else {
            print_sampling(state, 2, argv);
        }
    }
    return rv;
}

int print_sampling(struct cli_state *state, int argc, char **argv)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    bladerf_sampling mode;
    int status;

    if (argc != 2) {
        rv = CLI_RET_NARGS;
    } else {
        /* Read the ADC input mux */
        status = bladerf_get_sampling(state->dev, &mode);
        if (status) {
            *err = status;
            rv   = CLI_RET_LIBBLADERF;
        } else {
            switch (mode) {
                case BLADERF_SAMPLING_EXTERNAL:
                    printf("  Sampling: External\n");
                    break;

                case BLADERF_SAMPLING_INTERNAL:
                    printf("  Sampling: Internal\n");
                    break;

                default:
                    printf("  Sampling: Unknown\n");
            }
        }
    }

    return rv;
}

int print_smb_mode(struct cli_state *state, int argc, char **argv)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    bladerf_smb_mode mode;
    const char *mode_str;
    int status;

    status = bladerf_get_smb_mode(state->dev, &mode);

    mode_str = smb_mode_to_str(mode);

    printf("  SMB Mode:   %s\n", mode_str);

    if (mode == BLADERF_SMB_MODE_OUTPUT) {
        struct bladerf_rational_rate rate;

        status = bladerf_get_rational_smb_frequency(state->dev, &rate);
        if (status != 0) {
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
    if (status != 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
    }

    return rv;
}

int set_smb_mode(struct cli_state *state, int argc, char **argv)
{
    bladerf_smb_mode mode, curr_mode;
    struct bladerf_rational_rate rate;
    bool ok;
    int *err = &state->last_lib_error;
    int status;

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
        cli_err_nnl(state, argv[1],
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

int print_trimdac(struct cli_state *state, int argc, char **argv)
{
    uint16_t curr, cal;
    int *err = &state->last_lib_error;
    int status;

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
    unsigned int val;
    int status;
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
        cli_err_nnl(state, "Invalid VCTCXO tamer option", "%s\n", argv[2]);
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

int print_vctcxo_tamer(struct cli_state *state, int argc, char *argv[])
{
    int *err = &state->last_lib_error;
    const char *mode_str;
    int status;

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

int print_xb_spi(struct cli_state *state, int argc, char **argv)
{
    cli_err_nnl(state, "Error", "XB SPI is currently write-only.\n");
    return CLI_RET_OK;
}

int set_xb_spi(struct cli_state *state, int argc, char **argv)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    unsigned int val;

    if (argc != 3) {
        rv = CLI_RET_NARGS;
    } else {
        bool ok;
        val = str2uint(argv[2], 0, -1, &ok);
        if (!ok) {
            cli_err_nnl(state, argv[0], "Invalid XB SPI value (%s)\n", argv[2]);
            rv = CLI_RET_INVPARAM;
        } else {
            int status = bladerf_xb_spi_write(state->dev, val);
            if (status < 0) {
                *err = status;
                rv   = CLI_RET_LIBBLADERF;
            }
        }
    }
    return rv;
}

int print_txvga1(struct cli_state *state, int argc, char **argv)
{
    /* Usage: print txvga1 */
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int gain;
    int status;

    status = bladerf_get_txvga1(state->dev, &gain);
    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
    } else {
        printf("  TXVGA1 Gain: %3d dB\n", gain);
    }

    return rv;
}

int set_txvga1(struct cli_state *state, int argc, char **argv)
{
    /* Usage: set txvga1 <gain> */
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int gain;
    int status;

    if (argc == 3) {
        bool ok;
        gain = str2int(argv[2], BLADERF_TXVGA1_GAIN_MIN,
                       BLADERF_TXVGA1_GAIN_MAX, &ok);

        if (!ok) {
            invalid_gain(state, argv[0], argv[1], argv[2]);
            rv = CLI_RET_INVPARAM;
        } else {
            status = bladerf_set_txvga1(state->dev, gain);
            if (status < 0) {
                *err = status;
                rv   = CLI_RET_LIBBLADERF;
            } else {
                rv = print_txvga1(state, 2, argv);
            }
        }
    } else {
        rv = CLI_RET_NARGS;
    }

    return rv;
}

int print_txvga2(struct cli_state *state, int argc, char **argv)
{
    /* Usage: print txvga2 */
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int gain;
    int status;

    status = bladerf_get_txvga2(state->dev, &gain);
    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
    } else {
        printf("  TXVGA2 Gain: %3d dB\n", gain);
    }
    return rv;
}

int set_txvga2(struct cli_state *state, int argc, char **argv)
{
    /* Usage: set txvga2 <gain> */
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    bool ok;
    int gain;
    int status;

    if (argc != 3) {
        rv = CLI_RET_NARGS;
    } else {
        gain = str2int(argv[2], BLADERF_TXVGA2_GAIN_MIN,
                       BLADERF_TXVGA2_GAIN_MAX, &ok);
        if (!ok) {
            invalid_gain(state, argv[0], argv[1], argv[2]);
            rv = CLI_RET_INVPARAM;
        } else {
            status = bladerf_set_txvga2(state->dev, gain);
            if (status < 0) {
                *err = status;
                rv   = CLI_RET_LIBBLADERF;
            } else {
                rv = print_txvga2(state, 2, argv);
            }
        }
    }
    return rv;
}

struct printset_entry *get_printset_entry(char *name)
{
    struct printset_entry *entry = NULL;
    for (size_t i = 0; entry == NULL && printset_table[i].print != NULL; ++i) {
        if (strcasecmp(name, printset_table[i].name) == 0) {
            entry = &(printset_table[i]);
        }
    }
    return entry;
}

/* Set command */
int cmd_set(struct cli_state *state, int argc, char **argv)
{
    int rv = CLI_RET_OK;
    struct printset_entry *entry;

    if (argc > 1) {
        entry = get_printset_entry(argv[1]);

        if (entry != NULL) {
            printf("\n");

            /* Call the parameter setting function */
            rv = entry->set(state, argc, argv);

            printf("\n");
        } else {
            /* Incorrect parameter to print */
            cli_err(state, argv[0], "Invalid parameter (%s)\n", argv[1]);
            rv = CLI_RET_INVPARAM;
        }
    } else {
        rv = CLI_RET_NARGS;
    }
    return rv;
}

/* Print command */
int cmd_print(struct cli_state *state, int argc, char **argv)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    struct printset_entry *entry;
    char *empty_argv[3];
    int empty_argc;

    printf("\n");

    if (argc > 1) {
        entry = get_printset_entry(argv[1]);

        if (entry != NULL) {
            /* Call the parameter printing function */
            rv = entry->print(state, argc, argv);
        } else {
            /* Incorrect parameter to print */
            cli_err_nnl(state, argv[0], "Invalid parameter (%s)\n", argv[1]);
            rv = CLI_RET_INVPARAM;
        }

    } else {
        /* We fake an argumentless argv with each command's name
           supplied as argv[1]. */
        empty_argv[0] = argv[0];

        for (entry = &printset_table[0]; entry->print != NULL; entry++) {
            if (entry->pa_option == PRINTALL_OPTION_SKIP) {
                continue;
            } else if (!_is_board(state->dev, entry->board)) {
                continue;
            }

            empty_argv[1] = (char *)entry->name;

            if (!strcasecmp(entry->name, "xb_gpio_dir") ||
                !strcasecmp(entry->name, "xb_gpio")) {
                empty_argv[2] = "register";
                empty_argc    = 3;
            } else {
                empty_argv[2] = NULL;
                empty_argc    = 2;
            }

            rv = entry->print(state, empty_argc, (char **)empty_argv);
            if (rv != CLI_RET_OK) {
                if (cli_fatal(rv)) {
                    return rv;
                }

                /* Print error messages here so that we can continue
                 * onto the remaining items */
                cli_err_nnl(state, empty_argv[1], "%s\n",
                            cli_strerror(rv, *err));

                /* We've reported this, don't pass it back up
                 * the call stack */
                rv = 0;
            }

            if (entry->pa_option == PRINTALL_OPTION_APPEND_NEWLINE) {
                printf("\n");
            }
        }
    }

    printf("\n");

    return rv;
}
