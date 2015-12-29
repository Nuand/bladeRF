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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>

#include "cmd.h"
#include "conversions.h"
#include "script.h"
#include "thread.h"

#include "doc/cmd_help.h"

#define DECLARE_CMD(x) int cmd_##x (struct cli_state *, int, char **)
DECLARE_CMD(calibrate);
DECLARE_CMD(clear);
DECLARE_CMD(echo);
DECLARE_CMD(erase);
DECLARE_CMD(flash_backup);
DECLARE_CMD(flash_image);
DECLARE_CMD(flash_init_cal);
DECLARE_CMD(flash_restore);
DECLARE_CMD(fw_log);
DECLARE_CMD(help);
DECLARE_CMD(info);
DECLARE_CMD(jump_to_bootloader);
DECLARE_CMD(load);
DECLARE_CMD(xb);
DECLARE_CMD(mimo);
DECLARE_CMD(open);
DECLARE_CMD(peek);
DECLARE_CMD(poke);
DECLARE_CMD(print);
DECLARE_CMD(probe);
DECLARE_CMD(recover);
DECLARE_CMD(run);
DECLARE_CMD(set);
DECLARE_CMD(rx);
DECLARE_CMD(tx);
DECLARE_CMD(version);

#define MAX_ARGS    10
#define COMMENT_CHAR '#'

struct cmd {
    const char **names;
    int (*exec)(struct cli_state *s, int argc, char **argv);
    const char  *desc;
    const char  *help;
    bool        requires_device;
    bool        requires_fpga;
    bool        allow_while_streaming;
};

static const char *cmd_names_calibrate[] = { "calibrate", "cal", NULL };
static const char *cmd_names_clear[] = { "clear", "cls", NULL };
static const char *cmd_names_echo[] = { "echo", NULL };
static const char *cmd_names_erase[] = { "erase", "e", NULL };
static const char *cmd_names_flash_backup[] = { "flash_backup", "fb",  NULL };
static const char *cmd_names_flash_image[] = { "flash_image", "fi", NULL };
static const char *cmd_names_flash_init_cal[] = { "flash_init_cal", "fic", NULL };
static const char *cmd_names_flash_restore[] = { "flash_restore", "fr", NULL };
static const char *cmd_names_fw_log[] = { "fw_log", NULL };
static const char *cmd_names_help[] = { "help", "h", "?", NULL };
static const char *cmd_names_info[] = { "info", "i", NULL };
static const char *cmd_names_jump[] = { "jump_to_boot", "j", NULL };
static const char *cmd_names_load[] = { "load", "ld", NULL };
static const char *cmd_names_xb[] = { "xb", NULL };
static const char *cmd_names_mimo[] = { "mimo", NULL };
static const char *cmd_names_open[] = { "open", "op", "o", NULL };
static const char *cmd_names_peek[] = { "peek", "pe", NULL };
static const char *cmd_names_poke[] = { "poke", "po", NULL };
static const char *cmd_names_print[] = { "print", "pr", "p", NULL };
static const char *cmd_names_probe[] = { "probe", "pro", NULL };
static const char *cmd_names_quit[] = { "quit", "q", "exit", "x", NULL };
static const char *cmd_names_rec[] = { "recover", "r", NULL };
static const char *cmd_names_run[] = { "run", NULL };
static const char *cmd_names_rx[] = { "rx", "receive", NULL };
static const char *cmd_names_tx[] = { "tx", "transmit", NULL };
static const char *cmd_names_set[] = { "set", "s", NULL };
static const char *cmd_names_ver[] = { "version", "ver", "v", NULL };

static const struct cmd cmd_table[] = {
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
        FIELD_INIT(.desc, "Print a flash image's metadata or create a new flash image"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_flash_image),
        FIELD_INIT(.requires_device, false),
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
        FIELD_INIT(.names, cmd_names_flash_init_cal),
        FIELD_INIT(.exec, cmd_flash_init_cal),
        FIELD_INIT(.desc, "Write new calibration data to a device or to a file"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_flash_init_cal),
        FIELD_INIT(.requires_device, true),
        FIELD_INIT(.requires_fpga, true),
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
        FIELD_INIT(.desc, "Print information about the currently opened device"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_info),
        FIELD_INIT(.requires_device, true),
        FIELD_INIT(.requires_fpga, false),
        FIELD_INIT(.allow_while_streaming, true),
    },
    {
        FIELD_INIT(.names, cmd_names_jump),
        FIELD_INIT(.exec, cmd_jump_to_bootloader),
        FIELD_INIT(.desc, "Clear FW signature in flash and jump to FX3 bootloader"),
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
        FIELD_INIT(.names, cmd_names_xb),
        FIELD_INIT(.exec, cmd_xb),
        FIELD_INIT(.desc, "Enable and configure expansion boards"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_xb),
        FIELD_INIT(.requires_device, true),
        FIELD_INIT(.requires_fpga, true),
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
        FIELD_INIT(.exec, NULL), /* Default action on NULL exec function is to quit */
        FIELD_INIT(.desc, "Exit the CLI"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_quit),
        FIELD_INIT(.requires_device, false),
        FIELD_INIT(.requires_fpga, false),
        FIELD_INIT(.allow_while_streaming, true),
    },
    {
        FIELD_INIT(.names, cmd_names_rec),
        FIELD_INIT(.exec, cmd_recover),
        FIELD_INIT(.desc, "Load firmware when operating in FX3 bootloader mode"),
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
        FIELD_INIT(.names, cmd_names_tx),
        FIELD_INIT(.exec, cmd_tx),
        FIELD_INIT(.desc, "Transmit IQ samples"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_tx),
        FIELD_INIT(.requires_device, true),
        FIELD_INIT(.requires_fpga, true),
        FIELD_INIT(.allow_while_streaming, true),   /* Can tx while rx'ing */
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
        FIELD_INIT(.names, cmd_names_ver),
        FIELD_INIT(.exec, cmd_version),
        FIELD_INIT(.desc, "Host software and device version information"),
        FIELD_INIT(.help, CLI_CMD_HELPTEXT_version),
        FIELD_INIT(.requires_device, false),
        FIELD_INIT(.requires_fpga, false),
        FIELD_INIT(.allow_while_streaming, true),
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

const struct numeric_suffix freq_suffixes[NUM_FREQ_SUFFIXES] = {
    { "G",      1000 * 1000 * 1000 },
    { "GHz",    1000 * 1000 * 1000 },
    { "M",      1000 * 1000 },
    { "MHz",    1000 * 1000 },
    { "k",      1000 } ,
    { "kHz",    1000 }
};

const struct cmd *get_cmd( char *name )
{
    const struct cmd *rv = NULL;
    int i = 0, j = 0 ;
    for( i = 0; cmd_table[i].names != NULL && rv == 0; i++ ) {
        for( j = 0; cmd_table[i].names[j] != NULL && rv == 0; j++ ) {
            if( strcasecmp( name, cmd_table[i].names[j] ) == 0 ) {
                rv = &(cmd_table[i]);
            }
        }
    }

    return rv;
}

int cmd_help(struct cli_state *s, int argc, char **argv)
{
    int i = 0;
    int ret = CLI_RET_OK;
    const struct cmd *cmd;

    /* Asking for general help */
    if( argc == 1 ) {
        printf( "\n" );
        for( i = 0; cmd_table[i].names != NULL; i++ ) {
            /* Hidden functions for fun and profit */
            if( cmd_table[i].names[0][0] == '_' ) continue;
            printf( "  %-20s%s\n", cmd_table[i].names[0], cmd_table[i].desc );
        }
        printf( "\n" );

    /* Asking for command specific help */
    } else if( argc == 2 ) {

        /* Iterate through the commands */
        cmd = get_cmd(argv[1]);

        /* If we found it, print the help */
        if( cmd ) {
            printf( "\n%s\n", cmd->help );
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

    if (status == 0) {
        return CLI_RET_RUN_SCRIPT;
    } else if (status == 1) {
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

    for (i = 1; i < argc; i++) {
        printf("%s\n", argv[i]);
    }

    return 0;
}

int cmd_handle(struct cli_state *s, const char *line)
{
    const struct cmd *cmd;
    int argc, ret;
    char **argv = NULL;
    int fpga_status = -1;

    ret = 0;
    argc = str2args(line, COMMENT_CHAR, &argv);
    if (argc == -2) {
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
                if (ret == 0 && cmd->requires_fpga) {
                    fpga_status = bladerf_is_fpga_configured(s->dev);
                    if (fpga_status < 0) {
                        cli_err(s, argv[0],
                                "Failed to determine if the FPGA is configured:"
                                " %s\n", bladerf_strerror(fpga_status));

                        s->last_lib_error = fpga_status;
                        ret = CLI_RET_LIBBLADERF;
                    } else if (fpga_status != 1) {
                        ret = CLI_RET_NOFPGA;
                    }
                }

                /* Test if this command may be run while the device is
                 * actively running an RX/TX stream */
                if (ret == 0 &&
                    cli_device_is_streaming(s) && !cmd->allow_while_streaming) {
                    ret = CLI_RET_BUSY;
                }

                if (ret == 0) {
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
    } else if (argc == 0) {
        free_args(argc, argv);
    } else {
        ret = CLI_RET_UNKNOWN;
    }

    return ret;
}

void cmd_show_help_all() {
    int i = 0;
    for( i = 0; cmd_table[i].names != NULL; i++ )
        printf("COMMAND: %s\n", cmd_table[i].help);
}

