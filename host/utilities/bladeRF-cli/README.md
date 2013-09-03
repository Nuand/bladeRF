# bladeRF-cli: bladeRF Command Line Interface #
This tool is intended to aid in development and testing.  bladeRF-cli can be used from the command line for simple operations, such as loading the bladeRF FPGA.  However, a majority of this tool's functionality is accessible when run in its interactive mode.

## Dependencies ##
- [libbladeRF][libbladeRF]
- [libtecla][libtecla] (optional)

[libbladeRF]: ../../libraries/libbladeRF (libbladeRF)
[libtecla]: http://www.astro.caltech.edu/~mcs/tecla/ (libtecla)


The bladeRF-cli build should detect whether libtecla is installed, and configure the ENABLE_LIBTECLA variable accordingly. You can override this when configuring the build with CMake via a ```-DENABLE_LIBTECLA=OFF``` argument.


## Basic Usage ##
For usage information, run:

```
./bladeRF-cli --help
```

If you encounter issues and ask folks on IRC or in the forum assistance, it's always helpful to note the CLI and library versions:

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

The most useful control commands are `peek` and `poke` for setting registers at a very basic level and `print` and `set` for performing system level tasks like setting gains, frequency, bandwidth or sample rate.

The `rx` and `tx` commands allow data to be transmitted and received in the background, while the interactive console remains in the foreground.

`rx config` and `tx config` are used to configure and view various RX and TX parameters. Note that `rx` and `tx` is a supported shorthand for viewing the RX/TX configuration. See `help rx` and `help tx` for more information about these parameters. Below are a few examples:

Receive 10,000 samples to a binary file (host endianness):
```
rx config file=samples.bin format=bin n=10000
rx start
```

`rx` or `rx config` may be used to view the RX task status. When it no longer reports "Running", the samples should be available in the file. Note that for larger sample rate or small values of `n`, the RX task may finish before you can enter the `rx` command to view its state.


Receive 4096 samples to a CSV file:
```
rx config file=samples.csv format=csv n=4096
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

While `format=csv` is valid for `tx`, note that the CLI will first convert the CSV to a temporary binary file:

```
bladeRF> tx config file=samples.txt format=csv repeat=10 delay=10000000
bladeRF> tx start

Converted CSV file to binary file. Using /tmp/bladeRF-ddLNmc
Note that this program will not delete the temporary file.

bladeRF> tx

    State: Running
    File: /tmp/bladeRF-ddLNmc
    Format: C16, Binary (Little Endian)
    Repeat: 10 iterations
    Repeat delay: 10000000 us
```

## Creating a setup.txt ##
It is often useful in debugging to quickly get the device into a known state.  The bladeRF-cli supports script files to alleviate the need to write a small program or manually enter a number of commands. (Running scripts from within scripts or the interactive mode is comming soon!)

Below is a sample `setup.txt` file which can be provided via the -s command line option. Note that the syntax from the interactive mode is used, _#_ may be used to comment out the remainder of a line.


```
set frequency 1000000000  # Tune to 1 GHz on both RX and TX
set samplerate 10000000   # Sample @ 10 MHz
set bandwidth 28000000    # 28 MHz bandwidth

# Configure gains
set lnagain bypass
set txvga1 -14
set txvga2 20
set rxvga1 20
set rxvga2 20

# Print out a few settings

print frequency
print samplerate
print bandwidth
```
