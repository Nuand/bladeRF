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
2. Fetch the latest bladeRF [FPGA image]. See the README.md in the [hdl] directory for more information.
3. Follow the instructions in the [host] directory to build and install libbladeRF and the bladeRF-cli utility.
4. Attach the bladeRF board to your fastest USB port.
5. You should now be able to see your device in the list output via ```bladeRF-cli -p```
6. See the overview of the [bladeRF-cli] for more information about using the command line interface tool

<br>

*Coming Soon: Quick Start information for GNU Radio support via gr-osmosdr*

In the meantime, see the **Building GNURADIO from GIT** section of [kb3gtn's bladeRF setup guide] for information on this.

[firmware_common]: ./firmware_common (Host-Firmware common files)
[fx3_firmware]: ./fx3_firmware (FX3 Firmware)
[hdl]: ./hdl (HDL)
[host]: ./host (Host)
