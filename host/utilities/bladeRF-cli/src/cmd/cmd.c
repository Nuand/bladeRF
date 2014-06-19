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
#include <errno.h>
#include <pthread.h>

#include "cmd.h"
#include "conversions.h"
#include "script.h"

#define DECLARE_CMD(x) int cmd_##x (struct cli_state *, int, char **)
DECLARE_CMD(calibrate);
DECLARE_CMD(clear);
DECLARE_CMD(correct);
DECLARE_CMD(echo);
DECLARE_CMD(erase);
DECLARE_CMD(flash_backup);
DECLARE_CMD(flash_image);
DECLARE_CMD(flash_init_cal);
DECLARE_CMD(flash_restore);
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
#ifdef CLI_LIBUSB_ENABLED
DECLARE_CMD(recover);
#endif
DECLARE_CMD(run);
DECLARE_CMD(set);
DECLARE_CMD(rx);
DECLARE_CMD(tx);
DECLARE_CMD(version);

#define MAX_ARGS    10

struct cmd {
    const char **names;
    int (*exec)(struct cli_state *s, int argc, char **argv);
    const char  *desc;
    const char  *help;
};

static const char *cmd_names_correct[] = { "correct", NULL };
static const char *cmd_names_calibrate[] = { "calibrate", "cal", NULL };
static const char *cmd_names_clear[] = { "clear", "cls", NULL };
static const char *cmd_names_echo[] = { "echo", NULL };
static const char *cmd_names_erase[] = { "erase", "e", NULL };
static const char *cmd_names_flash_backup[] = { "flash_backup", "fb",  NULL };
static const char *cmd_names_flash_image[] = { "flash_image", "fi", NULL };
static const char *cmd_names_flash_init_cal[] = { "flash_init_cal", "fic", NULL };
static const char *cmd_names_flash_restore[] = { "flash_restore", "fr", NULL };
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
        FIELD_INIT(.desc, "Calibrate transceiver"),
        FIELD_INIT(.help,
            "calibrate <operation>\n"
            "\n"
            "Perform the specified calibration operation."
            "\n"
            "Available operations:\n"
            "\n"
            "\tdc_lms_tuning\tDC calibration of LMS LPF Tuning\n"
            "\tdc_lms_txlpf \tDC calibration of LMS TX LPF\n"
            "\tdc_lms_rxlpf \tDC calibration of LMS RX LPF\n"
            "\tdc_lms_rxvga2\tDC calibration of LMS RX VGA2\n"
            "\tdc_lms       \tDC calibration of all of the above LMS items\n"
            "\tdc_rx        \tDC calibration of LMS I and Q RX channels\n"
            "\tdc_tx        \tDC calibration of LMS I and Q TX channels\n"
            "\tdc           \tDC calibration of LMS I and Q RX+TX channels\n"
            "\n"
        )
    },
    {
        FIELD_INIT(.names, cmd_names_clear),
        FIELD_INIT(.exec, cmd_clear),
        FIELD_INIT(.desc, "Clear the screen"),
        FIELD_INIT(.help,
            "clear\n"
            "\n"
            "Clears the screen\n"
        )
    },
    {
        FIELD_INIT(.names, cmd_names_echo),
        FIELD_INIT(.exec, cmd_echo),
        FIELD_INIT(.desc, "Echo each argument on a new line"),
        FIELD_INIT(.help,
            "echo [arg 1] [arg 2] ... [arg n]\n"
            "\n"
            "Echo each argument on a new line.\n")

    },
    {
        FIELD_INIT(.names, cmd_names_erase),
        FIELD_INIT(.exec, cmd_erase),
        FIELD_INIT(.desc, "Erase specified erase blocks of SPI flash"),
        FIELD_INIT(.help,
            "erase <offset> <count>\n"
            "\n"
            "Erase specified erase blocks SPI flash\n"
            "\n"
            "<offset>\tErase block offset\n"
            "<count>\tNumber of erase blocks to erase\n"
        )
    },
    {
        FIELD_INIT(.names, cmd_names_flash_backup),
        FIELD_INIT(.exec, cmd_flash_backup),
        FIELD_INIT(.desc, "Back up flash data to a file with metadata."),
        FIELD_INIT(.help, "flash_backup <file> (<type> | <address> <length>)\n"
            "\n"
            "Back up flash data to the specified file. This command takes either two or\n"
            "four arguments. The two-argument invocation is generally recommended for\n"
            "non-development use.\n"
            "\n"
            "Parameters:\n"
            "\n"
            "<type>\n"
            "\tType of backup. This selects the appropriate address and length\n"
            "\tvalues based upon the selected type. Valid values are:\n"
            "\t\tcal\tCalibration data\n"
            "\t\tfw\tFirmware\n"
            "\t\tfpga40\tMetadata and bitstream for a 40 kLE FPGA\n"
            "\t\tfpga115\tMetadata and bitstream for a 115 kLE FPGA\n"
            "\n"
            "<address>\tAddress of data to back up. Must be erase block-aligned.\n"
            "<len>\tLength of region to back up. Must be erase block-aligned.\n"
            "\n"
            "Note: When an address and length are provided, the image type will default\n"
            "to \"raw\".\n"
            "\n"
            "Examples:\n"
            "\tflash_backup cal.bin cal\tBack up the calibration data region.\n"
            "\tflash_backup cal_raw.bin 0x30000 0x10000\n"
            "\t\tBack up the calibration region as a raw data image.\n"
            ),
    },
    {
        FIELD_INIT(.names, cmd_names_flash_image),
        FIELD_INIT(.exec, cmd_flash_image),
        FIELD_INIT(.desc, "Print a flash image's metadata or create a new flash image"),
        FIELD_INIT(.help, "flash_image <image> [output options]\n"
            "\n"
            "Print a flash image's metadata or create a new flash image."
            "\n"
            "When provided with the name of a flash image file as the only argument,\n"
            "this command will print the metadata contents of the image.\n"
            "\n"
            "The following options may be used to create a new flash image.\n"
            "\tdata=<file>\tFile to containing data to store in the image.\n"
            "\taddress=<addr>\tFlash address. Default depends upon 'type' parameter.\n"
            "\ttype=<type>\n"
            "\t\tType of flash image. Defaults to \"raw\".\n"
            "\t\tValid options are:\n"
            "\t\t\tcal\tCalibration data\n"
            "\t\t\tfw\tFirmware\n"
            "\t\t\tfpga40\tMetadata and bitstream for 40 kLE FPGA\n"
            "\t\t\tfpga115\tMetadata and bitstream for 115 kLE FPGA\n"
            "\t\t\traw\n"
            "\t\t\t\tRaw data. The address and length parameters\n"
            "\t\t\t\tmust be provided if this type is selected.\n"
            "\tserial=<serial>\tSerial # to store in image. Defaults to zeros.\n"
            )
    },
    {
        FIELD_INIT(.names, cmd_names_flash_init_cal),
        FIELD_INIT(.exec, cmd_flash_init_cal),
        FIELD_INIT(.desc, "Write new calibration data to a device or to a file"),
        FIELD_INIT(.help, "flash_init_cal <fpga_size> <vctcxo_trim> [<output_file>]\n"
            "\n"
            "Create and write a new calibration data region to the currently opened device,\n"
            "or to a file. Be sure to back up calibration data prior to running this\n"
            "command.\n"
            "(See the `flash_backup` command.)\n\n"
            "\t<fpga_size>\tEither 40 or 115, depending on the device model.\n"
            "\t<vctcxo_trim>\tVCTCXO/DAC trim value (0x0-0xffff)\n"
            "\t<output_file>\n"
            "\t\tFile to write calibration data to. When this argument\n"
            "\t\tis provided, no data will be written to the device's flash.\n"
            )
    },
    {
        FIELD_INIT(.names, cmd_names_flash_restore),
        FIELD_INIT(.exec, cmd_flash_restore),
        FIELD_INIT(.desc, "Restore flash data from a file"),
        FIELD_INIT(.help, "flash_restore <file> [<address> <length>]\n\n"
            "Restore flash data from a file, optionally overriding values in the image\n"
            "metadata.\n"
            "\n"
            "\t<address>\n"
            "\t\tDefaults to the address specified in the provided\n"
            "\t\tflash image file.\n"
            "\t<length>\n"
            "\t\tDefaults to length of the data in the provided image\n"
            "\t\tfile.\n"
            ),
    },
    {
        FIELD_INIT(.names, cmd_names_help),
        FIELD_INIT(.exec, cmd_help),
        FIELD_INIT(.desc, "Provide information about specified command"),
        FIELD_INIT(.help,
            "help [<command>]\n"
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
            "\tSerial number\t\n"
            "\tVCTCXO DAC calibration value\t\n"
            "\tFPGA size\t\n"
            "\tWhether or not the FPGA is loaded\t\n"
            "\tUSB bus, address, and speed\t\n"
            "\tBackend (libusb or kernel module)\t\n"
            "\tInstance number\t\n"
        )
    },
    {
        FIELD_INIT(.names, cmd_names_jump),
        FIELD_INIT(.exec, cmd_jump_to_bootloader),
        FIELD_INIT(.desc, "Jump to FX3 bootloader"),
        FIELD_INIT(.help,
            "jump_to_boot\n"
            "\n"
            "Jumps to the FX3 bootloader.\n"
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
        FIELD_INIT(.names, cmd_names_xb),
        FIELD_INIT(.exec, cmd_xb),
        FIELD_INIT(.desc, "Enable and configure expansion boards"),
        FIELD_INIT(.help,
            "xb enable [100|200]\n"
            "\n"
            "Enable and configure expansion boards\n"
        )
    },
    {
        FIELD_INIT(.names, cmd_names_mimo),
        FIELD_INIT(.exec, cmd_mimo),
        FIELD_INIT(.desc, "Modify device MIMO operation"),
        FIELD_INIT(.help,
            "mimo [master | slave]\n"
            "\n"
            "Modify device MIMO operation\n"
        )
    },
    {
        FIELD_INIT(.names, cmd_names_open),
        FIELD_INIT(.exec, cmd_open),
        FIELD_INIT(.desc, "Open a bladeRF device"),
        FIELD_INIT(.help,
            "open [device identifiers]\n"
            "\n"
            "Open the specified device for use with successive commands.\n"
            "Any previously opened device will be closed.\n"
            "See the bladerf_open() documentation for the device specifier format.\n",
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
            "Valid Address Ranges:\n"
            "\tdac: 0 to 255\t\n"
            "\tlms: 0 to 127\t\n"
            "\tsi:  0 to 255\t\n"
            "\n"
            "Example:\n"
            "\tpeek si ...\n"
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
            "Valid Address Ranges:\n"
            "\tdac: 0 to 255\t\n"
            "\tlms: 0 to 127\t\n"
            "\tsi:  0 to 255\t\n"
            "\n"
            "Example:\n"
            "\tpoke lms ...\n"
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
            "\tbandwidth\tBandwidth settings\n"
            "\tfrequency\tFrequency settings\n"
            "\tgpio\tFX3 <-> FPGA GPIO state\n"
            "\tloopback\tLoopback settings\n"
            "\tlnagain\tGain setting of the LNA, in dB\n"
            "\trxvga1\tGain setting of RXVGA1, in dB\n"
            "\trxvga2\tGain setting of RXVGA2, in dB\n"
            "\ttxvga1\tGain setting of TXVGA1, in dB\n"
            "\ttxvga2\tGain setting of TXVGA2, in dB\n"
            "\tsampling\tExternal or internal sampling mode\n"
            "\tsamplerate\tSamplerate settings\n"
            "\ttrimdac\tVCTCXO Trim DAC settings\n"
        )
    },
    {
        FIELD_INIT(.names, cmd_names_probe),
        FIELD_INIT(.exec, cmd_probe),
        FIELD_INIT(.desc, "List attached bladeRF devices"),
        FIELD_INIT(.help,
            "probe\n"
            "\n"
            "Search for attached bladeRF device and print a list\n"
            "of results.\n",
        )
    },
    {
        FIELD_INIT(.names, cmd_names_quit),
        FIELD_INIT(.exec, NULL), /* Default action on NULL exec function is to quit */
        FIELD_INIT(.desc, "Exit the CLI"),
        FIELD_INIT(.help,
            "quit\n"
            "\n"
            "Exit the CLI\n"
        )
    },
#ifdef CLI_LIBUSB_ENABLED
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
            "\n"
            "Note:\tThis command is only available when bladeRF-cli is built\n"
            "\twith libusb support.\n"
        )
    },
#endif
    {
        FIELD_INIT(.names, cmd_names_run),
        FIELD_INIT(.exec, cmd_run),
        FIELD_INIT(.desc, "Run a script"),
        FIELD_INIT(.help, "run <script>\n"
            "\n"
            "Run the provided script.\n"
        )
    },
    {
        FIELD_INIT(.names, cmd_names_rx),
        FIELD_INIT(.exec, cmd_rx),
        FIELD_INIT(.desc, "Receive IQ samples"),
        FIELD_INIT(.help,
            "rx <start | stop | wait | config [param=val [param=val [...]]>\n"
            "\n"
            "Receive IQ samples and write them to the specified file.\n"
            "Reception is controlled and configured by one of the following:\n"
            "\n"
            "\tstart\tStart receiving samples\n"
            "\tstop\tStop Receiving samples\n"
            "\twait\n"
            "\t\tWait for sample transmission to complete, or until a specified\n"
            "\t\tamount of time elapses\n"
            "\tconfig\n"
            "\t\tConfigure sample reception. If no parameters\n"
            "\t\tare provided, the current parameters are printed.\n"
            "\n"
            "Running 'rx' without any additional commands is valid shorthand\n"
            "for 'rx config'.\n"
            "\n"
            "The wait command takes an optional timeout parameter. This parameter defaults\n"
            "to units of ms. The timeout unit may be specified using the ms, s, m, or h\n"
            "suffixes. If this parameter is not provided, the command will wait until\n"
            "the reception completes or Ctrl-C is pressed.\n"
            "\n"
            "Configuration parameters take the form 'param=value', and may be specified\n"
            "in a single or multiple 'rx config' invocations. Below is a list of\n"
            "available parameters.\n"
            "\n"
            "\tn\tNumber of samples to receive. 0 = inf.\n"
            "\n"
            "\tfile\tFilename to write received samples to\n"
            "\n"
            "\tformat\tOutput file format. One of the following:\n"
            "\t\tcsv\tCSV of SC16 Q11 samples\n"
            "\t\tbin\tRaw SC16 Q11 DAC samples\n"
            "\n"
            "\tsamples\n"
            "\t\tNumber of samples per buffer to use in the asynchronous\n"
            "\t\tstream. Must be divisible by 1024 and >= 1024.\n"
            "\n"
            "\tbuffers\n"
            "\t\tNumber of sample buffers to use in the asynchronous\n"
            "\t\tstream. The min value is 4.\n"
            "\n"
            "\txfers\n"
            "\t\tNumber of simultaneous transfers to allow the asynchronous\n"
            "\t\tstream to use. This should be < the 'buffers' parameter.\n"
            "\n"
            "\ttimeout\n"
            "\t\tData stream timeout. With no suffix, the default unit is ms.\n"
            "\t\tThe default value is 1s. Valid suffixes are 'ms' and 's'.\n"
            "\n"
            "Example:\n"
            "\tReceive (10240 = 10 * 1024) samples, writing them to /tmp/data.bin\n"
            "\tin the binary DAC format.\n"
            "\n"
            "\trx config file=/tmp/data.bin format=bin n=10K\n"
            "\n"
            "Notes:\n"
            "\tThe n, samples, buffers, and xfers parameters support the suffixes\n"
            "\t'K', 'M', and 'G', which are multiples of 1024.\n"
            "\n"
            "\tAn 'rx stop' followed by an 'rx start' will result in the samples file\n"
            "\tbeing truncated. If this is not desired, be sure to run 'rx config' to\n"
            "\tset another file before restarting the rx stream.\n"
            "\n"
            "\tFor higher sample rates, it is advised that the binary output format\n"
            "\tbe used, and the output file be written to RAM (e.g. /tmp, /dev/shm),\n"
            "\tif space allows. For larger captures at higher sample rates, consider\n"
            "\tusing an SSD instead of a HDD.\n"
            "\n"
        )
    },
    {
        FIELD_INIT(.names, cmd_names_tx),
        FIELD_INIT(.exec, cmd_tx),
        FIELD_INIT(.desc, "Transmit IQ samples"),
        FIELD_INIT(.help,
            "tx <start | stop | wait | config [parameters]>\n"
            "\n"
            "Read IQ samples from the specified file and transmit them.\n"
            "Transmission is controlled and configured by one of the following:\n"
            "\n"
            "\tstart\tStart transmitting samples\n"
            "\tstop\tStop transmitting samples\n"
            "\twait\n"
            "\t\tWait for sample transmission to complete, or until a specified\n"
            "\t\tamount of time elapses\n"
            "\tconfig\n"
            "\t\tConfigure sample transmission . If no parameters\n"
            "\t\tare provided, the current parameters are printed.\n"
            "\n"
            "Running 'tx' without any additional commands is valid shorthand\n"
            "for 'tx config'.\n"
            "\n"
            "The wait command takes an optional timeout parameter. This parameter defaults\n"
            "to units of ms. The timeout unit may be specified using the ms, s, m, or h\n"
            "suffixes. If this parameter is not provided, the command will wait until\n"
            "the transmission completes or Ctrl-C is pressed.\n"
            "\n"
            "Configuration parameters take the form 'param=value', and may be specified\n"
            "in a single or multiple 'tx config' invocations. Below is a list of\n"
            "available parameters.\n"
            "\n"
            "\tfile\tFilename to read samples from\n"
            "\n"
            "\tformat\tOutput file format. One of the following:\n"
            "\t\tcsv\tCSV of SC16 Q11 samples ([-2048, 2047])\n"
            "\t\tbin\tRaw SC16 Q11 DAC samples ([-2048, 2047])\n"
            "\n"
            "\trepeat\n"
            "\t\tThe number of times the file contents should be \n"
            "\t\ttransmitted. 0 implies repeat until stopped.\n"
            "\n"
            "\tdelay\n"
            "\t\tThe number of microseconds to delay between retransmitting\n"
            "\t\tfile contents. 0 implies no delay.\n"
            "\n"
            "\tsamples\n"
            "\t\tNumber of samples per buffer to use in the asynchronous\n"
            "\t\tstream. Must be divisible by 1024 and >= 1024.\n"
            "\n"
            "\tbuffers\n"
            "\t\tNumber of sample buffers to use in the asynchronous\n"
            "\t\tstream. The min value is 4.\n"
            "\n"
            "\txfers\n"
            "\t\tNumber of simultaneous transfers to allow the asynchronous\n"
            "\t\tstream to use. This should be < the 'buffers' parameter.\n"
            "\n"
            "\ttimeout\n"
            "\t\tData stream timeout. With no suffix, the default unit is ms.\n"
            "\t\tThe default value is 1s. Valid suffixes are 'ms' and 's'.\n"
            "\n"
            "Example:\n"
            "\tTransmitting the contents of data.bin two times, with a ~250ms\n"
            "\tdelay between transmissions.\n"
            "\n"
            "\ttx config file=data.bin format=bin repeat=2 delay=250000\n"
            "\n"
            "Notes:\n"
            "\tThe n, samples, buffers, and xfers parameters support the suffixes\n"
            "\t'K', 'M', and 'G', which are multiples of 1024.\n"
            "\n"
            "\tFor higher sample rates, it is advised that the input file be\n"
            "\tstored in RAM (e.g. /tmp, /dev/shm) or to an SSD, rather than a HDD.\n"
            "\n"
            "\tWhen providing CSV data, this command will first convert it to\n"
            "\ta binary format, stored in a file in the current working directory.\n"
            "\tDuring this process, out-of-range values will be clamped.\n"
            "\n"
            "\tWhen using a binary format, the user is responsible for ensuring\n"
            "\tthat the provided data values are within the allowed range.\n"
            "\tThis prerequisite alleviates the need for this program to perform\n"
            "\trange checks in time-sensititve callbacks.\n"
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
            "\tbandwidth\tBandwidth settings\n"
            "\tfrequency\tFrequency settings\n"
            "\tgpio\tFX3 <-> FPGA GPIO state\n"
            "\tloopback\tLoopback settings\n"
            "\tlnagain\tGain setting of the LNA, in dB. Valid values  are 0, 3, 6.\n"
            "\trxvga1\tGain setting of RXVGA1, in dB. Range: [5, 30]\n"
            "\trxvga2\tGain setting of RXVGA2, in dB. Range: [0, 30]\n"
            "\ttxvga1\tGain setting of TXVGA1, in dB. Range: [-35, -4]\n"
            "\ttxvga2\tGain setting of TXVGA2, in dB. Range: [0, 25]\n"
            "\tsampling\tExternal or internal sampling mode\n"
            "\tsamplerate\tSamplerate settings\n"
            "\ttrimdac\tVCTCXO Trim DAC settings\n"
        )
    },
    {
        FIELD_INIT(.names, cmd_names_ver),
        FIELD_INIT(.exec, cmd_version),
        FIELD_INIT(.desc, "Host software and device version information"),
        FIELD_INIT(.help,
            "version\n"
            "\n"
            "Prints version information for host software and the current device\n"
        )
    },
    {
        FIELD_INIT(.names, cmd_names_correct),
        FIELD_INIT(.exec, cmd_correct),
        FIELD_INIT(.desc, "Correct for IQ Imbalances"),
        FIELD_INIT(.help,
            "correct [tx|rx] [dc|phase|gain] [args]\n"
            "\n"
            "Correct for IQ Imbalances\n"
        )
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
            cli_err(s, argv[0], "No help info available for \"%s\"", argv[1]);
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
        cli_err(state, "run", "Recursive loop detected in script");
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

    ret = 0;
    argc = str2args(line, &argv);

    if (argc > 0) {
        cmd = get_cmd(argv[0]);

        if (cmd) {
            if (cmd->exec) {
                /* Commands own the device handle while they're executing.
                 * This is needed to prevent races on the device handle while
                 * the RX/TX make any necessary control calls while
                 * starting up or finishing up a stream() call*/
                pthread_mutex_lock(&s->dev_lock);
                ret = cmd->exec(s, argc, argv);
                pthread_mutex_unlock(&s->dev_lock);
            } else {
                ret = CLI_RET_QUIT;
            }
        } else {
            cli_err(s, "Unrecognized command", "%s", argv[0]);
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

