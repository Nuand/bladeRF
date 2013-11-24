# bladeRF Host Source #

This directory contains the items outlined in the following table. Note that this structure includes some items planned for future development.

| Directory                     | Description                                                                                       |
| ----------------------------- |:--------------------------------------------------------------------------------------------------|
| [cmake][cmake]                | CMake scripts and modules                                                                         |
| [drivers][drivers]            | Device drivers for supported operating systems                                                    |
| [libraries][libraries]        | The main bladeRF support library and supplemental libraries                                       |
| [misc][misc]                  | Miscellaneous files                                                                               |
| [utilities][utilities]        | Applications and tools

## Dependencies ##

### libUSB ###
libusb-1.0 and its development headers are required. \>= v1.0.12 is recommended for Linux, and \>= v1.0.13 is recommended for Windows. Ideally, the
latest released version is always recommended. Please see the [libusb ChangeLog] for more information.

### CMake ###
[CMake][CMake.org] is used to build the items in this directory.  \>= v2.8.3 is required.

#### Ubuntu/Debian: ####
```sudo apt-get install libusb-1.0.0 libusb-1.0.0-dev cmake```

## Build ##
From this directory, create a 'build' directory. This is personal preference, of course. It allows for all build files to be cleaned up via a single ```rm -r build/``` command.
Next, run cmake to configure the build, followed by ```make``` and ```sudo make install``` to build and install libraries and applications.

```
mkdir -p build
cd build
cmake [options] ../
make
sudo make install
```

Below is a list of useful and project-specific CMake options. Please see the CMake [variable list] in CMake's documentation for
more information.

| Option                                    | Description
| ----------------------------------------- |:---------------------------------------------------------------------------------------------------------|
| -DENABLE_HOST_BUILD=\<ON/OFF\>            | Enables building of the Host components overall. Default is ON.                                          |
| -DCMAKE_BUILD_TYPE=\<type\>               | Set <type> to "Debug" to enable a debug build. "Release" is the default.                                 |
| -DBUILD_DOCUMENTATION=\<ON/OFF\>          | Build libbladeRF API documentation and manpages for utilities. OFF by default.                           |
| -DCMAKE_C_COMPILER=\<compiler\>           | Specify the compiler to use. Otherwise, CMake will determine a default.                                  |
| -DENABLE_GDB_EXTENSIONS=\<ON/OFF\>        | GCC & GDB users may want to set this to use -ggdb3 instead of -g. Disabled by default.                   |
| -DENABLE_BACKEND_LIBUSB=\<ON/OFF\>        | Enables libusb backend in libbladeRF. Enabled by default.                                                |
| -DENABLE_BACKEND_LINUX_DRIVER=\<ON/OFF\>  | Enables Linux driver in libbladeRF. Disabled by default.                                                 |
| -DENABLE_LIBTECLA=\<ON/OFF\>              | Enable libtecla support in the bladeRF-cli program. Default: ON if libtecla is detected, OFF otherwise.  |
| -DINSTALL_UDEV_RULES=\<ON/OFF\>           | Install udev rules to /etc/udev/rules.d/. Default: ON for Linux, OFF default otherwise.                  |
| -DBLADERF_GROUP=\<group\>                 | Sets the group associated with the bladeRF in the installed udev rules. The default is plugdev.          |
| -DTREAT_WARNINGS_AS_ERRORS=\<Yes/No\>     | Treat compiler warnings as errors, defaults to Yes. Contributors should keep this enabled.               |

[cmake]: ./cmake (CMake scripts)
[drivers]: ./drivers (Drivers)
[libraries]: ./libraries (Libraries)
[misc]: ./misc (Miscellaneous)
[utilities]: ./utilities (Utilites)
[libusb ChangeLog]: https://github.com/libusbx/libusbx/blob/master/ChangeLog (libusb ChangeLog)
[CMake.org]: http://www.cmake.org/ (CMake)
[variable list]: http://www.cmake.org/cmake/help/v2.8.11/cmake.html#section_Variables (CMake variables)
