# bladeRF FX3 Firmware Source #
The FX3 firmware source is compiled for the Cypress FX3 USB 3.0 Superspeed controller.  This controller converts the USB connection to a 32-bit GPIF-II programmable interface via Cypress' GPIF-II designer software.

The FX3 currently has 3 interfaces associated with it:

  - Cyclone IV FPGA Loader
  - RF Link with the LMS6002D
  - Control Interface

The FPGA loader is responsible for loading, or reloading, the FPGA over the USB link.

The RF link transfers baseband IQ samples between the FPGA and the host over the USB link.

The control interface uses the built in UART to communicate with the FPGA.  Messages sent over this interface include general GPIO settings, LMS6002D register accesses, VCTCXO trim DAC settings and the Si5338 clock generator register accesses.

## Building ##
Building the FX3 firmware first requires the download of the [Cypress FX3 SDK][cypress_sdk]. This is free (monetarily) and may require registration on the Cypress website. The FX3 uses ThreadX as an RTOS for the ARM9 which is distributed with their SDK and linked to from our software.

**This firmware build requires FX3 SDK version 1.3.3 or later**

[cypress_sdk]: http://www.cypress.com/?rID=57990 (Cypress FX3 SDK)

[CMake](https://cmake.org) is used to configure the build. There are two locations that we must provide to CMake when configuring the build:

1. The path to the FX3 SDK, via an `FX3_INSTALL_PATH` definition. For Windows FX3 SDK installations, this is generally already set in the environment.
2. The path to the toolchain description file in `cmake/fx3-toolchain.cmake`

### Command Line-Based Build (Recommended for Linux Users) ###

Below are instructions for building the FX3 firmware from the command-line. This assumes that `cmake` and `make`
are in your `PATH`, and is best suited for Linux developers.

Windows users may wish to skip to the next section for a GUI-based build. (Note that a command-line build is possible with the Code Sorcery tools
 provided in the FX3 SDK, however. The programs have a `cs-` prefix on many of them, such as `cs-make`.)

Adjust the paths as necessary on your system, depending where you installed the FX3 SDK.

```
$ mkdir build
$ cd build
$ cmake -DFX3_INSTALL_PATH=/opt/cypress/fx3_sdk -DCMAKE_TOOLCHAIN_FILE=../cmake/fx3-toolchain.cmake ../
$ make
```

When the firmware build is completed, you should have an `output` directory containing `bladeRF_fw_v<VERSION>.img`.

Note that it is still possible to use Eclipse to develop and debug if you followed the above steps. See the *Creating an Eclipse Project* section below.

### GUI-Based Build (Recommended for Windows Users) ###

The GUI-based build procedure is better suited for Windows users.

1. Run the CMake GUI
2. Fill in path to the source. For example: <br> `C:/Users/jon/Documents/projects/nuand/bladeRF/fx3_firmware`
3. Set the path to the binaries directory to `build`.<br> For example: `C:/Users/jon/Documents/projects/nuand/bladeRF/fx3_firmware/build`
4. Click `Add Entry`. An `Add Cache Entry` dialog will appear.
5. Set `Name` to `CMAKE_MAKE_PROGRAM`
6. Set `Type` to `FILEPATH`
7. For `Value`, use the `...` button to locate the path to `cs-make.exe`.  For a standard installation, this would be: <br> `C:\Program Files (x86)\Cypress\EZ-USB FX3 SDK\1.3\ARM GCC\bin\cs-make.exe`
8. Click `OK`. The `Description` field does not need to be filled in.
9. Click `Configure`. A dialog will appear, asking you to specify the Generator for this project.
10. Under `Select Generator...`, select `Unix Makefiles`
11. Choose `Specify toolchain file for cross-compiling`
12. Click `Next`.
13. Select the path to `cmake/fx3-toolchain.cmake`.  For example: <br> `C:/Users/jon/Documents/projects/nuand/bladeRF/fx3_firmware/cmake/fx3-toolchain.cmake`
14. Click `Finish`. You will see that a number of variables have been added to the CMake Cache.
15. Click `Configure`. This should print `Configuring done` when complete.
16. Click `Generate`. This should pring `Generating done` when complete.
17. There should now be an `fx3_firmware/build` directory containing a `Makefile`

The Cypress FX3 Installer creates an `FX3_INSTALL_PATH` environment variable that specifies the path to the FX3 SDK. This is used
by default above configuration. If you encounter issues in the above procedure, check that this environment variable is defined by echoing it from `cmd.exe`. If it is not present, you can add this in the same way you added the `CMAKE_MAKE_PROGRAM` variable definition. An example of this definition is: <br>
`C:/Program Files (x86)/Cypress/EZ-USB FX3 SDK/1.3`

### Creating an Eclipse Project ###

The FX3 SDK provides an Eclipse-based IDE. A pre-configured project file for this tool is provided in this `fx3_firmware` directory. To import this project, perform the following steps.

1. Start the `Eclipse` IDE provided with the FX3 SDK, navigate to your workspace, and switch to the C/C++ perspective.
2. From the C/C++ Projects view, select: <br> `Import...` --> `General` -> `Existing Projects into Workspace`.
3. Click `Next`.
3. Under `Select root directory`, select this directory (`bladeRF/fx3_firmware`)
4. You should see `bladeRF` listed under `Projects`. Ensure this is checked and click `Finish`.
5. **Linux users only**: From the C/C++ Projects view, right click bladeRF and select `Properties`. Under `C/C++ Build` --> `Builder Settings`, uncheck `Use default build command` and change `cs-make` to `make`. cs-make is provided with the Windows SDK. Linux users may simply use the `make` program provided by their distribution.
6. Kick off the build! The resulting `build/output/bladeRF_fw_v<VERSION>.img` should be the same as those obtained from the command-line build outlined in the previous section.

For more information about developing and debugging FX3 firmware, see the [FX3 Programmer's Manual][fx3_prog_manual].

[fx3_prog_manual]: http://www.cypress.com/?rID=52250  (FX3 Programmer's Manual)

## Pre-built Firmware Binaries ##
Pre-built binaries are available at: http://www.nuand.com/fx3

The latest image is pointed to by: http://www.nuand.com/fx3/latest.img
