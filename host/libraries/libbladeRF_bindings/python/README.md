# Python 3 Bindings #

This package provides libbladeRF bindings for Python 3, using the CFFI
interface.

# Installation #

- To install system-wide: `python3 -m pip install .`
- To install for development: `python3 -m pip install --editable .`
- To include NumPy for the sample conversion examples:
  `python3 -m pip install '.[numpy]'`

On Windows, match the Python architecture to the installed bladeRF package
architecture and make the directory containing `bladeRF.dll` visible to the DLL
loader before importing this module. The official shared library is named
`bladeRF.dll`; errors such as `error 0x7e` when that file exists usually mean a
dependent DLL, such as `libusb-1.0.dll` or a Microsoft Visual C++ runtime, is
missing from the loader path. For Python 3.8 and newer, either add the install
directory to `PATH` before launching Python or call `os.add_dll_directory()`
before `import bladerf`.

Set `BLADERF_LIBRARY` to the full path of the libbladeRF shared library when it
is installed outside the native loader's search path. Import failures report
the attempted library name or path and retain the native loader error.

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

# Synchronous buffer sizing #

The total interleaved sample count is supplied as `num_samples` to `sync_rx()`
and `sync_tx()`. For an X2 stream, half of those samples belong to each
channel. Allocate buffers using:

```
buffer_bytes = num_samples * bytes_per_sample
```

SC16 Q11 and packed SC16 Q11 use 4 bytes per caller sample, while SC8 Q7 uses
2 bytes. Packed mode compresses only the USB transport; libbladeRF unpacks
received samples into SC16 Q11 and accepts SC16 Q11 for transmission. The
binding rejects undersized buffers before calling libbladeRF and requires
receive buffers to be writable.

# Synchronous streaming #

The following example receives 4096 SC16 Q11 samples without exposing CFFI
objects:

```
import numpy
import bladerf

num_samples = 4096
samples = bladerf.allocate_buffer(num_samples, bladerf.Format.SC16_Q11)

with bladerf.BladeRF() as device:
    rx = device.Channel(bladerf.CHANNEL_RX(0))
    device.sync_config(
        bladerf.ChannelLayout.RX_X1,
        bladerf.Format.SC16_Q11,
        16,
        8192,
        8,
        3500,
    )
    rx.enable = True
    try:
        device.sync_rx(samples, num_samples, 3500)
    finally:
        rx.enable = False

raw = numpy.frombuffer(samples, dtype="<i2").reshape(-1, 2)
signal = (raw[:, 0] + 1j * raw[:, 1]) / 2048.0
```

For X2 reception, allocate twice the per-channel sample count and reshape the
SC16 buffer with `reshape(-1, 4)`. Each row is ordered RX1 I, RX1 Q, RX2 I,
RX2 Q.

Transmission uses the same caller representation. Convert complex samples into
interleaved signed 16-bit I/Q values, configure `TX_X1` or `TX_X2`, enable the
intended TX channels, and pass the buffer to `sync_tx()`. Enabling a TX channel
radiates unless the RF path is safely terminated or connected to suitable test
equipment.

# Development #

Implement each behavior by first adding a focused failing regression. Run the
focused test, the complete Python suite, and the two-device hardware suite
before committing. Keep callback-based asynchronous streaming, legacy
bladeRF1 controls, raw register access, and destructive flash or OTP helpers
outside this primary-use milestone.

Run the Python regression suite from this directory:

```
python3 -m unittest discover -s tests -v
```

Hardware regressions are opt-in. Provide a clock source as the master and its
external-clock consumer as the slave:

```
export BLADERF_TEST_MASTER_SERIAL=<clock-source-serial>
export BLADERF_TEST_SLAVE_SERIAL=<external-clock-serial>
python3 tests/test_hardware.py
```

The suite restores device and clock state. It checks packed RX with its
unpacked caller representation and verifies RX X2 continuity with the FPGA's
deterministic 12-bit counter. It also exercises the TX USB and CFFI path while
leaving the TX RF channel disabled.

The external trigger regression additionally requires `mini_exp[1]`
(J51-1 on bladeRF 2.0 micro) and ground to be connected between the devices.
Enable it only after both that trigger path and the shared SMB clock are
connected:

```
export BLADERF_TEST_TRIGGER_CROSSOVER=1
python3 tests/test_hardware.py
```

`bladerf/_cdef.py` is generated from the current `libbladeRF.h`. The regression
suite regenerates it and requires byte-for-byte equality, so an API change
cannot silently leave the Python CFFI declarations behind.

# License #

This code is distributed under an
[MIT License](https://github.com/Nuand/bladeRF/blob/master/legal/licenses/LICENSE.MIT.nuand).
