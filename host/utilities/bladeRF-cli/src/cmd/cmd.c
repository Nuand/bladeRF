/* * This file is part of the bladeRF project
 *
 * Copyright (C) 2013-2014 Nuand LLC
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
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmd.h"
#include "conversions.h"
#include "parse.h"
#include "script.h"
#include "thread.h"

#include "doc/cmd_help.h"

#define DECLARE_CMD(x, ...)                        \
    int cmd_##x(struct cli_state *, int, char **); \
    static char const *cmd_names_##x[] = { __VA_ARGS__, NULL };

DECLARE_CMD(calibrate, "calibrate", "cal");
DECLARE_CMD(clear, "clear", "cls");
DECLARE_CMD(echo, "echo");
DECLARE_CMD(erase, "erase", "e");
DECLARE_CMD(flash_backup, "flash_backup", "fb");
DECLARE_CMD(flash_image, "flash_image", "fi");
DECLARE_CMD(flash_init_cal, "flash_init_cal", "fic");
DECLARE_CMD(flash_restore, "flash_restore", "fr");
DECLARE_CMD(fw_log, "fw_log");
DECLARE_CMD(help, "help", "h", "?");
DECLARE_CMD(info, "info", "i");
DECLARE_CMD(jump_to_bootloader, "jump_to_boot", "j");
DECLARE_CMD(load, "load", "ld");
DECLARE_CMD(mimo, "mimo");
DECLARE_CMD(open, "open", "op", "o");
DECLARE_CMD(peek, "peek", "pe");
DECLARE_CMD(poke, "poke", "po");
DECLARE_CMD(print, "print", "pr", "p");
DECLARE_CMD(probe, "probe", "pro");
DECLARE_CMD(quit, "quit", "q", "exit", "x");
DECLARE_CMD(recover, "recover", "r");
DECLARE_CMD(run, "run");
DECLARE_CMD(rx, "rx", "receive");
DECLARE_CMD(set, "set", "s");
DECLARE_CMD(trigger, "trigger", "tr");
DECLARE_CMD(tx, "tx", "transmit");
DECLARE_CMD(version, "version", "ver", "v");
DECLARE_CMD(xb, "xb");

#define MAX_ARGS 10
#define COMMENT_CHAR '#'

struct cmd {
    char const **names;
    int (*exec)(struct cli_state *s, int argc, char **argv);
    char const *desc;
    char const *help;
    bool requires_device;
    bool requires_fpga;
    bool allow_while_streaming;
};

// clang-format off
static struct cmd const cmd_table[] = {
    {
        FIELD_INIT(.names, cmd_names_calibrate),
        FIELD_INIT(.exec, cmd_calibrate),
        FIELD_INIT(.desc, "Perform transceiver and device calibrations"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_calibrate),
        FIELD_INIT(.requires_device, true),
        FIELD_INIT(.requires_fpga, true),
        FIELD_INIT(.allow_while_streaming, false),
    },
    {
        FIELD_INIT(.names, cmd_names_clear),
        FIELD_INIT(.exec, cmd_clear),
        FIELD_INIT(.desc, "Clear the screen"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_clear),
        FIELD_INIT(.requires_device, false),
        FIELD_INIT(.requires_fpga, false),
        FIELD_INIT(.allow_while_streaming, true),
    },
    {
        FIELD_INIT(.names, cmd_names_echo),
        FIELD_INIT(.exec, cmd_echo),
        FIELD_INIT(.desc, "Echo each argument on a new line"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_echo),
        FIELD_INIT(.requires_device, false),
        FIELD_INIT(.requires_fpga, false),
        FIELD_INIT(.allow_while_streaming, true),
    },
    {
        FIELD_INIT(.names, cmd_names_erase),
        FIELD_INIT(.exec, cmd_erase),
        FIELD_INIT(.desc, "Erase specified erase blocks of SPI flash"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_erase),
        FIELD_INIT(.requires_device, true),
        FIELD_INIT(.requires_fpga, false),
        FIELD_INIT(.allow_while_streaming, false),
    },
    {
        FIELD_INIT(.names, cmd_names_flash_backup),
        FIELD_INIT(.exec, cmd_flash_backup),
        FIELD_INIT(.desc, "Back up flash data to a file with metadata."),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_flash_backup),
        FIELD_INIT(.requires_device, true),
        FIELD_INIT(.requires_fpga, false),
        FIELD_INIT(.allow_while_streaming, false),
    },
    {
        FIELD_INIT(.names, cmd_names_flash_image),
        FIELD_INIT(.exec, cmd_flash_image),
        FIELD_INIT(.desc, "Print a flash image's metadata or create a new "
                          "flash image"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_flash_image),
        FIELD_INIT(.requires_device, false),
        FIELD_INIT(.requires_fpga, false),
        FIELD_INIT(.allow_while_streaming, false),
    },
    {
        FIELD_INIT(.names, cmd_names_flash_init_cal),
        FIELD_INIT(.exec, cmd_flash_init_cal),
        FIELD_INIT(.desc, "Write new calibration data to a device or to a "
                          "file"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_flash_init_cal),
        FIELD_INIT(.requires_device, true),
        FIELD_INIT(.requires_fpga, false),
        FIELD_INIT(.allow_while_streaming, false),
    },
    {
        FIELD_INIT(.names, cmd_names_flash_restore),
        FIELD_INIT(.exec, cmd_flash_restore),
        FIELD_INIT(.desc, "Restore flash data from a file"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_flash_restore),
        FIELD_INIT(.requires_device, true),
        FIELD_INIT(.requires_fpga, false),
        FIELD_INIT(.allow_while_streaming, false),
    },
    {
        FIELD_INIT(.names, cmd_names_fw_log),
        FIELD_INIT(.exec, cmd_fw_log),
        FIELD_INIT(.desc, "Read firmware log contents"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_fw_log),
        FIELD_INIT(.requires_device, true),
        FIELD_INIT(.requires_fpga, false),
        FIELD_INIT(.allow_while_streaming, true),
    },
    {
        FIELD_INIT(.names, cmd_names_help),
        FIELD_INIT(.exec, cmd_help),
        FIELD_INIT(.desc, "Provide information about specified command"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_help),
        FIELD_INIT(.requires_device, false),
        FIELD_INIT(.requires_fpga, false),
        FIELD_INIT(.allow_while_streaming, true),
    },
    {
        FIELD_INIT(.names, cmd_names_info),
        FIELD_INIT(.exec, cmd_info),
        FIELD_INIT(.desc, "Print information about the currently opened "
                          "device"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_info),
        FIELD_INIT(.requires_device, true),
        FIELD_INIT(.requires_fpga, false),
        FIELD_INIT(.allow_while_streaming, true),
    },
    {
        FIELD_INIT(.names, cmd_names_jump_to_bootloader),
        FIELD_INIT(.exec, cmd_jump_to_bootloader),
        FIELD_INIT(.desc, "Clear FW signature in flash and jump to FX3 "
                          "bootloader"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_jump_to_boot),
        FIELD_INIT(.requires_device, true),
        FIELD_INIT(.requires_fpga, false),
        FIELD_INIT(.allow_while_streaming, false),
    },
    {
        FIELD_INIT(.names, cmd_names_load),
        FIELD_INIT(.exec, cmd_load),
        FIELD_INIT(.desc, "Load FPGA or FX3"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_load),
        FIELD_INIT(.requires_device, true),
        FIELD_INIT(.requires_fpga, false),
        FIELD_INIT(.allow_while_streaming, false),
    },
    {
        FIELD_INIT(.names, cmd_names_mimo),
        FIELD_INIT(.exec, cmd_mimo),
        FIELD_INIT(.desc, "Modify device MIMO operation"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_mimo),
        FIELD_INIT(.requires_device, true),
        FIELD_INIT(.requires_fpga, true),
        FIELD_INIT(.allow_while_streaming, false),
    },
    {
        FIELD_INIT(.names, cmd_names_open),
        FIELD_INIT(.exec, cmd_open),
        FIELD_INIT(.desc, "Open a bladeRF device"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_open),
        FIELD_INIT(.requires_device, false),
        FIELD_INIT(.requires_fpga, false),
        FIELD_INIT(.allow_while_streaming, false),
    },
    {
        FIELD_INIT(.names, cmd_names_peek),
        FIELD_INIT(.exec, cmd_peek),
        FIELD_INIT(.desc, "Peek a memory location"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_peek),
        FIELD_INIT(.requires_device, true),
        FIELD_INIT(.requires_fpga, true),
        FIELD_INIT(.allow_while_streaming, true),
    },
    {
        FIELD_INIT(.names, cmd_names_poke),
        FIELD_INIT(.exec, cmd_poke),
        FIELD_INIT(.desc, "Poke a memory location"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_poke),
        FIELD_INIT(.requires_device, true),
        FIELD_INIT(.requires_fpga, true),
        FIELD_INIT(.allow_while_streaming, true),
    },
    {
        FIELD_INIT(.names, cmd_names_print),
        FIELD_INIT(.exec, cmd_print),
        FIELD_INIT(.desc, "Print information about the device"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_print),
        FIELD_INIT(.requires_device, true),
        FIELD_INIT(.requires_fpga, true),
        FIELD_INIT(.allow_while_streaming, true),
    },
    {
        FIELD_INIT(.names, cmd_names_probe),
        FIELD_INIT(.exec, cmd_probe),
        FIELD_INIT(.desc, "List attached bladeRF devices"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_probe),
        FIELD_INIT(.requires_device, false),
        FIELD_INIT(.requires_fpga, false),
        FIELD_INIT(.allow_while_streaming, true),
    },
    {
        FIELD_INIT(.names, cmd_names_quit),
        FIELD_INIT(.exec, NULL), /* Default action on NULL exec fn is to quit */
        FIELD_INIT(.desc, "Exit the CLI"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_quit),
        FIELD_INIT(.requires_device, false),
        FIELD_INIT(.requires_fpga, false),
        FIELD_INIT(.allow_while_streaming, true),
    },
    {
        FIELD_INIT(.names, cmd_names_recover),
        FIELD_INIT(.exec, cmd_recover),
        FIELD_INIT(.desc, "Load firmware when operating in FX3 bootloader "
                          "mode"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_recover),
        FIELD_INIT(.requires_device, false),
        FIELD_INIT(.requires_fpga, false),
        FIELD_INIT(.allow_while_streaming, false),
    },
    {
        FIELD_INIT(.names, cmd_names_run),
        FIELD_INIT(.exec, cmd_run),
        FIELD_INIT(.desc, "Run a script"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_run),
        FIELD_INIT(.requires_device, false),
        FIELD_INIT(.requires_fpga, false),
        FIELD_INIT(.allow_while_streaming, true),
    },
    {
        FIELD_INIT(.names, cmd_names_rx),
        FIELD_INIT(.exec, cmd_rx),
        FIELD_INIT(.desc, "Receive IQ samples"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_rx),
        FIELD_INIT(.requires_device, true),
        FIELD_INIT(.requires_fpga, true),
        FIELD_INIT(.allow_while_streaming, true),   /* Can rx while tx'ing */
    },
    {
        FIELD_INIT(.names, cmd_names_set),
        FIELD_INIT(.exec, cmd_set),
        FIELD_INIT(.desc, "Set device settings"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_set),
        FIELD_INIT(.requires_device, true),
        FIELD_INIT(.requires_fpga, true),
        FIELD_INIT(.allow_while_streaming, true),
    },
    {
        FIELD_INIT(.names, cmd_names_trigger),
        FIELD_INIT(.exec, cmd_trigger),
        FIELD_INIT(.desc, "Control triggering"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_trigger),
        FIELD_INIT(.requires_device, true),
        FIELD_INIT(.requires_fpga, true),
        FIELD_INIT(.allow_while_streaming, true),   /* Can control trigger 
                                                     * while running */
    },
    {
        FIELD_INIT(.names, cmd_names_tx),
        FIELD_INIT(.exec, cmd_tx),
        FIELD_INIT(.desc, "Transmit IQ samples"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_tx),
        FIELD_INIT(.requires_device, true),
        FIELD_INIT(.requires_fpga, true),
        FIELD_INIT(.allow_while_streaming, true),   /* Can tx while rx'ing */
    },
    {
        FIELD_INIT(.names, cmd_names_version),
        FIELD_INIT(.exec, cmd_version),
        FIELD_INIT(.desc, "Host software and device version information"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_version),
        FIELD_INIT(.requires_device, false),
        FIELD_INIT(.requires_fpga, false),
        FIELD_INIT(.allow_while_streaming, true),
    },
    {
        FIELD_INIT(.names, cmd_names_xb),
        FIELD_INIT(.exec, cmd_xb),
        FIELD_INIT(.desc, "Enable and configure expansion boards"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_xb),
        FIELD_INIT(.requires_device, true),
        FIELD_INIT(.requires_fpga, true),
        FIELD_INIT(.allow_while_streaming, false),
    },
    /* Always terminate the command entry with a completely NULL entry */
    {
        FIELD_INIT(.names, NULL),
        FIELD_INIT(.exec, NULL),
        FIELD_INIT(.desc, NULL),
        FIELD_INIT(.help, NULL),
        FIELD_INIT(.requires_device, false),
        FIELD_INIT(.requires_fpga, false),
        FIELD_INIT(.allow_while_streaming, true),
    }
};
// clang-format on

// clang-format off
struct numeric_suffix const freq_suffixes[NUM_FREQ_SUFFIXES] = {
    { "G",      1000 * 1000 * 1000 },
    { "GHz",    1000 * 1000 * 1000 },
    { "M",      1000 * 1000 },
    { "MHz",    1000 * 1000 },
    { "k",      1000 },
    { "kHz",    1000 }
};
// clang-format on

struct cmd const *get_cmd(char *name)
{
    struct cmd const *rv = NULL;
    int i, j;

    for (i = 0; cmd_table[i].names != NULL && rv == 0; ++i) {
        for (j = 0; cmd_table[i].names[j] != NULL && rv == 0; ++j) {
            if (strcasecmp(name, cmd_table[i].names[j]) == 0) {
                rv = &(cmd_table[i]);
            }
        }
    }

    return rv;
}

int cmd_help(struct cli_state *s, int argc, char **argv)
{
    struct cmd const *cmd = NULL;
    int i;
    int ret = CLI_RET_OK;

    /* Asking for general help */
    if (1 == argc) {
        printf("\n");
        for (i = 0; cmd_table[i].names != NULL; ++i) {
            /* Hidden functions for fun and profit */
            if (cmd_table[i].names[0][0] == '_') {
                continue;
            }
            printf("  %-20s%s\n", cmd_table[i].names[0], cmd_table[i].desc);
        }
        printf("\n");

        /* Asking for command specific help */
    } else if (2 == argc) {
        /* Iterate through the commands */
        cmd = get_cmd(argv[1]);

        /* If we found it, print the help */
        if (NULL != cmd) {
            printf("\n%s\n", cmd->help);
        } else {
            /* Otherwise, print that we couldn't find it :( */
            cli_err(s, argv[0], "No help info available for \"%s\"\n", argv[1]);
            ret = CLI_RET_INVPARAM;
        }
    } else {
        ret = CLI_RET_NARGS;
    }

    return ret;
}

int cmd_clear(struct cli_state *s, int argc, char **argv)
{
    return CLI_RET_CLEAR_TERM;
}

int cmd_run(struct cli_state *state, int argc, char **argv)
{
    int status;

    if (argc != 2) {
        return CLI_RET_NARGS;
    }

    status = cli_open_script(&state->scripts, argv[1]);

    if (0 == status) {
        return CLI_RET_RUN_SCRIPT;
    } else if (1 == status) {
        cli_err(state, "run", "Recursive loop detected in script.\n");
        return CLI_RET_INVPARAM;
    } else if (status < 0) {
        if (-status == ENOENT) {
            return CLI_RET_NOFILE;
        } else {
            return CLI_RET_FILEOP;
        }
    } else {
        /* Shouldn't happen */
        return CLI_RET_UNKNOWN;
    }
}

int cmd_echo(struct cli_state *state, int argc, char **argv)
{
    int i;

    for (i = 1; i < argc; ++i) {
        printf("%s\n", argv[i]);
    }

    return 0;
}

int cmd_handle(struct cli_state *s, char const *line)
{
    struct cmd const *cmd;
    int argc;
    int ret         = 0;
    char **argv     = NULL;
    int fpga_status = -1;

    argc = str2args(line, COMMENT_CHAR, &argv);
    if (-2 == argc) {
        cli_err(s, "Error", "Input contains unterminated quote.\n");
        return CLI_RET_INVPARAM;
    }

    if (argc > 0) {
        cmd = get_cmd(argv[0]);

        if (cmd) {
            if (cmd->exec) {
                /* Check if the command requires an opened device */
                if ((cmd->requires_device) && !cli_device_is_opened(s)) {
                    ret = CLI_RET_NODEV;
                }

                /* Check if the command requires a loaded FPGA */
                if (0 == ret && cmd->requires_fpga) {
                    fpga_status = bladerf_is_fpga_configured(s->dev);
                    if (fpga_status < 0) {
                        cli_err(s, argv[0],
                                "Failed to determine if the FPGA is configured:"
                                " %s\n",
                                bladerf_strerror(fpga_status));

                        s->last_lib_error = fpga_status;
                        ret               = CLI_RET_LIBBLADERF;
                    } else if (fpga_status != 1) {
                        ret = CLI_RET_NOFPGA;
                    }
                }

                /* Test if this command may be run while the device is
                 * actively running an RX/TX stream */
                if (0 == ret && cli_device_is_streaming(s) &&
                    !cmd->allow_while_streaming) {
                    ret = CLI_RET_BUSY;
                }

                if (0 == ret) {
                    /* Commands own the device handle while they're executing.
                     * This is needed to prevent races on the device handle
                     * while the RX/TX make any necessary control calls while
                     * starting up or finishing up a stream() call*/
                    MUTEX_LOCK(&s->dev_lock);
                    ret = cmd->exec(s, argc, argv);
                    MUTEX_UNLOCK(&s->dev_lock);
                }
            } else {
                ret = CLI_RET_QUIT;
            }
        } else {
            cli_err(s, "Unrecognized command", "%s\n", argv[0]);
            ret = CLI_RET_NOCMD;
        }

        free_args(argc, argv);
    } else if (0 == argc) {
        free_args(argc, argv);
    } else {
        ret = CLI_RET_UNKNOWN;
    }

    return ret;
}

void cmd_show_help_all()
{
    int i = 0;

    for (i = 0; cmd_table[i].names != NULL; ++i)
        printf("COMMAND: %s\n", cmd_table[i].help);
}
