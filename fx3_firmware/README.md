# bladeRF FX3 Firmware Source #
The FX3 firmware source is compiled for the Cypress FX3 USB 3.0 Superspeed controller.  This controller converts the USB connection to a 32-bit GPIF-II programmable interface via Cypress' GPIF-II designer software.

The FX3 currently has 3 interfaces associated with it:

  - Cyclone IV FPGA Loader
  - RF Link with the LMS6002D
  - Control Interface

The FPGA loader is responsible for loading, or reloading, the FPGA over the USB link.

The RF link ushers the baseband IQ samples between the FPGA and the host over the USB link.

The control interface uses the built in UART to talk to the FPGA in an out-of-band way for checking status and configuring the system in general.  Some messages that go over this are general GPIO settings, LMS SPI messages, VTCXO trim DAC settings and the Si5338 clock generator programming.

## Building ##
Building the FX3 firmware first requires the download of the [Cypress FX3 SDK][cypress_sdk] which may require registration on their website.  The FX3 uses ThreadX as an RTOS for the ARM9 which is distributed with their SDK and linked to from our software.

[cypress_sdk]: http://www.cypress.com/?rID=57990 (Cypress FX3 SDK)

Once the SDK is downloaded, building the firmware requires a file defining the toolchain in `make/toolchain.mk`.  A sample can be found in the `make` directory.

1. Create your own `toolchain.mk` file from the example given: `cp make/toolchain.mk.sample make/toolchain.mk`
2. Modify your `make/toolchain.mk` to declare an `FX3_ROOT` where the SDK was installed.
3. Create and enter a `build/` directory
4. Run `cmake ../`
5. Compile the firmware via `make`.
6. The file `bladeRF.img` should have been produced with a note saying that 256 bytes of interrupt vector code have been removed.  This warning is normal and you have just built the FX3 firmware successfully!

### Eclipse ###
The FX3 SDK provides an Eclipse-based IDE. A project file for this tool is provided in this directory. To import this project, perform the following steps.

1. Start the `Eclipse` IDE provided with the FX3 SDK, navigate to your workspace, and switch to the C/C++ perspective.
2. From the C/C++ Projects view, select `Import...` --> `General` -> `Existing Projects into Workspace`. Click `Next >`."
3. Under `Select root directory`, select this directory (`bladeRF/fx3_firmware`)
4. You should see `bladeRF` listed under `Projects`. Ensure this is checked and click `Finish`.
5. Linux users only -- (Windows users skip this step) -- From the C/C++ Projects view, right click bladeRF and select `Properties`. Under `C/C++ Build` --> `Builder Settings`, uncheck `Use default build command` and change `cs-make` to `make`. cs-make is provided with the Windows SDK. Linux users may simply use the make program provided by their distribution.
6. Kick off the build! The results here should be the same as those obtained from the command-line build outlined in the previous section.

For more information about developing and debugging FX3 firmware, see the [FX3 Programmer's Manual][fx3_prog_manual].

[fx3_prog_manual]: http://www.cypress.com/?rID=52250  (FX3 Programmer's Manual)

## Pre-built Firmware Binaries ##
Pre-built binaries are available at: http://www.nuand.com/fx3

The latest image is pointed to by: http://www.nuand.com/fx3/latest.img
