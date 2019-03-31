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
#include "printset.h"
#include "cmd.h"

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
#define READONLY_ENTRY(x, printall_option, _board) \
    {                                              \
        FIELD_INIT(.print, print_##x),             \
        FIELD_INIT(.set, NULL),                    \
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
    enum ps_board_option const board;
};

/* Declarations */
PRINTSET_DECL(agc);
PRINTSET_DECL(bandwidth);
PRINTSET_DECL(biastee);
PRINTSET_DECL(clock_out);
PRINTSET_DECL(clock_ref);
PRINTSET_DECL(clock_sel);
PRINTSET_DECL(filter);
PRINTSET_DECL(frequency);
PRINTSET_DECL(gain);
PRINTSET_DECL(gpio);
PRINTSET_DECL(hardware);
PRINTSET_DECL(lnagain);
PRINTSET_DECL(loopback);
PRINTSET_DECL(refin_freq);
PRINTSET_DECL(rssi);
PRINTSET_DECL(rx_mux);
PRINTSET_DECL(tuning_mode);
PRINTSET_DECL(rxvga1);
PRINTSET_DECL(rxvga2);
PRINTSET_DECL(samplerate);
PRINTSET_DECL(sampling);
PRINTSET_DECL(smb_mode);
PRINTSET_DECL(trimdac);
PRINTSET_DECL(txvga1);
PRINTSET_DECL(txvga2);
PRINTSET_DECL(vctcxo_tamer);
PRINTSET_DECL(xb_gpio);
PRINTSET_DECL(xb_gpio_dir);
PRINTSET_DECL(xb_spi);

/* print/set parameter table */
// clang-format off
struct printset_entry printset_table[] = {
    PRINTSET_ENTRY(bandwidth,    PRINTALL_OPTION_APPEND_NEWLINE, BOARD_ANY),
    PRINTSET_ENTRY(frequency,    PRINTALL_OPTION_APPEND_NEWLINE, BOARD_ANY),
    PRINTSET_ENTRY(tuning_mode,  PRINTALL_OPTION_APPEND_NEWLINE, BOARD_ANY),
    PRINTSET_ENTRY(agc,          PRINTALL_OPTION_APPEND_NEWLINE, BOARD_ANY),
    PRINTSET_ENTRY(clock_ref,    PRINTALL_OPTION_NONE,           BOARD_BLADERF2),
    PRINTSET_ENTRY(refin_freq,   PRINTALL_OPTION_NONE,           BOARD_BLADERF2),
    PRINTSET_ENTRY(clock_sel,    PRINTALL_OPTION_NONE,           BOARD_BLADERF2),
    PRINTSET_ENTRY(clock_out,    PRINTALL_OPTION_APPEND_NEWLINE, BOARD_BLADERF2),
    PRINTSET_ENTRY(gpio,         PRINTALL_OPTION_APPEND_NEWLINE, BOARD_BLADERF1),
    READONLY_ENTRY(rssi,         PRINTALL_OPTION_APPEND_NEWLINE, BOARD_BLADERF2),
    PRINTSET_ENTRY(loopback,     PRINTALL_OPTION_APPEND_NEWLINE, BOARD_ANY),
    PRINTSET_ENTRY(rx_mux,       PRINTALL_OPTION_APPEND_NEWLINE, BOARD_ANY),
    PRINTSET_ENTRY(filter,       PRINTALL_OPTION_APPEND_NEWLINE, BOARD_BLADERF2),
    PRINTSET_ENTRY(gain,         PRINTALL_OPTION_APPEND_NEWLINE, BOARD_ANY),
    PRINTSET_ENTRY(lnagain,      PRINTALL_OPTION_SKIP,           BOARD_BLADERF1),
    PRINTSET_ENTRY(rxvga1,       PRINTALL_OPTION_SKIP,           BOARD_BLADERF1),
    PRINTSET_ENTRY(rxvga2,       PRINTALL_OPTION_SKIP,           BOARD_BLADERF1),
    PRINTSET_ENTRY(txvga1,       PRINTALL_OPTION_SKIP,           BOARD_BLADERF1),
    PRINTSET_ENTRY(txvga2,       PRINTALL_OPTION_SKIP,           BOARD_BLADERF1),
    PRINTSET_ENTRY(sampling,     PRINTALL_OPTION_APPEND_NEWLINE, BOARD_BLADERF1),
    PRINTSET_ENTRY(samplerate,   PRINTALL_OPTION_APPEND_NEWLINE, BOARD_ANY),
    PRINTSET_ENTRY(smb_mode,     PRINTALL_OPTION_APPEND_NEWLINE, BOARD_BLADERF1),
    PRINTSET_ENTRY(biastee,      PRINTALL_OPTION_APPEND_NEWLINE, BOARD_BLADERF2),
    PRINTSET_ENTRY(trimdac,      PRINTALL_OPTION_APPEND_NEWLINE, BOARD_ANY),
    PRINTSET_ENTRY(vctcxo_tamer, PRINTALL_OPTION_APPEND_NEWLINE, BOARD_BLADERF1),
    PRINTSET_ENTRY(xb_spi,       PRINTALL_OPTION_SKIP,           BOARD_BLADERF1),
    PRINTSET_ENTRY(xb_gpio,      PRINTALL_OPTION_NONE,           BOARD_BLADERF1),
    PRINTSET_ENTRY(xb_gpio_dir,  PRINTALL_OPTION_APPEND_NEWLINE, BOARD_BLADERF1),
    READONLY_ENTRY(hardware,     PRINTALL_OPTION_NONE,           BOARD_ANY),

    /* End of table marked by entry with NULL/empty fields */
    { FIELD_INIT(.print, NULL), FIELD_INIT(.set, NULL), FIELD_INIT(.name, ""),
      FIELD_INIT(.pa_option, PRINTALL_OPTION_NONE),
      FIELD_INIT(.board, BOARD_ANY) }
};
// clang-format on

/* Utility functions */

int ps_foreach_chan(struct cli_state *state,
                    bool const rx,
                    bool const tx,
                    bladerf_channel const only_chan,
                    int (*func)(struct cli_state *, bladerf_channel, void *),
                    void *const arg)
{
    int rv = CLI_RET_OK;
    size_t chi;

    if (NULL == state || NULL == func) {
        rv = CLI_RET_INVPARAM;
    }

    // RX channels
    if (rx) {
        for (chi = 0; chi < bladerf_get_channel_count(state->dev, BLADERF_RX);
             ++chi) {
            if (CLI_RET_OK == rv) {
                bladerf_channel const ch = BLADERF_CHANNEL_RX(chi);

                if (BLADERF_CHANNEL_INVALID == only_chan || ch == only_chan) {
                    rv = func(state, ch, arg);
                }
            }
        }
    }

    // TX channels
    if (tx) {
        for (chi = 0; chi < bladerf_get_channel_count(state->dev, BLADERF_TX);
             ++chi) {
            if (CLI_RET_OK == rv) {
                bladerf_channel const ch = BLADERF_CHANNEL_TX(chi);

                if (BLADERF_CHANNEL_INVALID == only_chan || ch == only_chan) {
                    rv = func(state, ch, arg);
                }
            }
        }
    }

    return rv;
}

bool ps_is_board(struct bladerf *dev, enum ps_board_option board)
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

struct printset_entry *get_printset_entry(char *name)
{
    struct printset_entry *entry = NULL;
    size_t i;

    for (i = 0; entry == NULL && printset_table[i].print != NULL; ++i) {
        if (strcasecmp(name, printset_table[i].name) == 0) {
            entry = &(printset_table[i]);
        }
    }
    return entry;
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

        if (entry != NULL && entry->print != NULL) {
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
            } else if (!ps_is_board(state->dev, entry->board)) {
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

/* Set command */
int cmd_set(struct cli_state *state, int argc, char **argv)
{
    int rv = CLI_RET_OK;
    struct printset_entry *entry;

    if (argc > 1) {
        entry = get_printset_entry(argv[1]);

        if (entry != NULL && entry->set != NULL) {
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
