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

### libusb ###
[libusb] and its development headers are required. \>= v1.0.16 is recommended for Linux, and \>= v1.0.19 is recommended for Windows. 

Ideally, the latest released version is always recommended. Please see the [libusb ChangeLog] for more information.

### (Optional) FX3 SDK - Windows-only ###
Windows users that wish to use a Cypress driver/library based libbladeRF backend will need to download and install the [Cypress FX3 SDK]. 

### CMake ###
[CMake][CMake.org] is used to build the items in this directory.  \>= v2.8.5 is required, but the latest available version is recommended.

### pthreads-win32 - Windows-only ###
Windows users will need to get [pthreads-win32], and may (with newer Windows versions) need to build it from scratch.

## Build ##

### Linux and OSX ###
From this directory, create a ```build``` directory. This is personal preference, of course. It allows for all build files to be cleaned up via a single ```rm -r build/``` command.

Next, run cmake to configure the build, followed by ```make``` and ```sudo make install``` to build and install libraries and applications.

```
mkdir -p build
cd build
cmake [options] ../
make
sudo make install
sudo ldconfig
```

**Fedora Note:** By default, libbladeRF will install to /usr/local/{lib,lib64}, which is [not listed in /etc/ld.so.conf][redhat144967]. On these systems, it is recommended to create a ```/etc/ld.so.conf.d/local.conf``` specifying these directories:

```
sudo tee /etc/ld.so.conf.d/local.conf <<EOF
/usr/local/lib
/usr/local/lib64
EOF
```

Then, run ```sudo ldconfig``` again.

### Windows ###
This process was most recently validated on Windows 10 Enterprise with CMake 3.9.0, libusb 1.0.21, and Microsoft Visual Studio Community 2017 (Version 15.2).

#### bladeRF ####
- Create a ```build``` directory under ```host```
- Run the CMake GUI, pointing the source location to this ```host``` directory and the build directory to the ```build``` directory you just created.
- Click the CMake GUI's "Configure" button. Select the Visual Studio version installed on your system when prompted for a "generator."
    - If you run into any failures, check these items, then re-run Configure:
        - LIBUSB_PATH (and ENABLE_BACKEND_LIBUSB) - for using libusb
        - FX3_SDK_PATH (and ENABLE_BACKEND_CYAPI) - for using the Cypress driver
        - LIBPTHREADSWIN32_PATH
- Adjust options as desired and re-run the "Configure" command as needed. Values in red are new, and should be reviewed before running "Configure" again to get the red out.
- Click the CMake GUI's "Generate" button.
- In the ```build``` directory, a Visual Studio solution will now exist.  You can click "Open Project" to get there.
- Build the solution (right-click `Solution 'bladeRF'` and click "Build Solution")
- Upon building the solution, build artifacts will be present in: ```build\output\<build type>```
    - ```<build_type>``` may refer to Debug or Release, for example

#### pthreads-win32 ####
If you are using a newer version of Windows / Visual Studio and are getting ```error C0211: 'timespec': 'struct' type redefinition``` when trying to build bladeRF, you will need to rebuild pthreads-win32. The below process seems to work with pthreads-w32 2.9.1.

- Get full source tree from [pthreads-win32] website, and unpack someplace convenient
- In Visual Studio, open `pthreads-w32-2-9-1-release\pthreads.2\pthread.dsw` (one-way upgrade is OK)
- Right-click the solution, select "Retarget solution", hit OK.
- Change "Solution Configurations" dropdown to "Release" ("Debug" doesn't work, but that's OK)
- Edit pthread.h to add `#define HAVE_STRUCT_TIMESPEC` and `#define PTW32_ARCHx64` near the top, just below the include guard
- Build the solution

- Back over in the bladeRF CMake:
    - File -> Delete Cache
    - Redo the "Configure" dance as before
    - Point `LIBPTHREADSWIN32_PATH` to the `pthreads-w32-2-9-1-release\pthreads.2` directory you were just working in

## CMake Options and Flags ##

Below is a list of useful and project-specific CMake options. Please see the [CMake documentation] for
more information.

| Option                                    | Description
| ----------------------------------------- |:-----------------------------------------------------------------------------------------------------------------------------------|
| -DCMAKE_INSTALL_PREFIX=\</prefix/path\>   | Override system-specific path to install output files.  Default depends on system; see ```cmake -L```                              |
| -DCMAKE_BUILD_TYPE=\<type\>               | Set <type> to "Debug" to enable a debug build. Default: "Release"                                                                  |
| -DBUILD_DOCUMENTATION=\<ON/OFF\>          | Build libbladeRF API documentation and manpages for utilities. Default: OFF                                                        |
| -DCMAKE_C_COMPILER=\<compiler\>           | Specify the compiler to use. Otherwise, CMake will determine a default.                                                            |
| -DENABLE_GDB_EXTENSIONS=\<ON/OFF\>        | GCC & GDB users may want to set this to use -ggdb3 instead of -g. Default: OFF                                                     |
| -DENABLE_BACKEND_LIBUSB=\<ON/OFF\>        | Enables libusb backend in libbladeRF. Default: ON if libusb is available, OFF otherwise.                                           |
| -DENABLE_BACKEND_CYAPI=\<ON/OFF\>a        | Enables (Windows-only) Cypress driver/library based backend in libbladeRF. Default: ON if the FX3 SDK is available, OFF otherwise. |
| -DENABLE_BACKEND_DUMMY=\<ON/OFF\>         | Enables dummy backend support in libbladeRF.  Only useful for some developers.  Default: OFF                                       |
| -DENABLE_LIBTECLA=\<ON/OFF\>              | Enable libtecla support in the bladeRF-cli program. Default: ON if libtecla is detected, OFF otherwise.                            |
| -DINSTALL_UDEV_RULES=\<ON/OFF\>           | Install udev rules to /etc/udev/rules.d/. Default: ON for Linux, OFF default otherwise.                                            |
| -DUDEV_RULES_PATH=\</path/to/udev/rules\> | Override the path for installing udev rules.  Default: /etc/udev/rules.d                                                           |
| -DBLADERF_GROUP=\<group\>                 | Sets the group associated with the bladeRF in the installed udev rules. Default: ```plugdev```                                     |
| -DTREAT_WARNINGS_AS_ERRORS=\<ON/OFF\>     | Treat compiler warnings as errors. Contributors should keep this enabled. Default: ON                                              |
| -DENABLE_LOG_FILE_INFO=\<ON/OFF\>         | Enable source file and line number information in log messages. Default: ON                                                        |

[cmake]: ./cmake (CMake scripts)
[drivers]: ./drivers (Drivers)
[libraries]: ./libraries (Libraries)
[misc]: ./misc (Miscellaneous)
[utilities]: ./utilities (Utilites)
[libusb]: http://libusb.info/ (libusb project site)
[libusb ChangeLog]: http://log.libusb.info (libusb ChangeLog)
[CMake.org]: http://www.cmake.org/ (CMake)
[variable list]: http://www.cmake.org/cmake/help/v2.8.11/cmake.html#section_Variables (CMake variables)
[CMake documentation]: http://www.cmake.org/cmake/help/documentation.html (Cmake documentation)
[Cypress FX3 SDK]: http://www.cypress.com/?rID=57990 (Cypress FX3 SDK)
[redhat144967]: https://bugzilla.redhat.com/show_bug.cgi?id=144967 (Red Hat Bugzilla - Bug 144967)
[pthreads-win32]: https://sourceware.org/pthreads-win32/ (POSIX Threads for Win32)
