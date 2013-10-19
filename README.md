# bladeRF Source #
This repository contains all the source code required to program and interact with a bladeRF platform, including firmware for the Cypress FX3 USB controller, HDL for the Altera Cyclone IV FPGA, and C code for the host side libraries, drivers, and utilities.
The source is organized as follows:


| Directory         | Description                                                                                       |
| ----------------- |:--------------------------------------------------------------------------------------------------|
| [firmware_common] | Source and header files common between firmware and host software                                 |
| [fx3_firmware]    | Firmware for the Cypress FX3 USB controller                                                       |
| [hdl]             | All HDL code associated with the Cyclone IV FPGA                                                  |
| [host]            | Host-side libraries, drivers, utilities and samples                                               |


## Quick Start ##
1. Clone this repository via: ```git clone https://github.com/Nuand/bladeRF.git```
2. Fetch the latest pre-built bladeRF [FPGA image]. See the README.md in the [hdl] directory for more information.
3. Fetch the latest pre-built bladeRF [firmware image]. See the README.md in the [fx3_firmware] directory for more information.
4. Follow the instructions in the [host] directory to build and install libbladeRF and the bladeRF-cli utility.
5. Attach the bladeRF board to your fastest USB port. After flashing firmware, be sure to press the reset button or unplug/replug the device.
6. If you haven't upgraded your firmware, run ```bladeRF-flash -f <path_to_prebuilt_firmware>```. This upgrade is required to utilize libusb support. Be sure to reset or power-cycle the board after flashing the firmware.
 - If your board is stuck in the FX3 bootloader or is at factory firmware or v1.2, see [bladeRF-flash] for additional details.
7. You should now be able to see your device in the list output via ```bladeRF-cli -p```
8. See the overview of the [bladeRF-cli] for more information about loading the FPGA and using the command line interface tool

For more information, see the [bladeRF wiki]

[firmware_common]: ./firmware_common (Host-Firmware common files)
[fx3_firmware]: ./fx3_firmware (FX3 Firmware)
[hdl]: ./hdl (HDL)
[host]: ./host (Host)
[FPGA image]: http://nuand.com/fpga (Pre-built FPGA images)
[firmware image]: ./fx3_firmware/README.md#pre-built-firmware-binaries (Pre-build firmware binaries)
[bladeRF-cli]: ./host/utilities/bladeRF-cli (bladeRF Command Line Interface)
[bladeRF-flash]: ./host/utilities/bladeRF-flash (bladeRF Flashing Utility)
[bladeRF wiki]: https://github.com/nuand/bladeRF/wiki

