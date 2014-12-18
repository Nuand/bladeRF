[INTERACTIVE COMMANDS]

calibrate
---------

Usage: `calibrate <operation> [options]`

Perform the specified transceiver calibration operation.

Available operations:

 * LMS internal DC offset auto-calibrations

     * `calibrate lms [show]`
     * `calibrate lms tuning [value]`
     * `calibrate lms txlpf [<I filter> <Q filter>]`
     * `calibrate lms rxlpf [<I filter> <Q filter>]`
     * `calibrate lms rxvga2 [<DC ref> <I1> <Q1> <I2> <Q2>]`

    Perform the specified auto-calibration, or all of them if none are
    provided. When values are provided, these are used instead of the
    results of the auto-calibration procedure. Use `lms show` to read and
    print the current LMS calibration values.

    For `rxvga2`, `I1` and `Q1` are the Stage 1 I and Q components
    respectively, and `I2` and `Q2` are the Stage 2 I and Q components.

 * RX and TX I/Q DC offset correction parameter calibration

     * `calibrate dc <rx|tx> [<I> <Q>]`
     * `calibrate dc <rxtx> dc`

    Calibrate the DC offset correction parameters for the current frequency
    and gain settings. If a I/Q values are provided, they are applied
    directly. `cal rxtx` is shorthand for `cal rx` followed by
    `cal tx`.

 * RX and TX I/Q balance correction parameter calibration

     * `calibrate iq <rx|tx> <gain|phase> <value>`

    Set the specified IQ gain or phase balance parameters.

 * Generate RX or TX I/Q DC correction parameter tables

     * `calibrate table dc <rx|tx> [<f_min> <f_max> [f_inc]]`

    Generate and write an I/Q correction parameter table to the current
    working directory, in a file named `<serial>_dc_<rx|tx>.tbl`.
    `f_min` and `f_max` are min and max frequencies to include in the
    table. `f_inc` is the frequency increment.

    By default, tables are generated over the entire frequency range, in
    2 MHz steps.


clear
-----

Usage: `clear`

Clears the screen.


echo
----

Usage: `echo [arg 1] [arg 2] ... [arg n]`

Echo each argument on a new line.


erase
-----

Usage: `erase <offset> <count>`

Erase specified erase blocks SPI flash.

 * `<offset>` - Erase block offset
 * `<count>` - Number of erase blocks to erase


flash_backup
------------

Usage: `flash_backup <file> (<type> | <address> <length>)`

Back up flash data to the specified file. This command takes either two or
four arguments. The two-argument invocation is generally recommended for
non-development use.

Parameters:

 * `<type>` - Type of backup.

    This selects the appropriate address and length values based upon the
    selected type.

    Valid options are:

    ------------------------------------------------------------------
         Option Description
    ----------- ------------------------------------------------------
    `cal`       Calibration data

    `fw`        Firmware

    `fpga40`    Metadata and bitstream for 40 kLE FPGA

    `fpga115`   Metadata and bitstream for 115 kLE FPGA
    ------------------------------------------------------------------

 * `<address>` - Address of data to back up. Must be erase block-aligned.
 * `<len>` - Length of region to back up. Must be erase block-aligned.

Note: When an address and length are provided, the image type will default
to `raw`.

Examples:

 * `flash_backup cal.bin cal`

    Backs up the calibration data region.

 * `flash_backup cal_raw.bin 0x30000 0x10000`

    Backs up the calibration region as a raw data image.


flash_image
-----------

Usage: `flash_image <image> [output options]`

Print a flash image's metadata or create a new flash image. When provided
with the name of a flash image file as the only argument, this command will
print the metadata contents of the image.

The following options may be used to create a new flash image.

 * `data=<file>`

    File to containing data to store in the image.

 * `address=<addr>`

    Flash address. The default depends upon `type` parameter.

 * `type=<type>`

    Type of flash image. Defaults to `raw`.

    Valid options are:

    ------------------------------------------------------------------
         Option Description
    ----------- ------------------------------------------------------
    `cal`       Calibration data

    `fw`        Firmware

    `fpga40`    Metadata and bitstream for 40 kLE FPGA

    `fpga115`   Metadata and bitstream for 115 kLE FPGA

    `raw`       Raw data. The address and length parameters must be
                provided if this type is selected.
    ------------------------------------------------------------------

 * `serial=<serial>`

    Serial # to store in image. Defaults to zeros.


flash_init_cal
--------------

Usage: `flash_init_cal <fpga_size> <vctcxo_trim> [<output_file>]`

Create and write a new calibration data region to the currently opened
device, or to a file. Be sure to back up calibration data prior to running
this command. (See the `flash_backup` command.)

 * `<fpga_size>`

    Either 40 or 115, depending on the device model.

 * `<vctcxo_trim>`

    VCTCXO/DAC trim value (`0x0`-`0xffff`)

 * `<output_file>`

    File to write calibration data to. When this argument is provided, no
    data will be written to the device's flash.


flash_restore
-------------

Usage: `flash_restore <file> [<address> <length>]`

Restore flash data from a file, optionally overriding values in the image
metadata.

 * `<address>`

    Defaults to the address specified in the provided flash image file.

 * `<length>`

    Defaults to length of the data in the provided image file.


help
----

Usage: `help [<command>]`

Provides extended help, like this, on any command.


info
----

Usage: `info`

Prints the following information about an opened device:

 * Serial number
 * VCTCXO DAC calibration value
 * FPGA size
 * Whether or not the FPGA is loaded
 * USB bus, address, and speed
 * Backend (Denotes which device interface code is being used.)
 * Instance number


jump_to_boot
------------

Usage: `jump_to_boot`

Jumps to the FX3 bootloader.


load
----

Usage: `load <fpga|fx3> <filename>`

Load an FPGA bitstream or program the FX3's SPI flash.


xb
--

Usage: `xb <board_model> <subcommand> [parameters]`

Enable or configure an expansion board.

Valid values for `board_model`:

  - `100`

      XB-100 GPIO expansion board.

  - `200`

      XB-200 LF/MF/HF/VHF transverter expansion board

Valid subcommands:

  - `enable <board_model>`

      Enable the XB-100 or XB-200 expansion board.

  - `filter 200 [rx|tx] [50|144|222|custom|auto_1db|auto_3db]`

      Selects the specified RX or TX filter on the XB-200 board. Below
      are descriptions of each of the filter options.

      * 50

            Selects the 50-54 MHz (6 meter band) filter

      * 144

            Selects the 144-148 MHz (2 meter band) filter

      * 222

            Selects 222-225 MHz (1.25 meter band) filter. Realistically,
            this filter option is actually slightly wider, covering
            206 MHz - 235 MHz.

      * custom

            Selects the custom filter path. The user should connect a filter
            along the corresponding FILT and FILT-ANT connections when using
            this option.  Alternatively one may jumper the FILT and FILT-ANT
            connections to achieve "no filter" for reception. (However, this is
            _highly_ discouraged for transmissions.)

      * auto_1db

            Automatically selects one of the above choices based upon frequency
            and the filters' 1dB points. The custom path is used for cases
            that are not associated with the on-board filters.

      * auto_3db

            Automatically selects one of the above choices based upon frequency
            and the filters' 3dB points. The custom path is used for cases
            that are not associated with the on-board filters.

Examples:

 * `xb 100 enable`

      Enables the XB-100 GPIO expansion board.

 * `xb 200 filter rx 144`

      Selects the 144-148 MHz receive filter on the XB-200 expansion board.


mimo
----

Usage: `mimo [master | slave]`

Modify device MIMO operation.


open
----

Usage: `open [device identifiers]`

Open the specified device for use with successive commands. Any previously
opened device will be closed.

The general form of the device identifier string is:

`<backend>:[device=<bus>:<addr>] [instance=<n>] [serial=<serial>]`

See the `bladerf_open()` documentation in libbladeRF for the complete
device specifier format.


peek
----

Usage: `peek <dac|lms|si> <address> [num_addresses]`

The peek command can read any of the devices hanging off the FPGA which
includes the LMS6002D transceiver, VCTCXO trim DAC or the Si5338 clock
generator chip.

If `num_addresses` is supplied, the address is incremented by 1 and
another peek is performed for that many addresses.

Valid Address Ranges:

     Device Address Range
----------- ----------------
`dac`       0 to 255
`lms`       0 to 127
`si`        0 to 255

Example:

 * `peek si ...`


poke
----

Usage: `poke <dac|lms|si> <address> <data>`

The poke command can write any of the devices hanging off the FPGA which
includes the LMS6002D transceiver, VCTCXO trim DAC or the Si5338 clock
generator chip.

Valid Address Ranges:

     Device Address Range
----------- ----------------
`dac`       0 to 255
`lms`       0 to 127
`si`        0 to 255

Example:

 * `poke lms ...`


print
-----

Usage: `print <param>`

The print command takes a parameter to print.  The parameter is one of:

----------------------------------------------------------------------
    Parameter Description
------------- --------------------------------------------------------
`bandwidth`   Bandwidth settings

`frequency`   Frequency settings

`gpio`        FX3 <-> FPGA GPIO state

`loopback`    Loopback settings

`lnagain`     Gain setting of the LNA, in dB

`rxvga1`      Gain setting of RXVGA1, in dB

`rxvga2`      Gain setting of RXVGA2, in dB

`txvga1`      Gain setting of TXVGA1, in dB

`txvga2`      Gain setting of TXVGA2, in dB

`sampling`    External or internal sampling mode

`samplerate`  Samplerate settings

`trimdac`     VCTCXO Trim DAC settings
----------------------------------------------------------------------


probe
-----

Usage: `probe [strict]`

Search for attached bladeRF device and print a list of results.

Without specifying `strict`, the lack of any available devices is not considered
an error.

When provided the optional `strict` argument, this command will treat the
situation where no devices are found as an error, causing scripts or
lists of commands provided via the `-e` command line argument to terminate
immediately.


quit
----

Usage: `quit`

Exit the CLI.


recover
-------

Usage: `recover [<bus> <address> <firmware file>]`

Load firmware onto a device running in bootloader mode, or list all devices
currently in bootloader mode.

With no arguments, this command lists the USB bus and address for FX3-based
devices running in bootloader mode.

When provided a bus, address, and path to a firmware file, the specified
device will be loaded with and begin executing the provided firmware.

In most cases, after successfully loading firmware into the device's RAM,
users should open the device with the "`open`" command, and write the
firmware to flash via "`load fx3 <firmware file>`"


run
---

Usage: `run <script>`

Run the provided script.


rx
--

Usage: `rx <start | stop | wait | config [param=val [param=val [...]]>`

Receive IQ samples and write them to the specified file. Reception is
controlled and configured by one of the following:

----------------------------------------------------------------------
    Command Description
----------- ----------------------------------------------------------
`start`     Start receiving samples

`stop`      Stop receiving samples

`wait`      Wait for sample transmission to complete, or until a
            specified amount of time elapses

`config`    Configure sample reception. If no parameters are
            provided, the current parameters are printed.
----------------------------------------------------------------------

Running `rx` without any additional commands is valid shorthand for
`rx config`.

The `wait` command takes an optional `timeout` parameter. This parameter
defaults to units of milliseconds (`ms`). The timeout unit may be specified
using the `ms` or `s` suffixes. If this parameter is not provided, the
command will wait until the reception completes or `Ctrl-C` is pressed.

Configuration parameters take the form `param=value`, and may be specified
in a single or multiple `rx config` invocations. Below is a list of
available parameters.

----------------------------------------------------------------------
      Parameter Description
--------------- ------------------------------------------------------
`n`             Number of samples to receive. 0 = inf.

`file`          Filename to write received samples to

`format`        Output file format. One of the following:

                `csv`: CSV of SC16 Q11 samples

                `bin`: Raw SC16 Q11 DAC samples

`samples`       Number of samples per buffer to use in the
                asynchronous stream.  Must be divisible by 1024 and
                >= 1024.

`buffers`       Number of sample buffers to use in the asynchronous
                stream. The min value is 4.

`xfers`         Number of simultaneous transfers to allow the
                asynchronous stream to use. This should be less than
                the `buffers` parameter.

`timeout`       Data stream timeout. With no suffix, the default
                unit is `ms`. The default value is 1000 ms (1 s).
                Valid suffixes are `ms` and `s`.
----------------------------------------------------------------------

Example:

 * `rx config file=/tmp/data.bin format=bin n=10K`

    Receive (10240 = 10 * 1024) samples, writing them to `/tmp/data.bin` in
    the binary DAC format.

Notes:

 * The `n`, `samples`, `buffers`, and `xfers` parameters support the
   suffixes `K`, `M`, and `G`, which are multiples of 1024.
 * An `rx stop` followed by an `rx start` will result in the samples
   file being truncated. If this is not desired, be sure to run
   `rx config` to set another file before restarting the rx stream.
 * For higher sample rates, it is advised that the `bin`ary output format be
   used, and the output file be written to RAM (e.g. `/tmp`, `/dev/shm`), if
   space allows. For larger captures at higher sample rates, consider using
   an SSD instead of a HDD.


tx
--

Usage: `tx <start | stop | wait | config [parameters]>`

Read IQ samples from the specified file and transmit them. Transmission is
controlled and configured by one of the following:

----------------------------------------------------------------------
    Command Description
----------- ----------------------------------------------------------
`start`     Start transmitting samples

`stop`      Stop transmitting samples

`wait`      Wait for sample transmission to complete, or until a
            specified amount of time elapses

`config`    Configure sample transmission. If no parameters are
            provided, the current parameters are printed.
----------------------------------------------------------------------

Running `tx` without any additional commands is valid shorthand for
`tx config`.

The `wait` command takes an optional `timeout` parameter. This parameter
defaults to units of milliseconds (`ms`). The timeout unit may be specified
using the `ms` or `s` suffixes. If this parameter is not provided, the
command will wait until the transmission completes or `Ctrl-C` is pressed.

Configuration parameters take the form `param=value`, and may be specified
in a single or multiple `tx config` invocations. Below is a list of
available parameters.

----------------------------------------------------------------------
      Parameter Description
--------------- ------------------------------------------------------
`file`          Filename to read samples from

`format`        Input file format. One of the following:

                `csv`: CSV of SC16 Q11 samples ([-2048, 2047])

                `bin`: Raw SC16 Q11 DAC samples ([-2048, 2047])

`repeat`        The number of times the file contents should be
                transmitted. 0 implies repeat until stopped.

`delay`         The number of microseconds to delay between
                retransmitting file contents. 0 implies no delay.

`samples`       Number of samples per buffer to use in the
                asynchronous stream. Must be divisible by 1024 and
                >= 1024.

`buffers`       Number of sample buffers to use in the asynchronous
                stream. The min value is 4.

`xfers`         Number of simultaneous transfers to allow the
                asynchronous stream to use. This should be < the
                `buffers` parameter.

`timeout`       Data stream timeout. With no suffix, the default
                unit is ms. The default value is 1000 ms (1 s).
                Valid suffixes are 'ms' and 's'.
----------------------------------------------------------------------

Example:

 * `tx config file=data.bin format=bin repeat=2 delay=250000`

    Transmitting the contents of `data.bin` two times, with a ~250ms delay
    between transmissions.

Notes:

 * The `n`, `samples`, `buffers`, and `xfers` parameters support the
   suffixes `K`, `M`, and `G`, which are multiples of 1024.
 * For higher sample rates, it is advised that the input file be
   stored in RAM (e.g. `/tmp`, `/dev/shm`) or on an SSD, rather than a
   HDD.
 * When providing CSV data, this command will first convert it to a
   binary format, stored in a file in the current working directory.
   During this process, out-of-range values will be clamped.
 * When using a binary format, the user is responsible for ensuring
   that the provided data values are within the allowed range. This
   prerequisite alleviates the need for this program to perform range
   checks in time-sensitive callbacks.


set
---

Usage: `set <param> <arguments>`

The set command takes a parameter and an arbitrary number of arguments for
that particular command.  The parameter is one of:

----------------------------------------------------------------------
    Parameter Description
------------- --------------------------------------------------------
`bandwidth`   Bandwidth settings

`frequency`   Frequency settings

`gpio`        FX3 <-> FPGA GPIO state

`loopback`    Loopback settings. Run 'set loopback' for available modes.

`lnagain`     Gain setting of the LNA, in dB. Valid values: 0, 3, 6

`rxvga1`      Gain setting of RXVGA1, in dB. Range: [5, 30]

`rxvga2`      Gain setting of RXVGA2, in dB. Range: [0, 30]

`txvga1`      Gain setting of TXVGA1, in dB. Range: [-35, -4]

`txvga2`      Gain setting of TXVGA2, in dB. Range: [0, 25]

`sampling`    External or internal sampling mode

`samplerate`  Samplerate settings

`trimdac`     VCTCXO Trim DAC settings
----------------------------------------------------------------------


version
-------

Usage: `version`

Prints version information for host software and the current device.
