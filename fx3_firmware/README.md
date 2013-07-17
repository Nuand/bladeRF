# bladeRF FX3 Firmware Source #
The FX3 firmware source is compiled for the Cypress FX3 USB 3.0 Superspeed controller.  This controller converts the USB connection to a 32-bit GPIF-II programmable interface via Cypress' GPIF-II designer software.

The FX3 currently has 3 interfaces associated with it:

  - Cyclone IV FPGA Loader
  - RF Link with the LMS6002D
  - Control Interface

The FPGA loder is responsible for loading, or reloading, the FPGA over the USB link.

The RF link ushers the baseband IQ samples between the FPGA and the host over the USB link.

The control interface uses the built in UART to talk to the FPGA in an out-of-band way for checking status and configuring the system in general.  Some messages that go over this are general GPIO settings, LMS SPI messages, VTCXO trim DAC settings and the Si5338 clock generator programming.

## Building ##
Building the FX3 firmware first requires the download of the [Cypress FX3 SDK][cypress_sdk] which may require registration on their website.  The FX3 uses ThreadX as an RTOS for the ARM9 which is distributed with their SDK and linked to from our software.

[cypress_sdk]: http://www.cypress.com/?rID=57990 (Cypress FX3 SDK)

Once the SDK is downloaded, building the firmware requires a file defining the toolchain in `make/toolchain.mk`.  A sample can be found in the `make` directory.

1. Create your own `toolchain.mk` file from the example given: `cp make/toolchain.mk.sample make/toolchain.mk`
1. Modify your `make/toolchain.mk` to declare an `FX3_ROOT` where the SDK was installed.
1. Compile the firmware using `make`.  Use `DEBUG=yes` if you want to include debug symbols.
1. The file `bladeRF.img` should have been produced with a note saying that 256 bytes of interrupt vector code have been removed.  This warning is normal and you have just built the FX3 firmware successfully!

## Pre-built Firmware Binaries ##
Pre-built binaries are not available in a specific place yet due to the volatility of the current firmware.

