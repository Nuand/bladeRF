bladeRF flashing utility {#bladerf-flash}
========

\section SYNPOSIS
\b bladeRF-flash \e \<options\>

\section DESCRIPTION
The \b bladeRF-flash utility is used when you are having a very bad day.

For more information on obtaining or building firmware files and FPGA bitstreams, please visit http://nuand.com/.

\section OPTIONS
The program follows the usual GNU command line syntax, with long options starting with two dashes (--).

\par -d, --device \<device\>
Use the specified bladeRF device.
\note The -d option takes a device specifier string. See the bladerf_open() documentation for more information about the format of this string.

\par -f, --flash-firmware \<file\>
Flash specified firmware file.

\par -r, --reset
Start with RESET instead of JUMP_TO_BOOTLOADER

\par -l, --load-ram
Only load FX3 RAM instead of also flashing.

\par -L, --lib-version
Print libbladeRF version and exit.

\par -v, --verbosity \<level\>
Set the libbladeRF verbosity level. Levels, listed in increasing verbosity, are:
    \e critical, \e error, \e warning, \e info, \e debug, \e verbose

\par -V, --version
Print flasher version and exit.

\par -h, --help
Show this help text.

\section SEE ALSO
bladeRF-cli(1).

More documentation is available at http://nuand.com/ and https://github.com/nuand/bladerf.

\section AUTHOR
\b bladeRF-flash was written by the contributors to the bladeRF Project.  See the CONTRIBUTORS file for more information.
