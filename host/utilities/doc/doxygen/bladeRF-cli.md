bladeRF command line interface and test utility {#bladerf-cli}
========

\section SYNPOSIS
\b bladeRF-cli \e [options]

\section DESCRIPTION
The \b bladeRF-cli utility is used to flash firmware files, load FPGA bitstreams, and perform other tasks on the Nuand bladeRF software-defined radio system.

For more information on obtaining or building firmware files and FPGA bitstreams, please visit http://nuand.com/.

\section OPTIONS
The program follows the usual GNU command line syntax, with long options starting with two dashes (--).

\par -d, --device \<device\>
Use the specified bladeRF device.
\note The -d option takes a device specifier string. See the bladerf_open() documentation for more information about the format of this string. If the -d parameter is not provided, the first available device will be used for the provided command, or will be opened prior to entering interactive mode.

\par -f, --flash-firmware \<file\>
Flash specified firmware file.

\par -l, --load-fpga \<file\>
Load specified FPGA bitstream.

\par -p, --probe
Probe for devices, print results, then exit.

\par -s, --script \<file\>
Run provided script.

\par -i, --interactive
Enter interactive mode.

\par -L, --lib-version
Print libbladeRF version and exit.

\par -v, --verbosity \<level\>
Set the libbladeRF verbosity level. Levels, listed in increasing verbosity, are:
    \e critical, \e error, \e warning, \e info, \e debug, \e verbose

\par -V, --version
Print CLI version and exit.

\par -h, --help
Show this help text.

\section INTERACTIVE MODE COMMANDS

The following commands are available when interactive mode is enabled by the \b -i or \b --interactive command line options.

\par calibrate
Calibrate transceiver

\par rx \<start | stop | config [param=val [param=val [...]]\>
Receive IQ samples

\par tx \<start | stop | config [parameters]\>
Transmit IQ samples

\par set \<param\> \<arguments\>
Set device settings.  The set command takes a parameter and an arbitrary number of arguments for that particular command.

         \param bandwidth       Bandwidth settings
         \param config          Overview of everything
         \param frequency       Frequency settings
         \param lmsregs         LMS6002D register dump
         \param loopback        Loopback settings
         \param mimo            MIMO settings
         \param pa              PA settings
         \param pps             PPS settings
         \param refclk          Reference clock settings
         \param rxvga1          Gain setting of RXVGA1 in dB (range: )
         \param rxvga2          Gain setting of RXVGA2 in dB (range: )
         \param samplerate      Samplerate settings
         \param trimdac         VCTCXO Trim DAC settings
         \param txvga1          Gain setting of TXVGA1 in dB (range: )
         \param txvga2          Gain setting of TXVGA2 in dB (range: )

\par poke \<dac|lms|si\> \<address\> \<data\>
Poke a memory location

\par peek \<dac|lms|si\> \<address\> [num addresses]
Peek a memory location

\par help [\<command\>]
Provide information about specified command

\par info
Print information about the currently opened device

\par load \<fpga|fx3\> \<filename\>
Load FPGA or FX3

\par print \<param\>
Print information about the device.  The print command takes a parameter to print.

         \param bandwidth       Bandwidth settings
         \param config          Overview of everything
         \param frequency       Frequency settings
         \param lmsregs         LMS6002D register dump
         \param loopback        Loopback settings
         \param mimo            MIMO settings
         \param pa              PA settings
         \param pps             PPS settings
         \param refclk          Reference clock settings
         \param rxvga1          Gain setting of RXVGA1 in dB (range: )
         \param rxvga2          Gain setting of RXVGA2 in dB (range: )
         \param samplerate      Samplerate settings
         \param trimdac         VCTCXO Trim DAC settings
         \param txvga1          Gain setting of TXVGA1 in dB (range: )
         \param txvga2          Gain setting of TXVGA2 in dB (range: )

\par open [device identifiers]
Open a bladeRF device.
Open the specified device for use with successive commands.  Any previously opened device will be closed.  See the bladerf_open() documentation for the device specifier format.

\par probe
List attached bladeRF devices

\par erase
Erase part of FX3 flash device

\par version
Host software and device version information

\par recover [device_str] [file]
Load firmware when operating in FX3 bootloader mode

\par jump_to_boot
Jump to FX3 bootloader

\par clear
Clear the screen

\par quit
Exit the CLI

\section SEE ALSO
bladeRF-flash(1).

More documentation is available at http://nuand.com/ and https://github.com/nuand/bladerf.

\section AUTHOR
bladeRF-cli was written by the contributors to the bladeRF Project.  See the CONTRIBUTORS file for more information.
