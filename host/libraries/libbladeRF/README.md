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
cmake -DBUILD_DOCUMENTATION=ON ../
make libbladeRF-doxygen
```
The HTML documentation will be placed in **\<build dir\>/libraries/libbladerf/doc/doxygen/html**.  The **index.html** file is the "main" documentation page.

Please see the [CHANGELOG](CHANGELOG) file for a summary of changes across libbladeRF versions.

## Build Variables ##

Below is a list of useful and project-specific CMake options. Please see the CMake [variable list] in CMake's documentation for
more information.

| Option                                            | Description
| ------------------------------------------------- |:---------------------------------------------------------------------------------------------------------------------|
| -DBUILD_LIBBLADERF_DOCUMENTATION=\<ON/OFF\>       | Builds API documentation using Doxygen.  Default: equal to BUILD_DOCUMENTATION                                       |
| -DENABLE_BACKEND_USB=\<ON/OFF\>                   | Enables USB backends in libbladeRF.  Default: ON                                                                     |
| -DENABLE_BACKEND_LIBUSB=\<ON/OFF\>                | Enables libusb backend. Default: ON if libusb is available, OFF otherwise.                                           |
| -DENABLE_BACKEND_CYAPI=\<ON/OFF\>a                | Enables (Windows-only) Cypress driver/library based backend. Default: ON if the FX3 SDK is available, OFF otherwise. |
| -DENABLE_BACKEND_DUMMY=\<ON/OFF\>                 | Enables dummy backend support.  Only useful for some developers.  Default: OFF                                       |
| -DENABLE_LIBBLADERF_LOGGING=\<ON/OFF\>            | Enable log messages.  Default: ON                                                                                    |
| -DENABLE_LIBBLADERF_SYSLOG=\<ON/OFF\>             | Enable log messages to syslog (Linux/OSX) if ENABLE_LIBBLADERF_LOGGING is enabled. Default: OFF                      |
| -DENABLE_LIBBLADERF_SYNC_LOG_VERBOSE=\<ON/OFF\>   | Enable log_verbose() calls in the sync interface's data path. Note that this may harm performance. Default: OFF      |
| -DENABLE_LOCK_CHECKS=\<ON/OFF\>                   | Enable checks for lock acquistion failures (e.g., deadlock). Default: OFF                                            |
| -DENABLE_USB_DEV_RESET_ON_OPEN=\<ON/OFF\>         | Enable USB port reset when opening a device. Defaults to ON for Linux, OFF otherwise.                                |
| -DLIBUSB_PATH=\</path/to/libusb\>                 | Path to libusb files. This is generally only needed for Windows users who downloaded binary distributions.           |
| -DLIBBLADERF_SEARCH_PREFIX_OVERRIDE=\<path\>      | Override path prefix used by libbladeRF to search for files (e.g., FPGA bitstreams). If not specified, ${CMAKE_INSTALL_PREFIX} is used as the default search prefix. This may be required when cross-compiling. |
