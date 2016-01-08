# MATLAB & Simulink Bindings #

This directory provides libbladeRF bindings for [MATLAB](http://www.mathworks.com/products/matlab/)
and [Simulink](http://www.mathworks.com/products/simulink/).

These bindings are implemented as a thin layer of MATLAB code that utilizes
[loadlibrary](http://www.mathworks.com/help/matlab/ref/loadlibrary.html) to load
libbladeRF into the MATLAB process and [calllib](http://www.mathworks.com/help/matlab/ref/calllib.html) 
to execute libbladeRF functions.

The various device control and configuration parameters are mapped to accessors methods,
which yields a simple and intuitive interface. For example, to open a device and configure
its RX and TX frequencies:

```
device = bladeRF();
device.rx.frequency = 915.125e6;
device.tx.frequency = 921.700e6;
```

Before starting, run `help bladeRF.build_thunk` to view instructions on how to
have MATLAB build a Thunk file to use in conjunction with libbladeRF.

For more information, use the MATLAB `help` and `doc` commands on the files in this
directory, as well as on the various object properties contained in these files.

# Files #

| Filename                 | Description                                                                                                                                                          |
| ------------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `bladeRF.m`              | Top-level object for MATLAB libbladeRF bindings. See `doc bladeRF` for information about the available properties and methods.                                       |
| `bladeRF_Simulink.m`     | MATLAB System Object implementation for Simulink. This exposes `bladeRF.m` to Simulink, allowing the device to be used to receive, transmit, or operate full duplex. |
| `bladeRF_rx_gui.m`       | A configurable demo GUI that receives and plots samples in real-time.                                                                                                |
| `bladeRF_rx_gui.fig`     | GUI layout for `bladeRF_rx_gui.m`                                                                                                                                    |
| `bladeRF_XCVR.m`         | A submodule of `bladeRF.m` that provides parameter-based access to RX and TX module configuration items.                                                             |
| `bladeRF_VCTCXO.m`       | A submodule of `bladeRF.m` that provides access to the VCTCXO trim DAC settings.                                                                                     |
| `bladeRF_IQCorr.m`       | A submodule of `bladeRF.m` that provides access to IQ DC offset and gain imbalance correction functionality.                                                         |
| `bladeRF_StreamConfig.m` | An object that encapsulates bladeRF stream parameters.                                                                                                               |
| `libbladeRF_proto.m`     | This file establishes the bindings and mappings to libbladeRF.                                                                                                       |

# Supported Versions #

These bindings have been tested on MATLAB 2014b and 2015b. Earlier versions may be compatible, but this is not guaranteed.

# License #

This code is distributed under an [MIT License](../../../../legal/licenses/LICENSE.MIT.nuand).

# Trademarks #

MATLAB and Simulink are registered trademarks of [The MathWorks, Inc.](http://www.mathworks.com/)
