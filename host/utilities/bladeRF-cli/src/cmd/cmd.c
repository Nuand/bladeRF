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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cmd.h"

#define DECLARE_CMD(x) int cmd_##x (struct cli_state *, int, char **)
DECLARE_CMD(calibrate);
DECLARE_CMD(clear);
DECLARE_CMD(help);
DECLARE_CMD(info);
DECLARE_CMD(load);
DECLARE_CMD(open);
DECLARE_CMD(peek);
DECLARE_CMD(poke);
DECLARE_CMD(print);
DECLARE_CMD(probe);
DECLARE_CMD(erase);
DECLARE_CMD(set);
DECLARE_CMD(rx);
DECLARE_CMD(tx);
DECLARE_CMD(version);
DECLARE_CMD(recover);
DECLARE_CMD(jump_to_bootloader);
DECLARE_CMD(mimo);

#define MAX_ARGS    10

struct cmd {
    const char **names;
    int (*exec)(struct cli_state *s, int argc, char **argv);
    const char  *desc;
    const char  *help;
};

static const char *cmd_names_calibrate[] = { "calibrate", "cal", NULL };
static const char *cmd_names_clear[] = { "clear", "cls", NULL };
static const char *cmd_names_help[] = { "help", "h", "?", NULL };
static const char *cmd_names_info[] = { "info", "i", NULL };
static const char *cmd_names_load[] = { "load", "ld", NULL };
static const char *cmd_names_open[] = { "open", "op", "o", NULL };
static const char *cmd_names_peek[] = { "peek", "pe", NULL };
static const char *cmd_names_poke[] = { "poke", "po", NULL };
static const char *cmd_names_print[] = { "print", "pr", "p", NULL };
static const char *cmd_names_probe[] = { "probe", "pro", NULL };
static const char *cmd_names_erase[] = { "erase", "e", NULL };
static const char *cmd_names_quit[] = { "quit", "q", "exit", "x", NULL };
static const char *cmd_names_rx[] = { "rx", "receive", NULL };
static const char *cmd_names_tx[] = { "tx", "transmit", NULL };
static const char *cmd_names_set[] = { "set", "s", NULL };
static const char *cmd_names_ver[] = { "version", "ver", "v", NULL };
static const char *cmd_names_rec[] = { "recover", "r", NULL };
static const char *cmd_names_jump[] = { "jump_to_boot", "j", NULL };
static const char *cmd_names_mimo[] = { "mimo", NULL };

static const struct cmd cmd_table[] = {
    {
        FIELD_INIT(.names, cmd_names_calibrate),
        FIELD_INIT(.exec, cmd_calibrate),
        FIELD_INIT(.desc, "Calibrate transceiver"),
        FIELD_INIT(.help,
            "Calibrate the DC offset for both RX and TX\n"
        )
    },
    {
        FIELD_INIT(.names, cmd_names_rx),
        FIELD_INIT(.exec, cmd_rx),
        FIELD_INIT(.desc, "Receive IQ samples"),
        FIELD_INIT(.help,
" rx <start | stop | config [param=val [param=val [...]]>\n"
"\n"
"Receive IQ samples and write them to the specified file.\n"
"Reception is controlled and configured by one of the following:\n"
"\n"
"    start         Start receiving samples\n"
"    stop          Stop Receiving samples\n"
"    config        Configure sample reception. If no parameters\n"
"                  are provided, the current parameters are printed.\n"
"\n"
"Running 'rx' without any additional commands is valid shorthand "
"for 'rx config'.\n"
"\n"
"Configuration parameters take the form 'param=value', and may be specified\n"
"in a single or multiple 'rx config' invocations. Below is a list of\n"
"available parameters.\n"
"\n"
"    n             Number of samples to receive. 0 = inf.\n"
"\n"
"    file          Filename to write received samples to\n"
"\n"
"    format        Output file format. One of the following:\n"
"                      csv          CSV of SC16 Q12 samples\n"
"                      bin          Raw SC16 Q12 DAC samples\n"
"\n"
"    samples       Number of samples per buffer to use in the asynchronous\n"
"                  stream. Must be divisible by 1024 and >= 1024.\n"
"\n"
"    buffers       Number of sample buffers to use in the asynchronous\n"
"                  stream. The min value is 4.\n"
"\n"
"    xfers         Number of simultaneous transfers to allow the asynchronous\n"
"                  stream to use. This should be < the 'buffers' parameter.\n"
"\n"
"    The n, samples, buffers, and xfers parameters support the suffixes\n"
"    'K', 'M', and 'G', which are multiples of 1024.\n"
"\n"
"Example:\n"
"  rx config file=/tmp/some_file format=bin n=10K\n"
"\n"
"Notes:\n"
"\n"
"  An 'rx stop' followed by an 'rx start' will result in the samples file\n"
"  being truncated. If this is not desired, be sure to run 'rx config' to set\n"
"  another file before restarting the rx stream.\n"
        )
    },
    {
        FIELD_INIT(.names, cmd_names_tx),
        FIELD_INIT(.exec, cmd_tx),
        FIELD_INIT(.desc, "Transmit IQ samples"),
        FIELD_INIT(.help,
" tx <start | stop | config [parameters]>\n"
"\n"
"Read IQ samples from the specified file and transmit them.\n"
"Transmission is controlled and configured by one of the following:\n"
"\n"
"    start         Start transmitting samples\n"
"    stop          Stop transmitting samples\n"
"    config        Configure sample transmission . If no parameters\n"
"                  are provided, the current parameters are printed.\n"
"\n"
"Running 'tx' without any additional commands is valid shorthand for\n"
"'tx config'.\n"
"\n"
"Configuration parameters take the form 'param=value', and may be specified\n"
"in a single or multiple 'tx config' invocations. Below is a list of\n"
"available parameters.\n"
"\n"
"\n"
"    file          Filename to read samples from\n"
"\n"
"    format        Output file format. One of the following:\n"
"                      csv          CSV of SC16 Q12 samples\n"
"                      bin          Raw SC16 Q12 DAC samples\n"
"\n"
"    repeat        The number of times the file contents should be \n"
"                  transmitted. 0 implies repeat until stopped.\n"
"\n"
"    delay         The number of microseconds to delay between retransmitting\n"
"                  file contents. 0 implies no delay.\n"
"\n"
"    samples       Number of samples per buffer to use in the asynchronous\n"
"                  stream. Must be divisible by 1024 and >= 1024.\n"
"\n"
"    buffers       Number of sample buffers to use in the asynchronous\n"
"                  stream. The min value is 4.\n"
"\n"
"    xfers         Number of simultaneous transfers to allow the asynchronous\n"
"                  stream to use. This should be < the 'buffers' parameter.\n"
"\n"
"\n"
"    The n, samples, buffers, and xfers parameters support the suffixes\n"
"    'K', 'M', and 'G', which are multiples of 1024.\n"
"\n"
"Example:\n"
"  tx config file=data.bin format=bin repeat=2 delay=250000\n"
"\n"
        )
    },
    {
        FIELD_INIT(.names, cmd_names_set),
        FIELD_INIT(.exec, cmd_set),
        FIELD_INIT(.desc, "Set device settings"),
        FIELD_INIT(.help,
            "set <param> <arguments>\n"
            "\n"
            "The set command takes a parameter and an arbitrary number of\n"
            "arguments for that particular command.  The parameter is one\n"
            "of:\n"
            "\n"
            "   bandwidth       Bandwidth settings\n"
            "   config          Overview of everything\n"
            "   frequency       Frequency settings\n"
            "   lmsregs         LMS6002D register dump\n"
            "   loopback        Loopback settings\n"
            "   mimo            MIMO settings\n"
            "   pa              PA settings\n"
            "   pps             PPS settings\n"
            "   refclk          Reference clock settings\n"
            "   rxvga1          Gain setting of RXVGA1 in dB (range: )\n"
            "   rxvga2          Gain setting of RXVGA2 in dB (range: )\n"
            "   samplerate      Samplerate settings\n"
            "   trimdac         VCTCXO Trim DAC settings\n"
            "   txvga1          Gain setting of TXVGA1 in dB (range: )\n"
            "   txvga2          Gain setting of TXVGA2 in dB (range: )\n"
        )
    },
    {
        FIELD_INIT(.names, cmd_names_poke),
        FIELD_INIT(.exec, cmd_poke),
        FIELD_INIT(.desc, "Poke a memory location"),
        FIELD_INIT(.help,
            "poke <dac|lms|si> <address> <data>\n"
            "\n"
            "The poke command can write any of the devices hanging off\n"
            "the FPGA which includes the LMS6002D transceiver, VCTCXO\n"
            "trim DAC or the Si5338 clock generator chip.\n"
            "\n"
            "If num_addresses is supplied, the address is incremented by\n"
            "1 and another poke is performed.\n"
            "\n"
            "Valid Address Ranges\n"
            "--------------------\n"
            "dac            0   255\n"
            "lms            0   127\n"
            "si             0   255\n"
            "\n"
            "Examples\n"
            "-------\n"
            "  bladeRF> poke lms ...\n"
        )
    },
    {
        FIELD_INIT(.names, cmd_names_peek),
        FIELD_INIT(.exec, cmd_peek),
        FIELD_INIT(.desc, "Peek a memory location"),
        FIELD_INIT(.help,
            "peek <dac|lms|si> <address> [num addresses]\n"
            "\n"
            "The peek command can read any of the devices hanging off\n"
            "the FPGA which includes the LMS6002D transceiver, VCTCXO\n"
            "trim DAC or the Si5338 clock generator chip.\n"
            "\n"
            "If num_addresses is supplied, the address is incremented by\n"
            "1 and another peek is performed.\n"
            "\n"
            "Valid Address Ranges\n"
            "--------------------\n"
            "dac            0   255\n"
            "lms            0   127\n"
            "si             0   255\n"
            "\n"
            "Examples\n"
            "--------\n"
            "  bladeRF> peek si ...\n"
        )
    },
    {
        FIELD_INIT(.names, cmd_names_help),
        FIELD_INIT(.exec, cmd_help),
        FIELD_INIT(.desc, "Provide information about specified command"),
        FIELD_INIT(.help,
            "help <command>\n"
            "\n"
            "Provides extended help, like this, on any command.\n"
        )
    },
    {
        FIELD_INIT(.names, cmd_names_info),
        FIELD_INIT(.exec, cmd_info),
        FIELD_INIT(.desc, "Print information about the currently opened device"),
        FIELD_INIT(.help,
                "info\n"
                "\n"
                "Prints the following information about an opened device:\n"
                "  Serial number\n"
                "  VCTCXO DAC calibration value\n"
                "  FPGA size\n"
                "  Whether or not the FPGA is loaded\n"
                "  USB bus and address\n"
        )
    },
    {
        FIELD_INIT(.names, cmd_names_load),
        FIELD_INIT(.exec, cmd_load),
        FIELD_INIT(.desc, "Load FPGA or FX3"),
        FIELD_INIT(.help,
            "load <fpga|fx3> <filename>\n"
            "\n"
            "Load an FPGA bitstream or program the FX3's SPI flash.\n"
        )
    },
    {
        FIELD_INIT(.names, cmd_names_print),
        FIELD_INIT(.exec, cmd_print),
        FIELD_INIT(.desc, "Print information about the device"),
        FIELD_INIT(.help,
            "print <param>\n"
            "\n"
            "The print command takes a parameter to print.  The parameter\n"
            "is one of:\n"
            "\n"
            "   bandwidth       Bandwidth settings\n"
            "   config          Overview of everything\n"
            "   frequency       Frequency settings\n"
            "   lmsregs         LMS6002D register dump\n"
            "   loopback        Loopback settings\n"
            "   mimo            MIMO settings\n"
            "   pa              PA settings\n"
            "   pps             PPS settings\n"
            "   refclk          Reference clock settings\n"
            "   rxvga1          Gain setting of RXVGA1 in dB (range: )\n"
            "   rxvga2          Gain setting of RXVGA2 in dB (range: )\n"
            "   samplerate      Samplerate settings\n"
            "   trimdac         VCTCXO Trim DAC settings\n"
            "   txvga1          Gain setting of TXVGA1 in dB (range: )\n"
            "   txvga2          Gain setting of TXVGA2 in dB (range: )\n"
        )
    },
    {
        FIELD_INIT(.names, cmd_names_open),
        FIELD_INIT(.exec, cmd_open),
        FIELD_INIT(.desc, "Open a bladeRF device"),
        FIELD_INIT(.help, "open [device identifiers]\n"
                "\n"
                "Open the specified device for use with successive commands.\n"
                "Any previously opened device will be closed.\n"
                "See the bladerf_open() documentation for the device specifier format.\n",
        )
    },
    {
        FIELD_INIT(.names, cmd_names_probe),
        FIELD_INIT(.exec, cmd_probe),
        FIELD_INIT(.desc, "List attached bladeRF devices"),
        FIELD_INIT(.help, "probe\n"
                "\n"
                "Search for attached bladeRF device and print a list\n"
                "of results.\n",
        )
    },
    {
        FIELD_INIT(.names, cmd_names_erase),
        FIELD_INIT(.exec, cmd_erase),
        FIELD_INIT(.desc, "Erase part of FX3 flash device"),
        FIELD_INIT(.help, "erase\n"
                "\n"
                "Erase sectors from FX3 flash device.\n"
                "\n"
                "    sector_offset   Starting sector to erase\n"
                "    n_sectors       Number of sectors to erase\n"
        )
    },
    {
        FIELD_INIT(.names, cmd_names_ver),
        FIELD_INIT(.exec, cmd_version),
        FIELD_INIT(.desc, "Host software and device version information"),
        FIELD_INIT(.help,
            "version\n"
            "\n"
            "Prints version information for both host software and the current device\n"
        )
    },
    {
        FIELD_INIT(.names, cmd_names_rec),
        FIELD_INIT(.exec, cmd_recover),
        FIELD_INIT(.desc, "Load firmware when operating in FX3 bootloader mode"),
        FIELD_INIT(.help,
            "recover [<bus> <address> <firmware file>]\n"
            "\n"
            "Load firmware onto a device running in bootloader mode, or list\n"
            "all devices currently in bootloader mode.\n"
            "\n"
            "With no arguments, this command lists the USB bus and address for\n"
            "FX3-based devices running in bootloader mode.\n"
            "\n"
            "When provided a bus, address, and path to a firmware file, the\n"
            "specified device will be loaded with and begin executing the\n"
            "provided firmware.\n"
            "\n"
            "In most cases, after successfully loading firmware into the\n"
            "device's RAM, users should open the device with the \"open\"\n"
            "command, and write the firmware to flash via \n"
            "\"load fx3 <firmware file>\"\n"
        )
    },
    {
        FIELD_INIT(.names, cmd_names_jump),
        FIELD_INIT(.exec, cmd_jump_to_bootloader),
        FIELD_INIT(.desc, "Jump to FX3 bootloader"),
        FIELD_INIT(.help,
            "jump_to_boot\n"
        )
    },
    {
        FIELD_INIT(.names, cmd_names_clear),
        FIELD_INIT(.exec, cmd_clear),
        FIELD_INIT(.desc, "Clear the screen"),
        FIELD_INIT(.help, "clear\n"
                "\n"
                "Clears the screen\n"
        )
    },
    {
        FIELD_INIT(.names, cmd_names_quit),
        FIELD_INIT(.exec, NULL), /* Default action on NULL exec function is to quit */
        FIELD_INIT(.desc, "Exit the CLI"),
        FIELD_INIT(.help, "Exit the CLI\n")
    },
    {
        FIELD_INIT(.names, cmd_names_mimo),
        FIELD_INIT(.exec, cmd_mimo),
        FIELD_INIT(.desc, "Modify device MIMO operation"),
        FIELD_INIT(.help, "mimo [master|slave]\n")
    },
    /* Always terminate the command entry with a completely NULL entry */
    {
        FIELD_INIT(.names, NULL),
        FIELD_INIT(.exec, NULL),
        FIELD_INIT(.desc, NULL),
        FIELD_INIT(.help, NULL),
    }
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
    int ret = CMD_RET_OK;
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
            cli_err(s, argv[0], "No help info available for \"%s\"", argv[1]);
            ret = CMD_RET_INVPARAM;
        }
    } else {
        ret = CMD_RET_NARGS;
    }

    return ret;
}

const char * cmd_strerror(int error, int lib_error)
{
    switch (error) {
        case CMD_RET_MEM:
            return "A fatal memory allocation error has occurred";

        case CMD_RET_UNKNOWN:
            return "A fatal unknown error has occurred";

        case CMD_RET_MAX_ARGC:
            return "Number of arguments exceeds allowed maximum";

        case CMD_RET_LIBBLADERF:
            return bladerf_strerror(lib_error);

        case CMD_RET_NODEV:
            return "No devices are currently opened";

        case CMD_RET_NARGS:
            return "Invalid number of arguments provided";

        case CMD_RET_NOFPGA:
            return "Command requires FPGA to be loaded";

        case CMD_RET_STATE:
            return "Operation invalid in current state";

        case CMD_RET_FILEOP:
            return "File operation failed";

        case CMD_RET_BUSY:
            return "Could not complete operation - device is currently busy";

        /* Other commands shall print out helpful info from within their
         * implementation */
        default:
            return NULL;
    }
}

int cmd_clear(struct cli_state *s, int argc, char **argv)
{
    return CMD_RET_CLEAR_TERM;
}

int cmd_handle(struct cli_state *s, const char *line_)
{
    int argc, ret;
    char *saveptr;
    char *line;
    char *token;
    char *argv[MAX_ARGS]; /* TODO dynamically allocate? */

    const struct cmd *cmd;

    if (!(line = strdup(line_)))
        return CMD_RET_MEM;

    argc = 0;
    ret = 0;

    token = strtok_r(line, " \t\r\n", &saveptr);

    /* Fill argv as long as we see tokens that aren't comments */
    while (token && token[0] != '#') {
        argv[argc++] = token;
        token = strtok_r(NULL, " \t\r\n", &saveptr);
    }

    if (argc > 0) {
        cmd = get_cmd(argv[0]);

        if (cmd) {
            if (cmd->exec) {
                ret = cmd->exec(s, argc, argv);
            } else {
                ret = CMD_RET_QUIT;
            }
        } else {
            cli_err(s, "Unrecognized command", "%s", argv[0]);
            ret = CMD_RET_NOCMD;
        }
    }

    free(line);
    return ret;
}
