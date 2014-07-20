# bladeRF Library (libbladeRF) #
This is the source for the library which interacts with the bladeRF device.

It is meant to be used as a simple and intuitive way to think about not just the bladeRF device itself, but most any radio hardware.  Fundamental operations such as setting the tuning frequency, manipulating switches on the board for different paths, and changing the different transmit and receive gains are all parts of a radio system which need to be exposed at a library level.

Functionality provided by this library includes:
- Opening/closing devices and querying information about them
- Tuning to various frequencies
- Configuring the sample rate
- Configuring the bandwidth and gain of the RF chain
- Configuring RF gains
- Transmitting and receiving complex baseband samples
- Updating the device's firmware and FPGA
- Low-level access to on-board devices, for testing and debugging

For more information, please generate and view the doxygen documentation. From your host/build directory, run:
```
cmake -DBUILD_DOCUMENTATION=Yes BUILD_LIBBLADERF_DOCUMENTATION=ON ../
make libbladeRF-doxygen
```
The HTML documentation will be placed in **\<build dir\>/libraries/libbladerf/doc/doxygen/html**.  The **index.html** file is the "main" documentation page.

Please see the [CHANGELOG](CHANGELOG) file for a summary of changes across libbladeRF versions.
