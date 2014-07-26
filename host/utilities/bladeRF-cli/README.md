# bladeRF Command Line Interface #
This tool is intended to aid in development and testing.  bladeRF-cli can be used from the command line for simple operations, such as loading the bladeRF FPGA.  However, a majority of this tool's functionality is accessible when run in its interactive mode.

## Dependencies ##
- [libbladeRF][libbladeRF]
- [help2man][help2man] (optional)
- [libtecla][libtecla] (optional)
- [pandoc][pandoc] (optional)

[libbladeRF]: ../../libraries/libbladeRF (libbladeRF)
[help2man]: https://www.gnu.org/software/help2man/ (help2man)
[libtecla]: http://www.astro.caltech.edu/~mcs/tecla/ (libtecla)
[pandoc]: http://johnmacfarlane.net/pandoc/ (pandoc)

## Build Variables ##

Below is a list of useful and project-specific CMake options. Please see the CMake [variable list] in CMake's documentation for
more information.

| Option                                        | Description
| --------------------------------------------- |:---------------------------------------------------------------------------------------------------------|
| -DENABLE_LIBTECLA=\<ON/OFF\>                  | Enable libtecla support in the bladeRF-cli program. Default: ON if libtecla is detected, OFF otherwise.  |
| -DBUILD_BLADERF_CLI_DOCUMENTATION=\<ON/OFF\>  | Builds a man page for the bladeRF-cli utility, using help2man.  Default: equal to BUILD_DOCUMENTATION    |

When pandoc is installed, the documentation for each bladeRF-cli command will be re-generated for the interactive 'help' command and the man page.

## Basic Usage ##
For usage information, run:

```
./bladeRF-cli --help
```

If you encounter issues and ask folks on IRC or in the forum for assistance, it's always helpful to note the CLI and library versions:

```
bladeRF-cli --version
bladeRF-cli --lib-version
```

If only one device is connected, the -d option is not needed. The CLI will find and open the attached device. This option is required if multiple devices are connected. For the sake of completeness, the following examples will include this command line option, using the libusb backend.

Load the FPGA and enter interactive mode:

```
./bladeRF-cli -d "libusb: instance=0" -l /path/to/fpga.rbf -i
```

Pass in a script to setup a device in a specific way, but do not enter interactive mode. (See [Creating a setup.txt](#creating-a-setuptxt) for more information about scripts.)

```
./bladeRF-cli -d "libusb: instance=0" -s setup.txt
```

## Some Useful Interactive Commands ##
The `help` command prints out the top level commands that are available. Using `help <cmd>` gives a more detailed help on that command.

The most useful control commands are `peek` and `poke` for setting registers at a very basic level and `print` and `set` for performing system level tasks like setting gains, frequency, bandwidth, or sample rate.

The `rx` and `tx` commands allow data to be transmitted and received in the background, while the interactive console remains in the foreground.

`rx config` and `tx config` are used to configure and view various RX and TX parameters. Note that `rx` and `tx` is a supported shorthand for viewing the RX/TX configuration. See `help rx` and `help tx` for more information about these parameters. Below are a few examples:

Receive 10,000 samples to a binary file (in the libbladeRF SC16Q11 format):
```
rx config file=samples.bin format=bin n=10000
rx start
```

`rx` or `rx config` may be used to view the RX task status. When it no longer reports "Running", the samples should be available in the file. Note that for larger sample rates or small values of `n`, the RX task may finish before you can enter the `rx` command to view its state.


Receive (4 * 1024 * 1024) samples to a CSV file:
```
rx config file=samples.csv format=csv n=4M
rx start
```

Files can also be transmitted:

```
tx config file=samples.bin
tx start
```

The transmission of the file can be repeated a number of times, with a configurable delay (microseconds) between each repetition:

```
tx config file=samples.bin repeat=100 delay=10000
tx start
```

While `format=csv` is valid for `tx`, note that the CLI will first convert the CSV to a temporary binary file in the current working directory, named "bladeRF_samples_from_csv.bin", as shown below:

```
bladeRF> tx config file=samples.csv format=csv repeat=10 delay=10000000
bladeRF> tx start
    Converted CSV to SC16 Q11 file and switched to converted file.

bladeRF> tx

    State: Idle
    Last error: None
    File: bladeRF_samples_from_csv.bin
    File format: SC16 Q11, Binary
    Repetitions: 1
    Repetition delay: none
    # Buffers: 32
    # Samples per buffer: 32768
    # Transfers: 16
    Timeout (ms): 1000
```

## Running Scripts ##
It is often useful in debugging to quickly put the device into a known state.  The bladeRF-cli supports script files to alleviate the need to write a small program or manually enter a number of commands.

Below is a sample script file which can be provided via the -s command line option. Scripts may also be run from within interactive mode, via the `run` command. Note that the syntax from the interactive mode is used.

```
set frequency 1000000000
set samplerate 10000000
set bandwidth 28000000

set lnagain bypass
set txvga1 -14
set txvga2 20
set rxvga1 20
set rxvga2 20

print frequency
print samplerate
print bandwidth
```

When transmitting or receiving samples via scripts, be sure to use the `rx wait` or `tx wait` to ensure ongoing reception/transmission completes prior to exiting the script.
