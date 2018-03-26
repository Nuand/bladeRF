# Python 3 Bindings #

This package provides libbladeRF bindings for Python 3, using the CFFI
interface.

# Installation #

- To install system-wide: `sudo python3 setup.py install`
- To install for your user: `python3 setup.py install`

# Usage: Python module #

A Python module is provided. To use, `import bladerf` and then instantiate the
`bladerf.BladeRF` class to access your board.

An example of opening the first available device and changing the frequency of
the first RX channel:

```
>>> import bladerf
>>> d = bladerf.BladeRF()
>>> d
<BladeRF(<DevInfo(libusb:device=6:53 instance=0 serial=...)>)>
>>> ch = d.Channel(bladerf.CHANNEL_RX(0))
>>> ch
<Channel(<BladeRF(<DevInfo(libusb:device=6:53 instance=0 serial=...)>)>,CHANNEL_RX(0))>
>>> ch.frequency
2484000000
>>> ch.frequency = 1.0e9
>>> ch.frequency
1000000000
```

# Usage: bladerf-tool #

A command-line interface named `bladerf-tool` is provided. For usage
instructions, type `bladerf-tool --help`.

Example usage:

```
$ bladerf-tool info
*** Devices found: 1

*** Device 0
Board Name        bladerf1
Device Speed      Super
FPGA Size         40
FPGA Configured   True
FPGA Version      v0.6.0 ("0.6.0")
Firmware Version  v2.1.0 ("2.1.0")
RX Channel Count  1
  Channel RX1:
    Gain          39
    Gain Mode     Manual
    Symbol RSSI   None
    Frequency     1000000000
    Bandwidth     28000000
    Sample Rate   1000000
TX Channel Count  1
  Channel TX1:
    Gain          -14
    Gain Mode     Manual
    Frequency     2446999999
    Bandwidth     28000000
    Sample Rate   1000000
```

# License #

This code is distributed under an
[MIT License](https://github.com/Nuand/bladeRF/blob/master/legal/licenses/LICENSE.MIT.nuand).
