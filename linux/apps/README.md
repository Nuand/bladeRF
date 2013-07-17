# bladeRF Linux Applications #
This directory contains applications that utilize libbladeRF.  Currently, this consists of a command-line "bladerf" tool intended to aid in development and testing.  This tool can be used in a batch mode with single, simple operations but the real power is when it is compiled with an interactive shell.

## Dependencies ##
- [libbladeRF][lib]
- [libtecla][tecla] (optional)

[libtecla][tecla] can be installed via most package managers.  Check your distribution for support.

[lib]: ../lib (libbladeRF)
[tecla]: http://www.astro.caltech.edu/~mcs/tecla/ (libtecla)

## Building ##
1. `make`
1. Notice the binary `bin/bladeRF`

The default with the build is to want to be interactive.  To disable this:

```
make INTERACTIVE=n
```

To enable debug symbols in the resulting binary:

```
make DEBUG=y
```

## Usage ##
Print the help:

```
./bladeRF --help
```

Load the FPGA and enter interactive mode:

```
./bladeRF -d /dev/bladerf0 -l /path/to/fpga.rbf
```

Pass in a script to run to setup a device in a specific way:

```
./bladeRF -d /dev/bladerf0 < setup.txt
```

## Some Useful Interactive Commands ##
The `help` command prints out the top level commands that are available.  The most useful commands are `peek` and `poke` for setting registers at a very basic level and `print` and `set` for performing system level tasks like setting gains, frequency, bandwidth or samplerate.

Using `help <cmd>` gives a more detailed help on that command.

`print gpio` will show what the current state of the LMS reset, enables, and the band selection.

`set gpio 0x57` is a good value to use to:

- Bring the LMS6002D out of reset
- Enable LMS TX
- Enable LMS RX

To make the LMS more functional, the registers must be set internally to enable operation, so `poke lms 5 0x3e` will enable all the appropriate subsystems.

## Creating a setup.txt ##
It was useful in debugging to get into a known state easily and reproducibly.  We created a `setup.txt` file which we would feed into the `stdin` of the command to get here.

Our `setup.txt` looked like this:

```
set gpio 0x57

poke lms 5 0x3E
poke lms 0x59 0x29  # Per Lime Micro FAQ, improves ADC performance
poke lms 0x64 0x36  # Per Lime Micro FAQ, set ADC common mode voltage

set frequency 1000000000
set samplerate 10000000
set bandwidth 28000000

set lnagain bypass
set txvga1 -14
set txvga2 20
set rxvga1 20
set rxvga2 20

print frequency
print bandwidth
print gpio
```
