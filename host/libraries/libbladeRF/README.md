# bladeRF Library (libbladeRF) #
This is the source for the library which interacts with the bladeRF device. Our goal is to make this library compatible across Linux, Windows, OSX, and the NIOS core on the FPGA. The last item will allow the same library code to be run in a headless (without a host) mode of operation.

Functionality provided by this library includes:

- Opening and closing devices
- Tuning to various frequencies
- Configuring the sample rate
- Configuring the bandwidth and gain of the RF chain
- Transmitting and receiving complex baseband samples

The library is meant to be used as a simple and intuitive way to think about not just the bladeRF device itself, but most any radio hardware.  Fundamental operations such as setting the tuning frequency, manipulating switches on the board for different paths, and changing the different transmit and receive gains are all parts of a radio system which need to be exposed at a library level.

For more information, please generate and view the doxygen documentation. From your host/build directory, run:
```
cmake -DBUILD_DOCUMENTATION=Yes ../
make doc
```

The HTML documentation will be placed in **\<build dir\>/libraries/libbladerf/doc/doxygen/html**.  The **index.html** file is the "main" documentation page.

