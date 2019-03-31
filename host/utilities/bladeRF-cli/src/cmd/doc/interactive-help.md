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
     * `calibrate dc <rxtx>`

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
    10 MHz steps.

 * Generate RX or TX I/Q DC correction parameter tables for AGC Look Up Table

     * `calibrate table agc <rx|tx> [<f_min> <f_max> [f_inc]]`

    Similar usage as `calibrate table dc` except the call will set gains to
    the AGC's base gain value before running `calibrate table dc`.


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

    `fpgaA4`    Metadata and bitstream for 49 kLE (A4) FPGA

    `fpgaA9`    Metadata and bitstream for 301 kLE (A9) FPGA
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

    `fpgaA4`    Metadata and bitstream for 49 kLE (A4) FPGA

    `fpgaA9`    Metadata and bitstream for 301 kLE (A9) FPGA

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


fw_log
------

Usage: `fw_log` [filename]

Read the contents of the device's firmware log and write it to the
specified file. If no filename is specified, the log content is written
to stdout.


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

Clear out a FW signature word in flash and jump to FX3 bootloader.

The device will continue to boot into the FX3 bootloader across power cycles
until new firmware is written to the device.


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

      XB-100 GPIO expansion board

  - `200`

      XB-200 LF/MF/HF/VHF transverter expansion board

  - `300`

      XB-300 amplifier board


Common subcommands:

  - `enable`

      Enable the XB-100, XB-200, or XB-300 expansion board.

XB-200 subcommands:

  - `filter [rx|tx] [50|144|222|custom|auto_1db|auto_3db]`

      Selects the specified RX or TX filter on the XB-200 board. Below
      are descriptions of each of the filter options.

      * 50

            Select the 50-54 MHz (6 meter band) filter.

      * 144

            Select the 144-148 MHz (2 meter band) filter.

      * 222

            Select the 222-225 MHz (1.25 meter band) filter. Realistically,
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

XB-300 subcommands:

  - `<pa|lna|aux> [on|off]`

    Enable or disable the power amplifier (PA), low-noise amplifier (lna) or
    auxillary LNA (aux). The current state if the specified device is printed
    if [on|off] is not specified.

    Note: The auxillary path on the XB-300 is not populated with components by
          default; the `aux` control will have no effect upon the RX signal.
          This option is available for users to modify their board with custom
          hardware.

  - `<pwr>`

    Read the current Power Detect (PDET) voltage and compute the output power.

  - `trx <rx|tx>`

    The default XB-300 hardware configuration includes separate RX and TX paths.
    However, users wishing to use only a single antenna for TRX can do so via a
    modification to resistor population options on the XB-300 and use this command
    to switch between RX an TX operation. (See R8, R10, and R23 on the schematic.)

Examples:

 * `xb 200 enable`

      Enables and configures the XB-200 transverter expansion board.

 * `xb 200 filter rx 144`

      Selects the 144-148 MHz receive filter on the XB-200 transverter
      expansion board.

 * `xb 300 enable`

      Enables and configures the use of GPIOs to interact with the XB-300. The
      PA and LNA will disabled by default.

 * `xb 300 lna on`

      Enables the RX LNA on the XB-300. LED D1 (green) is illuminated when the
      LNA is enabled, and off when it is disabled.

 *  `xb 300 pa off`

      Disables the TX PA on the XB-300. LED D2 (blue) is illuminated when the
      PA is enabled, and off when it is disabled.

mimo
----

Usage: `mimo [master | slave]`

Modify device MIMO operation.

IMPORTANT: This command is deprecated and has been superseded by
           `"print/set smb_mode"`. For usage text, run: "`set smb_mode`"


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

Usage: `peek <rfic|pll|dac|lms|si> <address> [num_addresses]`

The peek command can read any of the devices hanging off the FPGA. This
includes the:

 * bladeRF 1: LMS6002D transceiver, VCTCXO trim DAC, Si5338 clock generator
 * bladeRF 2: AD9361 transceiver, VCTCXO trim DAC, ADF4002 frequency synthesizer

If `num_addresses` is supplied, the address is incremented by 1 and
another peek is performed for that many addresses.

Valid Address Ranges:

     Device Address Range
----------- -----------------
`rfic`      0 to 0x3F7 (1015)
`pll`       0 to 3
`dac`       0 to 255
`lms`       0 to 127
`si`        0 to 255

Example:

 * `peek si ...`


poke
----

Usage: `poke <rfic|pll|dac|lms|si> <address> <data>`

The poke command can write any of the devices hanging off the FPGA. This
includes the:

 * bladeRF 1: LMS6002D transceiver, VCTCXO trim DAC, Si5338 clock generator
 * bladeRF 2: AD9361 transceiver, VCTCXO trim DAC, ADF4002 frequency synthesizer

Valid Address Ranges:

     Device Address Range
----------- -----------------
`rfic`      0 to 0x3F7 (1015)
`pll`       0 to 3
`dac`       0 to 255
`lms`       0 to 127
`si`        0 to 255

Example:

 * `poke lms ...`


print
-----

Usage: `print [parameter]`

The `print` command takes a parameter to print. Available parameters are listed
below. If no parameter is specified, all parameters are printed.

Common parameters:

----------------------------------------------------------------------
    Parameter Description
------------- --------------------------------------------------------
`bandwidth`     Bandwidth settings

`frequency`     Frequency settings

`agc`           Automatic gain control

`loopback`      Loopback settings

`rx_mux`        FPGA RX FIFO input mux setting

`gain`          Gain settings

`samplerate`    Samplerate settings

`trimdac`       VCTCXO Trim DAC settings

`tuning_mode`   Tuning mode settings

`hardware`      Low-level hardware status
----------------------------------------------------------------------

BladeRF1-only parameters:

----------------------------------------------------------------------
    Parameter Description
------------- --------------------------------------------------------
`gpio`          FX3 <-> FPGA GPIO state

`lnagain`       RX LNA gain, in dB (deprecated)

`rxvga1`        RXVGA1 gain, in dB (deprecated)

`rxvga2`        RXVGA2 gain, in dB (deprecated)

`txvga1`        TXVGA1 gain, in dB (deprecated)

`txvga2`        TXVGA2 gain, in dB (deprecated)

`sampling`      External or internal sampling mode

`smb_mode`      SMB clock port mode of operation

`vctcxo_tamer`  Current VCTCXO tamer mode

`xb_gpio`       Expansion board GPIO values

`xb_gpio_dir`   Expansion board GPIO direction (1=output, 0=input)
----------------------------------------------------------------------

BladeRF2-only parameters:

----------------------------------------------------------------------
    Parameter Description
------------- --------------------------------------------------------
`clock_sel`     System clock selection

`clock_out`     Clock output selection

`rssi`          Received signal strength indication

`clock_ref`     ADF4002 chip status

`refin_freq`    ADF4002 reference clock frequency

`biastee`       Current bias-tee status

`filter`        RFIC FIR filter selection
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

`channel`       Comma-delimited list of physical RF channels to use
----------------------------------------------------------------------

Example:

 * `rx config file=/tmp/data.bin format=bin n=10K`

    Receive (10240 = 10 * 1024) samples, writing them to `/tmp/data.bin` in
    the binary DAC format.

 * `rx config file=mimo.csv format=csv n=32768 channel=1,2`

    Receive 32768 samples from RX1 and RX2, outputting them to a file named
    `mimo.csv`, with four columns (RX1 I, RX1 Q, RX2 I, RX2 Q).

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
 * The CSV format produces two columns per channel, with the first two columns
   corresponding to the I,Q pair for the first channel configured with the
   `channel` parameter; the next two columns corresponding to the I,Q of the
   second channel, and so on.


trigger
-------

Usage: `trigger [<trigger> <tx | rx> [<off slave master fire>]]`

If used without parameters, this command prints the state of all triggers.
When <trigger> and <tx | rx> and supplied, the specified trigger is printed.

Below are the available options for <trigger>:

----------------------------------------------------------------------
     Trigger Description
------------ ---------------------------------------------------------
`J71-4`      Trigger signal is on `mini_exp1` (bladeRF x40/x115, J71, pin 4).

`J51-1`      Trigger signal is on `mini_exp1` (bladeRF xA4/xA9, J51, pin 1).

`Miniexp-1`  Trigger signal is on `mini_exp1`, hardware-independent

----------- ----------------------------------------------------------

Note that all three of the above options map to the same logical port on
all devices (`mini_exp[1]`). Multiple options are provided for reverse
compatibility and clarity.

The trigger is controlled and configured by providing the last argument,
which may be one of the following:

----------------------------------------------------------------------
    Command Description
----------- ----------------------------------------------------------
`off`       Clears fire request and disables trigger functionality.

`slave`     Configures trigger as slave, clears fire request, and
            arms the device.

`master`    Configures trigger as master, clears fire request, and
            arms the device.

`fire`      Sets fire request. Only applicable to the master.

----------------------------------------------------------------------

A trigger chain consists of a single or multiple bladeRF units and
may contain TX and RX modules. If multiple bladeRF units are used
they need to be connected via the signal specified by <trigger> and
a common ground.

The following sequence of commands should be used to ensure proper
synchronization. It is assumed that all triggers are off initially.

1.   Configure designated trigger master

     __IMPORTANT__

        Never configure two devices as trigger masters on a single chain.
        Contention on the same signal could damage the devices.

2.   Configure all other devices as trigger slaves

3.   Configure and start transmit or receive streams.

        The operation will stall until the triggers fire. As such, sufficiently
        large timeouts should be used to allow the trigger signal to be emitted
        by the master and received by the slaves prior to libbladeRF returning
        BLADERF_ERR_TIMEOUT.

4.   Set fire-request on master trigger

        All devices will synchronously start transmitting or receiving data.

5.   Finish the transmit and receive tasks as usual

6.   Re-configure the master and slaves to clear fire requests and re-arm.

        Steps 1 through 5 may be repeated as neccessary.

7.   Disable triggering on all slaves

8.   Disable triggering on master

Notes:

 * Synchronizing transmitters and receivers on a single chain will cause an
   offset of 11 samples between TX and RX; these samples should be discarded.
   This is caused by different processing pipeline lengths of TX and RX. This
   value might change if the FPGA code is updated in the future.


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

`channel`       Comma-delimited list of physical RF channels to use
----------------------------------------------------------------------

Example:

 * `tx config file=data.bin format=bin repeat=2 delay=250000`

    Transmitting the contents of `data.bin` two times, with a ~250ms delay
    between transmissions.

 * `tx config file=mimo.csv format=csv repeat=0 channel=1,2`

    Transmitting the contents of `mimo.csv` repeatedly, with the first channel
    in the file mapped to channel TX1 and the second channel mapped to TX2.

Notes:

 * The `n`, `samples`, `buffers`, and `xfers` parameters support the
   suffixes `K`, `M`, and `G`, which are multiples of 1024.
 * For higher sample rates, it is advised that the input file be
   stored in RAM (e.g. `/tmp`, `/dev/shm`) or on an SSD, rather than a
   HDD.
 * The CSV format expects two columns per channel, with the first two columns
   corresponding to the I,Q pair for the first channel configured with the
   `channel` parameter; the next two columns corresponding to the I,Q of the
   second channel, and so on.  For example, in the mimo.csv example above,
   `-128,128,-256,256` would transmit (-128,128) on TX1 and (-256,256) on TX2.
 * When providing CSV data, this command will first convert it to a
   binary format, stored in a file in the current working directory.
   During this process, out-of-range values will be clamped.
 * When using a binary format, the user is responsible for ensuring
   that the provided data values are within the allowed range. This
   prerequisite alleviates the need for this program to perform range
   checks in time-sensitive callbacks.


set
---

Usage: `set <parameter> <arguments>`

The `set` command takes a parameter and an arbitrary number of arguments for
that particular parameter. In general, `set <parameter>` will display
more help for that parameter.

Common parameters:

----------------------------------------------------------------------
    Parameter Description
------------- --------------------------------------------------------
`bandwidth`     Bandwidth settings

`frequency`     Frequency settings

`agc`           Automatic gain control

`loopback`      Loopback settings

`rx_mux`        FPGA RX FIFO input mux mode

`gain`          Gain settings

`samplerate`    Samplerate settings

`trimdac`       VCTCXO Trim DAC settings

`tuning_mode`   Tuning mode settings
----------------------------------------------------------------------

BladeRF1-only parameters:

----------------------------------------------------------------------
    Parameter Description
------------- --------------------------------------------------------
`gpio`          FX3 <-> FPGA GPIO state

`lnagain`       RX LNA gain, in dB. Values: 0, 3, 6
                (deprecated)

`rxvga1`        RXVGA1 gain, in dB. Range: [5, 30]
                (deprecated)

`rxvga2`        RXVGA2 gain, in dB. Range: [0, 30]
                (deprecated)

`txvga1`        TXVGA1 gain, in dB. Range: [-35, -4]
                (deprecated)

`txvga2`        TXVGA2 gain, in dB. Range: [0, 25]
                (deprecated)

`sampling`      External or internal sampling mode

`smb_mode`      SMB clock port mode of operation

`vctcxo_tamer`  VCTCXO tamer mode. Options: Disabled, 1PPS, 10MHz

`xb_gpio`       Expansion board GPIO values

`xb_gpio_dir`   Expansion board GPIO direction (1=output, 0=input)
----------------------------------------------------------------------

BladeRF2-only parameters:

----------------------------------------------------------------------
    Parameter Description
------------- --------------------------------------------------------
`clock_sel`     System clock selection

`clock_out`     Clock output selection

`rssi`          Received signal strength indication

`clock_ref`     Enables (1) or disables (0) the ADF4002 chip

`refin_freq`    ADF4002 reference clock frequency

`biastee`       Enables or disables the bias tee on a given channel

`filter`        RFIC FIR filter selection
----------------------------------------------------------------------

version
-------

Usage: `version`

Prints version information for host software and the current device.
