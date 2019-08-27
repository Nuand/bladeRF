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
5. Attach the bladeRF board to your fastest USB port.
6. You should now be able to see your device in the list output via ```bladeRF-cli -p```
7. You can view additional information about the device via ```bladeRF-cli -e info -e version```.
8. If any warnings indicate that a firmware update is needed, run:```bladeRF-cli -f <firmware_file>```. 
 - If you ever find the device booting into the FX3 bootloader (e.g., if you unplug the device in the middle of a firmware upgrade), see the ```recovery``` command in bladeRF-cli for additional details.
9. See the overview of the [bladeRF-cli] for more information about loading the FPGA and using the command line interface tool

For more information, see the [bladeRF wiki].

## Build Variables ##

Below are global options to choose which parts of the bladeRF project should
be built from the top level.  Please see the [fx3_firmware] and [host]
subdirectories for more specific options.

| Option                            | Description
| --------------------------------- |:--------------------------------------------------------------------------|
| -DENABLE_FX3_BUILD=\<ON/OFF\>     | Enables building the FX3 firmware. Default: OFF                           |                                   |
| -DENABLE_HOST_BUILD=\<ON/OFF\>    | Enables building the host library and utilities overall. Default: ON      |

[firmware_common]: ./firmware_common (Host-Firmware common files)
[fx3_firmware]: ./fx3_firmware (FX3 Firmware)
[hdl]: ./hdl (HDL)
[host]: ./host (Host)
[FPGA image]: https://www.nuand.com/fpga.php (Pre-built FPGA images)
[firmware image]: https://www.nuand.com/fx3.php (Pre-built firmware binaries)
[bladeRF-cli]: ./host/utilities/bladeRF-cli (bladeRF Command Line Interface)
[bladeRF wiki]: https://github.com/nuand/bladeRF/wiki (bladeRF wiki)
